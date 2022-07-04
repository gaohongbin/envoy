#include "source/exe/main_common.h"

#ifdef WIN32
#include "source/exe/service_base.h"
#endif

// NOLINT(namespace-envoy)

/**
 * Basic Site-Specific main()
 *
 * This should be used to do setup tasks specific to a particular site's
 * deployment such as initializing signal handling. It calls main_common
 * after setting up command line options.
 */
 // C 语言启动函数 main 函数
int main(int argc, char** argv) {
#ifdef WIN32
  Envoy::ServiceBase service;
  if (!Envoy::ServiceBase::TryRunAsService(service)) {
    return Envoy::MainCommon::main(argc, argv);
  }
  return EXIT_SUCCESS;
#endif
  // 不用管 win32, 直接看我们需要的内容
  return Envoy::MainCommon::main(argc, argv);
}
