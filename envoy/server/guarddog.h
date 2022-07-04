#pragma once

#include "envoy/common/pure.h"
#include "envoy/event/dispatcher.h"
#include "envoy/server/watchdog.h"

namespace Envoy {
namespace Server {

/**
 * The GuardDog runs a background thread which scans a number of shared WatchDog
 * objects periodically to verify that they have been recently touched. If some
 * of the watched items have not responded the GuardDog will take action ranging
 * from stats counter increments to killing the entire process (if a deadlock is
 * suspected).
 *
 * The lifespan of the GuardDog thread is tied to the lifespan of this object.
 */
 // GuardDog 运行一个后台线程，该线程定期扫描许多共享的 WatchDog 对象以验证它们最近是否被触摸过。如果某些被监视的项目没有响应，GuardDog 将采取从统计计数器增量到终止整个进程（如果怀疑死锁）的行动。
class GuardDog {
public:
  virtual ~GuardDog() = default;

  /**
   * Get a WatchDog object pointer to a new WatchDog.
   *
   * After this method returns the WatchDog object must be touched periodically
   * to avoid triggering the GuardDog. If no longer needed use the
   * stopWatching() method to remove it from the list of watched objects.
   *
   * @param thread_id a Thread::ThreadId containing the system thread id
   * @param thread_name supplies the name of the thread which is used for per-thread miss stats.
   * @param dispatcher dispatcher responsible for petting the watchdog.
   */
  virtual WatchDogSharedPtr createWatchDog(Thread::ThreadId thread_id,
                                           const std::string& thread_name,
                                           Event::Dispatcher& dispatcher) PURE;

  /**
   * Tell the GuardDog to forget about this WatchDog.
   * After calling this method it is no longer necessary to touch the WatchDog
   * object.
   *
   * @param wd A WatchDogSharedPtr obtained from createWatchDog.
   */
  virtual void stopWatching(WatchDogSharedPtr wd) PURE;
};

} // namespace Server
} // namespace Envoy
