#pragma once

#include <memory>
#include <string>

#include "envoy/extensions/filters/network/rocketmq_proxy/v3/rocketmq_proxy.pb.h"
#include "envoy/extensions/filters/network/rocketmq_proxy/v3/rocketmq_proxy.pb.validate.h"

#include "extensions/filters/network/common/factory_base.h"
#include "extensions/filters/network/rocketmq_proxy/conn_manager.h"
#include "extensions/filters/network/rocketmq_proxy/router/route_matcher.h"
#include "extensions/filters/network/rocketmq_proxy/router/router_impl.h"
#include "extensions/filters/network/well_known_names.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace RocketmqProxy {

class RocketmqProxyFilterConfigFactory
    : public Common::FactoryBase<
          envoy::extensions::filters::network::rocketmq_proxy::v3::RocketmqProxy> {
public:
  RocketmqProxyFilterConfigFactory() : FactoryBase(NetworkFilterNames::get().RocketmqProxy, true) {}

private:
  Network::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::extensions::filters::network::rocketmq_proxy::v3::RocketmqProxy& proto_config,
      Server::Configuration::FactoryContext& context) override;
};

// Config 的实现类
class ConfigImpl : public Config, public Router::Config, Logger::Loggable<Logger::Id::config> {
public:
  using RocketmqProxyConfig =
      envoy::extensions::filters::network::rocketmq_proxy::v3::RocketmqProxy;

  ConfigImpl(const RocketmqProxyConfig& config, Server::Configuration::FactoryContext& context);
  ~ConfigImpl() override = default;

  // Config
  RocketmqFilterStats& stats() override { return stats_; }
  Upstream::ClusterManager& clusterManager() override { return context_.clusterManager(); }
  Router::RouterPtr createRouter() override {
    // 直接用 context_.clusterManager() 来初始化 RouterImpl
    return std::make_unique<Router::RouterImpl>(context_.clusterManager());
  }
  bool developMode() const override { return develop_mode_; }

  std::chrono::milliseconds transientObjectLifeSpan() const override {
    return transient_object_life_span_;
  }

  std::string proxyAddress() override;
  // 这里返回的是 RocketmqProxy::Config, 但是该类是 Router::Config 的子类。
  Router::Config& routerConfig() override { return *this; }

  // Router::Config
  Router::RouteConstSharedPtr route(const MessageMetadata& metadata) const override;

private:
  Server::Configuration::FactoryContext& context_;
  const std::string stats_prefix_;
  RocketmqFilterStats stats_;
  Router::RouteMatcherPtr route_matcher_;
  const bool develop_mode_;
  // 瞬态对象存活的最大持续时间，建议超过 10s。
  std::chrono::milliseconds transient_object_life_span_;

  // 默认 30s
  static constexpr uint64_t TransientObjectLifeSpan = 30 * 1000;
};

} // namespace RocketmqProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
