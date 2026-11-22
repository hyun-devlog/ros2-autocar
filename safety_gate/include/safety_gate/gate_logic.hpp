#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace safety_gate
{

enum class StatusLevel : uint8_t
{
  OK = 0,
  DEGRADED = 1,
  FAULT = 2
};
enum class GateMode : uint8_t
{
  PASS = 0,
  SLOW = 1,
  STOP = 2
};

struct Velocity
{
  double linear_x{ 0.0 };
  double linear_y{ 0.0 };
  double linear_z{ 0.0 };
  double angular_x{ 0.0 };
  double angular_y{ 0.0 };
  double angular_z{ 0.0 };
};

struct GateConfig
{
  double input_timeout_sec{ 1.0 };
  double status_timeout_sec{ 0.5 };
  double degraded_scale{ 0.5 };
};

class GateLogic
{
public:
  GateLogic(const GateConfig& config, size_t channel_count);

  void on_status(StatusLevel level, double now_sec);
  Velocity on_command(size_t channel, const Velocity& command, double now_sec);
  std::vector<std::optional<Velocity>> on_tick(double now_sec);
  GateMode mode(double now_sec) const;

private:
  struct Channel
  {
    bool active{ false };
    double last_input_sec{ 0.0 };
    Velocity last_command;
  };

  GateConfig config_;
  std::vector<Channel> channels_;
  std::optional<StatusLevel> level_;
  double last_status_sec_{ 0.0 };
  GateMode last_tick_mode_{ GateMode::STOP };
};

const char* to_string(GateMode mode);
}  // namespace safety_gate