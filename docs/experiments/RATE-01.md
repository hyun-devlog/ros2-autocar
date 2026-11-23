# RATE-01 라이다 rate 오탐 원인과 조치

- 일시: 2026-11-22
- 문제: 정상 라이다에서 U1003 DEGRADED 반복
  - [E-01](E-01.md) 관찰 3: 시작 17 s 뒤 1회
  - [GATE-02](GATE-02.md) 관찰 3: 약 2분 동안 3회, 매번 safety_gate가 약 2.9 s 감속
- 관련 요구사항: SR-03

## 1. 측정

`ros2 topic hz /scan_raw --window 100`, 라이다만 켠 상태로 약 60 s

| 항목 | 값 |
|---|---|
| 평균 주기 | 10.000 Hz |
| 평소 간격 | 0.095~0.105 s, 표준편차 약 0.001 s |
| 가끔 나오는 최대 간격 | 0.200 s (스캔 1개 누락) |

처음 가설은 "주기 흔들림"이었으나, 표준편차 약 1 ms로 흔들림은 원인이 아니었다.

## 2. 원인

`RateTracker`의 rate = 간격 수 ÷ 윈도우 안 첫·마지막 스캔 사이 시간.

| 윈도우 | 정상 | 스캔 1개 누락 | 기준 9.0 Hz |
|---|---|---|---|
| 1.0 s | 9 ÷ 0.9 s = 10.0 Hz | 8 ÷ 0.9 s = **8.89 Hz** | 아래 → U1003 |
| 2.0 s | 19 ÷ 1.9 s = 10.0 Hz | 18 ÷ 1.9 s = **9.47 Hz** | 위 → 정상 |

1.0 s 윈도우에서는 스캔 하나만 빠져도 기준 아래로 떨어진다.

## 3. 조치

- `rate_window_sec` 1.0 → **2.0**. 2 s 안에 2개 이상 누락되면 8.95 Hz로 계속 감지한다
- `min_hz` 9.0 (정상의 90 %)은 유지. 다른 토픽과 같은 규칙을 지키기 위해 기준이 아니라 윈도우를 바꿈
- 회귀 테스트 추가: `RateTrackerSR03.OneDroppedScanStaysAboveMinWithTwoSecondWindow`
  (1.0 s 윈도우는 기준 아래, 2.0 s 윈도우는 기준 이상임을 함께 확인)
- 영향: 라이다가 완전히 멈췄을 때 rate 저하 감지가 약 0.9 s → 약 2.0 s로 늦어져, E-01·GATE-02의 "SLOW 후 STOP" 단계가 거의 사라진다.
  정지는 age timeout(2.0 s, SR-01)이 담당하므로 감지 시간은 변하지 않는다

## 4. 검증

- 조건: controller·라이다 켜짐, 명령 없음, health_monitor 세 토픽 감시, `rate_window_sec 2.0`
- 녹화: `experiments_raw/RATE-01_20261122_143456/bag` (git 제외), 523.9 s
- 분석: `python3 tools/bag_timeline.py <bag> --gap 0.15`

| 항목 | 결과 |
|---|---|
| 스캔 누락 (간격 > 0.15 s) | **15회**, 모두 0.20 s (스캔 1개) |
| 누락 비율 | 약 0.29 % (15 ÷ 약 5,239) |
| 평균 / 최소 누락 간격 | 약 35 s / 8.4 s. 2 s 안에 2개가 겹친 적 없음 |
| `/robot_status` | 523.9 s 동안 level 0, raw_level 0, faults 없음 |
| 로그 U1003 | 0회 |

- 결론: 누락은 그대로 발생했지만 오탐은 없었다
- 이전 설정이었다면: 누락 15회 × 약 2.9 s ≈ 43 s (전체의 약 8 %) 감속했을 것으로 추정

## 5. 부수 관찰

- controller를 끈 상태로 health_monitor를 시작하면, IMU·odom을 구독하지 못한 채 5 s 유예 뒤
  U1001 FAULT가 된다 (`experiments_raw/RATE-01/health_monitor_no_controller.log`). SR-10 동작 확인
