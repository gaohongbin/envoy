#pragma once

#include "envoy/common/pure.h"
#include "envoy/event/deferred_deletable.h"
#include "envoy/upstream/upstream.h"

namespace Envoy {
namespace ConnectionPool {
// 这里 ConnectionPool 是一种对下面协议的提纲挈领式的 interface.
// 但目前需要建立连接的只有 TCP 协议。后面还会有 Envoy::Tcp::ConnectionPool, 还有 Envoy::Http::ConnectionPool
// 所以要分清楚不要乱, 这里是根据协议来的。

/**
 * Controls the behavior of a canceled stream.
 */
enum class CancelPolicy {
  // By default, canceled streams allow a pending connection to complete and become
  // available for a future stream.
  Default,
  // When a stream is canceled, closes a pending connection if there will still be sufficient
  // connections to serve pending streams. CloseExcess is largely useful for callers that never
  // re-use connections (e.g. by closing rather than releasing connections). Using CloseExcess in
  // this situation guarantees that no idle connections will be held open by the conn pool awaiting
  // a connection stream.
  CloseExcess,
};

/**
 * Handle that allows a pending connection or stream to be canceled before it is completed.
 */
 // 允许pending connection 或 stream 在完成之前被取消的回调
class Cancellable {
public:
  virtual ~Cancellable() = default;

  /**
   * Cancel the pending connection or stream.
   * @param cancel_policy a CancelPolicy that controls the behavior of this cancellation.
   */
  virtual void cancel(CancelPolicy cancel_policy) PURE;
};

/**
 * Controls the behavior when draining a connection pool.
 */
enum class DrainBehavior {
  // Starts draining a pool, by gracefully completing all requests and gracefully closing all
  // connections, in preparation for deletion. It is invalid to create new streams or
  // connections from this pool after draining a pool with this behavior.
  DrainAndDelete,
  // Actively drain all existing connection pool connections. This can be used in cases where
  // the connection pool is not being destroyed, but the caller wishes to make sure that
  // all new streams take place on a new connection. For example, when a health check failure
  // occurs.
  DrainExistingConnections,
};

/**
 * An instance of a generic connection pool.
 */
 // 通用连接池实例
class Instance {
public:
  virtual ~Instance() = default;

  /**
   * Called when a connection pool has no pending streams, busy connections, or ready connections.
   */
  using IdleCb = std::function<void()>;

  /**
   * Register a callback that gets called when the connection pool is fully idle.
   */
  virtual void addIdleCallback(IdleCb cb) PURE;
   // 当耗尽 pending streams, busy connections, ready connections 时调用
  // using DrainedCb = std::function<void()>;

  /**
   * Returns true if the pool does not have any connections or pending requests.
   */
  virtual bool isIdle() const PURE;

  /**
   * Drains the connections in a pool.
   * @param drain_behavior A DrainBehavior that controls the behavior of the draining.
   */
  // 主动耗尽所有现有的连接池连接。此方法可用于连接池未被销毁但调用者希望确保所有新流都发生在新连接上的情况。例如，当发生健康检查失败时。
  virtual void drainConnections(DrainBehavior drain_behavior) PURE;
  // virtual void drainConnections() PURE;

  /**
   * @return Upstream::HostDescriptionConstSharedPtr the host for which connections are pooled.
   */
  virtual Upstream::HostDescriptionConstSharedPtr host() const PURE;

  /**
   * Creates an upstream connection, if existing connections do not meet both current and
   * anticipated load.
   *
   * @return true if a connection was preconnected, false otherwise.
   */
   // 如果现有连接不能同时满足当前和预期负载，则创建上游连接。
  virtual bool maybePreconnect(float preconnect_ratio) PURE;
};

enum class PoolFailureReason {
  // A resource overflowed and policy prevented a new connection from being created.
  // 资源溢出，策略阻止创建新连接。
  Overflow,
  // A local connection failure took place while creating a new connection.
  LocalConnectionFailure,
  // A remote connection failure took place while creating a new connection.
  RemoteConnectionFailure,
  // A timeout occurred while creating a new connection.
  Timeout,
};

} // namespace ConnectionPool
} // namespace Envoy
