#include "health_monitor/system_probe.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace health_monitor
{
namespace
{
constexpr uint32_t kUnderVoltageNowBit = 1u << 0;
constexpr uint32_t kThrottledNowBit = 1u << 2;

std::optional<std::string> read_first_line(const std::string& path)
{
  if (path.empty())
  {
    return std::nullopt;
  }
  std::ifstream file(path);
  std::string line;
  if (!file || !std::getline(file, line))
  {
    return std::nullopt;
  }
  return line;
}

std::optional<double> parse_double(const std::string& text)
{
  char* end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  if (end == text.c_str())
  {
    return std::nullopt;
  }
  return value;
}
}  // namespace

std::string find_hwmon_file(const std::string & hwmon_root, const std::string & hwmon_name, const std::string & file_name)
{
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(hwmon_root, ec))
  {
    const auto name = read_first_line((entry.path() / "name").string());
    const auto candidate = entry.path() / file_name;
    if (name && *name == hwmon_name && std::filesystem::exists(candidate, ec))
    {
      return candidate.string();
    }
  }
  return "";
}

SystemProbe::SystemProbe(const ProbePaths& paths) : paths_(paths)
{
}

SystemSample SystemProbe::sample()
{
  SystemSample result;
  result.cpu_temp_c = read_cpu_temp();
  const auto bits = read_throttled_bits();
  result.under_voltage_now = read_under_voltage(bits);
  if (bits)
  {
    result.throttled_now = (*bits & kThrottledNowBit) != 0;
  }
  result.cpu_load_pct = read_cpu_load();
  result.mem_available_pct = read_mem_available();
  return result;
}

std::optional<double> SystemProbe::read_cpu_temp() const
{
  const auto line = read_first_line(paths_.cpu_temp);
  if (!line)
  {
    return std::nullopt;
  }
  const auto millideg = parse_double(*line);
  if (!millideg)
  {
    return std::nullopt;
  }
  return *millideg / 1000.0;
}

std::optional<uint32_t> SystemProbe::read_throttled_bits() const
{
  auto line = read_first_line(paths_.throttled);
  if (!line)
  {
    return std::nullopt;
  }
  const auto eq = line->find('=');
  if (eq != std::string::npos)
  {
    line = line->substr(eq + 1);
  }
  char* end = nullptr;
  const auto value = std::strtoul(line->c_str(), &end, 16);
  if (end == line->c_str())
  {
    return std::nullopt;
  }
  return static_cast<uint32_t>(value);
}

std::optional<bool> SystemProbe::read_under_voltage(std::optional<uint32_t> throttled_bits) const
{
  const auto line = read_first_line(paths_.under_voltage_alarm);
  if (line)
  {
    const auto alarm = parse_double(*line);
    if (alarm)
    {
      return *alarm != 0.0;
    }
  }
  if (throttled_bits)
  {
    return (*throttled_bits & kUnderVoltageNowBit) != 0;
  }
  return std::nullopt;
}

std::optional<double> SystemProbe::read_cpu_load()
{
  const auto line = read_first_line(paths_.proc_stat);
  if (!line || line->rfind("cpu ", 0) != 0)
  {
    return std::nullopt;
  }
  std::istringstream fields(line->substr(4));
  std::vector<uint64_t> values;
  uint64_t value = 0;
  while (fields >> value)
  {
    values.push_back(value);
  }
  if (values.size() < 4)
  {
    return std::nullopt;
  }

  CpuTimes now{ 0, values[3] };
  if (values.size() > 4)
  {
    now.idle += values[4];
  }
  const size_t counted = std::min<size_t>(values.size(), 8);
  for (size_t i = 0; i < counted; ++i)
  {
    now.total += values[i];
  }

  const auto prev = prev_cpu_;
  prev_cpu_ = now;
  if (!prev || now.total <= prev->total)
  {
    return std::nullopt;
  }
  const double total_delta = static_cast<double>(now.total - prev->total);
  const double idle_delta = static_cast<double>(now.idle - prev->idle);
  return 100.0 * (total_delta - idle_delta) / total_delta;
}

std::optional<double> SystemProbe::read_mem_available() const
{
  std::ifstream file(paths_.meminfo);
  if (!file)
  {
    return std::nullopt;
  }
  std::optional<double> total_kb;
  std::optional<double> available_kb;
  std::string key;
  double kb = 0.0;
  std::string uint;
  while (file >> key >> kb >> uint)
  {
    if (key == "MemTotal:")
    {
      total_kb = kb;
    }
    else if (key == "MemAvailable:")
    {
      available_kb = kb;
    }
  }
  if (!total_kb || !available_kb || *total_kb <= 0.0)
  {
    return std::nullopt;
  }
  return 100.0 * *available_kb / *total_kb;
}
}  // namespace health_monitor