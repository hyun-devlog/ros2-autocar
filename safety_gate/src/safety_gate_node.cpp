#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "geometry_msgs/msg/twist.hpp"
#include "rclcpp/rclcpp.hpp"
#include "robot_interfaces/msg/robot_status.hpp"
#include "safety_gate/gate_logic.hpp"

namespace safety_gate
{
using geometry_msgs::msg::Twist;
using robot_interfaces::msg::RobotStatus;

static_assert(RobotStatus::LEVEL_OK == static_cast<uint8_t>(StatusLevel::OK), "level mismatch");
static_assert(RobotStatus::LEVEL_DEGRADED == static_cast<uint8_t>(StatusLevel::DEGRADED), "level mismatch");
static_assert(RobotStatus::LEVEL_FAULT == static_cast<uint8_t>(StatusLevel::FAULT), "level_mismatch");

namespace
{
double steady_seconds()
{
  using Clock = std::chrono::steady_clock;
  return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

Velocity from_msg(const Twist& msg)
{
  return Velocity{ msg.linear.x, msg.linear.y, msg.linear.z, msg.angular.x, msg.angular.y, msg.angular.z };
}

Twist to_msg(const Velocity& v)
{
  Twist msg;
  msg.linear.x = v.linear_x;
  msg.linear.y = v.linear_y;
  msg.linear.z = v.linear_z;
  msg.angular.x = v.angular_x;
  msg.angular.y = v.angular_y;
  msg.angular.z = v.angular_z;
  return msg;
}
}  // namespace

class SafetyGateNode : public rclcpp::Node
{
public:
  explicit SafetyGateNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions())
    : Node("safety_gate", options)
    , inputs_(declare_parameter("inputs", std::vector<std::string>{ "/cmd_vel", "/controller/cmd_vel" }))
    , outputs_(declare_parameter("outputs",
                                 std::vector<std::string>{ "/safety_gate/cmd_vel", "/safety_gate/controller/cmd_vel" }))
    , gate_(load_config(), inputs_.size())
  {
    if (inputs_.empty() || outputs_.size() != inputs_.size())
    {
      throw std::invalid_argument("inputs and outputs must be non-empty and the same length");
    }

    publishers_.reserve(outputs_.size());
    for (const auto& topic : outputs_)
    {
      publishers_.push_back(create_publisher<Twist>(topic, rclcpp::QoS(10).reliable()));
    }

    subscriptions_.reserve(inputs_.size());
    for (size_t i = 0; i < inputs_.size(); ++i)
    {
      subscriptions_.push_back(
          create_subscription<Twist>(inputs_[i], rclcpp::QoS(10).best_effort(), [this, i](Twist::ConstSharedPtr msg) {
            const auto out = gate_.on_command(i, from_msg(*msg), steady_seconds());
            publishers_[i]->publish(to_msg(out));
          }));
      RCLCPP_INFO(get_logger(), "gating %s -> %s", inputs_[i].c_str(), outputs_[i].c_str());
    }

    status_sub_ = create_subscription<RobotStatus>(
        "robot_status", rclcpp::QoS(1).reliable().transient_local(), [this](RobotStatus::ConstSharedPtr msg) {
          gate_.on_status(static_cast<StatusLevel>(msg->level), steady_seconds());
        });

    const double tick_hz = declare_parameter("tick_hz", 10.0);
    timer_ = create_wall_timer(std::chrono::milliseconds(static_cast<int>(1000.0 / tick_hz)), [this]() { on_tick(); });

    shutdown_handle_ =
        get_node_base_interface()->get_context()->add_pre_shutdown_callback([this]() { publish_zero_all(); });
  }

private:
  GateConfig load_config()
  {
    GateConfig config;
    config.input_timeout_sec = declare_parameter("input_timeout_sec", config.input_timeout_sec);
    config.status_timeout_sec = declare_parameter("status_timeout_sec", config.status_timeout_sec);
    config.degraded_scale = declare_parameter("degraded_scale", config.degraded_scale);
    if (config.degraded_scale < 0.0 || config.degraded_scale > 1.0)
    {
      throw std::invalid_argument("degraded_scale must be between 0 and 1");
    }
    return config;
  }

  void on_tick()
  {
    const double now = steady_seconds();
    const auto out = gate_.on_tick(now);
    for (size_t i = 0; i < out.size(); ++i)
    {
      if (out[i])
      {
        publishers_[i]->publish(to_msg(*out[i]));
      }
    }

    const GateMode mode = gate_.mode(now);
    if (mode != last_mode_)
    {
      if (mode == GateMode::PASS)
      {
        RCLCPP_INFO(get_logger(), "gate -> PASS");
      }
      else
      {
        RCLCPP_WARN(get_logger(), "gate -> %s", to_string(mode));
      }
      last_mode_ = mode;
    }
  }

  void publish_zero_all()
  {
    for (const auto& publisher : publishers_)
    {
      publisher->publish(Twist{});
    }
    RCLCPP_INFO(get_logger(), "shutdown: sent zero velocity");
  }

  std::vector<std::string> inputs_;
  std::vector<std::string> outputs_;
  GateLogic gate_;
  GateMode last_mode_{ GateMode::STOP };
  std::vector<rclcpp::Publisher<Twist>::SharedPtr> publishers_;
  std::vector<rclcpp::Subscription<Twist>::SharedPtr> subscriptions_;
  rclcpp::Subscription<RobotStatus>::SharedPtr status_sub_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::PreShutdownCallbackHandle shutdown_handle_;
};
}  // namespace safety_gate

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<safety_gate::SafetyGateNode>());
  rclcpp::shutdown();
  return 0;
}