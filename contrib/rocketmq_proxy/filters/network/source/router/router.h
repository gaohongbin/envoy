#pragma once

#include "envoy/tcp/conn_pool.h"

#include "source/common/upstream/load_balancer_impl.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace RocketmqProxy {

class ActiveMessage;
class MessageMetadata;

namespace Router {

/**
 * RouteEntry is an individual resolved route entry.
 */
 // RouteEntry 是一个单独解析的路由条目。
 // 单独看这个注解, 云里雾里, 还是看代码吧
class RouteEntry {
public:
  virtual ~RouteEntry() = default;

  /**
   * @return const std::string& the upstream cluster that owns the route.
   */
   // 上游集群的名称
  virtual const std::string& clusterName() const PURE;

  /**
   * @return MetadataMatchCriteria* the metadata that a subset load balancer should match when
   * selecting an upstream host
   */
   // 选择符合要求的一群上游节点
  virtual const Envoy::Router::MetadataMatchCriteria* metadataMatchCriteria() const PURE;
};

/**
 * Route holds the RouteEntry for a request.
 */
 // Route 拥有 request 的 RouteEntry。
class Route {
public:
  virtual ~Route() = default;

  /**
   * @return the route entry or nullptr if there is no matching route for the request.
   */
   // 满足匹配 request 的 RouteEntry
  virtual const RouteEntry* routeEntry() const PURE;
};

using RouteConstSharedPtr = std::shared_ptr<const Route>;
using RouteSharedPtr = std::shared_ptr<Route>;

/**
 * The router configuration.
 */
 // 该 Config 是在 Router namespace 下面。
 // router 的配置信息
class Config {
public:
  virtual ~Config() = default;

  virtual RouteConstSharedPtr route(const MessageMetadata& metadata) const PURE;
};

// 这个是 Router, 上面是 Route
class Router : public Tcp::ConnectionPool::UpstreamCallbacks,
               public Upstream::LoadBalancerContextBase {

public:
  virtual void sendRequestToUpstream(ActiveMessage& active_message) PURE;

  /**
   * Release resources associated with this router.
   */
   // 释放与该 router 绑定的资源。
  virtual void reset() PURE;

  /**
   * Return host description that is eventually connected.
   * @return upstream host if a connection has been established; nullptr otherwise.
   */
  // 返回最终连接的上游主机
  virtual Upstream::HostDescriptionConstSharedPtr upstreamHost() PURE;
};

using RouterPtr = std::unique_ptr<Router>;
} // namespace Router
} // namespace RocketmqProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
