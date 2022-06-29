#pragma once

#include <memory>
#include <string>
#include <vector>

#include "envoy/access_log/access_log.h"
#include "envoy/common/pure.h"
#include "envoy/http/header_map.h"
#include "envoy/tracing/trace_driver.h"
#include "envoy/tracing/trace_reason.h"

namespace Envoy {
namespace Tracing {

/**
 * HttpTracer is responsible for handling traces and delegate actions to the
 * corresponding drivers.
 */
 // HttpTracer 处理跟踪并将操作委托给相应的驱动程序。
 // 这个是不是就是如果配置了 Zipkin, 就会调用 Zipkin 等来进行处理.
class HttpTracer {
public:
  virtual ~HttpTracer() = default;

  virtual SpanPtr startSpan(const Config& config, Http::RequestHeaderMap& request_headers,
                            const StreamInfo::StreamInfo& stream_info,
                            const Tracing::Decision tracing_decision) PURE;
};

using HttpTracerSharedPtr = std::shared_ptr<HttpTracer>;

} // namespace Tracing
} // namespace Envoy
