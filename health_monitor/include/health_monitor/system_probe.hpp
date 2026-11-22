#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "health_monitor/types.hpp"

namespace health_monitor
{
struct ProbePaths
{
  std::string cpu_temp{ "/sys/class/thermal/thermal_zone0/temp" };
  std::string under_voltage_alarm;
  std::string throttled;
  std::string proc_stat{ "/proc/stat" };
  std::string meminfo{ "/proc/meminfo" };
};

std::string find_hwmon_file(const std::string& hwmon_root, const std::string& hwmon_name, const std::string& file_name);

class SystemProbe
{
public:
  explicit SystemProbe(const ProbePaths& paths);

  SystemSample sample();

private:
  struct CpuTimes
  {
    uint64_t total;
    uint64_t idle;
  };

  std::optional<double> read_cpu_temp() const;
  std::optional<uint32_t> read_throttled_bits() const;
  std::optional<bool> read_under_voltage(std::optional<uint32_t> throttled_bits) const;
  std::optional<double> read_cpu_load();
  std::optional<double> read_mem_available() const;

  ProbePaths paths_;
  std::optional<CpuTimes> prev_cpu_;
};
}  // namespace health_monitor