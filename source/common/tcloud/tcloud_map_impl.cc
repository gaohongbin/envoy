#include "common/tcloud/tcloud_map_impl.h"

namespace Envoy {
namespace TcloudMap {

absl::string_view TcloudMapImpl::getValue(const absl::string_view& key) {
  map<absl::string_view, absl::string_view>::iterator iter = map_data_.find(key);
  if (iter != map_data_.end()) {
    return iter->second;
  } else {
    return defaultEmptyResult;
  }
}

bool TcloudMapImpl::setKV(const absl::string_view& key, const absl::string_view& value) {
  map<absl::string_view, absl::string_view>::iterator iter = map_data_.find(key);
  if (iter != map_data_.end()) {
    iter -> second = value;
  } else {
    map_data_.insert(std::make_pair(key, value));
  }
  return true;
}

} // namespace TcloudMap
} // namespace Envoy
