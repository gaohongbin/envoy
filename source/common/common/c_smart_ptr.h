#pragma once

#include <memory>

namespace Envoy {
/**
 * This is a helper that wraps C style API objects that need to be deleted with a smart pointer.
 */
 // 这是一个帮助程序，它包装了需要使用智能指针删除的 C 风格 API 对象。
 // TODO 说是用了什么 RAII 机制
template <class T, void (*deleter)(T*)> class CSmartPtr : public std::unique_ptr<T, void (*)(T*)> {
public:
  CSmartPtr() : std::unique_ptr<T, void (*)(T*)>(nullptr, deleter) {}
  CSmartPtr(T* object) : std::unique_ptr<T, void (*)(T*)>(object, deleter) {}
};
} // namespace Envoy
