# 안전 분석 및 안전 요구사항 (Safety Analysis & Requirements)

> 이 문서는 ISO 26262 / Automotive SPICE / ISO 13482를 **준수하거나 인증받은 것이 아닙니다.**
> 교육에서 배운 기능안전 방법론(위험 분석 → 안전 요구사항 → 검증 추적)을
> 개인 프로젝트 규모에 맞게 가볍게 적용한 기록입니다.

## 1. 범위

| 항목 | 내용 |
|---|---|
| 대상 | Hiwonder MentorPi A1 (Raspberry Pi 5 + RRC Lite, Ackermann 섀시) |
| 대상 SW | ros2-robot-supervisor: health_monitor, safety_gate |
| 제외 | Hiwonder 벤더 코드, RRC Lite 펌웨어, SLAM/Nav2 알고리즘 |
| 안전 상태 (Safe State) | **모든 모터 입구에 0 속도 명령이 들어가 정지한 상태** |

## 2. 적용한 개발 흐름 (V-Model)

| V-Model 왼쪽 (설계) | 산출물 | V-Model 오른쪽 (검증) | 산출물 |
|---|---|---|---|
| 안전 요구사항 | 이 문서의 SR 목록 | 시스템 검증 | 장애 주입 결과표 (감지·정지 시간 실측) |
| 아키텍처 | README 구조도, `robot_interfaces` | 통합 테스트 | 실로봇 장애 주입 실험 |
| 상세 설계 | 판정 규칙, `docs/fault_codes.md` | 단위 테스트 | GoogleTest (`colcon test`) |
| 구현 | C++ / rclcpp 코드 | | |

## 3. 위험 분석 (Hazard Analysis)

ASIL 등급은 매기지 않았습니다. ASIL은 심각도·노출·통제 가능성을 실제 운용 근거로 평가해야 하는데,
교육용 로봇 1대로는 근거 있는 값을 낼 수 없기 때문입니다.

| ID | 위험 | 발견 경위 (실측·실험) | 관련 SR |
|---|---|---|---|
| H-01 | 마지막 속도 명령이 유지되어 로봇이 계속 주행 | `ros2 topic pub --once /cmd_vel` 1회 발행 후 바퀴가 계속 회전 (명령 타임아웃 없음 확인) | SR-04, SR-09 |
| H-02 | 라이다 데이터 없이 주행 (장애물 인지 불가) | 라이다 토픽명이 `/scan`이 아닌 `/scan_raw`였음. 잘못된 설정 시 감시 누락 가능 | SR-01, SR-03 |
| H-03 | RRC Lite 시리얼 통신 단절 → 구동 제어 불능 | 터미널 종료 후 잔존 프로세스로 `multiple access on port` 오류 발생 | SR-02 |
| H-04 | 안전 계층 우회 | `odom_publisher`가 `/cmd_vel`, `/controller/cmd_vel` 두 입구를 구독 | SR-05 |
| H-05 | 정지·출발 반복 (센서 간헐 단절) | 설계 검토 | SR-06 |
| H-06 | 과열로 인한 처리 지연 → 반응 지연 | 설계 검토 (Pi 5 스로틀링) | SR-07 |
| H-07 | 전원 불안정 → 동작 불안정 | 설계 검토 (모터 부하 시 전압 강하) | SR-08 |
| H-08 | CPU·메모리 자원 고갈 → 노드 주기 저하 또는 강제 종료 | 설계 검토 | SR-11, SR-12 |
| H-09 | 감시자(health_monitor) 자체가 멈춤 → 고장을 알릴 수 없음 | 설계 검토. GATE-02 ⑤에서 health_monitor 종료 시험 | SR-13 |
| H-10 | 정상 라이다의 스캔 누락을 성능 저하로 오판 → 불필요한 감속 | GATE-02: 2분에 3회 감속. RATE-01: 약 35 s마다 스캔 1개 누락 실측 | SR-03 |
| H-11 | safety_gate 비정상 종료(크래시, kill -9) → 0 속도를 보내지 못하고 벤더가 마지막 명령 유지 | 설계 검토 | 잔여 위험 (8절) |

## 4. 안전 요구사항 (Safety Requirements)

"근거" 열의 출처 링크는 9절에 모아 두었습니다. 공식 근거가 없는 값은 **설계값**으로 명시합니다.

| ID | 요구사항 | 기준값 | 근거 | 담당 | 검증 방법 |
|---|---|---|---|---|---|
| SR-01 | `/scan_raw`가 기준 시간 이상 수신되지 않으면 FAULT(U1002)로 판정한다 | 2.0 s | [S5] Nav2 `source_timeout` 기본값 | health_monitor | 단위 테스트, 장애 주입(라이다 노드 종료) |
| SR-02 | `/ros_robot_controller/imu_raw`가 기준 시간 이상 수신되지 않으면 FAULT(U1002)로 판정한다 | 2.0 s | [S5] | health_monitor | 단위 테스트, 장애 주입(`ros_robot_controller` 종료) |
| SR-03 | 감시 토픽 주기가 기준 주기 × 0.9 미만이면 DEGRADED(U1003)로 판정한다. 주기는 최근 2.0 s 윈도우로 계산한다 | 라이다 9.0 Hz, IMU·odom 44 Hz, 윈도우 2.0 s | [S6] diagnostic_updater 허용오차 0.1, [S7] LD19 10 Hz, IMU·odom은 실측 49 Hz. 윈도우는 **실측 기반 설계값** (RATE-01: 스캔 1개 누락 허용) | health_monitor | 단위 테스트, 장시간 실측 |
| SR-04 | safety_gate는 입력 명령이 기준 시간 이상 없으면 0 속도를 발행한다 | 1.0 s | [S8] Nav2 `velocity_timeout` 기본값 | safety_gate | 단위 테스트, 장애 주입(teleop 종료) |
| SR-05 | `/cmd_vel`, `/controller/cmd_vel` 두 입구 모두 safety_gate를 거친다 | — | H-04 실측 | bringup launch | `ros2 topic info -v`로 구독자 확인 |
| SR-06 | 등급 하향(복구)은 더 나은 상태가 기준 시간 유지된 뒤에만 반영한다 | 2.0 s | **설계값** (공식 근거 없음) | health_monitor | 단위 테스트 |
| SR-07 | SoC 온도 80 °C 이상이면 DEGRADED(B1001), 85 °C 이상이면 FAULT(B1002) | 80 / 85 °C | [S3][S4] Raspberry Pi 공식 문서 | health_monitor | 단위 테스트(가짜 sysfs) |
| SR-08 | 펌웨어 저전압 비트가 켜지면 DEGRADED(P1001), 스로틀링 비트가 켜지면 DEGRADED(B1101) | bit 0 / bit 2 | [S9] `get_throttled` 정의. 등급은 **설계값** | health_monitor | 단위 테스트(가짜 sysfs) |
| SR-09 | FAULT 동안 safety_gate는 0 속도를 주기적으로 발행한다 | 10 Hz | H-01 실측 (벤더가 마지막 명령 유지) | safety_gate | 장애 주입 |
| SR-10 | 시작 후 한 번도 수신되지 않은 토픽은 유예 시간 이후 FAULT(U1001)로 판정한다 | 5.0 s | **설계값** (bringup 소요 시간 실측 후 조정) | health_monitor | 단위 테스트 |
| SR-11 | CPU 사용률이 기준 이상이면 DEGRADED(B1201)로 판정한다 | **실측 후 결정** (그 전까지 비활성) | 측정 방법: [S10] `/proc/stat`. 기준값: **실측 기반 설계값** | health_monitor | 단위 테스트(가짜 `/proc/stat`), 부하 실측 |
| SR-12 | 가용 메모리(MemAvailable) 비율이 기준 이하이면 DEGRADED(B1202)로 판정한다 | **실측 후 결정** (그 전까지 비활성) | 측정 방법: [S11] `/proc/meminfo`. 기준값: **실측 기반 설계값** | health_monitor | 단위 테스트(가짜 `/proc/meminfo`), 부하 실측 |
| SR-13 | safety_gate는 `/robot_status`를 한 번도 받지 못했거나 기준 시간 이상 받지 못하면 0 속도를 발행한다 | 0.5 s | **설계값** (health_monitor 10 Hz 발행의 5회 누락) | safety_gate | 단위 테스트, 장애 주입(health_monitor 종료) |
| SR-14 | DEGRADED 동안 safety_gate는 모든 속도 성분에 같은 배율을 곱해 감속한다 | 0.5 | **설계값** (감속 비율을 정한 공식 규격을 찾지 못함) | safety_gate | 단위 테스트 |

### SR-11, SR-12 기준값 정하는 절차

1. bringup + 라이다 + 무거운 노드(SLAM 등)를 띄운 상태에서 10~20분 동안 CPU 사용률, MemAvailable 비율, 토픽 주기를 함께 기록한다.
2. 토픽 주기가 기준 × 0.9 아래로 떨어지기 시작하는 CPU 사용률·가용 메모리 지점을 찾는다.
3. 그 지점보다 여유를 둔 값을 기준으로 정하고, 측정 날짜·조건·결과를 이 표에 기록한다.

## 5. 추적성 (Traceability)

단위 테스트는 테스트 묶음 이름에 SR 번호를 넣었습니다 (예: `StateEvaluatorSR01.*`). 실험 기록은 `docs/experiments/`에 있습니다.
GATE-02는 모터를 연결하지 않은 상태의 예비 확인이며, 모터 연결 후 본 실험은 E-01~E-05입니다.

| SR | 코드 위치 | 단위 테스트 | 실험 | 결과 |
|---|---|---|---|---|
| SR-01 | `state_evaluator.cpp`, `rate_tracker.cpp` | `StateEvaluatorSR01.*`, `RateTrackerSR01.*` | E-01 라이다 종료 | 모터 연결: 마지막 스캔 +2.017 s에 U1002, +2.069 s에 정지 명령 (E-MAIN) |
| SR-02 | `state_evaluator.cpp` | `StateEvaluatorSR01.*` (같은 규칙) | E-02 RRC 노드 종료 | 마지막 IMU +2.013 s에 U1002 (E-02). `/odom_raw`는 단절 후에도 계속 발행되어 감시 신호로 부적합 |
| SR-03 | `state_evaluator.cpp`, `rate_tracker.cpp` | `StateEvaluatorSR03.*`, `RateTrackerSR03.*` | RATE-01 장시간 실측 | 스캔 누락 15회 / 523.9 s 동안 오탐 0회 |
| SR-04 | `gate_logic.cpp` | `GateLogicSR04.*` | E-03 명령 소스 종료 | 모터 연결: 6회 평균 1.040 s에 정지, 바퀴 정지 확인 (BRINGUP-01) |
| SR-05 | `supervisor_bringup/launch/bringup.launch.py` | | E-04 구독자 확인 | 1차에서 우회 경로 발견 → 수정 후 두 입구 모두 safety_gate만 구독 (BRINGUP-01) |
| SR-06 | `state_evaluator.cpp` | `StateEvaluatorSR06.*` | E-01 | 예비: 스캔 재개 +2.56 s에 OK (E-01), gate 0.1 복귀 +2.28~2.34 s (GATE-02) |
| SR-07 | `state_evaluator.cpp`, `system_probe.cpp` | `StateEvaluatorSR07.*`, `SystemProbeTest.SR07*` | | |
| SR-08 | `state_evaluator.cpp`, `system_probe.cpp` | `StateEvaluatorSR08.*`, `SystemProbeTest.SR08*` | | |
| SR-09 | `gate_logic.cpp` | `GateLogicSR09.*` | E-01~E-03 공통 | 예비: STOP 중 두 채널 모두 0 반복 (GATE-02) |
| SR-10 | `state_evaluator.cpp`, `topic_watchdog.cpp` | `StateEvaluatorSR10.*`, `RateTrackerSR10.*` | | 실로봇: controller 없이 시작 시 5 s 뒤 U1001 (RATE-01 5절) |
| SR-11 | `state_evaluator.cpp`, `system_probe.cpp` | `StateEvaluatorSR11.*`, `SystemProbeTest.SR11*` | 부하 실측 | |
| SR-12 | `state_evaluator.cpp`, `system_probe.cpp` | `StateEvaluatorSR11.*`, `SystemProbeTest.SR12*` | 부하 실측 | |
| SR-13 | `gate_logic.cpp` | `GateLogicSR13.*` | E-05 health_monitor 종료 | 모터 연결: 마지막 상태 +0.552 s에 정지 명령 (E-MAIN) |
| SR-14 | `gate_logic.cpp` | `GateLogic.DegradedScalesCommand`, `GateLogic.EnteringDegradedResendsScaledLastCommand` | | 예비: DEGRADED 중 0.1 → 0.05 (GATE-02) |

## 6. 감지·대응 시간 (FTTI 개념 차용)

ISO 26262의 "고장 감지 + 대응 시간" 개념을 빌려, 장애 주입 실험에서 아래 두 값을 실측합니다.

"장애 발생 시각"은 마지막 정상 메시지의 수신 시각입니다 (Unix s).

| 실험 | 장애 발생 시각 | FAULT 판정 시각 | 0 속도 발행 시각 | 감지 시간 | 대응 시간 |
|---|---|---|---|---|---|
| E-01 예비 (gate 없음) | 1795341827.218 | 1795341829.282 | — | 2.06 s | — |
| GATE-02 ② (모터 없음) | 1795356946.733 | (로그 없음) | 1795356948.840 | — | 2.11 s (장애 → 0 출력) |
| E-01 라이다 종료 (모터 연결) | 1795436518.047 | 1795436520.064 | 1795436520.116 | 2.017 s | 2.069 s |
| E-02 RRC 노드 종료 | 1795437059.967 | 1795437061.980 | 1795437062.002 | 2.013 s | 2.035 s (바퀴는 노드 종료 직후 정지) |
| E-03 명령 소스 종료 (6회 평균) | — | — | — | — | 1.040 s (1.005~1.074) |
| E-05 health_monitor 종료 | 1795436548.963 | — (gate 자체 판단) | 1795436549.515 | — | 0.552 s |
| E-06 safety_gate 크래시 | 1795437680.883 | 감지 안 됨 | 발행 안 됨 | — | **정지 실패** (수동 개입까지 21.2 s 회전) |
| E-02b ros_robot_controller 크래시 | (기록 손실) | (기록 손실) | 전달 경로 없음 | — | **정지 실패** (전원 차단) |

## 7. 측정 기록: 시스템 소스 (2026-11-22, MentorPi 컨테이너, ubuntu 사용자)

| 소스 | 경로 | 측정값 |
|---|---|---|
| CPU 온도 | /sys/class/thermal/thermal_zone0/temp | 47400 (47.4 °C) |
| 저전압 | /sys/class/hwmon/hwmon3/in0_lcrit_alarm (name=rpi_volt) | 0 |
| 쓰로틀 비트 | /sys/devices/platform/soc/soc:firmware/get_throttled | 0 |
| 메모리 | /proc/meminfo | MemAvailable 4674080 / MemTotal 8245696 kB (56.7 %) |
| CPU 시간 | /proc/stat | 첫 줄 cpu 필드 10개 |

hwmon 번호(`hwmon3`)는 부팅마다 바뀔 수 있어, 노드는 이름(`rpi_volt`)으로 찾습니다.

## 8. 잔여 위험 (Residual Risk)

소프트웨어만으로 없앨 수 없어 남아 있는 위험입니다. 숨기지 않고 기록하고, 실험으로 실제 동작을 확인합니다.

| ID | 위험 | 왜 남는가 | 확인 계획 |
|---|---|---|---|
| RR-01 | safety_gate가 크래시하면 0 속도를 보내지 못하고, 벤더 `odom_publisher`가 마지막 명령을 유지 (H-11) | 정상 종료 시에만 pre-shutdown 훅으로 0을 보냄. 모터 경로에 gate 외 다른 차단 수단이 없음 | **확인됨 (E-06)**: SIGKILL 후 바퀴가 21.2 s 동안 회전, 감시자는 계속 OK. 하드웨어 비상 정지 또는 펌웨어 명령 timeout 필요 |
| RR-04 | `ros_robot_controller`가 크래시하면 정지 명령을 보드에 전달할 수 없고, RRC Lite 펌웨어는 마지막 모터 명령을 유지 | 펌웨어에 명령 timeout이 없음. 소프트웨어가 감지해도 대응 경로가 없음 | **확인됨 (E-02b)**: SIGKILL 후 바퀴 계속 회전, 전원 스위치로만 정지. SIGINT(E-02)에서는 노드가 종료하며 정지 명령을 보내 즉시 정지 |
| RR-03 | 감시자를 재시작하면 기동 유예(5 s) 동안 데이터 확인 없이 OK를 발행 | SR-10 설계상 "아직 안 온 토픽"은 유예 중 고장이 아님 | E-05 관찰 3. 개선 후보: 유예 중 OK 대신 DEGRADED 발행 |
| RR-02 | health_monitor와 safety_gate가 같은 컴퓨터(Pi 5)에서 돌기 때문에, 컴퓨터 전체가 멈추면 둘 다 동작하지 않음 | 공통 원인 고장 | 범위 밖. 하드웨어 와치독이 필요함을 기록 |

## 9. 출처

| 번호 | 내용 | 링크 |
|---|---|---|
| S1 | SAE J2012 DTC 정의 (코드 체계) | https://saemobilus.sae.org/standards/j2012_201612-diagnostic-trouble-code-definitions |
| S2 | DTC 형식 해설 (P/C/B/U 비트, 자리별 의미) | https://uds.readthedocs.io/en/latest/pages/knowledge_base/dtc.html |
| S3 | Raspberry Pi 공식 문서: Frequency management and thermal control (80 °C / 85 °C) | https://www.raspberrypi.com/documentation/computers/raspberry-pi.html |
| S4 | Raspberry Pi 공식 블로그: Heating and cooling Raspberry Pi 5 | https://www.raspberrypi.com/news/heating-and-cooling-raspberry-pi-5/ |
| S5 | Nav2 Collision Monitor 설정 (`source_timeout` 기본 2.0 s) | https://docs.nav2.org/configuration/packages/collision_monitor/configuring-collision-monitor-node.html |
| S6 | ROS diagnostic_updater FrequencyStatusParam (허용오차 0.1) | https://docs.ros.org/en/iron/p/diagnostic_updater/generated/structdiagnostic__updater_1_1FrequencyStatusParam.html |
| S7 | LD19 데이터시트 V2.5 (스캔 5/10/13 Hz) | https://botland.com.pl/img/art/inne/21991_Karta%20katalogowa.pdf |
| S8 | Nav2 Velocity Smoother 설정 (`velocity_timeout` 기본 1.0 s) | https://docs.nav2.org/configuration/packages/configuring-velocity-smoother.html |
| S9 | Raspberry Pi 공식 문서: `vcgencmd get_throttled` 비트 정의 | https://www.raspberrypi.com/documentation/computers/os.html |
| S10 | Linux man-pages `proc_stat(5)`: CPU 상태별 누적 시간 | https://man7.org/linux/man-pages/man5/proc_stat.5.html |
| S11 | Linux man-pages `proc_meminfo(5)`: MemAvailable 정의 | https://man7.org/linux/man-pages/man5/proc_meminfo.5.html |
