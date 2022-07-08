#pragma once

#include<map>

#include "absl/strings/string_view.h"
#include "envoy/common/pure.h"

namespace Envoy {
namespace TcloudMap {

// 泳道所使用的 map 接口类
class TcloudMap {
public:
  virtual ~TcloudMap() = default;
  virtual absl::string_view getValue(const absl::string_view& key) PURE;
  virtual bool setKV(const absl::string_view& key, const absl::string_view& value) PURE;

};

} // namespace TcloudMap
} // namespace Envoy
