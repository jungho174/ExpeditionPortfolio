# 전투 State Machine

## 기본 규칙

- 현재 상태의 소유자는 전투 흐름 조정 객체 하나로 제한한다.
- 모든 전이는 명시적인 요청과 검증을 거치며 UI나 Animation Blueprint가 상태 값을 직접 변경하지 않는다.
- 입력은 상태별로 허용 목록을 사용한다. 허용되지 않은 입력은 상태를 변경하지 않는다.
- Action, Animation, Timer가 비동기로 끝날 때 중복 완료 신호가 발생해도 한 번만 전이하도록 보호한다.
- `Victory`와 `Defeat` 판정은 참가자의 생존 상태가 바뀔 수 있는 Action 종료 시점에 수행한다.

## 상태 정의

| 상태 | 진입 조건 | 허용 입력 | 종료 조건 | 가능한 다음 상태 |
|---|---|---|---|---|
| `Exploration` | 게임 시작 또는 `ReturnToField` 정리 완료 | 이동, 시점 조작, 필드 상호작용 | 유효한 몬스터 조우가 확정됨 | `BattleIntro` |
| `BattleIntro` | 조우가 승인되고 전투 참가자가 등록됨 | 기본적으로 없음, 필요 시 연출 Skip만 허용 | 전투 배치와 Intro 연출이 완료되고 참가자가 유효함 | `PlayerCommand`, 초기화 실패 시 `ReturnToField` |
| `PlayerCommand` | 플레이어 턴 시작, 플레이어가 행동 가능함 | 공격/스킬 선택, 대상 선택, 확정, 취소 | 유효한 명령이 확정됨 | `PlayerAction`, 선택 가능한 행동이 없으면 `TurnEnd` |
| `PlayerAction` | 검증된 플레이어 명령과 대상이 준비됨 | 기본적으로 없음 | 판정과 필수 Action 연출이 완료됨 | `TurnEnd` |
| `EnemyTelegraph` | 적 턴이 시작되고 실행할 패턴이 선택됨 | 방어 준비를 위한 시점 조작 등 최소 입력만 허용 | Telegraph 시간이 끝나고 패턴 실행 준비가 완료됨 | `EnemyAction` |
| `EnemyAction` | 선택된 적 패턴의 실행이 시작됨 | 패턴이 허용한 방어 입력만 수신하며 아직 판정하지 않을 수 있음 | 방어 가능 타격이 활성화되거나, 방어 불가 Action이 완료됨 | `DefenseWindow`, `TurnEnd` |
| `DefenseWindow` | 회피 또는 패링 판정 구간이 열림 | 회피, 패링 | 입력 판정과 공격 결과 적용이 끝나거나 제한 시간이 만료됨 | 추가 타격이 있으면 `EnemyAction`, 아니면 `TurnEnd` |
| `TurnEnd` | 플레이어 또는 적 Action의 결과 적용이 완료됨 | 없음 | 승패 판정과 다음 턴 결정이 완료됨 | `Victory`, `Defeat`, `PlayerCommand`, `EnemyTelegraph` |
| `Victory` | 모든 필수 적이 전투 불능임 | 결과 확인 또는 계속 | 보상 및 전투 종료 처리가 완료됨 | `ReturnToField` |
| `Defeat` | 플레이어가 전투 불능임 | 재시도 또는 종료 선택(정책 확정 전) | 선택한 패배 처리 정책이 확정됨 | `BattleIntro`, `ReturnToField` 또는 별도 실패 흐름(추후 확정) |
| `ReturnToField` | 승리 처리 완료, 패배 후 복귀 선택 또는 전투 초기화 실패 | 없음 | 전투 전용 객체와 입력을 정리하고 탐험 상태를 복원함 | `Exploration` |

## 기준 전이 흐름

정상적인 1대1 전투의 최소 흐름은 다음과 같다.

`Exploration -> BattleIntro -> PlayerCommand -> PlayerAction -> TurnEnd -> EnemyTelegraph -> EnemyAction -> DefenseWindow -> TurnEnd -> PlayerCommand`

각 `TurnEnd`에서 승리 또는 패배 조건이 만족되면 반복 대신 `Victory` 또는 `Defeat`로 전이한다. 승리 흐름은 `Victory -> ReturnToField -> Exploration`으로 종료한다.

현재 최소 전투 루프에서는 `RequestPlayerBasicAttack()`이 승인되면 플레이어와 적의 고정 Damage Action을 한 라운드 동안 동기식으로 처리한다. 적 Action은 아직 방어 입력을 허용하지 않으므로 `EnemyAction -> TurnEnd`로 종료하며, `DefenseWindow` 연결은 Milestone 3에서 추가한다.

상태 변경 Event 방송 중에는 새로운 전이와 명령을 거부한다. 따라서 Event 소비자는 상태를 관찰할 수 있지만 같은 호출 스택에서 전투 흐름을 다시 진행시킬 수 없다.

## 구현 시 검증할 예외

- Action 도중 참가자가 제거되거나 무효화된 경우
- Animation이나 Effect 완료 Event가 오지 않는 경우
- DefenseWindow 경계 프레임에 입력이 들어오는 경우
- 동일한 명령이나 완료 Event가 두 번 전달되는 경우
- 전투 종료 중 새로운 조우가 발생하는 경우
- 패배 후 재시도 정책과 Level 재설정 범위
