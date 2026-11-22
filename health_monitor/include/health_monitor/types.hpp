#pragma once

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

namespace health_monitor
{
enum class Level : uint8_t
{
  OK = 0,
  DEGRADED = 1,
  FAULT = 2
};

// FaultItem.msg 상수와 같은 값 (SAE J2012 DTC 형식)
enum class FaultCode : uint16_t
{
  TOPIC_NEVER_RECEIVED = 0xD001,
  TOPIC_STALE = 0xD002,
  TOPIC_RATE_LOW = 0xD003,
  CPU_TEMP_HIGH = 0x9001,
  CPU_TEMP_CRITICAL = 0x9002,
  THROTTLED = 0x9101,
  CPU_LOAD_HIGH = 0x9201,
  MEM_LOW = 0x9202,
  UNDER_VOLTAGE = 0x1001,
};

struct Fault
{
  FaultCode code;
  Level severity;
  std::string source;
  std::string message;
  double value;
  double threshold;
};

struct TopicSample
{
  std::string name;
  bool ever_received{ false };
  double age_sec{ 0.0 };
  double rate_hz{ 0.0 };
  double min_rate_hz{ 0.0 };
  double timeout_sec{ 2.0 };
  Level severity_on_loss{ Level::FAULT };
};

struct SystemSample
{
  std::optional<double> cpu_temp_c;
  std::optional<bool> under_voltage_now;
  std::optional<bool> throttled_now;
  std::optional<double> cpu_load_pct;
  std::optional<double> mem_available_pct;
};

inline const char* to_string(Level level)
{
  switch (level)
  {
    case Level::OK:
      return "OK";
    case Level::DEGRADED:
      return "DEGRADED";
    case Level::FAULT:
      return "FAULT";
  }
  return "UNKNOWN";
}

inline Level max_level(Level a, Level b)
{
  return static_cast<uint8_t>(a) >= static_cast<uint8_t>(b) ? a : b;
}

inline std::string to_dtc_string(FaultCode code)
{
  const auto v = static_cast<uint16_t>(code);
  const char system = "PCBU"[(v >> 14) & 0x3];
  char buf[8];
  std::snprintf(buf, sizeof(buf), "%c%u%X%X%X", system, (v >> 12) & 0x3u, (v >> 8) & 0xFu, (v >> 4) & 0xFu, v & 0xFu);
  return buf;
}
}  // namespace health_monitor