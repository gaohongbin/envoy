#include "extensions/tracers/tcloud/tcloud_tracer_impl.h"

#include "envoy/config/trace/v3/tcloud.pb.h"
#include "envoy/http/header_map.h"
#include "common/common/logger.h"


namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace TCloudTrace {

class Span : public Tracing::Span {
public:
    Span(Http::RequestHeaderMap& request_headers, SystemTime start_time, TimeSource& time_source,
         Random::RandomGenerator& random_generator, const LocalInfo::LocalInfo& local_info, bool hasDownStreamEnd);

    void setOperation(absl::string_view operation) override;
    void setTag(absl::string_view name, absl::string_view value) override;
    void log(SystemTime timestamp, const std::string& event) override;
    void finishSpan() override;
    void injectContext(Http::RequestHeaderMap& request_headers) override;
    Tracing::SpanPtr spawnChild(const Tracing::Config& config, const std::string& name, SystemTime start_time) override;
    void setSampled(bool sampled) override;
    void setBaggage(absl::string_view, absl::string_view) override{};
    std::string getBaggage(absl::string_view) override { return EMPTY_STRING; };

    std::string getTraceIdAsHex() const override;

private:
    std::unique_ptr<ClientSpan> clientSpanPtr_{nullptr};
    std::unique_ptr<ServerSpan> serverSpanPtr_{nullptr};

    TimeSource& time_source_;
    Random::RandomGenerator& random_generator_;
    const LocalInfo::LocalInfo& local_info_;
    bool hasDownStreamEnd_{false};
};


// Span 实现
Span::Span(Http::RequestHeaderMap& request_headers, SystemTime start_time, TimeSource& time_source,
    Random::RandomGenerator& random_generator, const LocalInfo::LocalInfo& local_info, bool hasDownStreamEnd)
    :time_source_(time_source), random_generator_(random_generator), local_info_(local_info), hasDownStreamEnd_(hasDownStreamEnd) {

    // 先获取 header 中的字段
    std::string zpBenchId = std::string(request_headers.getZpBenchIdValue());
    std::string traceId = std::string(request_headers.getTraceIdValue());
    std::string twlSpanContext = std::string(request_headers.getTwlSpanContextValue());

    // 初始化 serverSpan 和 clientSpan
    serverSpanPtr_ = std::make_unique<ServerSpan>();
    clientSpanPtr_ = std::make_unique<ClientSpan>();

    if (!traceId.empty()) {
        // 生成 server Span
        serverSpanPtr_.traceId = traceId;
        serverSpanPtr_.spanId = std::to_string(random_generator_.random());
        serverSpanPtr_.parentId = "0";
        serverSpanPtr_.deepId = "1";
        serverSpanPtr_.serviceName = std::string(std::getenv("TWL_LABEL_app"));
        serverSpanPtr_.parentServiceName = std::string(std::getenv("TWL_LABEL_app"));
        serverSpanPtr_.spanType = "http.gateway";
        serverSpanPtr_.spanName = std::string(request_headers.getMethodValue()) + "_" + std::string(request_headers.getPathValue());
        serverSpanPtr_.spanKind = "server";
        serverSpanPtr_.localIp = local_info_.address()->ip()->addressAsString();
        serverSpanPtr_.startTime = std::chrono::duration_cast<std::chrono::milliseconds>(time_source_.systemTime().time_since_epoch()).count();
        if (!zpBenchId.empty()) {
            serverSpanPtr_.zpBenchId = zpBenchId;
        } else {
            serverSpanPtr_.zpBenchId = "0";
        }

        // 生成 client span
        clientSpanPtr_.traceId = traceId;
        clientSpanPtr_.spanId = std::to_string(random_generator_.random());
        clientSpanPtr_.parentId = serverSpan.spanId;
        clientSpanPtr_.deepId = "1.1";
        clientSpanPtr_.serviceName = std::string(std::getenv("TWL_LABEL_app"));
        clientSpanPtr_.parentServiceName = std::string(std::getenv("TWL_LABEL_app"));
        clientSpanPtr_.spanType = "http.gateway";
        clientSpanPtr_.spanName = std::string(request_headers.getMethodValue()) + "_" + std::string(request_headers.getPathValue());
        clientSpanPtr_.spanKind = "client";
        clientSpanPtr_.localIp = local_info_.address()->ip()->addressAsString();
        clientSpanPtr_.startTime = std::chrono::duration_cast<std::chrono::milliseconds>(time_source_.systemTime().time_since_epoch()).count();
        if (!zpBenchId.empty()) {
            clientSpanPtr_.zpBenchId = zpBenchId;
        } else {
            clientSpanPtr_.zpBenchId = "0";
        }

        // twlSpanContext = traceId:spanId:parentId:parentServiceName:deepId:nextDeepId:zpBenchId
        std::string newTwlSpanContext = clientSpanPtr_.traceId + ":" + clientSpanPtr_.spanId + ":" + clientSpanPtr_.parentId + ":" + clientSpanPtr_.parentServiceName + ":" + clientSpanPtr_.deepId + ":2:" + clientSpanPtr_.zpBenchId;
        request_headers.removeTwlSpanContext();
        request_headers.setTwlSpanContext(newTwlSpanContext);
        return ;

    } else if (!twlSpanContext.empty()){
        std::vector<std::string> twlSpanSpilts = absl::StrSplit(twlSpanContext, ':');
        if (twlSpanSpilts.size() != 7) {
            return ;
        }

        std::string nextDeepId = twlSpanSpilts[5];
        int nextDeepIdInt = atoi(nextDeepId.c_str());


        // twlSpanContext = traceId:spanId:parentId:parentServiceName:deepId:nextDeepId:zpBenchId
        serverSpanPtr_.traceId = twlSpanSpilts[0];
        serverSpanPtr_.spanId = std::to_string(random_generator_.random());
        serverSpanPtr_.parentId = twlSpanSpilts[2];
        serverSpanPtr_.deepId = twlSpanSpilts[4] + '.' + twlSpanSpilts[5];
        serverSpanPtr_.serviceName = std::string(std::getenv("TWL_LABEL_app"));
        serverSpanPtr_.parentServiceName = twlSpanSpilts[3];
        serverSpanPtr_.spanType = "http.gateway";
        serverSpanPtr_.spanName = std::string(request_headers.getMethodValue()) + "_" + std::string(request_headers.getPathValue());
        serverSpanPtr_.spanKind = "server";
        serverSpanPtr_.localIp = local_info_.address()->ip()->addressAsString();
        serverSpanPtr_.startTime = std::chrono::duration_cast<std::chrono::milliseconds>(time_source_.systemTime().time_since_epoch()).count();
        serverSpanPtr_.zpBenchId = twlSpanSpilts[6];

        clientSpanPtr_.traceId = twlSpanSpilts[0];
        clientSpanPtr_.spanId = std::to_string(random_generator_.random());
        clientSpanPtr_.parentId = serverSpan.spanId;
        // 这里有个 +1 的逻辑
        clientSpanPtr_.deepId = twlSpanSpilts[4] + '.' + std::to_string(nextDeepIdInt + 1);
        clientSpanPtr_.serviceName = std::string(std::getenv("TWL_LABEL_app"));
        clientSpanPtr_.parentServiceName = std::string(std::getenv("TWL_LABEL_app"));
        clientSpanPtr_.spanType = "http.gateway";
        clientSpanPtr_.spanName = std::string(request_headers.getMethodValue()) + "_" + std::string(request_headers.getPathValue());
        clientSpanPtr_.spanKind = "client";
        clientSpanPtr_.localIp = local_info_.address()->ip()->addressAsString();
        clientSpanPtr_.startTime = std::chrono::duration_cast<std::chrono::milliseconds>(time_source_.systemTime().time_since_epoch()).count();
        clientSpanPtr_.zpBenchId = twlSpanSpilts[6];

        // todo 这里 nextDeepId 需要修改
        std::string newTwlSpanContext = clientSpanPtr_.traceId + ":" + clientSpanPtr_.spanId + ":" + clientSpanPtr_.parentId + ":" + clientSpanPtr_.parentServiceName + ":" + clientSpanPtr_.deepId + ":" + std::to_string(nextDeepIdInt + 2) + ":" + clientSpanPtr_.zpBenchId;
        request_headers.removeTwlSpanContext();
        request_headers.setTwlSpanContext(newTwlSpanContext);
        return;

    } else {

        serverSpanPtr_.traceId = std::to_string(random_generator_.random());
        serverSpanPtr_.spanId = std::to_string(random_generator_.random());
        serverSpanPtr_.parentId = "0";
        serverSpanPtr_.deepId = "1";
        serverSpanPtr_.serviceName = std::string(std::getenv("TWL_LABEL_app"));
        serverSpanPtr_.parentServiceName = std::string(std::getenv("TWL_LABEL_app"));
        serverSpanPtr_.spanType = "http.gateway";
        serverSpanPtr_.spanName = std::string(request_headers.getMethodValue()) + "_" + std::string(request_headers.getPathValue());
        serverSpanPtr_.spanKind = "server";
        serverSpanPtr_.localIp = local_info_.address()->ip()->addressAsString();
        serverSpanPtr_.startTime = std::chrono::duration_cast<std::chrono::milliseconds>(time_source_.systemTime().time_since_epoch()).count();
        serverSpanPtr_.zpBenchId = "0";

        clientSpanPtr_.traceId = serverSpan.traceId;
        clientSpanPtr_.spanId = std::to_string(random_generator_.random());
        clientSpanPtr_.parentId = serverSpan.spanId;
        clientSpanPtr_.deepId = "1.1";
        clientSpanPtr_.serviceName = std::string(std::getenv("TWL_LABEL_app"));
        clientSpanPtr_.parentServiceName = std::string(std::getenv("TWL_LABEL_app"));
        clientSpanPtr_.spanType = "http.gateway";
        clientSpanPtr_.spanName = std::string(request_headers.getMethodValue()) + "_" + std::string(request_headers.getPathValue());
        clientSpanPtr_.spanKind = "client";
        clientSpanPtr_.localIp = local_info_.address()->ip()->addressAsString();
        clientSpanPtr_.startTime = std::chrono::duration_cast<std::chrono::milliseconds>(time_source_.systemTime().time_since_epoch()).count();
        clientSpanPtr_.zpBenchId = "0";

        std::string newTwlSpanContext = clientSpanPtr_.traceId + ":" + clientSpanPtr_.spanId + ":" + clientSpanPtr_.parentId + ":" + clientSpanPtr_.parentServiceName + ":" + clientSpanPtr_.deepId + ":2:" + clientSpanPtr_.zpBenchId;
        request_headers.removeTwlSpanContext();
        request_headers.setTwlSpanContext(newTwlSpanContext);
        return ;
    }
    return ;
}

void Span::setOperation(absl::string_view operation) { }

void Span::setTag(absl::string_view name, absl::string_view value) {}

void Span::log(SystemTime, const std::string& event) {}

void Span::finishSpan() {
    uint64_t endTime = std::chrono::duration_cast<std::chrono::milliseconds>(time_source_.systemTime().time_since_epoch()).count();

    if (hasDownStreamEnd_) {
        clientSpanPtr_.duration = endTime - clientSpanPtr_.startTime;
        ENVOY_TRACE_LOG(info, "{} - {} - {} - {} - {} - {} - {} - {} - {} - {} - {} - {} - {} - {}",  clientSpanPtr_.traceId, clientSpanPtr_.spanId, clientSpanPtr_.parentId, clientSpanPtr_.deepId, clientSpanPtr_.serviceName, clientSpanPtr_.parentServiceName, clientSpanPtr_.spanType, clientSpanPtr_.spanName, clientSpanPtr_.spanKind, clientSpanPtr_.localIp, clientSpanPtr_.startTime, clientSpanPtr_.duration, clientSpanPtr_.tags, clientSpanPtr_.zpBenchId);
        return;

    } else {
        serverSpanPtr_.duration = endTime - serverSpanPtr_.startTime;
        // traceId - spanId - parentId - deepId - serviceName - parentServiceName - spanType - spanName - spanKind - localIp - startTime - duration - tags - zpBenchId
        ENVOY_TRACE_LOG(info, "{} - {} - {} - {} - {} - {} - {} - {} - {} - {} - {} - {} - {} - {}",  serverSpanPtr_.traceId, serverSpanPtr_.spanId, serverSpanPtr_.parentId, serverSpanPtr_.deepId, serverSpanPtr_.serviceName, serverSpanPtr_.parentServiceName, serverSpanPtr_.spanType, serverSpanPtr_.spanName, serverSpanPtr_.spanKind, serverSpanPtr_.localIp, serverSpanPtr_.startTime, serverSpanPtr_.duration, serverSpanPtr_.tags, serverSpanPtr_.zpBenchId);
        hasDownStreamEnd_ = true;
        return;
    }
}

void Span::injectContext(Http::RequestHeaderMap& request_headers) {
}

std::string Span::getTraceIdAsHex() const {
}

Tracing::SpanPtr Span::spawnChild(const Tracing::Config& /*config*/, const std::string& name,
                                  SystemTime /*start_time*/) {
}

void Span::setSampled(bool sampled) { }



// Driver 实现
Driver::Driver(const envoy::config::trace::v3::TCloudTraceConfig& config,
               Server::Configuration::TracerFactoryContext& context) {

    auto& factory_context = context.serverFactoryContext();
    random_generator_ = factory_context.api().randomGenerator();
    local_info_ = factory_context.localInfo();
    time_source_ = factory_context.api().timeSource();
}

Tracing::SpanPtr Driver::startSpan(const Tracing::Config& /* config */,
                                   Http::RequestHeaderMap& request_headers,
                                   const std::string& /* operation_name */, SystemTime start_time,
                                   const Tracing::Decision /* tracing_decision */) {
    return std::make_unique<Span>(request_headers, start_time, time_source_; random_generator_; local_info_; false);
}

} // namespace OpenCensus
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
