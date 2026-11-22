| 요구사항 | 테스트 | 결과 |
|---|---|---|
| SR-01 | RateTrackerSR01.RecentStopIsCaughtByAgeNotRate, OldStampsLeaveWindow | Pass |
| SR-03 | RateTrackerSR03.LidarTenHz, ImuFiftyHz | Pass |
| SR-10 | RateTrackerSR10.NeverReceived | Pass |

## 측정 기록 : 시스템 소스 (2026-11-22, MentorPi 컨테이너, ubuntu 사용자)
| 소스 | 경로 | 측정값 |
|---|---|---|
| CPU 온도 | /sys/class/thermal/thermal_zone0/temp | 47400 (47.4 ℃) |
| 저전압 | /sys/class/hwmon/hwmon3/in0_lcrit_alarm (name=rpi_volt) | 0 |
| 쓰로틀 비트 | /sys/devices/platform/soc/soc:firmware/get_throttled | 0 |
| 메모리 | /proc/meminfo | MemAvailable 4674080 / MemTotal 8245696 kB (56.7 %) |
| CPU 시간 | /proc/stat | 첫 줄 CPU 필드 10개 | 