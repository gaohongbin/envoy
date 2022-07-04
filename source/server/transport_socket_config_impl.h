#pragma once

#include "envoy/server/transport_socket_config.h"
#include "envoy/stats/scope.h"

namespace Envoy {
namespace Server {
namespace Configuration {

/**
 * Implementation of TransportSocketFactoryContext.
 */
class TransportSocketFactoryContextImpl : public TransportSocketFactoryContext {
public:
  TransportSocketFactoryContextImpl(Server::Configuration::ServerFactoryContext& server_context,
                                    Ssl::ContextManager& context_manager, Stats::Scope& stats_scope,
                                    Upstream::ClusterManager& cm, Stats::Store& stats,
                                    ProtobufMessage::ValidationVisitor& validation_visitor,
                                    std::shared_ptr<Envoy::TcloudMap::TcloudMap<std::string, std::string, Envoy::TcloudMap::LFUCachePolicy>> tcloud_map = nullptr)
      : server_context_(server_context), context_manager_(context_manager),
        stats_scope_(stats_scope), cluster_manager_(cm), stats_(stats),
        validation_visitor_(validation_visitor), tcloud_map_(tcloud_map) {}

  /**
   * Pass an init manager to register dynamic secret provider.
   * @param init_manager instance of init manager.
   */
  void setInitManager(Init::Manager& init_manager) { init_manager_ = &init_manager; }

  // TransportSocketFactoryContext
  Server::Admin& admin() override { return server_context_.admin(); }
  Ssl::ContextManager& sslContextManager() override { return context_manager_; }
  Stats::Scope& scope() override { return stats_scope_; }
  Secret::SecretManager& secretManager() override {
    return clusterManager().clusterManagerFactory().secretManager();
  }
  Upstream::ClusterManager& clusterManager() override { return cluster_manager_; }
  const LocalInfo::LocalInfo& localInfo() const override { return server_context_.localInfo(); }
  Event::Dispatcher& mainThreadDispatcher() override {
    return server_context_.mainThreadDispatcher();
  }
  Stats::Store& stats() override { return stats_; }
  Init::Manager& initManager() override {
    ASSERT(init_manager_ != nullptr);
    return *init_manager_;
  }
  Singleton::Manager& singletonManager() override { return server_context_.singletonManager(); }
  ThreadLocal::SlotAllocator& threadLocal() override { return server_context_.threadLocal(); }
  ProtobufMessage::ValidationVisitor& messageValidationVisitor() override {
    return validation_visitor_;
  }
  Api::Api& api() override { return server_context_.api(); }
  const Server::Options& options() override { return server_context_.options(); }
  AccessLog::AccessLogManager& accessLogManager() override {
    return server_context_.accessLogManager();
  }

  // tcloud 泳道
  std::shared_ptr<Envoy::TcloudMap::TcloudMap<std::string, std::string, Envoy::TcloudMap::LFUCachePolicy>> getTcloudMap() override { return tcloud_map_; }

private:
  Server::Configuration::ServerFactoryContext& server_context_;
  Ssl::ContextManager& context_manager_;
  Stats::Scope& stats_scope_;
  Upstream::ClusterManager& cluster_manager_;
  Stats::Store& stats_;
  Init::Manager* init_manager_{};
  ProtobufMessage::ValidationVisitor& validation_visitor_;

  // tcloud 泳道
  std::shared_ptr<Envoy::TcloudMap::TcloudMap<std::string, std::string, Envoy::TcloudMap::LFUCachePolicy>> tcloud_map_;
};

} // namespace Configuration
} // namespace Server
} // namespace Envoy
