#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "health_monitor/rate_tracker.hpp"
#include "health_monitor/state_evaluator.hpp"
#include "health_monitor/system_probe.hpp"
#include "health_monitor/topic_watchdog.hpp"
#include "rclcpp/rclcpp.hpp"
#include "robot_interfaces/msg/robot_status.hpp"

namespace health_monitor
{
using robot_interfaces::msg::FaultItem;
using robot_interfaces::msg::RobotStatus;

static_assert(RobotStatus::LEVEL_OK == static_cast<uint8_t>(Level::OK), "level mismatch");
static_assert(RobotStatus::LEVEL_DEGRADED == static_cast<uint8_t>(Level::DEGRADED), "level mismatch");
static_assert(RobotStatus::LEVEL_FAULT == static_cast<uint8_t>(Level::FAULT), "level mismatch");
static_assert(FaultItem::TOPIC_NEVER_RECEIVED == static_cast<uint16_t>(FaultCode::TOPIC_NEVER_RECEIVED),
              "code mismatch");
static_assert(FaultItem::TOPIC_STALE == static_cast<uint16_t>(FaultCode::TOPIC_STALE), "code mismatch");
static_assert(FaultItem::TOPIC_RATE_LOW == static_cast<uint16_t>(FaultCode::TOPIC_RATE_LOW), "code mismatch");
static_assert(FaultItem::CPU_TEMP_HIGH == static_cast<uint16_t>(FaultCode::CPU_TEMP_HIGH), "code mismatch");
static_assert(FaultItem::CPU_TEMP_CRITICAL == static_cast<uint16_t>(FaultCode::CPU_TEMP_CRITICAL), "code mismatch");
static_assert(FaultItem::THROTTLED == static_cast<uint16_t>(FaultCode::THROTTLED), "code mismatch");
static_assert(FaultItem::CPU_LOAD_HIGH == static_cast<uint16_t>(FaultCode::CPU_LOAD_HIGH), "code mismatch");
static_assert(FaultItem::MEM_LOW == static_cast<uint16_t>(FaultCode::MEM_LOW), "code mismatch");
static_assert(FaultItem::UNDER_VOLTAGE == static_cast<uint16_t>(FaultCode::UNDER_VOLTAGE), "code mismatch");

namespace
{
Level parse_level(const std::string& text)
{
  if (text == "ok")
  {
    return Level::OK;
  }

  if (text == "degraded")
  {
    return Level::DEGRADED;
  }

  if (text == "fault")
  {
    return Level::FAULT;
  }

  throw std::invalid_argument("unknown level: " + text);
}

FaultItem to_msg(const Fault& fault)
{
  FaultItem item;
  item.code = static_cast<uint16_t>(fault.code);
  item.severity = static_cast<uint8_t>(fault.severity);
  item.source = fault.source;
  item.message = fault.message;
  item.value = fault.value;
  item.threshold = fault.threshold;
  return item;
}
}  // namespace

class HealthMonitorNode : public rclcpp::Node
{
public:
  explicit HealthMonitorNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions())
    : Node("health_monitor", options), evaluator_(load_evaluator_config()), probe_(load_probe_paths())
  {
    const double window = declare_parameter("rate_window_sec", 1.0);
    watchdog_ = std::make_unique<TopicWatchdog>(*this, load_watched_topics(), window);
    publisher_ = create_publisher<RobotStatus>("robot_status", rclcpp::QoS(1).reliable().transient_local());
    system_ = probe_.sample();

    const double publish_hz = declare_parameter("publish_hz", 10.0);
    fast_timer_ = create_wall_timer(std::chrono::milliseconds(static_cast<int>(1000.0 / publish_hz)),
                                    [this]() { on_fast_tick(); });
    slow_timer_ = create_wall_timer(std::chrono::seconds(1), [this]() { on_slow_tick(); });
  }

private:
  EvaluatorConfig load_evaluator_config()
  {
    EvaluatorConfig config;
    config.startup_grace_sec = declare_parameter("startup_grace_sec", config.startup_grace_sec);
    config.recovery_hold_sec = declare_parameter("recovery_hold_sec", config.recovery_hold_sec);
    config.cpu_temp_high_c = declare_parameter("cpu_temp_high_c", config.cpu_temp_high_c);
    config.cpu_temp_critical_c = declare_parameter("cpu_temp_critical_c", config.cpu_temp_critical_c);
    config.under_voltage_level = parse_level(declare_parameter("under_voltage_level", std::string("degraded")));
    config.throttled_level = parse_level(declare_parameter("throttled_level", std::string("degraded")));
    config.cpu_load_high_pct = declare_parameter("cpu_load_high_pct", 0.0);
    config.mem_available_low_pct = declare_parameter("mem_available_low_pct", 0.0);
    return config;
  }

  ProbePaths load_probe_paths()
  {
    ProbePaths paths;
    paths.cpu_temp = declare_parameter("probe.cpu_temp", paths.cpu_temp);
    paths.throttled = declare_parameter("probe.throttled", paths.throttled);
    paths.proc_stat = declare_parameter("probe.proc_stat", paths.proc_stat);
    paths.meminfo = declare_parameter("probe.meminfo", paths.meminfo);
    paths.under_voltage_alarm = declare_parameter("probe.under_voltage_alarm", std::string());
    const auto hwmon_name = declare_parameter("probe.under_voltage_hwmon_name", std::string("rpi_volt"));
    if (paths.under_voltage_alarm.empty() && !hwmon_name.empty())
    {
      paths.under_voltage_alarm = find_hwmon_file("/sys/class/hwmon", hwmon_name, "in0_lcrit_alarm");
    }
    RCLCPP_INFO(get_logger(), "under-voltage source: %s",
                paths.under_voltage_alarm.empty() ? "(none)" : paths.under_voltage_alarm.c_str());
    return paths;
  }

  std::vector<WatchedTopic> load_watched_topics()
  {
    const auto names = declare_parameter("watch.topics", std::vector<std::string>{});
    const auto min_hz = declare_parameter("watch.min_hz", std::vector<double>{});
    const auto timeout = declare_parameter("watch.timeout_sec", std::vector<double>{});
    const auto severity = declare_parameter("watch.severity", std::vector<std::string>{});
    if (min_hz.size() != names.size() || timeout.size() != names.size() || severity.size() != names.size())
    {
      throw std::invalid_argument("watch.*arrays must have the same length");
    }

    std::vector<WatchedTopic> topics;
    for (size_t i = 0; i < names.size(); ++i)
    {
      topics.push_back(WatchedTopic{ names[i], min_hz[i], timeout[i], parse_level(severity[i]) });
    }
    return topics;
  }

  void on_fast_tick()
  {
    const double now = steady_seconds();
    const auto result = evaluator_.evaluate(watchdog_->samples(now), system_, now);

    RobotStatus msg;
    msg.header.stamp = this->now();
    msg.level = static_cast<uint8_t>(result.level);
    msg.raw_level = static_cast<uint8_t>(result.raw_level);
    msg.faults.reserve(result.faults.size());
    for (const auto& fault : result.faults)
    {
      msg.faults.push_back(to_msg(fault));
    }
    publisher_->publish(msg);

    if (result.level_changed)
    {
      log_transition(result);
    }
  }

  void on_slow_tick()
  {
    system_ = probe_.sample();
    watchdog_->subscribe_pending();
  }

  void log_transition(const Evaluation& result)
  {
    std::string causes;
    for (const auto& fault : result.faults)
    {
      if (!causes.empty())
      {
        causes += ", ";
      }
      causes += fault.message + " (" + fault.source + ")";
    }
    if (result.level == Level::FAULT)
    {
      RCLCPP_ERROR(get_logger(), "level -> FAULT: %s", causes.c_str());
    }
    else if (result.level == Level::DEGRADED)
    {
      RCLCPP_WARN(get_logger(), "level -> DEGRADED: %s", causes.c_str());
    }
    else
    {
      RCLCPP_INFO(get_logger(), "level -> OK");
    }
  }

  StateEvaluator evaluator_;
  SystemProbe probe_;
  SystemSample system_;
  std::unique_ptr<TopicWatchdog> watchdog_;
  rclcpp::Publisher<RobotStatus>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr fast_timer_;
  rclcpp::TimerBase::SharedPtr slow_timer_;
};
}  // namespace health_monitor

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<health_monitor::HealthMonitorNode>());
  rclcpp::shutdown();
  return 0;
}