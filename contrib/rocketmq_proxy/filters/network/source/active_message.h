#pragma once

#include "envoy/event/deferred_deletable.h"
#include "envoy/network/connection.h"
#include "envoy/network/filter.h"
#include "envoy/stats/timespan.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/linked_object.h"
#include "source/common/common/logger.h"

#include "absl/types/optional.h"
#include "contrib/rocketmq_proxy/filters/network/source/codec.h"
#include "contrib/rocketmq_proxy/filters/network/source/protocol.h"
#include "contrib/rocketmq_proxy/filters/network/source/router/router.h"
#include "contrib/rocketmq_proxy/filters/network/source/topic_route.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace RocketmqProxy {

class ConnectionManager;

/**
 * ActiveMessage represents an in-flight request from downstream that has not yet received response
 * from upstream.
 */
// ActiveMessage 表示来自下游但尚未收到来自上游的响应的正在进行的请求。
// 它包含了多个结构体: connection_manager_, request, response, metadata_, router_
// 我感觉这个 ActiveMessage 可以对应到 http 的 activeStream。
// 这个结构体非常的关键, 需要仔细阅读其代码
class ActiveMessage : public LinkedObject<ActiveMessage>,
                      public Event::DeferredDeletable,
                      Logger::Loggable<Logger::Id::rocketmq> {
public:
  ActiveMessage(ConnectionManager& conn_manager, RemotingCommandPtr&& request);

  ~ActiveMessage() override;

  /**
   * Set up filter-chain according to configuration from bootstrap config file and dynamic
   * configuration items from Pilot.
   */
   // 生成 network filterChain
  void createFilterChain();

  /**
   * Relay requests from downstream to upstream cluster. If the target cluster is absent at the
   * moment, it triggers cluster discovery service request and mark awaitCluster as true.
   * ClusterUpdateCallback will process requests marked await-cluster once the target cluster is
   * in place.
   */
   // 将请求从下游发送到上游集群。如果此时目标集群不存在，则触发集群发现服务请求，并将awaitCluster标记为true。
   // 一旦目标集群就位，ClusterUpdateCallback 将处理标记为 await-cluster 的请求。
  void sendRequestToUpstream();

  // 获取 request, 直接返回 request_ 即可。
  const RemotingCommandPtr& downstreamRequest() const;

  /**
   * Parse pop response and insert ack route directive such that ack requests will be forwarded to
   * the same broker host from which messages are popped.
   * @param buffer Pop response body.
   * @param group Consumer group name.
   * @param topic Topic from which messages are popped
   * @param directive ack route directive
   */
   // 解析 pop 响应并插入 ack 路由指令，这样 ack 请求将被转发到弹出消息的同一代理主机。
   // POP 是 RocketMQ 5.0 新增的消费模式
  virtual void fillAckMessageDirective(Buffer::Instance& buffer, const std::string& group,
                                       const std::string& topic,
                                       const AckMessageDirective& directive);

  // 将 response 发送给下游
  virtual void sendResponseToDownstream();

  void onQueryTopicRoute();

  virtual void onError(absl::string_view error_message);

  ConnectionManager& connectionManager() { return connection_manager_; }

  virtual void onReset();

  bool onUpstreamData(Buffer::Instance& data, bool end_stream,
                      Tcp::ConnectionPool::ConnectionDataPtr& conn_data);

  virtual MessageMetadataSharedPtr metadata() const { return metadata_; }

  virtual Router::RouteConstSharedPtr route();

  void recordPopRouteInfo(Upstream::HostDescriptionConstSharedPtr host_description);

  static void fillBrokerData(std::vector<BrokerData>& list, const std::string& cluster,
                             const std::string& broker_name, int64_t broker_id,
                             const std::string& address);

private:
  ConnectionManager& connection_manager_;
  RemotingCommandPtr request_;
  RemotingCommandPtr response_;
  MessageMetadataSharedPtr metadata_;
  Router::RouterPtr router_;
  absl::optional<Router::RouteConstSharedPtr> cached_route_;

  void updateActiveRequestStats(bool is_inc = true);
};

using ActiveMessagePtr = std::unique_ptr<ActiveMessage>;

} // namespace RocketmqProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
