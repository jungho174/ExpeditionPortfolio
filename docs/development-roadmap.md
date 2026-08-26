# 개발 로드맵

## 진행 원칙

- 각 마일스톤은 1~2주 단위로 완료 가능한 플레이 가능한 결과를 만든다.
- 1차 목표는 플레이어 1명과 몬스터 1종으로 전투 진입부터 종료까지 도는 최소 전투 루프다.
- 기능마다 C++ 핵심 로직, Blueprint 연결 지점, 검증 방법을 함께 정의한다.
- 다음 단계로 넘어가기 전에 `ExpeditionPortfolioEditor Win64 Development` 빌드와 해당 단계의 테스트를 통과한다.

## Milestone 1 — 최소 전투 골격 (1~2주)

목표: 플레이어 1명과 몬스터 1종이 번갈아 행동하고 승패가 결정되는 최소 전투 루프를 만든다.

- 전투 상태 enum과 전이 규칙 구현
- 전투 참가자 등록 및 최소 HP/피해 모델 구현
- 필드 조우에서 `BattleIntro` 진입
- 플레이어 기본 공격 1개와 적 기본 공격 1개
- `TurnEnd`, `Victory`, `Defeat`, `ReturnToField` 연결
- 개발용 로그 또는 Debug UI로 현재 상태 확인
- 정상 흐름과 잘못된 상태 전이 테스트

완료 기준: 한 번의 조우에서 양측이 턴을 교대하고, 승리 후 필드로 복귀할 수 있다.

## Milestone 2 — 플레이어 명령과 최소 UI (1주)

목표: 플레이어가 UI에서 공격 또는 스킬을 선택하고 전투 로직이 명령을 검증하도록 한다.

- `PlayerCommand` 입력 Context 분리
- 공격 및 대표 스킬 1개 선택 UI
- UI 명령 전달과 상태 Event 구독
- HP, 현재 턴, 선택 가능 Action 표시
- 잘못된 입력과 중복 확정 방지

완료 기준: UI가 상태를 소유하지 않은 채 명령 선택과 전투 정보 표시를 수행한다.

현재 C++ 기반은 `BasicAttack`, 전투당 1회의 `PowerStrike`, 상태별 Enhanced Input Context 전환, Blueprint용 상태/HP Event와 `UCombatHUDWidget` 연결 지점까지 구현됐다. Milestone 2의 플레이 가능한 완료에는 Input Action/Mapping Context와 `WBP_CombatHUD` 에셋 조립 및 PIE 수동 검증이 남아 있다.

## Milestone 3 — 패링과 회피 (1~2주)

목표: 적 턴에 Telegraph를 읽고 실시간 방어 입력으로 결과를 바꿀 수 있게 한다.

- `EnemyTelegraph`, `EnemyAction`, `DefenseWindow` 연결
- 패링과 회피 입력 및 판정 창 구현
- 성공, 실패, 미입력 결과 적용
- 경계 시간과 중복 입력 테스트
- 판정 결과를 확인할 Debug 표시

완료 기준: 같은 적 공격에 대해 패링, 회피, 피격 결과가 재현 가능하게 구분된다.

현재 C++ 기반은 Timer 기반 `EnemyTelegraph -> EnemyAction -> DefenseWindow` 흐름, 패링/회피/실패/미입력 판정, 패링 반격, 상태별 방어 입력 Context 전환, HUD용 방어 명령과 결과 Event까지 구현됐다. 경계 시간, 무입력 패배, 패링 반격 승리, 중복·재진입 입력, Timer 도중 참가자 무효화도 자동화 테스트로 검증한다. Milestone 3의 플레이 가능한 완료에는 `IA_Parry`, `IA_Dodge`, `IMC_DefenseWindow` 에셋 조립, PlayerController 지정, `WBP_CombatHUD` 방어 버튼/결과 표시 추가와 PIE 수동 검증이 남아 있다.

## Milestone 4 — 몬스터 패턴 데이터화 (1~2주)

목표: 코드 수정 없이 적 공격의 시간과 수치를 교체할 수 있게 한다.

- 스킬 및 몬스터 패턴 Data Asset 구조 확정
- Telegraph, DefenseWindow, 피해량, 타격 횟수 데이터화
- 가중치 기반 패턴 선택의 최소 구현
- 잘못된 데이터에 대한 검증과 안전한 실패 처리
- 테스트용 패턴 1~2개 추가

완료 기준: 서로 다른 패턴 데이터를 같은 실행 흐름에서 사용할 수 있다.

## Milestone 5 — 전투 연출 연결 (1~2주)

목표: 확정된 판정 로직에 Animation, 카메라, VFX, SFX 피드백을 연결한다.

- 플레이어와 적 Animation Montage 연결
- Anim Notify와 판정 시점 동기화
- Telegraph 및 패링/회피 성공 피드백
- 전투 카메라와 최소 화면 전환
- 연출 Event 누락 시에도 전투가 멈추지 않는 대체 경로

완료 기준: 전투 결과를 시각·청각적으로 명확히 이해할 수 있고 연출과 판정이 일치한다.

## Milestone 6 — Vertical Slice 정리 (1~2주)

목표: 짧지만 처음부터 끝까지 안정적으로 시연 가능한 포트폴리오 빌드를 만든다.

- 탐험 동선과 조우 배치 정리
- 난이도 및 타이밍 조정
- 상태 전이, 전투 재시도, 반복 조우 회귀 테스트
- 주요 시스템 Debug 정보와 개발 문서 정리
- 성능 확인 및 불필요한 `Tick` 제거
- Development 빌드와 패키징 후보 빌드 검증

완료 기준: 새 실행에서 탐험, 조우, 명령 전투, 실시간 방어, 승리, 필드 복귀를 중단 없이 시연할 수 있다.
