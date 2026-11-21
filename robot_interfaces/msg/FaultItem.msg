# 코드는 SAE J2012 DTC 형식

# U: 통신 (ROS 토픽) 
uint16 TOPIC_NEVER_RECEIVED=53249   # U1001 시작 후 한 번도 안 옴 (SR-10)
uint16 TOPIC_STALE=53250            # U1002 수신 끊김 (SR-01, SR-02)
uint16 TOPIC_RATE_LOW=53251         # U1003 주기 < 기준 x 0.9 (SR-03)

# B: 탑재 컴퓨터 (Raspberry Pi) 
uint16 CPU_TEMP_HIGH=36865          # B1001 SoC 80도 이상 (SR-07)
uint16 CPU_TEMP_CRITICAL=36866      # B1002 SoC 85도 이상 (SR-07)
uint16 THROTTLED=37121              # B1101 펌웨어 스로틀링 bit 2 (SR-08)
uint16 CPU_LOAD_HIGH=37377          # B1201 CPU 사용률 >= 실측 기준 (SR-11)
uint16 MEM_LOW=37378                # B1202 가용 메모리 <= 실측 기준 (SR-12) 

# P: 전원
uint16 UNDER_VOLTAGE=4097           # P1001 펌웨어 저전압 bit 0 (SR-08)

uint16 code                         # 위 상수 중 하나
uint8 severity                      # RobotStatus.LEVEL_*
string source                       # 원인 위치, e.g. "/scan_raw", "cpu_temp"
string message                      # 사람이 읽는 설명, 앞에 "U1002" 같은 DTC 문자열을 붙임
float64 value                       # 측정 값, 해당 없으면 NaN
float64 threshold                   # 넘어선 기준 값, 해당 없으면 NaN