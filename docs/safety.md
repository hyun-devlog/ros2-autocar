# 안전 분석 및 안전 요구사항 (Safety Analysis & Requirements)

> 본 프로젝트는 ISO 26262 / Automotive SPICE의 준수 또는 인증을 목적으로 수행되지 않았습니다.
> 교육 과정에서 학습한 기능안전 방법론을 프로젝트 규모와 개발 환경에 맞게 적용하여 위험 분석, 안전 요구사항 정의 및 검증 간 추적성을 확보하고자 했습니다.

## 1. 안전 분석 범위

| 항목 | 내용 |
|---|---|
| 대상 시스템 | Hiwonder MentorPi A1 (Raspberry Pi 5, RRC Lite, Ackermann 조향 방식) |
| 대상 소프트웨어 | ROS2 Auto Car의 health_monitor, safety_gate, supervisor_bringup |
| 분석 제외 범위 | Hiwonder 제공 소프트웨어, RRC Lite 펌웨어, SLAM/Nav2 알고리즘 |
| 안전 상태 (Safe State) | **모든 구동 모터가 정지하여 로봇의 주행이 중단된 상태** |

안전 상태에 도달하기 위한 소프트웨어 제어 동작으로 0 속도 명령을 출력합니다. 정지 명령 출력과 실제 모터 정지는 구분하여 검증합니다.

## 2. V-Model 기반 개발 및 검증 절차

V-Model의 개발 및 검증 구조를 참고하여 각 개발 단계의 산출물과 대응하는 검증 활동을 정의했습니다.

| 개발 단계 | 개발 산출물 | 검증 단계 | 검증 산출물 |
|---|---|---|---|
| 안전 요구사항 정의 | 안전 요구사항 목록 (본 문서 3·4절) | 시스템 검증 | 모터 연결 환경에서의 장애 주입 실험(E-01~E-06, E-02b), 장애 감지 및 정지 명령 출력 시간 측정 결과 (6절, `docs/metrics/`) |
| 소프트웨어 아키텍처 설계 | README 시스템 아키텍처, `robot_interfaces` 메시지 정의 | 통합 검증 | 주행 명령 전달 경로 및 안전 제어 동작 검증 결과 (BRINGUP-01, GATE-02) |
| 소프트웨어 상세 설계 | 장애 판정 규칙, 장애 코드 정의 (`docs/fault_codes.md`) | 단위 검증 | GoogleTest 기반 단위 테스트 33개, GitHub Actions 기반 CI 검증 결과 |
| 소프트웨어 구현 | C++17 및 rclcpp 기반 ROS 2 노드, ROS 2 비의존 판정 라이브러리(`health_core`, `gate_core`) | | |

## 3. 위험 분석 (Hazard Analysis)

ASIL (Automotive Safety Integrity Level) 등급은 산정하지 않았습니다. ASIL 산정에는 심각도(Severity), 노출도(Exposure), 통제 가능성(Controllability)에 대한 평가가 필요합니다. 본 프로젝트는 교육용 로봇을 대상으로 하므로, 실제 차량 운용에 기반한 평가 근거를 확보하기 어렵다는 점을 고려하여 위험 식별 및 안전 요구사항 정의에 중점을 두었습니다.

| ID | 식별된 위험 | 식별 근거 및 검증 결과 | 관련 안전 요구사항 |
|---|---|---|---|
| H-01 | 마지막 주행 명령 유지로 인한 의도하지 않은 지속 주행 | `ros2 topic pub --once /cmd_vel`을 통해 주행 명령을 1회 발행한 이후에도 바퀴가 지속적으로 회전하는 현상 확인 (명령 타임아웃 미적용) | SR-04, SR-09 |
| H-02 | LiDAR 데이터 수신 중단으로 인한 장애물 인지 불가 상태에서의 주행 | 실제 LiDAR 토픽이 `/scan`이 아닌 `/scan_raw`임을 확인. 토픽 설정 오류로 인한 모니터링 누락 가능성 식별 | SR-01, SR-03 |
| H-03 | RRC Lite 시리얼 통신 단절로 인한 구동 제어 불가 | 터미널 종료 후 잔존 프로세스에 의한 시리얼 포트 중복 접근 오류 `multiple access on port` 확인 | SR-02 |
| H-04 | 안전 제어 계층을 우회하는 주행 명령 전달 | `odom_publisher`가 `/cmd_vel`과 `/controller/cmd_vel`을 모두 구독하는 구조 확인 | SR-05 |
| H-05 | 간헐적인 센서 데이터 수신 중단으로 인한 정지 및 주행 재개 반복 | 설계 검토를 통해 식별하고, E-01 복구 시 2.0 s 대기 후 주행 재개를 확인 (E-MAIN) | SR-06 |
| H-06 | SoC 과열에 따른 처리 성능 저하 및 안전 제어 응답 지연 | Raspberry Pi 5의 온도 기반 스로틀링 동작을 고려한 설계 검토 | SR-07 |
| H-07 | 전원 공급 불안정으로 인한 시스템 동작 불안정 | 모터 부하에 따른 전압 강하 가능성을 고려한 설계 검토 | SR-08 |
| H-08 | CPU 및 메모리 자원 고갈로 인한 노드 처리 주기 저하 또는 비정상 종료 | 시스템 리소스 사용량을 고려한 설계 검토 | SR-11, SR-12 |
| H-09 | `health_monitor` 동작 중단으로 인한 장애 감지 및 상태 정보 전달 불가 | 설계 검토를 통해 식별하고 GATE-02의 5번 시험과 health_monitor 종료 실험 수행 (E-05: 0.552 s 후 정지 명령) | SR-13 |
| H-10 | 간헐적인 LiDAR 스캔 누락에 대한 성능 저하 오판정으로 인한 불필요한 감속 | GATE-02에서 2분간 3회 감속 발생. RATE-01에서 약 35 s 간격으로 스캔 데이터 1개 누락되는 현상 확인 | SR-03 |
| H-11 | safety_gate 비정상 종료로 인한 정지 명령 출력 불가 및 마지막 주행 명령 유지 | 설계 검토를 통해 식별하고 E-06에서 비정상 종료 이후 21.2 s간 바퀴가 지속적으로 회전하는 현상 확인 | 잔여 위험 RR-01 |
| H-12 | `ros_robot_controller` 비정상 종료로 인한 모터 제어 보드로의 정지 명령 전달 불가 | E-02b에서 바퀴가 지속적으로 회전했으며, 전원 차단을 통해 정지 | 잔여 위험 RR-04 |
| H-13 | `health_monitor` 재시작 직후 모니터링 대상의 상태 확인 이전에 OK 등급을 발행하여 장애 상태에서도 주행이 허용될 가능성 | E-05에서 health_monitor 재시작 후 0.005 s 만에 주행이 재개되는 현상 확인 | 잔여 위험 RR-03 |

## 4. 안전 요구사항 (Safety Requirements)

안전 요구사항별 판정 기준, 설정 근거, 담당 구성 요소 및 검증 방법을 정의합니다.

공식 문서 및 기술 자료에 대한 출처는 9절에 정리했습니다. 공식 기준이 확인되지 않아 프로젝트에서 자체적으로 정의한 값은 **설계값**으로 명시하며, 실험 및 측정 결과를 기반으로 설정한 값은 **실측 기반 설계값**으로 구분합니다.

| ID | 안전 요구사항 | 기준값 | 설정 근거 | 담당 구성 요소 | 검증 방법 |
|---|---|---|---|---|---|
| SR-01 | `/scan_raw`의 데이터 수신 중단 시간이 기준값 이상이면 FAULT(U1002)로 판정한다. | 2.0 s | [S5] Nav2 `source_timeout` 기본값 참고 | health_monitor | 단위 테스트, 장애 주입(LiDAR 노드 종료) |
| SR-02 | `/ros_robot_controller/imu_raw`의 데이터 수신 중단 시간이 기준값 이상이면 FAULT(U1002)로 판정한다. | 2.0 s | [S5] Nav2 `source_timeout` 기본값 참고 | health_monitor | 단위 테스트, 장애 주입(`ros_robot_controller` 종료) |
| SR-03 | 모니터링 대상 토픽의 수신율이 기준 수신율의 90% 미만이면 DEGRADED(U1003)로 판정한다. 수신율은 최근 2.0 s의 시간 윈도우를 기준으로 산출한다. | LiDAR 9.0 Hz, IMU·odom 44 Hz, 시간 윈도우 2.0 s | [S6] diagnostic_updater 허용오차 0.1, [S7] LD19 기준 수신율 10 Hz, IMU·odom 실측 수신율 49 Hz. 시간 윈도우는 **실측 기반 설계값** (RATE-01: 단일 스캔 누락 허용) | health_monitor | 단위 테스트, 장시간 수신율 측정 |
| SR-04 | 주행 명령 수신 중단 시간이 기준값 이상이면 0 속도 명령을 발행한다. | 1.0 s | [S8] Nav2 `velocity_timeout` 기본값 참고 | safety_gate | 단위 테스트, 장애 주입(주행 명령 발행 중단) |
| SR-05 | `/cmd_vel`과 `/controller/cmd_vel`을 통한 모든 주행 명령은 safety_gate를 경유해야 한다. | — | H-04에서 확인한 주행 명령 전달 경로 | supervisor_bringup | `ros2 topic info -v`를 통한 토픽 구독 및 명령 전달 경로 확인 |
| SR-06 | 상태 등급의 복구는 개선된 상태가 기준 시간 이상 유지된 이후에 반영한다. | 2.0 s | **설계값** | health_monitor | 단위 테스트 |
| SR-07 | SoC 온도 80 °C 이상이면 DEGRADED(B1001), 85 °C 이상이면 FAULT(B1002)로 판정한다. | 80 / 85 °C | [S3][S4] Raspberry Pi 공식 문서의 온도 기준 참고 | health_monitor | 단위 테스트(모의 sysfs 환경) |
| SR-08 | 펌웨어 상태 플래그에서 저전압 상태가 확인되면 DEGRADED(P1001), 스로틀링 상태가 확인되면 DEGRADED(B1101)로 판정한다. | bit 0 / bit 2 | [S9] `get_throttled` 상태 플래그 정의. 상태 등급은 **설계값** | health_monitor | 단위 테스트(모의 sysfs 환경) |
| SR-09 | FAULT 상태가 유지되는 동안 0 속도 명령을 지정된 발행 주기로 반복 발행한다. | 10 Hz | H-01에서 확인한 마지막 주행 명령 유지 동작 | safety_gate | 장애 주입 실험 |
| SR-10 | 시스템 시작 이후 수신 이력이 없는 토픽은 초기화 유예 시간이 종료된 시점에 FAULT(U1001)로 판정한다. | 5.0 s | **설계값** (시스템 초기화 소요 시간 측정 후 조정 예정) | health_monitor | 단위 테스트 |
| SR-11 | CPU 사용률이 설정된 기준값 이상이면 DEGRADED(B1201)로 판정한다. | **실측 후 결정** (기준값 설정 전까지 판정 비활성화) | 측정 방법: [S10] `/proc/stat`. 기준값: **실측 기반 설계값** | health_monitor | 단위 테스트(모의 `/proc/stat` 환경), 부하 환경에서의 CPU 사용률 측정 |
| SR-12 | 전체 메모리 대비 가용 메모리(MemAvailable) 비율이 설정된 기준값 이하이면 DEGRADED(B1202)로 판정한다. | **실측 후 결정** (기준값 설정 전까지 판정 비활성화) | 측정 방법: [S11] `/proc/meminfo`. 기준값: **실측 기반 설계값** | health_monitor | 단위 테스트(모의 `/proc/meminfo` 환경), 부하 환경에서의 메모리 사용량 측정 |
| SR-13 | `/robot_status`의 수신 이력이 없거나 상태 메시지 수신 중단 시간이 기준값 이상이면 0 속도 명령을 발행한다. | 0.5 s | **설계값** (health_monitor의 10 Hz 발행 주기 기준 5개 메시지 발행 간격에 해당) | safety_gate | 단위 테스트, 장애 주입(health_monitor 종료) |
| SR-14 | DEGRADED 상태가 유지되는 동안 주행 명령의 모든 속도 성분에 동일한 감속 계수를 적용한다. | 0.5 (50%) | **설계값** (감속 비율에 대한 공식 기준 미확인) | safety_gate | 단위 테스트 |

### SR-11 및 SR-12 판정 기준값 설정 절차  

1. supervisor_bringup, LiDAR 및 SLAM 등 연산 부하가 높은 노드를 실행한 상태에서 10~20분간 CPU 사용률, 가용 메모리 (MemAvailable) 비율 및 모니터링 대상 토픽의 수신율을 기록한다. 토픽 수신율은 `tools/record_experiment.sh`로, CPU 사용률과 가용 메모리 비율은 `vmstat 1` 출력으로 같은 시간 동안 기록한다.
2. 토픽 수신율이 기준 수신율의 90% (설정값 `watch.min_hz`) 미만으로 저하되기 시작하는 시점의 CPU 사용률과 가용 메모리 비율을 확인한다. 저하가 나타나지 않으면 `stress-ng`로 CPU 및 메모리 부하를 단계적으로 높여 저하 시점을 찾는다.
3. 측정 결과에 안전 여유 (Margin)를 반영하여 CPU 사용률의 상한값과 가용 메모리 비율의 하한값을 설정한다.
4. 설정된 기준값의 적절성을 반복 실험을 통해 검증하고, 측정 일자, 실험 환경, 부하 조건, 측정 결과 및 최종 기준값을 본 문서에 기록한다.

## 5. 추적성 (Traceability)

안전 요구사항과 검증 항목 간의 추적성을 확보하기 위해 단위 테스트 스위트 이름에 해당 안전 요구사항 ID를 포함했습니다. (예: StateEvaluatorSR01.*).

실험은 모터 연결 여부와 목적에 따라 다음과 같이 구분합니다.
- **예비 검증**: 모터를 연결하지 않은 환경에서의 안전 제어 동작 검증 (E-01: health_monitor 단독, GATE-02: safety_gate 포함)
- **장시간 측정**: 정상 동작 중인 LiDAR의 수신율 오판정 원인 분석 (RATE-01)
- **본 검증**: 모터를 연결한 환경에서의 통합 검증 및 장애 주입 실험 (BRINGUP-01, E-MAIN(E-01, E-05), E-02, E-CRASH(E-06, E-02b))

실험별 수행 절차 및 검증 결과는 `docs/experiments/`에 기록했으며, 실험 데이터에서 자동으로 산출한 정량적 측정 결과는 `docs/metrics/summary.md`에 정리했습니다.

| 안전 요구사항 | 구현 위치 | 단위 테스트 | 검증 실험 | 검증 결과 |
|---|---|---|---|---|
| SR-01 | `state_evaluator.cpp`, `rate_tracker.cpp` | `StateEvaluatorSR01.*`, `RateTrackerSR01.*` | E-01 LiDAR 종료 | 모터 연결 환경에서 마지막 스캔 수신 후 2.017 s에 U1002 판정, 2.068 s에 정지 명령 출력 (E-MAIN) |
| SR-02 | `state_evaluator.cpp` | `StateEvaluatorSR01.*` (동일 판정 규칙) | E-02 `ros_robot_controller` 종료 | 마지막 IMU 데이터 수신 후 각각 2.013 s, 2.058 s에 U1002 판정(E-02, E-MAIN). `/odom_raw`는 통신 단절 이후에도 발행이 지속되어 통신 단절 감지 지표로 부적합함을 확인 |
| SR-03 | `state_evaluator.cpp`, `rate_tracker.cpp` | `StateEvaluatorSR03.*`, `RateTrackerSR03.*` | RATE-01 장시간 수신율 측정 | 523.9 s 동안 스캔 데이터 누락 15회 발생, 장애 오탐지 0회 |
| SR-04 | `gate_logic.cpp` | `GateLogicSR04.*` | E-03: 주행 명령 발행 중단 | 모터 연결 환경에서 총 7회 실험 수행. 주행 명령 수신 중단 후 1.005~1.074 s에 정지 명령 출력 (BRINGUP-01, E-MAIN). BRINGUP-01의 6회 실험에서 실제 바퀴 정지 여부를 육안으로 확인 |
| SR-05 | `supervisor_bringup/launch/bringup.launch.py` | | E-04: 토픽 구독 및 명령 전달 경로 확인 | 초기 검증에서 안전 제어 계층 우회 경로 식별. 수정 후 두 주행 명령 입력 토픽의 구독자가 `safety_gate`로 제한됨을 확인 (BRINGUP-01) |
| SR-06 | `state_evaluator.cpp` | `StateEvaluatorSR06.*` | E-01: LiDAR 데이터 수신 복구 | 모터 연결 환경에서 스캔 데이터 수신 재개 후 2.251 s에 OK 등급으로 복구, 2.337 s에 주행 재개 (E-MAIN) |
| SR-07 | `state_evaluator.cpp`, `system_probe.cpp` | `StateEvaluatorSR07.*`, `SystemProbeTest.SR07*` | | |
| SR-08 | `state_evaluator.cpp`, `system_probe.cpp` | `StateEvaluatorSR08.*`, `SystemProbeTest.SR08*` | | |
| SR-09 | `gate_logic.cpp` | `GateLogicSR09.*` | E-01, E-02 | FAULT 판정 후 2.035~2.129 s에 정지 명령 출력. STOP 상태에서 두 주행 명령 경로에 0 속도 명령이 반복 출력됨을 확인 (E-MAIN, E-02, GATE-02) |
| SR-10 | `state_evaluator.cpp`, `topic_watchdog.cpp` | `StateEvaluatorSR10.*`, `RateTrackerSR10.*` | RATE-01: 초기화 상태 검증 | 실제 로봇에서 컨트롤러 노드를 실행하지 않은 상태로 시스템 시작 후 5 s에 U1001 판정 (RATE-01 5절) |
| SR-11 | `state_evaluator.cpp`, `system_probe.cpp` | `StateEvaluatorSR11.*` (CPU·메모리 공통, 기준값 설정 전 비활성 확인), `SystemProbeTest.SR11*` | 부하 환경에서의 CPU 사용률 측정 | 실측 및 판정 기준값 설정 예정 |
| SR-12 | `state_evaluator.cpp`, `system_probe.cpp` | `StateEvaluatorSR11.*` (SR-11과 공통), `SystemProbeTest.SR12*` | 부하 환경에서의 가용 메모리 비율 측정 | 실측 및 판정 기준값 설정 예정 |
| SR-13 | `gate_logic.cpp` | `GateLogicSR13.*` | E-05: health_monitor 종료 | 모터 연결 환경에서 마지막 상태 메시지 수신 후 0.552 s에 정지 명령 출력 (E-MAIN) |
| SR-14 | `gate_logic.cpp` | `GateLogic.DegradedScalesCommand`, `GateLogic.EnteringDegradedResendsScaledLastCommand` | GATE-02, E-MAIN | DEGRADED 상태에서 감속 계수 0.5가 적용되어 속도 명령이 0.1에서 0.05로 감소함을 확인 |

## 6. 장애 감지 및 대응 시간 (FTTI 개념 참고)

ISO 26262의 고장 감지 및 대응 시간 개념을 참고하여 장애 주입 실험에서 다음 두 항목을 측정합니다.
- **장애 감지 시간**: 마지막 정상 메시지 수신 시점부터 FAULT 판정까지 소요된 시간
- **장애 대응 시간**: 마지막 정상 메시지 수신 시점부터 safety_gate의 정지 명령 출력까지 소요된 시간 

장애 대응 시간은 소프트웨어의 정지 명령 출력 시점을 기준으로 하며, 실제 바퀴가 정지하기까지의 시간과는 구분합니다. 정지 명령 출력에 따른 실제 바퀴 정지는 BRINGUP-01에서 육안으로 확인했습니다.

E-02에서는 `ros_robot_controller`의 SIGINT 종료 처리 과정에서 바퀴가 먼저 정지했습니다. 이후 `safety_gate`에서 정지 명령을 출력했으나, 모터 제어 보드로의 명령 전달을 담당하는 `ros_robot_controller`의 종료로 인해 모터 제어 보드까지 명령을 전달할 수 없는 구조임을 확인했습니다. 해당 구조적 한계는 RR-04에 반영했습니다.

시간 측정의 기준 시점은 마지막 정상 메시지 수신 시각으로 정의하며, 시각은 Unix Timestamp(초 단위)로 기록합니다.

| 실험 | 측정 기준 시각 (Unix s) | FAULT 판정 시각 | 정지 명령 출력 시각 | 장애 감지 시간 | 장애 대응 시간 및 결과 |
|---|---|---|---|---|---|
| E-01 예비 검증 (safety_gate 미실행) | 1795341827.218 | 1795341829.282 | — | 2.06 s | 해당 없음 |
| GATE-02 2 (모터 미연결) | 1795356946.733 | 로그 없음 | 1795356948.840 | — | 2.11 s |
| E-01 LiDAR 노드 종료 (모터 연결) | 1795436518.047 | 1795436520.064 | 1795436520.116 | 2.017 s | 2.069 s |
| E-02 ros_robot_controller 종료 | 1795437059.967 | 1795437061.980 | 1795437062.002 | 2.013 s | 2.035 s (실제 바퀴 정지는 노드 종료 처리에 의해 수행) |
| E-03 주행 명령 발행 중단 | — | — | — | — | 1.005~1.074 s |
| E-05 health_monitor 종료 | 1795436548.963 | safety_gate 자체 판단 | 1795436549.515 | — | 0.552 s |
| E-06 safety_gate 비정상 종료 | 1795437680.883 | 현재 모니터링 범위 외 | 정지 명령 생성 기능 중단 | — | **자동 정지 실패** (수동 개입까지 21.2 s 동안 바퀴 회전 지속) |
| E-02b ros_robot_controller 비정상 종료 | 기록 손실 (전원 차단) | 기록 손실 | 전달 경로 없음 | — | **자동 정지 실패** (전원 차단을 통해 정지) |

본 실험은 장애 감지 및 소프트웨어 대응 시간의 정량적 측정과 안전 제어 구조의 한계 식별을 목적으로 수행했습니다. ISO 26262에서 정의하는 FTTI 산정 및 안전 상태 도달 시간에 대한 적절성 평가는 수행 범위에 포함하지 않았습니다.

## 7. 시스템 상태 정보 수집 및 측정 결과

| 항목 | 측정 환경 |
|---|---|
| 측정 일자 | 2026-11-22 |
| 실행 환경 | Raspberry Pi 5 (8 GB), MentorPi Docker 컨테이너 |
| 실행 계정 | ubuntu |

| 모니터링 항목 | 데이터 수집 경로 | 측정 결과 |
|---|---|---|
| SoC 온도 | `/sys/class/thermal/thermal_zone0/temp` | 47400 (47.4 °C) |
| 저전압 상태 | `/sys/class/hwmon/hwmon3/in0_lcrit_alarm` (name=`rpi_volt`) | 0 (저전압 미감지) |
| 스로틀링 상태 | `/sys/devices/platform/soc/soc:firmware/get_throttled` | 0 (모든 비트 0: 현재 및 부팅 이후 저전압·스로틀링 없음) |
| 메모리 상태 | `/proc/meminfo` | MemAvailable: 4674080 kB / MemTotal 8245696 kB (가용 메모리 비율 56.7 %) |
| CPU 사용 시간 | `/proc/stat` | 첫 번째 cpu 행의 누적 시간 필드 10개 확인 |

hwmon 장치 번호는 시스템 부팅 시 달라질 수 있습니다. 이에 따라 장치 번호를 고정하지 않고 장치 이름(`rpi_volt`)을 기반으로 데이터 수집 경로를 동적으로 탐색하도록 구현했습니다.

## 8. 잔여 위험 (Residual Risk)

현재 소프트웨어 기반 안전 제어 구조에서 추가적인 대응이 필요한 위험을 잔여 위험으로 정의합니다. 설계 검토 및 장애 주입 실험을 통해 위험 발생 조건과 시스템 동작 특성을 확인하고, 향후 안전 제어 구조의 개선 방향을 도출합니다.

| ID | 식별된 잔여 위험 | 발생 원인 및 구조적 한계 | 검증 결과 및 개선 방향 |
|---|---|---|---|
| RR-01 | `safety_gate` 비정상 종료 시 정지 명령 출력이 중단되고, 하위 구동 경로 (`odom_publisher` 또는 RRC Lite 펌웨어)에서 마지막 주행 명령이 유지됨(H-11) | 정상 종료 시에는 pre-shutdown 훅을 통해 정지 명령을 출력하는 구조이며, 비정상 종료에 대응할 독립적인 안전 제어 수단은 추가 확보가 필요함 | **실험 검증 (E-06)**: SIGKILL 이후 수동 개입까지 21.2 s 동안 바퀴 회전이 지속되었으며, `health_monitor`는 OK 등급을 유지함. **개선 방향**: 하드웨어 비상 정지 및 펌웨어 수준의 명령 타임아웃 |
| RR-02 | Raspberry Pi 5의 시스템 전체 장애 발생 시 `health_monitor`와 `safety_gate`의 동작이 동시에 중단될 가능성 | 두 노드가 동일한 컴퓨팅 환경에서 실행되는 구조로 인한 공통 원인 고장(Common Cause Failure) 가능성 | **설계 검토**: 현재 프로젝트의 검증 범위를 하드웨어 수준으로 확장할 경우, 독립적인 하드웨어 워치독(Watchdog) 적용 검토 필요 |
| RR-03 | `health_monitor` 재시작 직후 초기화 유예 시간(5.0 s) 동안 모니터링 대상의 상태 확인 이전에 OK 등급이 발행됨(H-13) | SR-10에 따라 초기화 유예 시간 동안 수신 이력이 없는 토픽을 장애 판정 대상에서 제외하는 구조 | **실험 관찰 (E-MAIN 관찰 3, E-05)**: 초기화 과정의 상태 등급 판정 방식에 대한 개선 필요성을 식별함. 초기화 유예 시간 동안 DEGRADED 등급을 발행하는 방안을 개선 후보로 정의함 |
| RR-04 | `ros_robot_controller` 비정상 종료 시 모터 제어 보드로의 정지 명령 전달이 중단되고, RRC Lite 펌웨어에서 마지막 모터 명령이 유지됨(H-12) | 모터 제어 보드로의 명령 전달 경로가 단일 노드에 의존하며, 펌웨어 수준의 명령 타임아웃 기능이 적용되어 있지 않은 구조 | **실험 검증 (E-02b)**: SIGKILL 이후 바퀴 회전이 지속되었으며, 전원 차단을 통해 정지함. SIGINT 종료 실험(E-02)에서는 노드의 정상 종료 처리에 따른 즉각적인 바퀴 정지를 확인함. **개선 방향**: 하드웨어 비상 정지 및 펌웨어 수준의 명령 타임아웃 검토 |

## 9. 참고 자료 (Reference)

| ID | 참고 자료 | 참고 내용 | 링크 |
|---|---|---|---|
| S1 | SAE J2012 - Diagnostic Trouble Code Definitions | 장애 코드 체계 및 분류 기준 | [SAE 표준 문서](https://saemobilus.sae.org/standards/j2012_201612-diagnostic-trouble-code-definitions) |
| S2 | UDS Documentation - Diagnostic Trouble Code (DTC) | DTC 구성 요소 및 비트 인코딩 방식 | [기술 문서](https://uds.readthedocs.io/en/latest/pages/knowledge_base/dtc.html) |
| S3 | Raspberry - Frequency Management and Thermal Control | 온도 기반 성능 제어 기준 (80 °C, 85 °C) | [공식 문서](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html) |
| S4 | Raspberry Pi - Heating and Cooling Raspberry Pi 5 | Raspberry Pi 5의 발열 특성 및 온도 제어 동작 | [공식 기술 블로그](https://www.raspberrypi.com/news/heating-and-cooling-raspberry-pi-5/) |
| S5 | Nav2 - Collision Monitor Configuration | 센서 데이터 수신 타임아웃 설정 (`source_timeout`, 2.0 s) | [공식 문서](https://docs.nav2.org/configuration/packages/collision_monitor/configuring-collision-monitor-node.html) |
| S6 | ROS - `diagnostic_updater::FrequencyStatusParam` | 메시지 수신율 판정 허용오차 (0.1) | [API 문서](https://docs.ros.org/en/iron/p/diagnostic_updater/generated/structdiagnostic__updater_1_1FrequencyStatusParam.html) |
| S7 | LD19 LiDAR Datasheet V2.5 | LiDAR 스캔 주파수 사양(5/10/13 Hz) | [제품 데이터 시트](https://botland.com.pl/img/art/inne/21991_Karta%20katalogowa.pdf) |
| S8 | Nav2 - Velocity Smoother Configuration | 주행 명령 수신 타임아웃 설정 (`velocity_timeout`, 1.0 s) | [공식 문서](https://docs.nav2.org/configuration/packages/configuring-velocity-smoother.html) |
| S9 | Raspberry Pi - `vcgencmd get_throttled` | 저전압 및 스로틀링 상태 플래그의 비트 정의 | [공식 문서](https://www.raspberrypi.com/documentation/computers/os.html) |
| S10 | Linux man-pages - `proc_stat(5)` | CPU 상태별 누적 사용 시간 및 측정 방법 | [시스템 매뉴얼](https://man7.org/linux/man-pages/man5/proc_stat.5.html) |
| S11 | Linux man-pages - `proc_meminfo(5)` | 시스템 메모리 정보 및 MemAvailable 필드 정의 | [시스템 매뉴얼](https://man7.org/linux/man-pages/man5/proc_meminfo.5.html) |
