#include "health_monitor/rate_tracker.hpp"

#include <algorithm>
#include <iterator>
#include <limits>

namespace health_monitor
{
RateTracker::RateTracker(double window_sec) : window_sec_(window_sec)
{
}

void RateTracker::on_message(double t_sec)
{
  ever_received_ = true;
  last_sec_ = t_sec;
  stamps_.push_back(t_sec);
  while (!stamps_.empty() && stamps_.front() < t_sec - window_sec_)
  {
    stamps_.pop_front();
  }
}

bool RateTracker::ever_received() const
{
  return ever_received_;
}

double RateTracker::age(double now_sec) const
{
  if (!ever_received_)
  {
    return std::numeric_limits<double>::infinity();
  }
  return now_sec - last_sec_;
}

double RateTracker::rate(double now_sec) const
{
  const auto first = std::lower_bound(stamps_.begin(), stamps_.end(), now_sec - window_sec_);
  const auto count = std::distance(first, stamps_.end());
  if (count < 2)
  {
    return 0.0;
  }
  const double span = stamps_.back() - *first;
  if (span <= 0.0)
  {
    return 0.0;
  }
  return static_cast<double>(count - 1) / span;
}
}  // namespace health_monitor