#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "envoy/buffer/buffer.h"
#include "envoy/common/pure.h"
#include "envoy/event/deferred_deletable.h"
#include "envoy/network/address.h"
#include "envoy/network/filter.h"
#include "envoy/network/listen_socket.h"
#include "envoy/network/socket.h"
#include "envoy/ssl/connection.h"
#include "envoy/stream_info/stream_info.h"

namespace Envoy {
namespace Event {
class Dispatcher;
}

namespace Network {

/**
 * Events that occur on a connection.
 */
 // connection 上可能发生的事件类型
enum class ConnectionEvent {
  RemoteClose,
  LocalClose,
  Connected,
};

/**
 * Connections have both a read and write buffer.
 */
 // 连接有一个读缓冲区和一个写缓冲区。
enum class ConnectionBufferType { Read, Write };

/**
 * Network level callbacks that happen on a connection.
 */
 // connection 上发生的网络级回调
class ConnectionCallbacks {
public:
  virtual ~ConnectionCallbacks() = default;

  /**
   * Callback for connection events.
   * @param events supplies the ConnectionEvent that occurred.
   */
   // connection 上事件的回调函数
  virtual void onEvent(ConnectionEvent event) PURE;

  /**
   * Called when the write buffer for a connection goes over its high watermark.
   */
   // 当连接的写入缓冲区超过其高水位线时调用。
  virtual void onAboveWriteBufferHighWatermark() PURE;

  /**
   * Called when the write buffer for a connection goes from over its high
   * watermark to under its low watermark.
   */
   // 当连接的写入缓冲区从其高水位线以上变为其低水位线以下时调用。
  virtual void onBelowWriteBufferLowWatermark() PURE;
};

/**
 * Type of connection close to perform.
 */
 // connection 关闭时要执行的操作
enum class ConnectionCloseType {
  // 刷入正处于 pending 的写数据
  FlushWrite, // Flush pending write data before raising ConnectionEvent::LocalClose
  // 挂起之前不进行刷入
  NoFlush,    // Do not flush any pending data and immediately raise ConnectionEvent::LocalClose
  // 刷入正处于 pending 的写数据, 并暂停挂起, 直到 delayed_close_timeout 超时
  FlushWriteAndDelay // Flush pending write data and delay raising a ConnectionEvent::LocalClose
                     // until the delayed_close_timeout expires
};

/**
 * An abstract raw connection. Free the connection or call close() to disconnect.
 */
 // 抽象的原始连接。释放连接或调用 close() 断开连接。
 // 这里的 FilterManager 是 network 这个 namespace 下的。
class Connection : public Event::DeferredDeletable, public FilterManager {
public:
  // Connection 可能的状态: Open, Closing, Closed
  enum class State { Open, Closing, Closed };

  /**
   * Callback function for when bytes have been sent by a connection.
   * @param bytes_sent supplies the number of bytes written to the connection.
   * @return indicates if callback should be called in the future. If true is returned
   * the callback will be called again in the future. If false is returned, the callback
   * will be removed from callback list.
   */
   // connection 发送数据后的回调函数
   // return bool 的含义:
   // 指示将来是否应调用回调。如果返回 true，回调将在未来再次调用。如果返回 false，回调将从回调列表中删除。
  using BytesSentCb = std::function<bool(uint64_t bytes_sent)>;

  // 一些统计数据而已
  struct ConnectionStats {
    Stats::Counter& read_total_;
    Stats::Gauge& read_current_;
    Stats::Counter& write_total_;
    Stats::Gauge& write_current_;
    // Counter* as this is an optional counter. Bind errors will not be tracked if this is nullptr.
    // Counter* 因为这是一个可选的计数器。如果这是 nullptr，则不会跟踪绑定错误。
    Stats::Counter* bind_errors_;
    // Optional counter. Delayed close timeouts will not be tracked if this is nullptr.
    // 可选计数器。如果这是 nullptr，则不会跟踪延迟关闭超时。
    Stats::Counter* delayed_close_timeouts_;
  };

  ~Connection() override = default;

  /**
   * Register callbacks that fire when connection events occur.
   */
   // 添加 ConnectionCallbacks
   // ConnectionCallbacks 可以参考上面的定义
  virtual void addConnectionCallbacks(ConnectionCallbacks& cb) PURE;

  /**
   * Unregister callbacks which previously fired when connection events occur.
   */
   // 移除 ConnectionCallbacks
  virtual void removeConnectionCallbacks(ConnectionCallbacks& cb) PURE;

  /**
   * Register for callback every time bytes are written to the underlying TransportSocket.
   */
   // 添加 BytesSentCb
   // 每次将字节写入底层 TransportSocket 时的回调函数。
  virtual void addBytesSentCallback(BytesSentCb cb) PURE;

  /**
   * Enable half-close semantics on this connection. Reading a remote half-close
   * will not fully close the connection. This is off by default.
   * @param enabled Whether to set half-close semantics as enabled or disabled.
   */
   // 在此连接上启用半关闭语义: 在对方半关闭的情况下, 读取数据不会导致关闭该连接。
   // 这是默认关闭的。
  virtual void enableHalfClose(bool enabled) PURE;

  /**
   * @return true if half-close semantics are enabled, false otherwise.
   */
  virtual bool isHalfCloseEnabled() PURE;

  /**
   * Close the connection.
   */
  virtual void close(ConnectionCloseType type) PURE;

  /**
   * @return Event::Dispatcher& the dispatcher backing this connection.
   */
   // 获取 dispatcher 调度程序
  virtual Event::Dispatcher& dispatcher() PURE;

  /**
   * @return uint64_t the unique local ID of this connection.
   */
   // connection 的唯一 ID
  virtual uint64_t id() const PURE;

  /**
   * @param vector of bytes to which the connection should append hash key data. Any data already in
   * the key vector must not be modified.
   */
  // 给 connection 绑定的 hash 数据, 而且不允许改变。
  virtual void hashKey(std::vector<uint8_t>& hash) const PURE;

  /**
   * @return std::string the next protocol to use as selected by network level negotiation. (E.g.,
   *         ALPN). If network level negotiation is not supported by the connection or no protocol
   *         has been negotiated the empty string is returned.
   */
   // 跟协议有关, 有兴趣回头看看
  virtual std::string nextProtocol() const PURE;

  /**
   * Enable/Disable TCP NO_DELAY on the connection.
   */
  virtual void noDelay(bool enable) PURE;

  /**
   * Disable socket reads on the connection, applying external back pressure. When reads are
   * enabled again if there is data still in the input buffer it will be re-dispatched through
   * the filter chain.
   * @param disable supplies TRUE is reads should be disabled, FALSE if they should be enabled.
   *
   * Note that this function reference counts calls. For example
   * readDisable(true);  // Disables data
   * readDisable(true);  // Notes the connection is blocked by two sources
   * readDisable(false);  // Notes the connection is blocked by one source
   * readDisable(false);  // Marks the connection as unblocked, so resumes reading.
   */
  virtual void readDisable(bool disable) PURE;

  /**
   * Set if Envoy should detect TCP connection close when readDisable(true) is called.
   * By default, this is true on newly created connections.
   *
   * @param should_detect supplies if disconnects should be detected when the connection has been
   * read disabled
   */
   // 设置 Envoy 是否应在调用 readDisable(true) 时检测 TCP 连接是否关闭。
   // 默认情况下，对于新创建的连接会设置为 true。
  virtual void detectEarlyCloseWhenReadDisabled(bool should_detect) PURE;

  /**
   * @return bool whether reading is enabled on the connection.
   */
  virtual bool readEnabled() const PURE;

  /**
   * @return the address provider backing this connection.
   */
  virtual const SocketAddressProvider& addressProvider() const PURE;
  virtual SocketAddressProviderSharedPtr addressProviderSharedPtr() const PURE;

  /**
   * Credentials of the peer of a socket as decided by SO_PEERCRED.
   */
  struct UnixDomainSocketPeerCredentials {
    /**
     * The process id of the peer.
     */
    int32_t pid;
    /**
     * The user id of the peer.
     */
    uint32_t uid;
    /**
     * The group id of the peer.
     */
    uint32_t gid;
  };

  /**
   * @return The unix socket peer credentials of the remote client. Note that this is only
   * supported for unix socket connections.
   */
  virtual absl::optional<UnixDomainSocketPeerCredentials> unixSocketPeerCredentials() const PURE;

  /**
   * Set the stats to update for various connection state changes. Note that for performance reasons
   * these stats are eventually consistent and may not always accurately represent the connection
   * state at any given point in time.
   */
  // 设置统计信息以针对各种连接状态更改进行更新。
  // 请注意，出于性能原因，这些统计数据最终是一致的，可能并不总是准确地表示任何给定时间点的连接状态。
  virtual void setConnectionStats(const ConnectionStats& stats) PURE;

  /**
   * @return the const SSL connection data if this is an SSL connection, or nullptr if it is not.
   */
  // TODO(snowp): Remove this in favor of StreamInfo::downstreamSslConnection.
  virtual Ssl::ConnectionInfoConstSharedPtr ssl() const PURE;

  /**
   * @return requested server name (e.g. SNI in TLS), if any.
   */
  virtual absl::string_view requestedServerName() const PURE;

  /**
   * @return State the current state of the connection.
   */
  virtual State state() const PURE;

  /**
   * @return true if the connection has not completed connecting, false if the connection is
   * established.
   */
   // 返回 true: 如果 connection 还没有完成连接.
   // 返回 false: 如果 connection 已经建立。
  virtual bool connecting() const PURE;

  /**
   * Write data to the connection. Will iterate through downstream filters with the buffer if any
   * are installed.
   * @param data Supplies the data to write to the connection.
   * @param end_stream If true, this indicates that this is the last write to the connection. If
   *        end_stream is true, the connection is half-closed. This may only be set to true if
   *        enableHalfClose(true) has been set on this connection.
   */
  virtual void write(Buffer::Instance& data, bool end_stream) PURE;

  /**
   * Set a soft limit on the size of buffers for the connection.
   * For the read buffer, this limits the bytes read prior to flushing to further stages in the
   * processing pipeline.
   * For the write buffer, it sets watermarks. When enough data is buffered it triggers a call to
   * onAboveWriteBufferHighWatermark, which allows subscribers to enforce flow control by disabling
   * reads on the socket funneling data to the write buffer. When enough data is drained from the
   * write buffer, onBelowWriteBufferHighWatermark is called which similarly allows subscribers
   * resuming reading.
   */
   // 对连接的缓冲区大小设置软限制。
  virtual void setBufferLimits(uint32_t limit) PURE;

  /**
   * Get the value set with setBufferLimits.
   */
  virtual uint32_t bufferLimit() const PURE;

  /**
   * @return boolean telling if the connection is currently above the high watermark.
   */
  virtual bool aboveHighWatermark() const PURE;

  /**
   * Get the socket options set on this connection.
   */
  virtual const ConnectionSocket::OptionsSharedPtr& socketOptions() const PURE;

  /**
   * The StreamInfo object associated with this connection. This is typically
   * used for logging purposes. Individual filters may add specific information
   * via the FilterState object within the StreamInfo object. The StreamInfo
   * object in this context is one per connection i.e. different than the one in
   * the http ConnectionManager implementation which is one per request.
   *
   * @return StreamInfo object associated with this connection.
   */
   // 与此连接关联的 StreamInfo 对象。这通常用于记录目的。
  // 各个过滤器可以通过 StreamInfo 对象中的 FilterState 对象添加特定信息。
  // 此上下文中的 StreamInfo 对象是每个连接一个，即不同于 http ConnectionManager 实现中每个请求一个的对象。
  virtual StreamInfo::StreamInfo& streamInfo() PURE;
  virtual const StreamInfo::StreamInfo& streamInfo() const PURE;

  /**
   * Set the timeout for delayed connection close()s.
   * This can only be called prior to issuing a close() on the connection.
   * @param timeout The timeout value in milliseconds
   */
  virtual void setDelayedCloseTimeout(std::chrono::milliseconds timeout) PURE;

  /**
   * @return std::string the failure reason of the underlying transport socket, if no failure
   *         occurred an empty string is returned.
   */
  virtual absl::string_view transportFailureReason() const PURE;

  /**
   * Instructs the connection to start using secure transport.
   * Note: Not all underlying transport sockets support such operation.
   * @return boolean telling if underlying transport socket was able to
             start secure transport.
   */
  virtual bool startSecureTransport() PURE;

  /**
   *  @return absl::optional<std::chrono::milliseconds> An optional of the most recent round-trip
   *  time of the connection. If the platform does not support this, then an empty optional is
   *  returned.
   */
  virtual absl::optional<std::chrono::milliseconds> lastRoundTripTime() const PURE;
};

using ConnectionPtr = std::unique_ptr<Connection>;

/**
 * Connections servicing inbound connects.
 */
 // 为 inbound 提供服务的连接。
class ServerConnection : public virtual Connection {
public:
  /**
   * Set the amount of time allowed for the transport socket to report that a connection is
   * established. The provided timeout is relative to the current time. If this method is called
   * after a connection has already been established, it is a no-op.
   */
  virtual void setTransportSocketConnectTimeout(std::chrono::milliseconds timeout) PURE;
};

using ServerConnectionPtr = std::unique_ptr<ServerConnection>;

/**
 * Connections capable of outbound connects.
 */
 // 为 outbound 提供服务的连接
class ClientConnection : public virtual Connection {
public:
  /**
   * Connect to a remote host. Errors or connection events are reported via the event callback
   * registered via addConnectionCallbacks().
   */
   // 连接到远程主机。通过 addConnectionCallbacks() 注册的事件回调报告错误或连接事件。
  virtual void connect() PURE;
};

using ClientConnectionPtr = std::unique_ptr<ClientConnection>;

} // namespace Network
} // namespace Envoy
