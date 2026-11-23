# BRINGUP-01 모터 입력 경로 확인과 명령 끊김 정지 (모터 연결)

- 일시: 2026-11-23
- 환경: MentorPi A1, Raspberry Pi 5, Docker MentorPi(ros:humble), **바퀴를 띄운 상태**
- 실행: `ros2 launch supervisor_bringup bringup.launch.py` (health_monitor, safety_gate, Hiwonder controller, 라이다)
- 데이터: rosbag `experiments_raw/BRINGUP-01_20261123_115047/bag` (git 제외), 그래프 확인 출력
- 분석: `python3 tools/bag_timeline.py <bag>`
- 관련 요구사항: SR-04, SR-05 / 위험: H-01, H-04

## 1. 경로 확인 (SR-05)

`ros2 topic info -v`로 네 토픽의 발행·구독 노드를 확인했다.

| 토픽 | 1차 (수정 전) | 2차 (수정 후) | 기대 |
|---|---|---|---|
| `/cmd_vel` 구독 | safety_gate | safety_gate | safety_gate만 |
| `/controller/cmd_vel` 구독 | safety_gate, **odom_publisher** | safety_gate | safety_gate만 |
| `/safety_gate/cmd_vel` 구독 | odom_publisher | odom_publisher | odom_publisher |
| `/safety_gate/controller/cmd_vel` 구독 | **없음** | odom_publisher | odom_publisher |

- 1차: launch 파일 두 번째 `SetRemap`의 오타로 `/controller/cmd_vel`이 `odom_publisher`에 직접 연결되어 있었다.
  이 경로로 들어온 명령은 gate를 거치지 않고, gate가 이 채널로 보내는 정지 명령은 아무도 받지 않는 상태였다.
- 바퀴 시험 전에 그래프를 확인하는 절차로 발견했고, 오타 수정 후 2차에서 기대와 일치했다.
- 원본: `experiments_raw/BRINGUP-01/graph_before_fix.txt`, `graph_after_fix.txt`

## 2. 명령 끊김 → 정지 (SR-04, H-01)

0.1 m/s 명령을 10 Hz로 보내다가 Ctrl+C로 멈췄다. 두 입구를 번갈아 각 3회.

| 회 | 입구 | 마지막 명령 (Unix s) | gate 0 출력 | 지연 |
|---|---|---|---|---|
| 1 | `/cmd_vel` | 1795434821.750 | 1795434822.755 | 1.005 s |
| 2 | `/controller/cmd_vel` | 1795434876.282 | 1795434877.356 | 1.074 s |
| 3 | `/cmd_vel` | 1795434903.318 | 1795434904.355 | 1.037 s |
| 4 | `/controller/cmd_vel` | 1795434927.797 | 1795434928.854 | 1.057 s |
| 5 | `/controller/cmd_vel` | 1795435032.944 | 1795435033.954 | 1.010 s |
| 6 | `/cmd_vel` | 1795435054.798 | 1795435055.853 | 1.055 s |

- 지연: 1.005~1.074 s, 평균 1.040 s. 기대 범위(input timeout 1.0 s + tick 최대 0.1 s) 안
- 매번 gate는 0을 **한 번**만 보낸 뒤 조용해짐 (쓰지 않는 채널을 덮어쓰지 않음)
- **6회 모두 바퀴가 실제로 멈춤** (육안 확인, 영상 촬영)
- 프로젝트를 시작한 문제 H-01(명령 한 번에 바퀴가 계속 회전)이 실제 모터에서 해결됨

## 3. 관찰

1. **전체 시스템에서 오탐 없음**: controller·라이다·health_monitor·safety_gate를 모두 띄운 480 s 동안
   `/robot_status`가 level 0, raw_level 0으로 유지됨 (RATE-01 조치가 전체 부하에서도 유효)
2. **종료 시 0 송신**: bringup 종료 직전 두 채널 모두 마지막 메시지가 0 (1795435128.502)
3. **`/controller/cmd_vel` 첫 명령 통과 지연**: 새 `ros2 topic pub` 프로세스를 띄울 때, 녹화기가 첫 명령을 받은 뒤
   gate 출력이 나오기까지 0.38~0.50 s 걸림 (`/cmd_vel`은 0.1 s 이내). 새 발행자와 연결되는 데 걸리는 시간으로 보이며,
   출발이 늦어질 뿐 정지 경로와는 관계없음. 원인 확인은 보류
4. 1차 경로 확인 실패는 실험 절차(바퀴 시험 전 그래프 확인)가 우회 경로를 잡아낸 사례로, E-04에 함께 기록

## 4. 결론

- SR-05: 두 입구 모두 safety_gate 경유, 우회 경로 없음 → **충족** (1차 발견 후 수정)
- SR-04: 명령 끊김 후 평균 1.04 s에 정지 명령, 실제 바퀴 정지 → **충족**
