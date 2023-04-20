#pragma once

#include <memory>
#include <string>
#include <vector>

#include "envoy/common/platform.h"
#include "envoy/network/connection.h"
#include "envoy/network/listen_socket.h"
#include "envoy/network/socket.h"
#include "envoy/network/socket_interface.h"

#include "common/common/assert.h"
#include "common/common/dump_state_utils.h"
#include "common/network/socket_impl.h"

namespace Envoy {
namespace Network {

// 服务端 Socket, 执行 bind
class ListenSocketImpl : public SocketImpl {
protected:
  ListenSocketImpl(IoHandlePtr&& io_handle, const Address::InstanceConstSharedPtr& local_address)
      : SocketImpl(std::move(io_handle), local_address, nullptr) {}

  SocketPtr duplicate() override {
    // Using `new` to access a non-public constructor.
    return absl::WrapUnique(
        new ListenSocketImpl(io_handle_->duplicate(), address_provider_->localAddress()));
  }

  void setupSocket(const Network::Socket::OptionsSharedPtr& options, bool bind_to_port);
  void setListenSocketOptions(const Network::Socket::OptionsSharedPtr& options);
  Api::SysCallIntResult bind(Network::Address::InstanceConstSharedPtr address) override;
};

/**
 * Wraps a unix socket.
 */
template <Socket::Type T> struct NetworkSocketTrait {};

template <> struct NetworkSocketTrait<Socket::Type::Stream> {
  static constexpr Socket::Type type = Socket::Type::Stream;
};

template <> struct NetworkSocketTrait<Socket::Type::Datagram> {
  static constexpr Socket::Type type = Socket::Type::Datagram;
};

// 继续对 ListenSocketImpl 进行抽象
template <typename T> class NetworkListenSocket : public ListenSocketImpl {
public:
  NetworkListenSocket(const Address::InstanceConstSharedPtr& address,
                      const Network::Socket::OptionsSharedPtr& options, bool bind_to_port)
      : ListenSocketImpl(Network::ioHandleForAddr(T::type, address), address) {
    RELEASE_ASSERT(io_handle_->isOpen(), "");

    setPrebindSocketOptions();

    setupSocket(options, bind_to_port);
  }

  NetworkListenSocket(IoHandlePtr&& io_handle, const Address::InstanceConstSharedPtr& address,
                      const Network::Socket::OptionsSharedPtr& options)
      : ListenSocketImpl(std::move(io_handle), address) {
    setListenSocketOptions(options);
  }

  Socket::Type socketType() const override { return T::type; }

protected:
  void setPrebindSocketOptions();
};

using TcpListenSocket = NetworkListenSocket<NetworkSocketTrait<Socket::Type::Stream>>;
using TcpListenSocketPtr = std::unique_ptr<TcpListenSocket>;

using UdpListenSocket = NetworkListenSocket<NetworkSocketTrait<Socket::Type::Datagram>>;
using UdpListenSocketPtr = std::unique_ptr<UdpListenSocket>;

class UdsListenSocket : public ListenSocketImpl {
public:
  UdsListenSocket(const Address::InstanceConstSharedPtr& address);
  UdsListenSocket(IoHandlePtr&& io_handle, const Address::InstanceConstSharedPtr& address);
  Socket::Type socketType() const override { return Socket::Type::Stream; }
};

// ConnectionSocket: A socket passed to a connection.
class ConnectionSocketImpl : public SocketImpl, public ConnectionSocket {
public:
  ConnectionSocketImpl(IoHandlePtr&& io_handle,
                       const Address::InstanceConstSharedPtr& local_address,
                       const Address::InstanceConstSharedPtr& remote_address)
      : SocketImpl(std::move(io_handle), local_address, remote_address) {}

  ConnectionSocketImpl(Socket::Type type, const Address::InstanceConstSharedPtr& local_address,
                       const Address::InstanceConstSharedPtr& remote_address)
      : SocketImpl(type, local_address, remote_address) {
    address_provider_->setLocalAddress(local_address);
  }

  // Network::Socket
  Socket::Type socketType() const override { return Socket::Type::Stream; }

  // Network::ConnectionSocket
  void setDetectedTransportProtocol(absl::string_view protocol) override {
    transport_protocol_ = std::string(protocol);
  }
  absl::string_view detectedTransportProtocol() const override { return transport_protocol_; }

  void setRequestedApplicationProtocols(const std::vector<absl::string_view>& protocols) override {
    application_protocols_.clear();
    for (const auto& protocol : protocols) {
      application_protocols_.emplace_back(protocol);
    }
  }
  const std::vector<std::string>& requestedApplicationProtocols() const override {
    return application_protocols_;
  }

  void setRequestedServerName(absl::string_view server_name) override {
    // Always keep the server_name_ as lower case.
    server_name_ = absl::AsciiStrToLower(server_name);
  }
  absl::string_view requestedServerName() const override { return server_name_; }

  absl::optional<std::chrono::milliseconds> lastRoundTripTime() override {
    return ioHandle().lastRoundTripTime();
  }

  void dumpState(std::ostream& os, int indent_level) const override {
    const char* spaces = spacesForLevel(indent_level);
    os << spaces << "ListenSocketImpl " << this << DUMP_MEMBER(transport_protocol_)
       << DUMP_MEMBER(server_name_) << "\n";
    DUMP_DETAILS(address_provider_);
  }

protected:
  std::string transport_protocol_;
  std::vector<std::string> application_protocols_;
  std::string server_name_;
};

// ConnectionSocket used with server connections.
// server connections 使用 AcceptedSocketImpl
class AcceptedSocketImpl : public ConnectionSocketImpl {
public:
  AcceptedSocketImpl(IoHandlePtr&& io_handle, const Address::InstanceConstSharedPtr& local_address,
                     const Address::InstanceConstSharedPtr& remote_address)
      : ConnectionSocketImpl(std::move(io_handle), local_address, remote_address) {
    ++global_accepted_socket_count_;
  }

  ~AcceptedSocketImpl() override {
    ASSERT(global_accepted_socket_count_.load() > 0);
    --global_accepted_socket_count_;
  }

  // TODO (tonya11en): Global connection count tracking is temporarily performed via a static
  // variable until the logic is moved into the overload manager.
  static uint64_t acceptedSocketCount() { return global_accepted_socket_count_.load(); }

private:
  static std::atomic<uint64_t> global_accepted_socket_count_;
};

// ConnectionSocket used with client connections.
// client connections 使用 ClientSocketImpl, 因为继承自 ConnectionSocketImpl, 所以也是有 connect 方法的。
class ClientSocketImpl : public ConnectionSocketImpl {
public:
  ClientSocketImpl(const Address::InstanceConstSharedPtr& remote_address,
                   const OptionsSharedPtr& options)
      : ConnectionSocketImpl(Network::ioHandleForAddr(Socket::Type::Stream, remote_address),
                             nullptr, remote_address) {
    if (options) {
      addOptions(options);
    }
  }
};

} // namespace Network
} // namespace Envoy
