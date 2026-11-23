# 지표 (Metrics)

실험 bag에서 `tools/supervisor_metrics.py`로 자동 계산합니다. 시각은 모두 녹화기가 메시지를 받은 시각입니다.
결과 표와 그래프는 `docs/metrics/`에 생성됩니다.

## 지표 정의

| ID | 지표 | 계산 | 목표 | 관련 |
|---|---|---|---|---|
| M-01 | 감지 시간 | 센서 토픽 마지막 메시지 → `/robot_status` level 2 | 2.0~2.1 s | SR-01, SR-02 |
| M-02 | 정지 명령 시간 | 고장 시작(마지막 정상 메시지) → gate 출력 0 | 센서 ≤ 2.2 s, 명령 끊김 ≤ 1.1 s, 감시자 끊김 ≤ 0.6 s | SR-04, SR-09, SR-13 |
| M-03 | 등급 타임라인 | `/robot_status` level, raw_level | — | SR-06 |
| M-04 | 라이다 주기 | `/scan_raw` 1 s 구간별 메시지 수 | ≥ 9 | SR-03 |
| M-05 | DTC 발생 횟수 | 고장 코드가 새로 나타난 횟수 | — | 전체 |
| M-06 | 정상 가동률 | level 0인 시간 ÷ 녹화 시간 | — | 가용성 |
| M-07 | 회복 시간 | 센서 재개 → level 0 | 약 2.0~2.3 s (회복 대기 포함) | SR-06 |

## 자동으로 찾는 고장 구간

| 종류 | 찾는 방법 |
|---|---|
| sensor | 감시 센서 토픽이 timeout(2.0 s)보다 오래 끊긴 구간 |
| command | 명령 입력(`/cmd_vel`, `/controller/cmd_vel`)이 1.0 s보다 오래 끊긴 구간 (gate가 이미 정지 중이면 제외) |
| monitor | `/robot_status`가 0.5 s보다 오래 끊긴 구간 |
| gate | 명령은 계속 들어오는데 gate 출력이 끊긴 구간 (gate 크래시). 0이 나오지 않으면 `NO STOP` |

녹화 끝 10 s 안에 모든 것이 멈춘 구간은 정상 종료로 보고 제외합니다.

## 버전

| 버전 | 내용 | 상태 |
|---|---|---|
| v1 | bag → 표(summary.md)와 그래프(PNG) | 완료 |
| v2 | PlotJuggler / Foxglove 레이아웃으로 실시간 확인 | 계획 |
| v3 | 원격 보고(MQTT) → Prometheus / Grafana 대시보드 | 계획 |
| v4 | CI에서 녹화 재생 → 지표 자동 계산, 목표 초과 시 실패 | 계획 |