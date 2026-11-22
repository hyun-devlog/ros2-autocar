#include "safety_gate/gate_logic.hpp"

namespace safety_gate
{
namespace
{
Velocity scaled(const Velocity& v, double scale)
{
  return Velocity{ v.linear_x * scale,  v.linear_y * scale,  v.linear_z * scale,
                   v.angular_x * scale, v.angular_y * scale, v.angular_z * scale };
}
}  // namespace

GateLogic::GateLogic(const GateConfig& config, size_t channel_count) : config_(config), channels_(channel_count)
{
}

void GateLogic::on_status(StatusLevel level, double now_sec)
{
  level_ = level;
  last_status_sec_ = now_sec;
}

GateMode GateLogic::mode(double now_sec) const
{
  if (!level_ || now_sec - last_status_sec_ > config_.status_timeout_sec)
  {
    return GateMode::STOP;
  }

  switch (*level_)
  {
    case StatusLevel::OK:
      return GateMode::PASS;
    case StatusLevel::DEGRADED:
      return GateMode::SLOW;
    case StatusLevel::FAULT:
      return GateMode::STOP;
  }
  return GateMode::STOP;
}

Velocity GateLogic::on_command(size_t channel, const Velocity & command, double now_sec)
{
  auto& ch = channels_.at(channel);
  ch.active = true;
  ch.last_input_sec = now_sec;
  ch.last_command = command;
  switch (mode(now_sec))
  {
    case GateMode::PASS:
      return command;
    case GateMode::SLOW:
      return scaled(command, config_.degraded_scale);
    case GateMode::STOP:
      return Velocity{};
  }
  return Velocity{};
}

std::vector<std::optional<Velocity>> GateLogic::on_tick(double now_sec)
{
  const GateMode current = mode(now_sec);
  const bool entered_slow = current == GateMode::SLOW && last_tick_mode_ == GateMode::PASS;
  last_tick_mode_ = current;

  std::vector<std::optional<Velocity>> out(channels_.size());
  for (size_t i = 0; i < channels_.size(); ++i)
  {
    auto& ch = channels_[i];
    if (current == GateMode::STOP)
    {
      out[i] = Velocity{};
    }
    else if (ch.active && now_sec - ch.last_input_sec > config_.input_timeout_sec)
    {
      out[i] = Velocity{};
      ch.active = false;
    }
    else if (entered_slow && ch.active)
    {
      out[i] = scaled(ch.last_command, config_.degraded_scale);
    }
  }
  return out;
}

const char* to_string(GateMode mode)
{
  switch (mode)
  {
    case GateMode::PASS:
      return "PASS";
    case GateMode::SLOW:
      return "SLOW";
    case GateMode::STOP:
      return "STOP";
  }
  return "UNKNOWN";
}
}  // namespace safety_gate