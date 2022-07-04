#pragma once

#include "event2/event_struct.h"

namespace Envoy {
namespace Event {

/**
 * Base class for libevent event implementations. The event struct is embedded inside of this class
 * and derived classes are expected to assign it inside of the constructor.
 */
 // 这个抽象的可真是太精辟了
 // 在Libevent中无论是定时器到期、收到信号、还是文件可读写等都是事件，统一使用event类型来表示，
 // Envoy中则将event作为ImplBase的成员，然后让所有的事件类型的对象都继承ImplBase，从而实现了事件的抽象。
class ImplBase {
protected:
  ~ImplBase();

  event raw_event_;
};

} // namespace Event
} // namespace Envoy
