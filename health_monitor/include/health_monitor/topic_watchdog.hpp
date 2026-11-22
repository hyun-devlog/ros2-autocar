#pragma once

#include <string>
#include <vector>

#include "health_monitor/rate_tracker.hpp"
#include "health_monitor/types.hpp"
#include "rclcpp/rclcpp.hpp"

namespace health_monitor
{
struct WatchedTopic
{
  std::string name;
  double min_rate_hz{ 0.0 };
  double timeout_sec{ 2.0 };
  Level severity_on_loss{ Level::FAULT };
};

class TopicWatchdog
{
public:
  TopicWatchdog(rclcpp::Node& node, const std::vector<WatchedTopic>& topics, double rate_window_sec);

  TopicWatchdog(const TopicWatchdog&) = delete;
  TopicWatchdog& operator=(const TopicWatchdog&) = delete;

  void subscribe_pending();
  std::vector<TopicSample> samples(double now_sec) const;

private:
  struct Entry
  {
    WatchedTopic config;
    RateTracker tracker;
    rclcpp::GenericSubscription::SharedPtr subscription;
  };

  rclcpp::Node& node_;
  std::vector<Entry> entries_;
};
}  // namespace health_monitor