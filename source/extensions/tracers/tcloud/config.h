#pragma once

#include <string>

#include "envoy/config/trace/v3/tcloud.pb.h"
#include "envoy/config/trace/v3/tcloud.pb.validate.h"

#include "extensions/tracers/common/factory_base.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace TCloudTrace {

/**
 * Config registration for the TCloudTrace tracer. @see TracerFactory.
 */
class TCloudTraceTracerFactory: public Common::FactoryBase<envoy::config::trace::v3::TCloudTraceConfig> {
public:
    TCloudTraceTracerFactory();

private:
    // FactoryBase
    Tracing::HttpTracerSharedPtr
    createHttpTracerTyped(const envoy::config::trace::v3::TCloudTraceConfig& proto_config,
                                          Server::Configuration::TracerFactoryContext& context) override;
};

} // namespace OpenCensus
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
