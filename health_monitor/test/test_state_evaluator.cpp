#include <gtest/gtest.h>

#include <cmath>

#include "health_monitor/state_evaluator.hpp"

using health_monitor::EvaluatorConfig;
using health_monitor::FaultCode;
using health_monitor::Level;
using health_monitor::StateEvaluator;
using health_monitor::SystemSample;
using health_monitor::TopicSample;

namespace
{
    TopicSample healthy_scan()
    {
        TopicSample topic;
        topic.name = "/scan_raw";
        topic.ever_received = true;
        topic.age_sec = 0.1;
        topic.rate_hz = 10.0;
        topic.min_rate_hz = 9.0;
        topic.timeout_sec = 2.0;
        return topic;
    }

    TopicSample stale_scan()
    {
        TopicSample topic = healthy_scan();
        topic.age_sec = 3.0;
        topic.rate_hz = 0.0;
        return topic;
    }
}

TEST(StateEvaluatorSR01, StaleTopicFaultsImmediately)
{
    StateEvaluator evaluator(EvaluatorConfig{});
    evaluator.evaluate({healthy_scan()}, SystemSample{}, 0.0);

    const auto result = evaluator.evaluate({stale_scan()}, SystemSample{}, 6.0);

    EXPECT_EQ(result.level, Level::FAULT);
    EXPECT_EQ(result.raw_level, Level::FAULT);
    EXPECT_TRUE(result.level_changed);
    ASSERT_EQ(result.faults.size(), 1u);
    EXPECT_EQ(result.faults[0].code, FaultCode::TOPIC_STALE);
    EXPECT_EQ(result.faults[0].source, "/scan_raw");
    EXPECT_EQ(result.faults[0].message, "U1002 no message within timeout");
    EXPECT_DOUBLE_EQ(result.faults[0].value, 3.0);
    EXPECT_DOUBLE_EQ(result.faults[0].threshold, 2.0);
}

TEST(StateEvaluator, AllHealthyIsOk)
{
    StateEvaluator evaluator(EvaluatorConfig{});

    const auto result = evaluator.evaluate({healthy_scan()}, SystemSample{}, 10.0);

    EXPECT_EQ(result.level, Level::OK);
    EXPECT_TRUE(result.faults.empty());
    EXPECT_FALSE(result.level_changed);
}

TEST(StateEvaluatorSR10, NeverReceivedWaitsForGrace)
{
    StateEvaluator evaluator(EvaluatorConfig{});
    TopicSample topic = healthy_scan();
    topic.ever_received = false;

    EXPECT_EQ(evaluator.evaluate({topic}, SystemSample{}, 0.0).level, Level::OK);
    EXPECT_EQ(evaluator.evaluate({topic}, SystemSample{}, 4.9).level, Level::OK);

    const auto result = evaluator.evaluate({topic}, SystemSample{}, 5.0);
    EXPECT_EQ(result.level, Level::FAULT);
    ASSERT_EQ(result.faults.size(), 1u);
    EXPECT_EQ(result.faults[0].code, FaultCode::TOPIC_NEVER_RECEIVED);
    EXPECT_TRUE(std::isnan(result.faults[0].value));
}

TEST(StateEvaluatorSR03, LowRateIsDegraded)
{
    StateEvaluator evaluator(EvaluatorConfig{});
    evaluator.evaluate({healthy_scan()}, SystemSample{}, 0.0);
    TopicSample topic = healthy_scan();
    topic.rate_hz = 7.0;

    const auto result = evaluator.evaluate({topic}, SystemSample{}, 6.0);

    EXPECT_EQ(result.level, Level::DEGRADED);
    ASSERT_EQ(result.faults.size(), 1u);
    EXPECT_EQ(result.faults[0].code, FaultCode::TOPIC_RATE_LOW);
}

TEST(StateEvaluatorSR06, RecoveryWaitForHold)
{
    StateEvaluator evaluator(EvaluatorConfig{});
    evaluator.evaluate({healthy_scan()}, SystemSample{}, 0.0);
    evaluator.evaluate({stale_scan()}, SystemSample{}, 10.0);

    auto result = evaluator.evaluate({healthy_scan()}, SystemSample{}, 10.1);
    EXPECT_EQ(result.level, Level::FAULT);
    EXPECT_EQ(result.raw_level, Level::OK);

    result = evaluator.evaluate({healthy_scan()}, SystemSample{}, 12.0);
    EXPECT_EQ(result.level, Level::FAULT);

    result = evaluator.evaluate({healthy_scan()}, SystemSample{}, 12.1);
    EXPECT_EQ(result.level, Level::OK);
    EXPECT_TRUE(result.level_changed);
}

TEST(StateEvaluatorSR06, FlappingRestartHold)
{
    StateEvaluator evaluator(EvaluatorConfig{});
    evaluator.evaluate({healthy_scan()}, SystemSample{}, 0.0);
    evaluator.evaluate({stale_scan()}, SystemSample{}, 10.0);
    evaluator.evaluate({healthy_scan()}, SystemSample{}, 10.1);
    evaluator.evaluate({stale_scan()}, SystemSample{}, 11.0);
    evaluator.evaluate({healthy_scan()}, SystemSample{}, 11.1);

    EXPECT_EQ(evaluator.evaluate({healthy_scan()}, SystemSample{}, 12.5).level, Level::FAULT);
    EXPECT_EQ(evaluator.evaluate({healthy_scan()}, SystemSample{}, 13.1).level, Level::OK);
}

TEST(StateEvaluatorSR07, CpuTemperatureThresholds)
{
    StateEvaluator evaluator(EvaluatorConfig{});
    SystemSample system;

    system.cpu_temp_c = 79.9;
    EXPECT_EQ(evaluator.evaluate({}, system, 0.0).raw_level, Level::OK);

    system.cpu_temp_c = 80.0;
    EXPECT_EQ(evaluator.evaluate({}, system, 0.0).raw_level, Level::DEGRADED);

    system.cpu_temp_c = 85.0;
    const auto result = evaluator.evaluate({}, system, 0.0);
    EXPECT_EQ(result.raw_level, Level::FAULT);
    ASSERT_EQ(result.faults.size(), 1u);
    EXPECT_EQ(result.faults[0].message, "B1002 temperature critical");
}

TEST(StateEvaluatorSR08, PowerFlagsAreDegraded)
{
    StateEvaluator evaluator(EvaluatorConfig{});
    SystemSample system;
    system.under_voltage_now = true;
    system.throttled_now = true;

    const auto result = evaluator.evaluate({}, system, 0.0);

    EXPECT_EQ(result.raw_level, Level::DEGRADED);
    ASSERT_EQ(result.faults.size(), 2u);
    EXPECT_EQ(result.faults[0].code, FaultCode::UNDER_VOLTAGE);
    EXPECT_EQ(result.faults[1].code, FaultCode::THROTTLED);
    EXPECT_TRUE(std::isnan(result.faults[0].value));
}

TEST(StateEvaluator, UnknownSystemValueAreNotFaults)
{
    StateEvaluator evaluator(EvaluatorConfig{});

    const auto result = evaluator.evaluate({}, SystemSample{}, 0.0);

    EXPECT_EQ(result.raw_level, Level::OK);
    EXPECT_TRUE(result.faults.empty());
}

TEST(StateEvaluatorSR11, ResourceChecksOffUntilConfigured)
{
    SystemSample system;
    system.cpu_load_pct = 99.0;
    system.mem_available_pct = 1.0;

    EvaluatorConfig config;
    StateEvaluator disabled(config);
    EXPECT_EQ(disabled.evaluate({}, system, 0.0).raw_level, Level::OK);

    config.cpu_load_high_pct = 90.0;
    config.mem_available_low_pct = 10.0;
    StateEvaluator enabled(config);
    const auto result = enabled.evaluate({}, system, 0.0);
    ASSERT_EQ(result.faults.size(), 2u);
    EXPECT_EQ(result.faults[0].code, FaultCode::CPU_LOAD_HIGH);
    EXPECT_EQ(result.faults[1].code, FaultCode::MEM_LOW);
}