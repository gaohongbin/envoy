#pragma once

#include <string>

#include "source/common/http/header_map_impl.h"

#include "absl/types/optional.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace RocketmqProxy {

// 这个是针对 RocketMQ request 的一些元信息的抽取
// 可以联想到 meta-protocol-proxy 中的 metadata
class MessageMetadata {
public:
  MessageMetadata() = default;

  void setOneWay(bool oneway) { is_oneway_ = oneway; }
  bool isOneWay() const { return is_oneway_; }

  bool hasTopicName() const { return topic_name_.has_value(); }
  const std::string& topicName() const { return topic_name_.value(); }
  void setTopicName(const std::string& topic_name) { topic_name_ = topic_name; }

  /**
   * @return HeaderMap of current headers
   */
  const Http::HeaderMap& headers() const { return *headers_; }
  Http::HeaderMap& headers() { return *headers_; }

private:
  bool is_oneway_{false};
  absl::optional<std::string> topic_name_{};

  Http::HeaderMapPtr headers_{Http::RequestHeaderMapImpl::create()};
};

using MessageMetadataSharedPtr = std::shared_ptr<MessageMetadata>;

} // namespace RocketmqProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
