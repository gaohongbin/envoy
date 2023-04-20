#include "extensions/filters/network/rocketmq_proxy/router/router_impl.h"

#include "common/common/enum_to_int.h"

#include "extensions/filters/network/rocketmq_proxy/active_message.h"
#include "extensions/filters/network/rocketmq_proxy/codec.h"
#include "extensions/filters/network/rocketmq_proxy/conn_manager.h"
#include "extensions/filters/network/rocketmq_proxy/protocol.h"
#include "extensions/filters/network/rocketmq_proxy/well_known_names.h"
#include "extensions/filters/network/well_known_names.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace RocketmqProxy {
namespace Router {

// TODO 了解一下 Envoy::Upstream::ClusterManager 的实现及更新逻辑 (CDS 是怎么更新的)
RouterImpl::RouterImpl(Envoy::Upstream::ClusterManager& cluster_manager)
    : cluster_manager_(cluster_manager), handle_(nullptr), active_message_(nullptr) {}

RouterImpl::~RouterImpl() {
  if (handle_) {
    handle_->cancel(Tcp::ConnectionPool::CancelPolicy::Default);
  }
}

// 返回最终选择的上游节点
Upstream::HostDescriptionConstSharedPtr RouterImpl::upstreamHost() { return upstream_host_; }

void RouterImpl::onAboveWriteBufferHighWatermark() {
  ENVOY_LOG(trace, "Above write buffer high watermark");
}

void RouterImpl::onBelowWriteBufferLowWatermark() {
  ENVOY_LOG(trace, "Below write buffer low watermark");
}

// 不管是哪种 Event 事件发生, 该 active_message_ 都需要重置清除。
void RouterImpl::onEvent(Network::ConnectionEvent event) {
  switch (event) {
  case Network::ConnectionEvent::RemoteClose: {
    ENVOY_LOG(error, "Connection to upstream: {} is closed by remote peer",
              upstream_host_->address()->asString());
    // Send local reply to downstream
    active_message_->onError("Connection to upstream is closed by remote peer");
    break;
  }
  case Network::ConnectionEvent::LocalClose: {
    ENVOY_LOG(error, "Connection to upstream: {} has been closed",
              upstream_host_->address()->asString());
    // Send local reply to downstream
    active_message_->onError("Connection to upstream has been closed");
    break;
  }
  default:
    // Ignore other events for now
    ENVOY_LOG(trace, "Ignore event type");
    return;
  }
  active_message_->onReset();
}

const Envoy::Router::MetadataMatchCriteria* RouterImpl::metadataMatchCriteria() {
  if (route_entry_) {
    return route_entry_->metadataMatchCriteria();
  }
  return nullptr;
}

// 上游返回 rsp 数据时, 进行回调
void RouterImpl::onUpstreamData(Buffer::Instance& data, bool end_stream) {
  ENVOY_LOG(trace, "Received some data from upstream: {} bytes, end_stream: {}", data.length(),
            end_stream);
  if (active_message_->onUpstreamData(data, end_stream, connection_data_)) {
    reset();
  }
}

 // 将 request 发送给上游
 // 主要逻辑: 通过上游 cluster, 获取 TCP 连接池, 再通过 request 生成一个和上游连接的 connection。
 // 所以关键在 TCP 连接池的代码
void RouterImpl::sendRequestToUpstream(ActiveMessage& active_message) {
  active_message_ = &active_message;
  int opaque = active_message_->downstreamRequest()->opaque();
  ASSERT(active_message_->metadata()->hasTopicName());
  std::string topic_name = active_message_->metadata()->topicName();

  // 通过 proto config 文件获取 route_config, 生成 routeMatcher, 然后再生成 route
  // 这里的 route 最终是根据 proto config 文件生成的
  RouteConstSharedPtr route = active_message.route();
  if (!route) {
    active_message.onError("No route for current request.");
    ENVOY_LOG(warn, "Can not find route for topic {}", topic_name);
    reset();
    return;
  }

  route_entry_ = route->routeEntry();
  // 根据 route 返回的上游 clusterName,  从 cluster_manager_ 中获取 cluster 详细信息。
  const std::string cluster_name = route_entry_->clusterName();
  Upstream::ThreadLocalCluster* cluster = cluster_manager_.getThreadLocalCluster(cluster_name);
  if (!cluster) {
    active_message.onError("Cluster does not exist.");
    ENVOY_LOG(warn, "Cluster for {} is not available", cluster_name);
    reset();
    return;
  }

  cluster_info_ = cluster->info();
  // cluster 正在维护
  if (cluster_info_->maintenanceMode()) {
    ENVOY_LOG(warn, "Cluster {} is under maintenance. Opaque: {}", cluster_name, opaque);
    active_message.onError("Cluster under maintenance.");
    active_message.connectionManager().stats().maintenance_failure_.inc();
    reset();
    return;
  }

  // TODO 获取 TCP 连接池, TCP 连接池的代码需要看看, 一些长链接的选择算法在这里
  Tcp::ConnectionPool::Instance* conn_pool =
      cluster->tcpConnPool(Upstream::ResourcePriority::Default, this);
  if (!conn_pool) {
    ENVOY_LOG(warn, "No host available for cluster {}. Opaque: {}", cluster_name, opaque);
    active_message.onError("No host available");
    reset();
    return;
  }

  // 看看连接池的代码, 这里的逻辑
  upstream_request_ = std::make_unique<UpstreamRequest>(*this);
  Tcp::ConnectionPool::Cancellable* cancellable = conn_pool->newConnection(*upstream_request_);
  if (cancellable) {
    handle_ = cancellable;
    ENVOY_LOG(trace, "No connection is available for now. Create a cancellable handle. Opaque: {}",
              opaque);
  } else {
    /*
     * UpstreamRequest#onPoolReady or #onPoolFailure should have been invoked.
     */
    ENVOY_LOG(trace,
              "One connection is picked up from connection pool, callback should have been "
              "executed. Opaque: {}",
              opaque);
  }
}

// 初始化 UpstreamRequest
RouterImpl::UpstreamRequest::UpstreamRequest(RouterImpl& router) : router_(router) {}

// UpstreamRequest 的两个回调接口。
// onPoolReady 和 onPoolFailure
void RouterImpl::UpstreamRequest::onPoolReady(Tcp::ConnectionPool::ConnectionDataPtr&& conn,
                                              Upstream::HostDescriptionConstSharedPtr host) {
  router_.connection_data_ = std::move(conn);
  router_.upstream_host_ = host;
  router_.connection_data_->addUpstreamCallbacks(router_);
  if (router_.handle_) {
    ENVOY_LOG(trace, "#onPoolReady, reset cancellable handle to nullptr");
    router_.handle_ = nullptr;
  }
  ENVOY_LOG(debug, "Current chosen host address: {}", host->address()->asString());
  // TODO(lizhanhui): we may optimize out encoding in case we there is no protocol translation.
  Buffer::OwnedImpl buffer;
  // 将 downstreamRequest 编码后写入 connection_data_
  Encoder::encode(router_.active_message_->downstreamRequest(), buffer);
  router_.connection_data_->connection().write(buffer, false);
  ENVOY_LOG(trace, "Write data to upstream OK. Opaque: {}",
            router_.active_message_->downstreamRequest()->opaque());

  // 针对 oneWay 的 req 进行处理
  if (router_.active_message_->metadata()->isOneWay()) {
    ENVOY_LOG(trace,
              "Reset ActiveMessage since data is written and the downstream request is one-way. "
              "Opaque: {}",
              router_.active_message_->downstreamRequest()->opaque());

    // For one-way ack-message requests, we need erase previously stored ack-directive.
    if (enumToSignedInt(RequestCode::AckMessage) ==
        router_.active_message_->downstreamRequest()->code()) {
      auto ack_header = router_.active_message_->downstreamRequest()
                            ->typedCustomHeader<AckMessageRequestHeader>();
      router_.active_message_->connectionManager().eraseAckDirective(ack_header->directiveKey());
    }

    router_.reset();
  }
}

// 连接池准备失败
void RouterImpl::UpstreamRequest::onPoolFailure(Tcp::ConnectionPool::PoolFailureReason reason,
                                                Upstream::HostDescriptionConstSharedPtr host) {
  if (router_.handle_) {
    ENVOY_LOG(trace, "#onPoolFailure, reset cancellable handle to nullptr");
    router_.handle_ = nullptr;
  }
  switch (reason) {
  case Tcp::ConnectionPool::PoolFailureReason::Overflow: {
    ENVOY_LOG(error, "Unable to acquire a connection to send request to upstream");
    router_.active_message_->onError("overflow");
  } break;

  case Tcp::ConnectionPool::PoolFailureReason::RemoteConnectionFailure: {
    ENVOY_LOG(error, "Failed to make request to upstream due to remote connection error. Host {}",
              host->address()->asString());
    router_.active_message_->onError("remote connection failure");
  } break;

  case Tcp::ConnectionPool::PoolFailureReason::LocalConnectionFailure: {
    ENVOY_LOG(error, "Failed to make request to upstream due to local connection error. Host: {}",
              host->address()->asString());
    router_.active_message_->onError("local connection failure");
  } break;

  case Tcp::ConnectionPool::PoolFailureReason::Timeout: {
    ENVOY_LOG(error, "Failed to make request to upstream due to timeout. Host: {}",
              host->address()->asString());
    router_.active_message_->onError("timeout");
  } break;
  }

  // Release resources allocated to this request.
  router_.reset();
}

// 这里 reset 两个地方
// 1、 active_message 进行 reset
// 2、 connection_data_ 进行 reset
void RouterImpl::reset() {
  active_message_->onReset();
  if (connection_data_) {
    connection_data_.reset(nullptr);
  }
}

} // namespace Router
} // namespace RocketmqProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
