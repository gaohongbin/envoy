#include<map>

#include "envoy/tcloud/tcloud_map.h"

using namespace std;

namespace Envoy {
namespace TcloudMap {


// 泳道所使用的 map 实现类。
class TcloudMapImpl : public Envoy::TcloudMap::TcloudMap {

public:
  absl::string_view getValue(const absl::string_view& key) override;
  bool setKV(const absl::string_view& key, const absl::string_view& value) override;

private:
  map<absl::string_view, absl::string_view> map_data_;
  absl::string_view defaultEmptyResult;
};

} // namespace TcloudMap
} // namespace Envoy
