#pragma once

#include "envoy/tcp/conn_pool.h"
#include "envoy/upstream/cluster_manager.h"
#include "envoy/upstream/thread_local_cluster.h"

#include "common/common/logger.h"
#include "common/upstream/load_balancer_impl.h"

#include "extensions/filters/network/rocketmq_proxy/router/router.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace RocketmqProxy {
namespace Router {

class RouterImpl : public Router, public Logger::Loggable<Logger::Id::rocketmq> {
public:
  explicit RouterImpl(Upstream::ClusterManager& cluster_manager);

  ~RouterImpl() override;

  // Tcp::ConnectionPool::UpstreamCallbacks
  void onUpstreamData(Buffer::Instance& data, bool end_stream) override;
  void onAboveWriteBufferHighWatermark() override;
  void onBelowWriteBufferLowWatermark() override;
  void onEvent(Network::ConnectionEvent event) override;

  // Upstream::LoadBalancerContextBase
  const Envoy::Router::MetadataMatchCriteria* metadataMatchCriteria() override;

  void sendRequestToUpstream(ActiveMessage& active_message) override;

  void reset() override;

  Upstream::HostDescriptionConstSharedPtr upstreamHost() override;

private:
  class UpstreamRequest : public Tcp::ConnectionPool::Callbacks {
  public:
    UpstreamRequest(RouterImpl& router);

    void onPoolFailure(Tcp::ConnectionPool::PoolFailureReason reason,
                       Upstream::HostDescriptionConstSharedPtr host) override;

    void onPoolReady(Tcp::ConnectionPool::ConnectionDataPtr&& conn,
                     Upstream::HostDescriptionConstSharedPtr host) override;

  private:
    RouterImpl& router_;
  };
  using UpstreamRequestPtr = std::unique_ptr<UpstreamRequest>;

  Upstream::ClusterManager& cluster_manager_;
  Tcp::ConnectionPool::ConnectionDataPtr connection_data_;

  /**
   * On requesting connection from upstream connection pool, this handle may be assigned when no
   * connection is readily available at the moment. We may cancel the request through this handle.
   *
   * If there are connections which can be returned immediately, this handle is assigned as nullptr.
   */
  // 在从上游连接池请求连接时，可能会在此时没有连接可用时分配此句柄。我们可以通过此句柄取消请求。
  // 如果有可以立即返回的连接，则将此句柄分配为 nullptr。
  Tcp::ConnectionPool::Cancellable* handle_;
  Upstream::HostDescriptionConstSharedPtr upstream_host_;
  ActiveMessage* active_message_;
  Upstream::ClusterInfoConstSharedPtr cluster_info_;
  UpstreamRequestPtr upstream_request_;
  // 默认空值
  const RouteEntry* route_entry_{};
};
} // namespace Router
} // namespace RocketmqProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
