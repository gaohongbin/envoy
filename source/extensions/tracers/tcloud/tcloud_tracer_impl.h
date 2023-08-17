#pragma once

#include "envoy/config/trace/v3/tcloud.pb.h"
#include "envoy/server/tracer_config.h"
#include "envoy/thread_local/thread_local.h"
#include "envoy/tracing/http_tracer.h"

#include "common/tracing/http_tracer_impl.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace TCloudTrace {

class Driver : public Tracing::Driver, public Logger::Loggable<Logger::Id::tracing> {
public:
    explicit Driver(const envoy::config::trace::v3::TCloudTraceConfig& config,
                    Server::Configuration::TracerFactoryContext& context);

    Tracing::SpanPtr startSpan(const Tracing::Config& config, Http::RequestHeaderMap& request_headers,
                               const std::string& operation, Envoy::SystemTime start_time,
                               const Tracing::Decision decision) override;
private:
    TimeSource& time_source_;
    Random::RandomGenerator& random_generator_;
    const LocalInfo::LocalInfo& local_info_;
    bool hasDownStreamEnd_{false};
};


struct ClientSpan {
    std::string traceId = "";
    std::string spanId = "";
    std::string parentId = "";
    std::string deepId = "";
    std::string serviceName = "";
    std::string parentServiceName = "";
    std::string spanType = "http.gateway";
    std::string spanName = "";
    std::string spanKind = "client";
    std::string localIp = "";
    uint64_t startTime;
    uint64_t duration = 0;
    std::string zpBenchId = "";
    std::string tags = "{}";
};

struct ServerSpan {
    std::string traceId = "";
    std::string spanId = "";
    std::string parentId;
    std::string deepId;
    std::string serviceName;
    std::string parentServiceName;
    std::string spanType = "http.gateway";
    std::string spanName;
    std::string spanKind = "server";
    std::string localIp = "";
    uint64_t startTime;
    uint64_t duration = 0;
    std::string zpBenchId = "";
    std::string tags = "{}";
};

} // namespace TCloudTrace
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
