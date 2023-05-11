#pragma once

#include <cstdint>

#include "envoy/event/file_event.h"

#include "common/event/dispatcher_impl.h"
#include "common/event/event_impl_base.h"

namespace Envoy {
namespace Event {

/**
 * Implementation of FileEvent for libevent that uses persistent events and
 * assumes the user will read/write until EAGAIN is returned from the file.
 */
 // 利用 libevent 进行了 file event 的封装
 // event 在 ImplBase 里面。
 // event_base 在 dispatcher 里面。
class FileEventImpl : public FileEvent, ImplBase {
public:
  FileEventImpl(DispatcherImpl& dispatcher, os_fd_t fd, FileReadyCb cb, FileTriggerType trigger,
                uint32_t events);

  // Event::FileEvent
  void activate(uint32_t events) override;
  void setEnabled(uint32_t events) override;
  void unregisterEventIfEmulatedEdge(uint32_t event) override;
  void registerEventIfEmulatedEdge(uint32_t event) override;

private:
  void assignEvents(uint32_t events, event_base* base);
  void mergeInjectedEventsAndRunCb(uint32_t events);
  void updateEvents(uint32_t events);

  // dispatcher 作为成员变量, event_base 又是 dispatcher 的成员变量
  Dispatcher& dispatcher_;
  // file event 事件触发以后的回调函数
  FileReadyCb cb_;
  // 监听的 file 文件描述符
  os_fd_t fd_;
  // 触发类型, 边沿触发还是水平触发。
  FileTriggerType trigger_;
  // Enabled events for this fd.
  uint32_t enabled_events_;

  // Injected FileReadyType events that were scheduled by recent calls to activate() and are pending
  // delivery.
  // 等待被触发的事件
  uint32_t injected_activation_events_{};
  // Used to schedule delayed event activation. Armed iff pending_activation_events_ != 0.
  // 事件被触发后回调
  SchedulableCallbackPtr activation_cb_;
};
} // namespace Event
} // namespace Envoy
