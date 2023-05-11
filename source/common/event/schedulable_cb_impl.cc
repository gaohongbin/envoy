#include "common/event/schedulable_cb_impl.h"

#include "common/common/assert.h"

#include "event2/event.h"

namespace Envoy {
namespace Event {

SchedulableCallbackImpl::SchedulableCallbackImpl(Libevent::BasePtr& libevent,
                                                 std::function<void()> cb)
    : cb_(cb) {
  ASSERT(cb_);
  // 赋值定时器事件
  evtimer_assign(
      &raw_event_, libevent.get(),
      [](evutil_socket_t, short, void* arg) -> void {
        SchedulableCallbackImpl* cb = static_cast<SchedulableCallbackImpl*>(arg);
        // 回调函数就是执行本 SchedulableCallbackImpl 的 cb_ 方法
        cb->cb_();
      },
      this);
}

// 调度当前事件的 callback
void SchedulableCallbackImpl::scheduleCallbackCurrentIteration() {
  if (enabled()) {
    return;
  }
  // event_active directly adds the event to the end of the work queue so it executes in the current
  // iteration of the event loop.
  // void event_active(struct event *ev, int what, short ncalls)
  // 手动激活某个事件
  // 这个函数让事件 ev 带有标志 what(EV_READ、EV_WRITE 和 EV_TIMEOUT 的组合)成 为激活的。事件不需要已经处于未决状态,激活事件也不会让它成为未决的。
  event_active(&raw_event_, EV_TIMEOUT, 0);
}

// 本质是 event_add
void SchedulableCallbackImpl::scheduleCallbackNextIteration() {
  if (enabled()) {
    return;
  }
  // libevent computes the list of timers to move to the work list after polling for fd events, but
  // iteration through the work list starts. Zero delay timers added while iterating through the
  // work list execute on the next iteration of the event loop.
  const timeval zero_tv{};
  event_add(&raw_event_, &zero_tv);
}

// 取消 raw_event
void SchedulableCallbackImpl::cancel() { event_del(&raw_event_); }

bool SchedulableCallbackImpl::enabled() { return 0 != evtimer_pending(&raw_event_, nullptr); }

} // namespace Event
} // namespace Envoy
