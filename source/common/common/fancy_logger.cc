#include "common/common/fancy_logger.h"

#include <atomic>
#include <memory>

#include "common/common/logger.h"

using spdlog::level::level_enum;

namespace Envoy {

/**
 * Implements a lock from BasicLockable, to avoid dependency problem of thread.h.
 */
class FancyBasicLockable : public Thread::BasicLockable {
public:
  // BasicLockable
  void lock() ABSL_EXCLUSIVE_LOCK_FUNCTION() override { mutex_.Lock(); }
  bool tryLock() ABSL_EXCLUSIVE_TRYLOCK_FUNCTION(true) override { return mutex_.TryLock(); }
  void unlock() ABSL_UNLOCK_FUNCTION() override { mutex_.Unlock(); }

private:
  absl::Mutex mutex_;
};

SpdLoggerSharedPtr FancyContext::getFancyLogEntry(std::string key)
    ABSL_LOCKS_EXCLUDED(fancy_log_lock_) {
  absl::ReaderMutexLock l(&fancy_log_lock_);
  auto it = fancy_log_map_->find(key);
  if (it != fancy_log_map_->end()) {
    return it->second;
  }
  return nullptr;
}

void FancyContext::setTraceLogPath(const std::string& tcloud_trace_log_path) {
    tcloud_trace_log_path_ = tcloud_trace_log_path;
}

const std::string& FancyContext::getTraceLogPath() {
    return tcloud_trace_log_path_;
}

void FancyContext::initFancyLogger(std::string key, std::atomic<spdlog::logger*>& logger)
    ABSL_LOCKS_EXCLUDED(fancy_log_lock_) {
  absl::WriterMutexLock l(&fancy_log_lock_);
  auto it = fancy_log_map_->find(key);
  spdlog::logger* target;
  if (it == fancy_log_map_->end()) {
    target = createLogger(key);
  } else {
    target = it->second.get();
  }
  logger.store(target);
}

// 初始化 trace 相关的 fancy logger
void FancyContext::initTraceFancyLogger(std::string key, std::atomic<spdlog::logger*>& logger)
ABSL_LOCKS_EXCLUDED(fancy_log_lock_) {
    absl::WriterMutexLock l(&fancy_log_lock_);
    auto it = fancy_log_map_->find(key);
    spdlog::logger* target;
    if (it == fancy_log_map_->end()) {
        target = createTraceLogger(key);
    } else {
        target = it->second.get();
    }
    logger.store(target);
}

bool FancyContext::setFancyLogger(std::string key, level_enum log_level)
    ABSL_LOCKS_EXCLUDED(fancy_log_lock_) {
  absl::ReaderMutexLock l(&fancy_log_lock_);
  auto it = fancy_log_map_->find(key);
  if (it != fancy_log_map_->end()) {
    it->second->set_level(log_level);
    return true;
  }
  return false;
}

void FancyContext::setDefaultFancyLevelFormat(spdlog::level::level_enum level, std::string format)
    ABSL_LOCKS_EXCLUDED(fancy_log_lock_) {
  if (level == Logger::Context::getFancyDefaultLevel() &&
      format == Logger::Context::getFancyLogFormat()) {
    return;
  }
  absl::ReaderMutexLock l(&fancy_log_lock_);
  for (const auto& it : *fancy_log_map_) {
    if (it.second->level() == Logger::Context::getFancyDefaultLevel()) {
      // if logger is default level now
      it.second->set_level(level);
    }
    it.second->set_pattern(format);
  }
}

std::string FancyContext::listFancyLoggers() ABSL_LOCKS_EXCLUDED(fancy_log_lock_) {
  std::string info = "";
  absl::ReaderMutexLock l(&fancy_log_lock_);
  for (const auto& it : *fancy_log_map_) {
    info += fmt::format("   {}: {}\n", it.first, static_cast<int>(it.second->level()));
  }
  return info;
}

void FancyContext::setAllFancyLoggers(spdlog::level::level_enum level)
    ABSL_LOCKS_EXCLUDED(fancy_log_lock_) {
  absl::ReaderMutexLock l(&fancy_log_lock_);
  for (const auto& it : *fancy_log_map_) {
    it.second->set_level(level);
  }
}

FancyLogLevelMap FancyContext::getAllFancyLogLevelsForTest() ABSL_LOCKS_EXCLUDED(fancy_log_lock_) {
  FancyLogLevelMap log_levels;
  absl::ReaderMutexLock l(&fancy_log_lock_);
  for (const auto& it : *fancy_log_map_) {
    log_levels[it.first] = it.second->level();
  }
  return log_levels;
}

void FancyContext::initSink() {
  spdlog::sink_ptr sink = Logger::Registry::getSink();
  Logger::DelegatingLogSinkSharedPtr sp = std::static_pointer_cast<Logger::DelegatingLogSink>(sink);
  if (!sp->hasLock()) {
    static FancyBasicLockable tlock;
    sp->setLock(tlock);
    sp->setShouldEscape(false);
  }
}

spdlog::logger* FancyContext::createLogger(std::string key, int level)
    ABSL_EXCLUSIVE_LOCKS_REQUIRED(fancy_log_lock_) {
  SpdLoggerSharedPtr new_logger =
      std::make_shared<spdlog::logger>(key, Logger::Registry::getSink());
  if (!Logger::Registry::getSink()->hasLock()) { // occurs in benchmark test
    initSink();
  }
  level_enum lv = Logger::Context::getFancyDefaultLevel();
  if (level > -1) {
    lv = static_cast<level_enum>(level);
  }
  new_logger->set_level(lv);
  new_logger->set_pattern(Logger::Context::getFancyLogFormat());
  new_logger->flush_on(level_enum::critical);
  fancy_log_map_->insert(std::make_pair(key, new_logger));
  return new_logger.get();
}

// 创建 trace logger
spdlog::logger* FancyContext::createTraceLogger(std::string key, int level)
ABSL_EXCLUSIVE_LOCKS_REQUIRED(fancy_log_lock_) {
    SpdLoggerSharedPtr new_trace_logger =
    std::make_shared<spdlog::logger>(key, Logger::Registry::getTraceFancySink(getTraceLogPath()));
    level_enum lv = Logger::Context::getFancyDefaultLevel();
    if (level > -1) {
        lv = static_cast<level_enum>(level);
    }
    new_trace_logger->set_level(lv);
    new_trace_logger->set_pattern(Logger::Context::getFancyLogFormat());
    new_trace_logger->flush_on(level_enum::critical);
    fancy_log_map_->insert(std::make_pair(key, new_trace_logger));
    return new_trace_logger.get();
}

// MUTABLE_CONSTRUCT_ON_FIRST_USE 返回静态变量, 所以每个 type 只初始化一次
FancyContext& getFancyContext() { MUTABLE_CONSTRUCT_ON_FIRST_USE(FancyContext); }

} // namespace Envoy
