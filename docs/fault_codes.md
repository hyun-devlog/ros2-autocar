# 장애 코드 체계 (Fault Codes)

## 1. 형식: SAE J2012 DTC 형식 차용

자동차 진단 코드(DTC, Diagnostic Trouble Code) 표준인 SAE J2012의 코드 형식을 참고하여 프로젝트의 장애 코드 체계를 정의했습니다.

```
  U  1  0  02
  │  │  │  └─ 세부 장애 유형 (16진수 2자리, 8비트)
  │  │  └──── 하위 시스템 분류 (16진수 1자리, 4비트)
  │  └─────── 코드 정의 구분 (0: 표준 정의, 1: 제조사 정의, 2비트)
  └────────── 시스템 분류 (P: 파워트레인, C: 섀시, B: 차체, U: 네트워크, 2비트)
```

- **제조사 정의 코드 범위 사용**: 프로젝트에서 정의한 장애 코드와 표준 정의 코드 (예: P0300)를 구분하기 위해 두 번째 자리를 1로 고정했습니다.
- **숫자 기반 코드 구성**: 마지막 세 자리는 0~9 범위의 숫자로 구성하여 16진수 및 BCD 표기에서 동일한 숫자 표현을 유지하도록 설계했습니다.
- **16비트 데이터 구조**: 시스템 분류 및 장애 유형을 2바이트 (uint16)로 인코딩하여 FaultItem.code 필드에 저장합니다.

## 2. 시스템 분류 및 적용 기준

| 분류 코드 | SAE J2012 분류 | 프로젝트 적용 범위 |
|---|---|---|
| P | Powertrain | 전원 시스템 (저전압 및 향후 배터리 상태 진단) |
| C | Chassis | 주행 시스템 (향후 엔코더 및 조향 상태 진단) |
| B | Body | 상위 제어기 (Raspberry Pi의 온도, 스로틀링 및 시스템 리소스 상태) |
| U | Network / Communication | 통신 시스템 (ROS 2 토픽 수신 상태 및 RRC Lite Serial 통신 상태 진단) |

## 3. 장애 코드 정의

| DTC | 16비트 값 | 메시지 상수 | 의미 | 기본 등급 | 안전 요구사항 |
|---|---|---|---|---|---|
| U1001 | 0xD001 = 53249 | `TOPIC_NEVER_RECEIVED` | 초기화 유예 시간 내 토픽 수신 이력 없음 | 토픽별 설정 | SR-10 |
| U1002 | 0xD002 = 53250 | `TOPIC_STALE` | 토픽 수신 중단 시간이 설정된 기준값 이상 | 토픽별 설정 | SR-01, SR-02 |
| U1003 | 0xD003 = 53251 | `TOPIC_RATE_LOW` | 토픽 수신율이 설정된 최소 수신율 (`watch.min_hz`) 미만 | DEGRADED | SR-03 |
| B1001 | 0x9001 = 36865 | `CPU_TEMP_HIGH` | SoC 80 °C 이상 (스로틀링 구간) | DEGRADED | SR-07 |
| B1002 | 0x9002 = 36866 | `CPU_TEMP_CRITICAL` | SoC 85 °C 이상 (온도 한계) | FAULT | SR-07 |
| B1101 | 0x9101 = 37121 | `THROTTLED` | 펌웨어 상태 플래그에서 현재 스로틀링 상태 확인 (bit 2) | DEGRADED | SR-08 |
| B1201 | 0x9201 = 37377 | `CPU_LOAD_HIGH` | CPU 사용률이 기준값 이상 (기준값 실측 전까지 비활성) | DEGRADED | SR-11 |
| B1202 | 0x9202 = 37378 | `MEM_LOW` | 가용 메모리 비율이 기준값 이하 (기준값 실측 전까지 비활성) | DEGRADED | SR-12 |
| P1001 | 0x1001 = 4097 | `UNDER_VOLTAGE` | 펌웨어 상태 플래그에서 현재 저전압 상태 확인 (bit 0) | DEGRADED | SR-08 |

B1201과 B1202는 판정 기준값에 대한 객관적인 근거가 확보되지 않은 상태이므로, 실제 시스템의 리소스 사용량을 측정하여 기준값을 설정하기 전까지 장애 판정 기능을 비활성화합니다.

관련 안전 요구사항: `docs/safety.md` (SR-11, SR-12)

### 확장 예정 코드 범위

| 코드 범위 | 적용 예정 항목 |
|---|---|
| P11xx | 배터리 상태 진단 |
| U11xx | RRC Lite 통신 상태 진단 | 
| C1xxx | 주행 시스템 상태 진단 | 


## 4. 16비트 장애 코드 인코딩

`U1002`의 16비트 인코딩 과정은 다음과 같습니다.

| 구성 요소 | 분류 및 의미 | 비트 값 |
|---|---|---|
| U | 시스템 분류 | 11 |
| 1 | 제조사 정의 코드 | 01 |
| 0 | 하위 시스템 분류 | 0000 |
| 02 | 세부 장애 유형 | 0000 0010 |

```
DTC     : U1002
Binary  : 1101 0000 0000 0010
Hex     : 0xD002
Decimal : 53250
```

## 5. 상태 등급 및 ROS 2 진단 메시지 대응

프로젝트의 상태 등급은 ROS 2의 diagnostic_msgs/DiagnosticStatus에 정의된 상태 등급과 동일한 수치로 구성했습니다.

| 프로젝트 상태 등급 | 값 | ROS 2 DiagnosticStatus |
|---|---|---|
| OK | 0 | OK = 0 |
| DEGRADED | 1 | WARN = 1 |
| FAULT | 2 | ERROR = 2 |

## 6. 참고 자료

- SAE J2012 - Diagnostic Trouble Code Definitions: https://saemobilus.sae.org/standards/j2012_201612-diagnostic-trouble-code-definitions
- UDS Documentation - Diagnostic Trouble Code (DTC): https://uds.readthedocs.io/en/latest/pages/knowledge_base/dtc.html
- 판정 기준값 및 설정 근거: `docs/safety.md` 4절
