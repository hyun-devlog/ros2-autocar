# 로봇 전체 상태
# health_monitor가 /robot_status로 10 Hz 발행
# safety_gate가 자동으로 정지 판단에 쓰는 상태 (전용 토픽으로 따로 발행)
# QoS: reliable + transient_local, depth 1 (구독 쪽도 transient_local이어야 최신 값을 바로 받음)

# 등급
# 값은 diagnostic_msgs/DiagnosticStatus와 같음 (OK=0, WARN=1, ERROR=2)
uint8 LEVEL_OK=0                    # 모든 감시 항목 정상 -> 명령 통과
uint8 LEVEL_DEGRADED=1              # 동작 가능, 성능 저하 -> 감속
uint8 LEVEL_FAULT=2                 # 안전 동작 불가 -> 0 속도 (안전 상태)

std_msgs/Header header              # stamp = 판정 시각

uint8 level                         # 최종 등급, 나빠지면 즉시, 좋아지면 2.0초 유지 후 반영 (SR-06)
uint8 raw_level                     # 이번 판정 순간의 등급, 회복 대기 여부 확인용

robot_interfaces/FaultItem[] faults # 현재 원인 목록, raw_level이 OK면 비어 있음 