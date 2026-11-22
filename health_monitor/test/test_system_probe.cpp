#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "health_monitor/system_probe.hpp"

namespace fs = std::filesystem;

using health_monitor::find_hwmon_file;
using health_monitor::ProbePaths;
using health_monitor::SystemProbe;

class SystemProbeTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    dir_ = fs::temp_directory_path() / ("system_probe_" + std::string(info->name()));
    fs::remove_all(dir_);
    fs::create_directories(dir_);
  }

  void TearDown() override
  {
    fs::remove_all(dir_);
  }

  std::string write(const std::string& name, const std::string& content)
  {
    const auto path = dir_ / name;
    fs::create_directories(path.parent_path());
    std::ofstream(path) << content;
    return path.string();
  }

  ProbePaths missing_paths() const
  {
    ProbePaths paths;
    paths.cpu_temp = (dir_ / "missing").string();
    paths.proc_stat = (dir_ / "missing").string();
    paths.meminfo = (dir_ / "missing").string();
    return paths;
  }

  fs::path dir_;
};

TEST_F(SystemProbeTest, SR07ReadsMillidegrees)
{
  ProbePaths paths = missing_paths();
  paths.cpu_temp = write("temp", "47400\n");
  SystemProbe probe(paths);

  const auto sample = probe.sample();

  ASSERT_TRUE(sample.cpu_temp_c.has_value());
  EXPECT_DOUBLE_EQ(*sample.cpu_temp_c, 47.4);
}

TEST_F(SystemProbeTest, SR08FindsRpiVoltByName)
{
  write("hwmon/hwmon0/name", "cpu_thermal\n");
  write("hwmon/hwmon3/name", "rpi_volt\n");
  const auto alarm = write("hwmon/hwmon3/in0_lcrit_alarm", "1\n");

  const auto found = find_hwmon_file((dir_ / "hwmon").string(), "rpi_volt", "in0_lcrit_alarm");
  EXPECT_EQ(found, alarm);

  ProbePaths paths = missing_paths();
  paths.under_voltage_alarm = found;
  const auto sample = SystemProbe(paths).sample();
  ASSERT_TRUE(sample.under_voltage_now.has_value());
  EXPECT_TRUE(*sample.under_voltage_now);
}

TEST_F(SystemProbeTest, SR08HwmonNotFoundGivesEmptyPath)
{
  write("hwmon/hwmon0/name", "cpu_thermal\n");

  EXPECT_EQ(find_hwmon_file((dir_ / "hwmon").string(), "rpi_volt", "in0_lcrit_alarm"), "");
  EXPECT_EQ(find_hwmon_file((dir_ / "no_such_dir").string(), "rpi_volt", "in0_lcrit_alarm"), "");
}

TEST_F(SystemProbeTest, SR08ThrottledBitsNow)
{
  ProbePaths paths = missing_paths();
  paths.throttled = write("get_throttled", "5005\n");

  const auto sample = SystemProbe(paths).sample();

  ASSERT_TRUE(sample.throttled_now.has_value());
  EXPECT_TRUE(*sample.throttled_now);
  ASSERT_TRUE(sample.under_voltage_now.has_value());
  EXPECT_TRUE(*sample.under_voltage_now);
}

TEST_F(SystemProbeTest, SR08PastOnlyBitsAreNotNow)
{
  ProbePaths paths = missing_paths();
  paths.throttled = write("get_throttled", "throttled = 0x50000\n");

  const auto sample = SystemProbe(paths).sample();

  EXPECT_FALSE(sample.throttled_now.value());
  EXPECT_FALSE(sample.under_voltage_now.value());
}

TEST_F(SystemProbeTest, SR11CpuLoadNeedsTwoReads)
{
  ProbePaths paths = missing_paths();
  paths.proc_stat = write("stat", "cpu    483363 1 169554 5280693 12599 0 3260 0 0 0\n");
  SystemProbe probe(paths);

  EXPECT_FALSE(probe.sample().cpu_load_pct.has_value());

  write("stat", "cpu  483413 1 169604 5280783 12609 0 3260 0 0 0\n");
  const auto sample = probe.sample();
  ASSERT_TRUE(sample.cpu_load_pct.has_value());
  EXPECT_NEAR(*sample.cpu_load_pct, 50.0, 1e-9);
}

TEST_F(SystemProbeTest, SR12MemAvailablePercent)
{
  ProbePaths paths = missing_paths();
  paths.meminfo = write("meminfo",
                        "MemTotal:          8245696 kB\n"
                        "MemFree:           1000000 kB\n"
                        "MemAvailable:      4674080 kB\n");

  const auto sample = SystemProbe(paths).sample();

  ASSERT_TRUE(sample.mem_available_pct.has_value());
  EXPECT_NEAR(*sample.mem_available_pct, 56.69, 0.01);
}

TEST_F(SystemProbeTest, MissingFilesAreUnknown)
{
  const auto sample = SystemProbe(missing_paths()).sample();

  EXPECT_FALSE(sample.cpu_temp_c.has_value());
  EXPECT_FALSE(sample.under_voltage_now.has_value());
  EXPECT_FALSE(sample.throttled_now.has_value());
  EXPECT_FALSE(sample.cpu_load_pct.has_value());
  EXPECT_FALSE(sample.mem_available_pct.has_value());
}