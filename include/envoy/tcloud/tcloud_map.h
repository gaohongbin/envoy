#pragma once

#include<map>
#include<list>
#include<string>
#include<unordered_map>

#include "envoy/common/pure.h"

using namespace std;

namespace Envoy {
namespace TcloudMap {

template <typename Key>
class ICachePolicy
{
public:
  virtual ~ICachePolicy() = default;

  /**
   * \brief Handle element insertion in a cache
   * \param[in] key Key that should be used by the policy
   */
  virtual void Insert(const Key &key) PURE;

  /**
   * \brief Handle request to the key-element in a cache
   * \param key
   */
  virtual void Touch(const Key &key) PURE;
  /**
   * \brief Handle element deletion from a cache
   * \param[in] key Key that should be used by the policy
   */
  virtual void Erase(const Key &key) PURE;

  /**
   * \brief Return a key of a replacement candidate
   * \return Replacement candidate according to selected policy
   */
  virtual const Key &ReplCandidate() const PURE;
};

// 定义 NoCachePolicy
template <typename Key>
class NoCachePolicy : public ICachePolicy<Key>
{
public:
  NoCachePolicy() = default;
  ~NoCachePolicy() override = default;

  void Insert(const Key &key) override {
    key_storage.emplace(key);
  }

  void Touch(const Key &key) override {
    UNUSED(key);
  }

  void Erase(const Key &key) override {
    key_storage.erase(key);
  }

  // return a key of a displacement candidate
  const Key &ReplCandidate() const override {
    return *key_storage.cbegin();
  }

private:
  std::unordered_set<Key> key_storage;
};

// LRU 实现
template <typename Key>
class LRUCachePolicy : public ICachePolicy<Key>
{

public:

  using lru_iterator = typename std::list<Key>::const_iterator;

  LRUCachePolicy() = default;
  ~LRUCachePolicy() = default;

  void Insert(const Key& key) override
  {
    lru_queue.emplace_front(key);
    key_finder[key] = lru_queue.cbegin();
  }

  void Touch(const Key& key) override
  {
    // move the touched element at the beginning of the lru_queue
    lru_queue.splice(lru_queue.cbegin(), lru_queue, key_finder[key]);
  }

  void Erase(const Key& key) override
  {
    (void)key;
    // remove the least recently used element
    key_finder.erase(lru_queue.back());

    lru_queue.pop_back();
  }

  // return a key of a displacement candidate
  const Key& ReplCandidate() const override
  {
    return lru_queue.back();
  }

  // return a key of a displacement candidate
  void Clear() override
  {
    lru_queue.clear();
    key_finder.clear();
  }

private:

  std::list<Key> lru_queue;

  std::unordered_map<Key, lru_iterator> key_finder;
};


// FIFO 实现
template <typename Key>
class FIFOCachePolicy : public ICachePolicy<Key>
{
public:

  FIFOCachePolicy() = default;
  ~FIFOCachePolicy() = default;

  void Insert(const Key& key) override
  {
    fifo_queue.emplace_front(key);
  }

  // handle request to the key-element in a cache
  void Touch(const Key& key) override
  {
    (void)key;
    // nothing to do here in the FIFO strategy
  }

  // handle element deletion from a cache
  void Erase(const Key& key) override
  {
    (void)key;
    fifo_queue.pop_back();
  }

  // return a key of a replacement candidate
  const Key& ReplCandidate() const override
  {
    return fifo_queue.back();
  }

  // return a key of a displacement candidate
  void Clear() override
  {
    fifo_queue.clear();
  }

private:

  std::list<Key> fifo_queue;
};

// LFU 实现
template <typename Key>
class LFUCachePolicy : public ICachePolicy<Key> {
public:
  using lfu_iterator = typename std::multimap<std::size_t, Key>::iterator;

  LFUCachePolicy() = default;
  ~LFUCachePolicy() override = default;

  void Insert(const Key &key) override {
    constexpr std::size_t INIT_VAL = 1;
    // all new value initialized with the frequency 1
    lfu_storage[key] =
        frequency_storage.emplace_hint(frequency_storage.cbegin(), INIT_VAL, key);
  }

  void Touch(const Key &key) override {
    // get the previous frequency value of a key
    auto elem_for_update = lfu_storage[key];
    auto updated_elem = std::make_pair(elem_for_update->first + 1, elem_for_update->second);
    // update the previous value
    frequency_storage.erase(elem_for_update);
    lfu_storage[key] =
        frequency_storage.emplace_hint(frequency_storage.cend(), std::move(updated_elem));
  }

  void Erase(const Key &key) override {
    frequency_storage.erase(lfu_storage[key]);
    lfu_storage.erase(key);
  }

  const Key &ReplCandidate() const override {
    // at the beginning of the frequency_storage we have the
    // least frequency used value
    return frequency_storage.cbegin()->second;
  }

private:
  std::multimap<std::size_t, Key> frequency_storage;
  std::unordered_map<Key, lfu_iterator> lfu_storage;
};

// 泳道所使用的 map 接口类
template <typename Key, typename Value, template <typename> class Policy = NoCachePolicy>
class TcloudMap {
public:
  virtual ~TcloudMap() = default;
  virtual const Value& getValue(const Key &key) PURE;
  virtual const Value& getDefaultValue() PURE;
  virtual bool setKV(const Key &key, const Value &value) PURE;
  virtual bool Cached(const Key &key) const PURE;
  virtual std::size_t Size() PURE;
  virtual bool Remove(const Key &key) PURE;
};

} // namespace TcloudMap
} // namespace Envoy
