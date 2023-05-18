#include<map>
#include <cstdlib>
#include <unordered_set>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "envoy/tcloud/tcloud_map.h"

namespace Envoy {
namespace TcloudMap {

// 泳道所使用的 map 实现类。
template <typename Key, typename Value, template <typename> class Policy = FIFOCachePolicy>
class TcloudMapImpl : public TcloudMap<Key, Value, Policy> {

public:
  using iterator = typename std::unordered_map<Key, Value>::iterator;
  using Const_iterator = typename std::unordered_map<Key, Value>::const_iterator;
  using Operation_guard = typename std::lock_guard<std::mutex>;
  using Callback = typename std::function<void(const Key &key, const Value &value)>;

  TcloudMapImpl(Callback OnErase = [](const Key &, const Value &) {})
      : OnEraseCallback{OnErase} {

    // tcloud map 大小
    const char* defaultTCloudMapSizePtr = std::getenv("DefaultTCloudMapSize");
    if (defaultTCloudMapSizePtr) {
      max_cache_size = std::size_t(defaultTCloudMapSizePtr);
    } else {
      max_cache_size = std::size_t(50000);  // 默认五万
    }

    // 默认泳道名称
    const char* defaultTCloudLanePtr = std::getenv("DefaultTCloudLane");
    if (defaultTCloudLanePtr) {
      defaultTCloudLane = std::string(defaultTCloudLanePtr);
    } else {
      defaultTCloudLane = "";
    }

    emptyValue = "";
  }

  ~TcloudMapImpl() {
    Clear();
  }

  const Value& getDefaultValue() {
    return defaultTCloudLane;
  }

  const Value& getValue(const Key &key) {
    Operation_guard lock{safe_op};
    auto elem = GetInternal(key);

    if (elem.second) {
      return elem.first->second;
    } else {
      return emptyValue;
    }
  }

  bool setKV(const Key &key, const Value &value) {
    Operation_guard lock{safe_op};
    auto elem_it = FindElem(key);

    if (elem_it == cache_items_map.end()) {
      // add new element to the cache
      if (cache_items_map.size() + 1 > max_cache_size) {
        auto disp_candidate_key = cache_policy.ReplCandidate();
        Erase(disp_candidate_key);
      }
      Insert(key, value);
    } else {
      // update previous value
      Update(key, value);
    }
    return true;
  }

  bool Cached(const Key &key) const {
    Operation_guard lock{safe_op};
    return FindElem(key) != cache_items_map.cend();
  }

  std::size_t Size() {
    Operation_guard lock{safe_op};
    return cache_items_map.size();
  }

  bool Remove(const Key &key) {
    Operation_guard lock{safe_op};

    auto elem = FindElem(key);
    if (elem == cache_items_map.end()) {
      return false;
    }

    Erase(elem);
    return true;
  }

protected:
  void Clear() {
    Operation_guard lock{safe_op};

    std::for_each(begin(), end(),
                  [&](const std::pair<const Key, Value> &el) { cache_policy.Erase(el.first); });
    cache_items_map.clear();
  }

  Const_iterator begin() const {
    return cache_items_map.cbegin();
  }

  Const_iterator end() const {
    return cache_items_map.cend();
  }

  void Insert(const Key &key, const Value &value) {
    cache_policy.Insert(key);
    cache_items_map.emplace(std::make_pair(key, value));
  }

  void Erase(Const_iterator elem) {
    cache_policy.Erase(elem->first);
    OnEraseCallback(elem->first, elem->second);
    cache_items_map.erase(elem);
  }

  void Erase(const Key &key) {
    auto elem_it = FindElem(key);

    Erase(elem_it);
  }

  void Update(const Key &key, const Value &value) {
    cache_policy.Touch(key);
    cache_items_map[key] = value;
  }

  Const_iterator FindElem(const Key &key) const {
    return cache_items_map.find(key);
  }

  std::pair<Const_iterator, bool> GetInternal(const Key &key) const {
    auto elem_it = FindElem(key);

    if (elem_it != end()) {
      cache_policy.Touch(key);
      return {elem_it, true};
    }

    return {elem_it, false};
  }

private:
  std::string defaultTCloudLane;
  std::string emptyValue;

  std::unordered_map<Key, Value> cache_items_map;
  mutable Policy<Key> cache_policy;
  mutable std::mutex safe_op;
  size_t max_cache_size;
  Callback OnEraseCallback;
};

} // namespace TcloudMap
} // namespace Envoy
