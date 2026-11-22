#include "health_monitor/state_evaluator.hpp"

#include <limits>
#include <string>

namespace health_monitor
{
namespace
{
constexpr double kNotApplicable = std::numeric_limits<double>::quiet_NaN();

Fault make_fault(FaultCode code, Level severity, const std::string& source, const std::string& detail, double value,
                 double threshold)
{
  return Fault{ code, severity, source, to_dtc_string(code) + " " + detail, value, threshold };
}
}  // namespace

StateEvaluator::StateEvaluator(const EvaluatorConfig& config) : config_(config)
{
}

Evaluation StateEvaluator::evaluate(const std::vector<TopicSample>& topics, const SystemSample& system, double now_sec)
{
  if (!start_sec_)
  {
    start_sec_ = now_sec;
  }

  const bool in_grace = now_sec - *start_sec_ < config_.startup_grace_sec;

  Evaluation result;
  for (const auto& topic : topics)
  {
    check_topic(topic, in_grace, result.faults);
  }

  check_system(system, result.faults);

  for (const auto& fault : result.faults)
  {
    result.raw_level = max_level(result.raw_level, fault.severity);
  }
  result.level_changed = apply_hysteresis(result.raw_level, now_sec);
  result.level = level_;
  return result;
}

void StateEvaluator::check_topic(const TopicSample& topic, bool in_grace, std::vector<Fault>& faults) const
{
  if (!topic.ever_received)
  {
    if (!in_grace)
    {
      faults.push_back(make_fault(FaultCode::TOPIC_NEVER_RECEIVED, topic.severity_on_loss, topic.name,
                                  "never received since startup", kNotApplicable, config_.startup_grace_sec));
    }
    return;
  }

  if (topic.age_sec > topic.timeout_sec)
  {
    faults.push_back(make_fault(FaultCode::TOPIC_STALE, topic.severity_on_loss, topic.name, "no message within timeout",
                                topic.age_sec, topic.timeout_sec));
    return;
  }

  if (!in_grace && topic.min_rate_hz > 0.0 && topic.rate_hz < topic.min_rate_hz)
  {
    faults.push_back(make_fault(FaultCode::TOPIC_RATE_LOW, Level::DEGRADED, topic.name, "rate below minimum",
                                topic.rate_hz, topic.min_rate_hz));
  }
}

void StateEvaluator::check_system(const SystemSample& system, std::vector<Fault>& faults) const
{
  if (system.cpu_temp_c)
  {
    const double temp = *system.cpu_temp_c;
    if (temp >= config_.cpu_temp_critical_c)
    {
      faults.push_back(make_fault(FaultCode::CPU_TEMP_CRITICAL, Level::FAULT, "cpu", "temperature critical", temp,
                                  config_.cpu_temp_critical_c));
    }
    else if (temp >= config_.cpu_temp_high_c)
    {
      faults.push_back(make_fault(FaultCode::CPU_TEMP_HIGH, Level::DEGRADED, "cpu", "temperature high", temp,
                                  config_.cpu_temp_high_c));
    }
  }

  if (system.under_voltage_now.value_or(false))
  {
    faults.push_back(make_fault(FaultCode::UNDER_VOLTAGE, config_.under_voltage_level, "power",
                                "under-voltage detected", kNotApplicable, kNotApplicable));
  }

  if (system.throttled_now.value_or(false))
  {
    faults.push_back(make_fault(FaultCode::THROTTLED, config_.throttled_level, "cpu", "throttling active",
                                kNotApplicable, kNotApplicable));
  }

  if (config_.cpu_load_high_pct > 0.0 && system.cpu_load_pct && *system.cpu_load_pct >= config_.cpu_load_high_pct)
  {
    faults.push_back(make_fault(FaultCode::CPU_LOAD_HIGH, Level::DEGRADED, "cpu", "load high", *system.cpu_load_pct,
                                config_.cpu_load_high_pct));
  }

  if (config_.mem_available_low_pct > 0.0 && system.mem_available_pct &&
      *system.mem_available_pct <= config_.mem_available_low_pct)
  {
    faults.push_back(make_fault(FaultCode::MEM_LOW, Level::DEGRADED, "memory", "available memory low",
                                *system.mem_available_pct, config_.mem_available_low_pct));
  }
}

bool StateEvaluator::apply_hysteresis(Level raw, double now_sec)
{
  if (raw > level_)
  {
    level_ = raw;
    pending_since_.reset();
    return true;
  }

  if (raw == level_)
  {
    pending_since_.reset();
    return false;
  }

  if (!pending_since_ || raw != pending_level_)
  {
    pending_level_ = raw;
    pending_since_ = now_sec;
  }

  if (now_sec - *pending_since_ < config_.recovery_hold_sec)
  {
    return false;
  }

  level_ = raw;
  pending_since_.reset();
  return true;
}
}  // namespace health_monitor