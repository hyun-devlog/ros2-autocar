#pragma once

#include <chrono>
#include <deque>

namespace health_monitor
{

class RateTracker
{
public:
  explicit RateTracker(double window_set);

  void on_message(double t_sec);
  bool ever_received() const;
  double age(double now_sec) const;
  double rate(double now_sec) const;

private:
  double window_sec_;
  bool ever_received_{ false };
  double last_sec_{ 0.0 };
  std::deque<double> stamps_;
};

inline double steady_seconds()
{
  using Clock = std::chrono::steady_clock;
  return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}
}  // namespace health_monitor