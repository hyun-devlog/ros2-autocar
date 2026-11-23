# E-CRASH 크래시 실험: E-06 safety_gate, E-02b ros_robot_controller (모터 연결, 주행 중)

- 일시: 2026-11-23
- 환경: MentorPi A1, Raspberry Pi 5, Docker MentorPi(ros:humble), **바퀴를 띄운 상태**
- 실행: `supervisor_bringup` 전체, `/cmd_vel`에 0.1 m/s를 10 Hz로 계속 발행 (바퀴 회전 중)
- 방법: 대상 프로세스를 `kill -KILL <PID>`로 종료 (SIGKILL: 정리 기회 없이 즉시 종료 = 크래시와 같은 상황)
- 데이터: rosbag `experiments_raw/E-CRASH_20261123_124019/bag` (git 제외), bringup 로그
- 관련: 잔여 위험 RR-01, H-11 / [E-02](E-02.md)(같은 노드를 SIGINT로 종료)

## 1. 왜 하는가

명령은 `safety_gate → odom_publisher → ros_robot_controller → USB 시리얼 → RRC Lite 보드 → 모터`로 간다.
이 경로의 두 지점을 **크래시**시켜, 정지 명령을 보낼 주체나 전달할 통로가 사라졌을 때 바퀴가 어떻게 되는지 확인한다.

| 실험 | 크래시 지점 | 질문 |
|---|---|---|
| E-06 | safety_gate | 정지 명령을 **보낼** 노드가 죽으면? |
| E-02b | ros_robot_controller | 정지 명령을 보드에 **전달할** 노드가 죽으면? |

## 2. E-06 safety_gate 크래시

| 시각 (Unix s) | 녹화 후 | 일 |
|---|---|---|
| 1795437655.082 | +35.1 | gate 출력 0.1 시작 (바퀴 회전) |
| 1795437680.883 | +60.9 | **gate 마지막 출력 = 크래시**. 0.0 없이 끊김 (pre-shutdown 훅 실행 안 됨) |
| 1795437691.683 | +71.7 | 명령 발행 중지 (창 C) |
| 1795437702.069 | +82.1 | `/safety_gate/cmd_vel`에 **직접** 0.0 발행 (창 D, 2 s 동안) → 바퀴 정지 |
| 1795437726.059 | +106.1 | `/robot_status` 마지막. 녹화 기간 내내 level 0, raw_level 0 |

- **바퀴가 21.2 s 동안 계속 회전** (크래시 → 수동 0 발행). 명령 발행을 멈춘 뒤에도 10.4 s 동안 계속 회전
- health_monitor는 끝까지 **OK**: 센서는 모두 정상이므로 감시자는 이 위험을 알 수 없음
- 벤더 `odom_publisher`가 마지막 명령(0.1 m/s)을 유지하는 H-01 동작이 그대로 나타남

## 3. E-02b ros_robot_controller 크래시

| 관찰 | 결과 |
|---|---|
| 바퀴 | **계속 회전**. 창 A `Ctrl + C`로도 멈추지 않아 **전원 스위치로 정지** |
| 감지 기록 | 전원 차단으로 녹화와 로그 끝부분이 디스크에 저장되지 않아 **기록 손실** (4절) |

- [E-02](E-02.md)(SIGINT)에서는 바퀴가 즉시 멈췄다. 정리 기회가 없는 SIGKILL에서는 멈추지 않았으므로,
  E-02의 즉시 정지는 **노드가 종료하면서 보드에 보낸 정지 명령** 때문이었다
- **RRC Lite 펌웨어는 시리얼 통신이 끊겨도 마지막 모터 명령을 유지한다**
- health_monitor의 감지는 E-02와 같은 경로(IMU 끊김)라 약 2 s로 예상되지만, 기록이 없어 **미확인**

## 4. 데이터 손실과 절차 개선

- 전원 차단 시 녹화(`bag_0.db3` 마지막 기록 12:42)와 `bringup_2.log` 끝부분이 손실됨
  - 원인 1: rosbag2가 메시지를 메모리 캐시에 모아 두었다가 기록
  - 원인 2: 운영체제가 파일 쓰기를 메모리에 모아 두었다가 디스크에 기록
- 복구: SQLite `pragma wal_checkpoint(TRUNCATE)`, `journal_mode=DELETE` 후 `ros2 bag reindex` → E-06 구간 복구 성공
- 개선 (`tools/record_experiment.sh`): `--max-cache-size 0`, `--storage-preset-profile resilient`, 1 s마다 `sync`
- 개선 (절차): 전원 차단 전 `sync` 입력

## 5. 결론

| | 결과 | 의미 |
|---|---|---|
| E-06 | gate 크래시 시 바퀴 계속 회전, 감시자도 OK | **RR-01 확인.** 소프트웨어 안전 계층 자체의 크래시는 소프트웨어로 막을 수 없음 |
| E-02b | RRC 노드 크래시 시 바퀴 계속 회전, 전원으로만 정지 | **펌웨어에 명령 timeout이 없음.** 보드 통신 크래시는 감지하더라도 정지 명령을 전달할 길이 없음 |

소프트웨어 안전 계층은 센서·감시자·명령 소스의 고장을 1~2 s 안에 정지로 바꾼다([E-MAIN](E-MAIN.md), [E-02](E-02.md), [BRINGUP-01](BRINGUP-01.md)).
그러나 안전 계층 자체나 모터 보드 통신 노드가 크래시하면 정지 명령을 보내거나 전달할 길이 없고, 벤더 펌웨어는 마지막 명령을 유지한다.
이 경우를 막으려면 **하드웨어 비상 정지** 또는 **펌웨어 쪽 명령 timeout**이 필요하다.

## 6. 남은 확인 (제출 후)

- E-02b를 개선된 녹화 절차로 다시 수행해 감지 시각 기록
- 소프트웨어 쪽 완화 후보: gate와 같은 프로세스에서 동작하지 않는 독립 감시(예: `odom_publisher` 입력 토픽의 마지막 명령 나이 감시) — 다만 `ros_robot_controller` 크래시는 여전히 해결 불가
