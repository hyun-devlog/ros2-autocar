#include <gtest/gtest.h>

#include <cmath>

#include "health_monitor/rate_tracker.hpp"

using health_monitor::RateTracker;

namespace
{

    void feed(RateTracker & tracker, double start_sec, double hz, int count)
    {
        for (int i = 0; i < count; i++) {
            tracker.on_message(start_sec + i / hz);
        }
    }
}

TEST(RateTrackerSR10, NeverReceived)
{
    RateTracker tracker(1.0);

    EXPECT_FALSE(tracker.ever_received());
    EXPECT_TRUE(std::isinf(tracker.age(10.0)));
    EXPECT_DOUBLE_EQ(tracker.rate(10.0), 0.0);
}

TEST(RateTrackerSR03, LidarTenHz)
{
    RateTracker tracker(1.0);
    feed(tracker, 10.0, 10.0, 21);

    EXPECT_TRUE(tracker.ever_received());
    EXPECT_NEAR(tracker.age(12.05), 0.05, 1e-9);
    EXPECT_NEAR(tracker.rate(12.05), 10.0, 0.01);
}

TEST(RateTrackerSR03, ImuFiftyHz)
{
    RateTracker tracker(1.0);
    feed(tracker, 0.0, 50.0, 101);

    EXPECT_NEAR(tracker.rate(2.0), 50.0, 0.05);
}

TEST(RateTracker, SingleMessageHasZeroRate)
{
    RateTracker tracker(1.0);
    tracker.on_message(5.0);

    EXPECT_TRUE(tracker.ever_received());
    EXPECT_DOUBLE_EQ(tracker.rate(5.1), 0.0);
    EXPECT_NEAR(tracker.age(5.1), 0.1, 1e-9);
}

TEST(RateTrackerSR01, RecentStopIsCaughtByAgeNotRate)
{
    RateTracker tracker(1.0);
    feed(tracker, 0.0, 10.0, 21);

    EXPECT_DOUBLE_EQ(tracker.rate(3.5), 0.0);
    EXPECT_NEAR(tracker.age(3.5), 1.5, 1e-9);
}