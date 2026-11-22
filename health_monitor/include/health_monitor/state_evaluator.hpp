#pragma once

#include <optional>
#include <vector>

#include "health_monitor/types.hpp"

namespace health_monitor
{
struct EvaluatorConfig
{
  double startup_grace_sec{ 5.0 };
  double recovery_hold_sec{ 2.0 };
  double cpu_temp_high_c{ 80.0 };
  double cpu_temp_critical_c{ 85.0 };
  Level under_voltage_level{ Level::DEGRADED };
  Level throttled_level{ Level::DEGRADED };
  double cpu_load_high_pct{ 0.0 };
  double mem_available_low_pct{ 0.0 };
};

struct Evaluation
{
  Level level{ Level::OK };
  Level raw_level{ Level::OK };
  std::vector<Fault> faults;
  bool level_changed{ false };
};

class StateEvaluator
{
public:
  explicit StateEvaluator(const EvaluatorConfig& config);

  Evaluation evaluate(const std::vector<TopicSample>& topics, const SystemSample& system, double now_sec);

private:
  void check_topic(const TopicSample& topic, bool in_grace, std::vector<Fault>& faults) const;
  void check_system(const SystemSample& system, std::vector<Fault>& faults) const;
  bool apply_hysteresis(Level raw, double now_sec);

  EvaluatorConfig config_;
  std::optional<double> start_sec_;
  Level level_{ Level::OK };
  Level pending_level_{ Level::OK };
  std::optional<double> pending_since_;
};
}  // namespace health_monitor