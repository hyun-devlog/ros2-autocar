#include <gtest/gtest.h>

#include "safety_gate/gate_logic.hpp"

using safety_gate::GateConfig;
using safety_gate::GateLogic;
using safety_gate::GateMode;
using safety_gate::StatusLevel;
using safety_gate::Velocity;

namespace
{
Velocity forward(double speed)
{
  Velocity v;
  v.linear_x = speed;
  v.angular_z = speed / 2.0;
  return v;
}

bool is_zero(const Velocity& v)
{
  return v.linear_x == 0.0 && v.linear_y == 0.0 && v.linear_z == 0.0 && v.angular_x == 0.0 && v.angular_y == 0.0 &&
         v.angular_z == 0.0;
}

}  // namespace

TEST(GateLogicSR09, FaultStopsAndRepeatsZeroOnAllChannels)
{
  GateLogic gate(GateConfig{}, 2);
  gate.on_status(StatusLevel::OK, 0.0);
  EXPECT_DOUBLE_EQ(gate.on_command(0, forward(0.3), 0.05).linear_x, 0.3);

  gate.on_status(StatusLevel::FAULT, 0.1);

  EXPECT_EQ(gate.mode(0.1), GateMode::STOP);
  EXPECT_TRUE(is_zero(gate.on_command(0, forward(0.3), 0.15)));
  for (double t = 0.2; t < 0.5; t += 0.1)
  {
    gate.on_status(StatusLevel::FAULT, t);
    const auto out = gate.on_tick(t);
    ASSERT_EQ(out.size(), 2u);
    ASSERT_TRUE(out[0].has_value());
    ASSERT_TRUE(out[1].has_value());
    EXPECT_TRUE(is_zero(*out[0]));
    EXPECT_TRUE(is_zero(*out[1]));
  }
}

TEST(GateLogic, OkPassesCommandUnchanged)
{
  GateLogic gate(GateConfig{}, 2);
  gate.on_status(StatusLevel::OK, 0.0);
  const auto out = gate.on_command(1, forward(0.4), 0.1);
  EXPECT_DOUBLE_EQ(out.linear_x, 0.4);
  EXPECT_DOUBLE_EQ(out.angular_z, 0.2);
  const auto tick = gate.on_tick(0.2);
  EXPECT_FALSE(tick[0].has_value());
  EXPECT_FALSE(tick[1].has_value());
}

TEST(GateLogic, DegradedScalesCommand)
{
  GateLogic gate(GateConfig{}, 1);
  gate.on_status(StatusLevel::DEGRADED, 0.0);
  const auto out = gate.on_command(0, forward(0.4), 0.1);
  EXPECT_DOUBLE_EQ(out.linear_x, 0.2);
  EXPECT_DOUBLE_EQ(out.angular_z, 0.1);
}

TEST(GateLogic, EnteringDegradedResendsScaledLastCommand)
{
  GateLogic gate(GateConfig{}, 2);
  gate.on_status(StatusLevel::OK, 0.0);
  gate.on_tick(0.0);
  gate.on_command(0, forward(0.4), 0.05);

  gate.on_status(StatusLevel::DEGRADED, 0.1);
  auto out = gate.on_tick(0.1);
  ASSERT_TRUE(out[0].has_value());
  EXPECT_DOUBLE_EQ(out[0]->linear_x, 0.2);
  EXPECT_FALSE(out[1].has_value());

  gate.on_status(StatusLevel::DEGRADED, 0.2);
  out = gate.on_tick(0.2);
  EXPECT_FALSE(out[0].has_value());
}

TEST(GateLogicSR04, InputTimeoutSendsZeroOnce)
{
  GateLogic gate(GateConfig{}, 2);
  gate.on_status(StatusLevel::OK, 0.0);
  gate.on_command(0, forward(0.3), 0.0);

  gate.on_status(StatusLevel::OK, 1.0);
  EXPECT_FALSE(gate.on_tick(1.0)[0].has_value());

  gate.on_status(StatusLevel::OK, 1.1);
  auto out = gate.on_tick(1.1);
  ASSERT_TRUE(out[0].has_value());
  EXPECT_TRUE(is_zero(*out[0]));
  EXPECT_FALSE(out[1].has_value());

  gate.on_status(StatusLevel::OK, 1.2);
  EXPECT_FALSE(gate.on_tick(1.2)[0].has_value());
}

TEST(GateLogicSR13, NoStatusMeansStop)
{
  GateLogic gate(GateConfig{}, 1);
  EXPECT_EQ(gate.mode(0.0), GateMode::STOP);
  EXPECT_TRUE(is_zero(gate.on_command(0, forward(0.3), 0.0)));
}

TEST(GateLogicSR13, StaleStatusMeansStop)
{
  GateLogic gate(GateConfig{}, 1);
  gate.on_status(StatusLevel::OK, 0.0);
  EXPECT_EQ(gate.mode(0.5), GateMode::PASS);
  EXPECT_EQ(gate.mode(0.51), GateMode::STOP);
  const auto out = gate.on_tick(0.6);
  ASSERT_TRUE(out[0].has_value());
  EXPECT_TRUE(is_zero(*out[0]));
}

TEST(GateLogic, RecoveryDoesNotResumeOldCommand)
{
  GateLogic gate(GateConfig{}, 1);
  gate.on_status(StatusLevel::OK, 0.0);
  gate.on_command(0, forward(0.3), 0.0);
  gate.on_status(StatusLevel::FAULT, 0.1);
  gate.on_tick(0.1);

  gate.on_status(StatusLevel::OK, 0.2);
  const auto out = gate.on_tick(0.2);
  EXPECT_FALSE(out[0].has_value());
  EXPECT_DOUBLE_EQ(gate.on_command(0, forward(0.2), 0.3).linear_x, 0.2);
}