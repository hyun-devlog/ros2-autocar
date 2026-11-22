#include "health_monitor/topic_watchdog.hpp"

#include <memory>

namespace health_monitor
{
TopicWatchdog::TopicWatchdog(rclcpp::Node& node, const std::vector<WatchedTopic>& topics, double rate_window_sec)
  : node_(node)
{
  entries_.reserve(topics.size());
  for (const auto& topic : topics)
  {
    entries_.push_back(Entry{ topic, RateTracker(rate_window_sec), nullptr });
  }
  subscribe_pending();
}

void TopicWatchdog::subscribe_pending()
{
  const auto graph = node_.get_topic_names_and_types();
  for (size_t i = 0; i < entries_.size(); ++i)
  {
    auto& entry = entries_[i];
    if (entry.subscription)
    {
      continue;
    }
    const auto found = graph.find(entry.config.name);
    if (found == graph.end() || found->second.empty())
    {
      continue;
    }
    const auto& type = found->second.front();
    entry.subscription = node_.create_generic_subscription(
        entry.config.name, type, rclcpp::SensorDataQoS(),
        [this, i](std::shared_ptr<rclcpp::SerializedMessage>) { entries_[i].tracker.on_message(steady_seconds()); });
    RCLCPP_INFO(node_.get_logger(), "watching %s [%s]", entry.config.name.c_str(), type.c_str());
  }
}

std::vector<TopicSample> TopicWatchdog::samples(double now_sec) const
{
  std::vector<TopicSample> result;
  result.reserve(entries_.size());
  for (const auto& entry : entries_)
  {
    TopicSample sample;
    sample.name = entry.config.name;
    sample.ever_received = entry.tracker.ever_received();
    sample.age_sec = entry.tracker.age(now_sec);
    sample.rate_hz = entry.tracker.rate(now_sec);
    sample.min_rate_hz = entry.config.min_rate_hz;
    sample.timeout_sec = entry.config.timeout_sec;
    sample.severity_on_loss = entry.config.severity_on_loss;
    result.push_back(sample);
  }
  return result;
}
}  // namespace health_monitor