#include "source/server/active_tcp_listener.h"

#include <chrono>

#include "envoy/event/dispatcher.h"
#include "envoy/stats/scope.h"

#include "source/common/common/assert.h"
#include "source/common/network/connection_impl.h"
#include "source/common/network/utility.h"

namespace Envoy {
namespace Server {

ActiveTcpListener::ActiveTcpListener(Network::TcpConnectionHandler& parent,
                                     Network::ListenerConfig& config, Runtime::Loader& runtime,
                                     Network::SocketSharedPtr&& socket,
                                     Network::Address::InstanceConstSharedPtr& listen_address,
                                     Network::ConnectionBalancer& connection_balancer)
    : OwnedActiveStreamListenerBase(
          parent, parent.dispatcher(),
          parent.dispatcher().createListener(std::move(socket), *this, runtime, config.bindToPort(),
                                             config.ignoreGlobalConnLimit()),
          config),
      tcp_conn_handler_(parent), connection_balancer_(connection_balancer),
      listen_address_(listen_address) {
  connection_balancer_.registerHandler(*this);
}

ActiveTcpListener::ActiveTcpListener(Network::TcpConnectionHandler& parent,
                                     Network::ListenerPtr&& listener,
                                     Network::Address::InstanceConstSharedPtr& listen_address,
                                     Network::ListenerConfig& config,
                                     Network::ConnectionBalancer& connection_balancer,
                                     Runtime::Loader&)
    : OwnedActiveStreamListenerBase(parent, parent.dispatcher(), std::move(listener), config),
      tcp_conn_handler_(parent), connection_balancer_(connection_balancer),
      listen_address_(listen_address) {
  connection_balancer_.registerHandler(*this);
}

ActiveTcpListener::~ActiveTcpListener() {
  is_deleting_ = true;
  connection_balancer_.unregisterHandler(*this);

  // Purge sockets that have not progressed to connections. This should only happen when
  // a listener filter stops iteration and never resumes.
  while (!sockets_.empty()) {
    auto removed = sockets_.front()->removeFromList(sockets_);
    dispatcher().deferredDelete(std::move(removed));
  }

  for (auto& [chain, active_connections] : connections_by_context_) {
    ASSERT(active_connections != nullptr);
    auto& connections = active_connections->connections_;
    while (!connections.empty()) {
      connections.front()->connection_->close(Network::ConnectionCloseType::NoFlush);
    }
  }
  dispatcher().clearDeferredDeleteList();

  // By the time a listener is destroyed, in the common case, there should be no connections.
  // However, this is not always true if there is an in flight rebalanced connection that is
  // being posted. This assert is extremely useful for debugging the common path so we will leave it
  // for now. If it becomes a problem (developers hitting this assert when using debug builds) we
  // can revisit. This case, if it happens, should be benign on production builds. This case is
  // covered in ConnectionHandlerTest::RemoveListenerDuringRebalance.
  ASSERT(num_listener_connections_ == 0, fmt::format("destroyed listener {} has {} connections",
                                                     config_->name(), numConnections()));
}

void ActiveTcpListener::updateListenerConfig(Network::ListenerConfig& config) {
  ENVOY_LOG(trace, "replacing listener ", config_->listenerTag(), " by ", config.listenerTag());
  config_ = &config;
}

//void ActiveTcpSocket::onTimeout() {
//  listener_.stats_.downstream_pre_cx_timeout_.inc();
//  ASSERT(inserted());
//  ENVOY_LOG(debug, "listener filter times out after {} ms",
//            listener_.listener_filters_timeout_.count());
//
//  if (listener_.continue_on_listener_filters_timeout_) {
//    ENVOY_LOG(debug, "fallback to default listener filter");
//    newConnection();
//  }
//  unlink();
//}
//
//void ActiveTcpSocket::startTimer() {
//  if (listener_.listener_filters_timeout_.count() > 0) {
//    timer_ = listener_.parent_.dispatcher().createTimer([this]() -> void { onTimeout(); });
//    timer_->enableTimer(listener_.listener_filters_timeout_);
//  }
//}
//
//void ActiveTcpSocket::unlink() {
//  ActiveTcpSocketPtr removed = removeFromList(listener_.sockets_);
//  if (removed->timer_ != nullptr) {
//    removed->timer_->disableTimer();
//  }
//  // Emit logs if a connection is not established.
//  if (!connected_) {
//    emitLogs(*listener_.config_, *stream_info_);
//  }
//  listener_.parent_.dispatcher().deferredDelete(std::move(removed));
//}
//
//void ActiveTcpSocket::continueFilterChain(bool success) {
//  if (success) {
//    bool no_error = true;
//    if (iter_ == accept_filters_.end()) {
//      iter_ = accept_filters_.begin();
//    } else {
//      iter_ = std::next(iter_);
//    }
//
//    for (; iter_ != accept_filters_.end(); iter_++) {
//      Network::FilterStatus status = (*iter_)->onAccept(*this);
//      if (status == Network::FilterStatus::StopIteration) {
//        // The filter is responsible for calling us again at a later time to continue the filter
//        // chain from the next filter.
//        if (!socket().ioHandle().isOpen()) {
//          // break the loop but should not create new connection
//          no_error = false;
//          break;
//        } else {
//          // Blocking at the filter but no error
//          return;
//        }
//      }
//    }
//    // Successfully ran all the accept filters.
//    if (no_error) {
//      newConnection();
//    } else {
//      // Signal the caller that no extra filter chain iteration is needed.
//      iter_ = accept_filters_.end();
//    }
//  }
//
//  // Filter execution concluded, unlink and delete this ActiveTcpSocket if it was linked.
//  if (inserted()) {
//    unlink();
//  }
//}
//
//void ActiveTcpSocket::setDynamicMetadata(const std::string& name,
//                                         const ProtobufWkt::Struct& value) {
//  stream_info_->setDynamicMetadata(name, value);
//}
//
//void ActiveTcpSocket::newConnection() {
//  connected_ = true;
//
//  // Check if the socket may need to be redirected to another listener.
//  Network::BalancedConnectionHandlerOptRef new_listener;
//
//  if (hand_off_restored_destination_connections_ &&
//      socket_->addressProvider().localAddressRestored()) {
//    // Find a listener associated with the original destination address.
//    // 对于网关来说, localAddress 就是请求的 destination address.
//    // 也就是说, 这里可能一个 envoy 部署在一个机器上, 但是这个机器有多个 IP, 所以监听器可以监听多个 IP。
//    new_listener =
//        listener_.parent_.getBalancedHandlerByAddress(*socket_->addressProvider().localAddress());
//  }
//  if (new_listener.has_value()) {
//    // Hands off connections redirected by iptables to the listener associated with the
//    // original destination address. Pass 'hand_off_restored_destination_connections' as false to
//    // prevent further redirection as well as 'rebalanced' as true since the connection has
//    // already been balanced if applicable inside onAcceptWorker() when the connection was
//    // initially accepted. Note also that we must account for the number of connections properly
//    // across both listeners.
//    // TODO(mattklein123): See note in ~ActiveTcpSocket() related to making this accounting better.
//    listener_.decNumConnections();
//    new_listener.value().get().incNumConnections();
//    new_listener.value().get().onAcceptWorker(std::move(socket_), false, true);
//  } else {
//    // Set default transport protocol if none of the listener filters did it.
//    if (socket_->detectedTransportProtocol().empty()) {
//      socket_->setDetectedTransportProtocol(
//          Extensions::TransportSockets::TransportProtocolNames::get().RawBuffer);
//    }
//    // TODO(lambdai): add integration test
//    // TODO: Address issues in wider scope. See https://github.com/envoyproxy/envoy/issues/8925
//    // Erase accept filter states because accept filters may not get the opportunity to clean up.
//    // Particularly the assigned events need to reset before assigning new events in the follow up.
//    accept_filters_.clear();
//    // Create a new connection on this listener.
//    listener_.newConnection(std::move(socket_), std::move(stream_info_));
//  }
//}

void ActiveTcpListener::onAccept(Network::ConnectionSocketPtr&& socket) {
  if (listenerConnectionLimitReached()) {
    RELEASE_ASSERT(socket->connectionInfoProvider().remoteAddress() != nullptr, "");
    ENVOY_LOG(trace, "closing connection from {}: listener connection limit reached for {}",
              socket->connectionInfoProvider().remoteAddress()->asString(), config_->name());
    socket->close();
    stats_.downstream_cx_overflow_.inc();
    return;
  }

  onAcceptWorker(std::move(socket), config_->handOffRestoredDestinationConnections(), false);
}

void ActiveTcpListener::onReject(RejectCause cause) {
  switch (cause) {
  case RejectCause::GlobalCxLimit:
    stats_.downstream_global_cx_overflow_.inc();
    break;
  case RejectCause::OverloadAction:
    stats_.downstream_cx_overload_reject_.inc();
    break;
  }
}

void ActiveTcpListener::onAcceptWorker(Network::ConnectionSocketPtr&& socket,
                                       bool hand_off_restored_destination_connections,
                                       bool rebalanced) {
  // 只是负载均衡一下而已
  if (!rebalanced) {
    Network::BalancedConnectionHandler& target_handler =
        connection_balancer_.pickTargetHandler(*this);
    if (&target_handler != this) {
      target_handler.post(std::move(socket));
      return;
    }
  }

  // 真正的逻辑在这里, 通过 ConnectionSocket 初始化 ActiveTcpSocket
  auto active_socket = std::make_unique<ActiveTcpSocket>(*this, std::move(socket),
                                                         hand_off_restored_destination_connections);

  onSocketAccepted(std::move(active_socket));
//  // Create and run the filters
//  // 创建 listener filter
//  config_->filterChainFactory().createListenerFilterChain(*active_socket);
//  active_socket->continueFilterChain(true);
//
//  // Move active_socket to the sockets_ list if filter iteration needs to continue later.
//  // Otherwise we let active_socket be destructed when it goes out of scope.
//  if (active_socket->iter_ != active_socket->accept_filters_.end()) {
//    active_socket->startTimer();
//    LinkedList::moveIntoListBack(std::move(active_socket), sockets_);
//  } else {
//    // If active_socket is about to be destructed, emit logs if a connection is not created.
//    if (!active_socket->connected_) {
//      if (active_socket->stream_info_ != nullptr) {
//        emitLogs(*config_, *active_socket->stream_info_);
//      } else {
//        // If the active_socket is not connected, this socket is not promoted to active connection.
//        // Thus the stream_info_ is owned by this active socket.
//        ENVOY_BUG(active_socket->stream_info_ != nullptr,
//                  "the unconnected active socket must have stream info.");
//      }
//    }
//  }
}

void ActiveTcpListener::pauseListening() {
  if (listener_ != nullptr) {
    listener_->disable();
  }
}

void ActiveTcpListener::resumeListening() {
  if (listener_ != nullptr) {
    listener_->enable();
  }
}

Network::BalancedConnectionHandlerOptRef
ActiveTcpListener::getBalancedHandlerByAddress(const Network::Address::Instance& address) {
  return tcp_conn_handler_.getBalancedHandlerByAddress(address);
}
// 新建一个 ActiveTcpConnection 纳入 connections_by_context_ 的管理中
//void ActiveTcpListener::newConnection(Network::ConnectionSocketPtr&& socket,
//                                      std::unique_ptr<StreamInfo::StreamInfo> stream_info) {
//
//  // Find matching filter chain.
//  // 对于网关而言, 这里直接就是 listener 对应的 FilterChain
//  const auto filter_chain = config_->filterChainManager().findFilterChain(*socket);
//  if (filter_chain == nullptr) {
//    ENVOY_LOG(debug, "closing connection: no matching filter chain found");
//    stats_.no_filter_chain_match_.inc();
//    stream_info->setResponseFlag(StreamInfo::ResponseFlag::NoRouteFound);
//    stream_info->setResponseCodeDetails(StreamInfo::ResponseCodeDetails::get().FilterChainNotFound);
//    emitLogs(*config_, *stream_info);
//    socket->close();
//    return;
//  }
//
//  // 将 filterChainName 赋值给 streamInfo
//  stream_info->setFilterChainName(filter_chain->name());
//  auto transport_socket = filter_chain->transportSocketFactory().createTransportSocket(nullptr);
//  stream_info->setDownstreamSslConnection(transport_socket->ssl());
//  // 获取相同 filterChain 对应的 ActiveConnections
//  // 注意这个 ActiveConnections 的结构体, 应该不是一个实例，因为其成员变量包含 std::list<ActiveTcpConnectionPtr> connections_;
//  auto& active_connections = getOrCreateActiveConnections(*filter_chain);
//  // 创建 ServerConnection
//  auto server_conn_ptr = parent_.dispatcher().createServerConnection(
//      std::move(socket), std::move(transport_socket), *stream_info);
//  // 设置 tls 超时
//  if (const auto timeout = filter_chain->transportSocketConnectTimeout();
//      timeout != std::chrono::milliseconds::zero()) {
//    server_conn_ptr->setTransportSocketConnectTimeout(timeout);
//  }
//  // 创建 ActiveTcpConnection 作为新的连接
//  ActiveTcpConnectionPtr active_connection(
//      new ActiveTcpConnection(active_connections, std::move(server_conn_ptr),
//                              parent_.dispatcher().timeSource(), std::move(stream_info)));
//  active_connection->connection_->setBufferLimits(config_->perConnectionBufferLimitBytes());
//
//  // 创建 networkFilterChain
//  const bool empty_filter_chain = !config_->filterChainFactory().createNetworkFilterChain(
//      *active_connection->connection_, filter_chain->networkFilterFactories());
//  if (empty_filter_chain) {
//    ENVOY_CONN_LOG(debug, "closing connection: no filters", *active_connection->connection_);
//    active_connection->connection_->close(Network::ConnectionCloseType::NoFlush);
//  }

void ActiveTcpListener::newActiveConnection(const Network::FilterChain& filter_chain,
                                            Network::ServerConnectionPtr server_conn_ptr,
                                            std::unique_ptr<StreamInfo::StreamInfo> stream_info) {
  auto& active_connections = getOrCreateActiveConnections(filter_chain);
  auto active_connection =
      std::make_unique<ActiveTcpConnection>(active_connections, std::move(server_conn_ptr),
                                            dispatcher().timeSource(), std::move(stream_info));
  // If the connection is already closed, we can just let this connection immediately die.
  // 将 active_connection 添加到 ActiveConnections 的成员变量 connections_ 列表里面。
  if (active_connection->connection_->state() != Network::Connection::State::Closed) {
    ENVOY_CONN_LOG(
        debug, "new connection from {}", *active_connection->connection_,
        active_connection->connection_->connectionInfoProvider().remoteAddress()->asString());
    active_connection->connection_->addConnectionCallbacks(*active_connection);
    LinkedList::moveIntoList(std::move(active_connection), active_connections.connections_);
  }
}

//ActiveConnections&
//ActiveTcpListener::getOrCreateActiveConnections(const Network::FilterChain& filter_chain) {
//  // 这种语句，是不是后面操作 connections 也会影响到 connections_by_context_
//  ActiveConnectionsPtr& connections = connections_by_context_[&filter_chain];
//  if (connections == nullptr) {
//    connections = std::make_unique<ActiveConnections>(*this, filter_chain);
//  }
//  return *connections;
//}
//
//void ActiveTcpListener::deferredRemoveFilterChains(
//    const std::list<const Network::FilterChain*>& draining_filter_chains) {
//  // Need to recover the original deleting state.
//  const bool was_deleting = is_deleting_;
//  is_deleting_ = true;
//  for (const auto* filter_chain : draining_filter_chains) {
//    auto iter = connections_by_context_.find(filter_chain);
//    if (iter == connections_by_context_.end()) {
//      // It is possible when listener is stopping.
//    } else {
//      auto& connections = iter->second->connections_;
//      while (!connections.empty()) {
//        connections.front()->connection_->close(Network::ConnectionCloseType::NoFlush);
//      }
//      // Since is_deleting_ is on, we need to manually remove the map value and drive the iterator.
//      // Defer delete connection container to avoid race condition in destroying connection.
//      parent_.dispatcher().deferredDelete(std::move(iter->second));
//      connections_by_context_.erase(iter);
//    }
//  }
//  is_deleting_ = was_deleting;
//}

// 这里将 socket 传给了 dispatcher
void ActiveTcpListener::post(Network::ConnectionSocketPtr&& socket) {
  // It is not possible to capture a unique_ptr because the post() API copies the lambda, so we must
  // bundle the socket inside a shared_ptr that can be captured.
  // TODO(mattklein123): It may be possible to change the post() API such that the lambda is only
  // moved, but this is non-trivial and needs investigation.
  RebalancedSocketSharedPtr socket_to_rebalance = std::make_shared<RebalancedSocket>();
  socket_to_rebalance->socket = std::move(socket);

  dispatcher().post([socket_to_rebalance, address = listen_address_, tag = config_->listenerTag(),
                     &tcp_conn_handler = tcp_conn_handler_,
                     handoff = config_->handOffRestoredDestinationConnections()]() {
    auto balanced_handler = tcp_conn_handler.getBalancedHandlerByTag(tag, *address);
    if (balanced_handler.has_value()) {
      balanced_handler->get().onAcceptWorker(std::move(socket_to_rebalance->socket), handoff, true);
      return;
    }
  });
}

} // namespace Server
} // namespace Envoy
