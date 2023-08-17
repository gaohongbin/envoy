#include "extensions/tracers/tcloud/config.h"

#include "envoy/config/trace/v3/tcloud.pb.h"
#include "envoy/config/trace/v3/tcloud.pb.validate.h"
#include "envoy/registry/registry.h"

#include "common/tracing/http_tracer_impl.h"

#include "extensions/tracers/tcloud/tcloud_tracer_impl.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace TCloudTrace {

TCloudTracerFactory::TCloudTracerFactory() : FactoryBase("envoy.tracers.tcloud") {}

Tracing::HttpTracerSharedPtr TCloudTracerFactory::createHttpTracerTyped(
        const envoy::config::trace::v3::TCloudTraceConfig& proto_config,
        Server::Configuration::TracerFactoryContext& context) {

    Tracing::DriverPtr tcloud_trace_driver = std::make_unique<TCloudTrace::Driver>(proto_config, context);
    return std::make_shared<Tracing::HttpTracerImpl>(std::move(tcloud_trace_driver), context.serverFactoryContext().localInfo());
}

/**
 * Static registration for the TCloudTrace tracer. @see RegisterFactory.
 */
REGISTER_FACTORY(TCloudTracerFactory, Server::Configuration::TracerFactory);

} // namespace TCloudTrace
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
