# 전투 State Machine

현재는 `TurnEnd` 뒤 다음 생존 참가자를 선택한다. 다음 참가자가 아군이면 `PlayerCommand`, 적이면 `EnemyTelegraph`로 진행하며 라운드 끝에서 처음으로 돌아간다. 패배는 모든 행동 아군의 전투불능으로 판정한다. 명령 대기 중 현재 아군의 전투불능과 예고 중 참가자 전투불능에는 검증된 `TurnEnd` 취소 경로를 허용한다. 최신 명세는 [다수 적 조우](grouped-encounters.md)를 참조한다.

## 승패 결과 처리

치명타에서는 행동 완료를 기다리지 않고 Victory/Defeat에 진입한다. 약 3초의 실시간 슬로모션 뒤 World Pause와 결과창을 유지한다. 승리는 계속, 패배는 재도전/후퇴 명령에서만 전투 컨텍스트를 정리한다. 재도전과 후퇴는 진입 HP를 복원하며 후퇴만 조우 영역 밖으로 이동한다. 설정·통계·세션 HP 유지는 `battle-results.md`를 참조한다. 아래 초기 전이 설명의 자동 필드 복귀는 이 결과 확인 단계로 대체한다.

## 기본 규칙

플레이어 공격·물약 또는 적 공격(패링 반격 포함)이 완전히 끝나면 TurnEnd에서 기본 1초 대기한 뒤 상대 턴으로 넘어간다. Combat/Commands/DA_PlayerCommands의 Turn Transition Delay(0~5초)를 전투 시작 시 복사한다. 0은 즉시 전환이다. 대기 중 명령·방어 입력은 허용하지 않으며, 다음 턴 시작음은 대기 종료 후 실제 턴 진입에만 재생한다. 승리·패배는 상대 턴을 열지 않고 기존처럼 즉시 판정한다. 예약은 전투 세션과 참가자를 재확인하며 전투 정리 시 취소한다.

턴 대기 변경 검증: Editor Development 빌드 및 전투 테스트 48개 통과. 기존 타이밍 테스트는 테스트 전용 접근으로 대기를 0으로 분리하고 신규 테스트는 실제 1초 대기로 양방향 전환·입력 차단·참가자 제거·즉시 승리를 검사했다. PIE에서 물약·약한 마법·강한 마법·평타를 실행하여 양방향 총 8개 대기가 약 1.00~1.02초임을 측정했고 HP만 보이는 TurnEnd 화면을 확인했다. 네 행동 모두 대상 선택 진입과 확정에 지정된 선택음만 각 1회 재생함을 로그로 확인했다. 사용자 직접 플레이로 체감과 청음을 확인하는 단계는 남아 있다.

플레이어 명령은 약한 마법(`BasicAttack`), 강한 마법(`PowerStrike`), 근접 평타(`MeleeAttack`), 회복 물약(`UsePotion`)이다. 첫 메뉴/기술/아이템 사이의 이동은 `PlayerCommand`를 유지한다. 유효한 행동 확정만 `PlayerAction`으로 전이한다. 물약도 연출과 회복 후 턴을 소비하며 품절·중복 입력을 거부한다. 풀피에서도 효과 없이 물약 1개와 턴을 소비할 수 있다. 수량은 전투마다 초기화한다.

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
| `DefenseWindow` | 회피 또는 패링 입력 구간이 열림 | 타격 전 첫 회피/패링만, 이후 입력 거부 | 타격 판정 후 적 모션 완료 또는 취소 | 현재 단일 타격은 `TurnEnd` (`ReturnToField`는 안전 정리 경로) |
| `TurnEnd` | 플레이어 또는 적 Action의 결과 적용이 완료됨 | 없음 | 승패 판정과 다음 턴 결정이 완료됨 | `Victory`, `Defeat`, `PlayerCommand`, `EnemyTelegraph` |
| `Victory` | 모든 필수 적이 전투 불능임 | 결과 확인 또는 계속 | 보상 및 전투 종료 처리가 완료됨 | `ReturnToField` |
| `Defeat` | 플레이어가 전투 불능임 | 재시도 또는 종료 선택(정책 확정 전) | 선택한 패배 처리 정책이 확정됨 | `BattleIntro`, `ReturnToField` 또는 별도 실패 흐름(추후 확정) |
| `ReturnToField` | 승리 처리 완료, 패배 후 복귀 선택 또는 전투 초기화 실패 | 없음 | 전투 전용 객체와 입력을 정리하고 탐험 상태를 복원함 | `Exploration` |

## 기준 전이 흐름

공격 메뉴의 대상 선택은 `PlayerCommand` 내부의 보류 명령이다. `BeginActionSelection` → A/D 선택 또는 취소 → `ConfirmTargetSelection` 순서로 진행하며 확정 전에는 상태·피해·사용 횟수를 변경하지 않는다. 물약은 생존 아군을 선택한다. UI와 카메라는 `OnTargetSelectionChanged`를 관찰한다. 후보는 현재 조우에 등록된 생존 참가자이며 확정은 Space다. 상세 설정은 `docs/target-selection.md`를 참조한다.

정상적인 1대1 전투의 최소 흐름은 다음과 같다.

`Exploration -> BattleIntro -> PlayerCommand -> PlayerAction -> TurnEnd -> EnemyTelegraph -> EnemyAction -> DefenseWindow -> TurnEnd -> PlayerCommand`

각 `TurnEnd`에서 승리 또는 패배 조건이 만족되면 반복 대신 `Victory` 또는 `Defeat`로 전이한다. 승리 흐름은 `Victory -> ReturnToField -> Exploration`으로 종료한다.

현재 최소 전투 루프에서는 `RequestPlayerCommand()`가 `BasicAttack` 또는 `PowerStrike`를 검증한다. `PowerStrike`는 조우마다 한 번만 사용할 수 있다. 플레이어 Action 후 적이 생존하면 패턴을 선택하고 `EnemyTelegraph`에서 기본 1초를 기다린다. `EnemyAction`에서 적 Montage를 시작하고, 보정된 타격 0.6초 전에 `DefenseWindow`를 연다. 기본 모션은 AS_Spell_Cast_Attack이며 타격은 팔을 앞으로 뻗은 직후인 약 4.7초, 모션 길이는 약 6.8초다.

플레이어 캐릭터의 마법 연출이 연결된 경우 `PlayerAction`은 Montage 종료까지 유지된다. 약한 마법은 Notify에서 왼손 발사체를 생성하고 적 몸통에 도착할 때 피해와 폭발을 한 번 적용한다. 발사체 비행과 모션이 모두 완료되어야 `TurnEnd`에서 승패를 판정한다. 강한 마법은 기존 타격 Notify에서 피해를 적용하며 준비는 2.5초다. 준비 중에는 피해가 없다. 정상 완료 시 Notify 누락은 발사형이면 종료 시점의 발사로, 그 외 마법이면 1회 피해로 대체한다. 중단/안전 Timer 만료는 미적용 피해를 취소하며, 발사형 안전 Timer에는 최대 비행 시간도 포함한다. 피해 적용 후 적이 쓰러져도 행동 완료까지 명령 UI는 숨겨진다.

`DefenseWindow`에서 첫 입력을 보관한다. 타격까지 남은 시간이 패링 성공 시간 이하이면 `ParrySuccess`, 너무 이른 패링이면 `Failed`, 회피는 `DodgeSuccess`다. 중복 입력과 타격 이후 입력은 거부한다. 실제 타격 시점에 결과를 발행하고 피격은 기본 20을 한 번 적용한다. 패링 성공은 반격 연출을 시작하고 반격 발사체 명중 시 기본 15를 적용한다. 입력이 없으면 `NoInput`이다. 타격 이후에도 적 모션이 끝날 때까지 상태는 `DefenseWindow`에 머물지만 방어 입력은 받을 수 없다. 모션 완료 후 `TurnEnd`에서 승패와 다음 턴을 결정한다.

`RequestDefenseCommand()`는 방어 상태, 남은 타격 시간, 참가자와 입력 제출 여부를 검증한다. 입력은 타격 Timer를 취소하지 않는다. 타격 결과 Event의 재진입은 Action Guard로 차단한다. Notify는 메시/모션 인스턴스/행동 ID 검증을 거치며 보정값이 있으면 복사한 예약 시각을 우선한다. 누락 Notify는 타격 Timer로 대체한다. 중단·완료 시간 초과 시 미적용 타격은 취소한다. 참가자가 무효화되면 남은 흐름을 정리하고 `ReturnToField -> Exploration`으로 복귀한다.

위 시간과 적 피해량은 기본 패턴의 값이다. 유효한 후보를 가중치로 선택하며 `EnemyTelegraph` 진입 통지 전에 값을 복사한다. PIE 중 원본 Data Asset의 방어 시간·패링 시간·HitTimeOffset을 바꾸면 다음 선택부터 반영된다. 진행 중 공격은 기존 값을 유지한다. 패턴 누락/무효/전체 가중치 0은 경고 후 애니메이션 없는 기본 Timer 공격으로 대체한다. 이 경우에도 타격 직전 패링 규칙과 타격 시 피해 적용은 동일하다. 행동 완료와 전투 정리는 선택 정보도 해제한다.

상태 변경 Event 방송 중에는 새로운 전이와 명령을 거부한다. 따라서 Event 소비자는 상태를 관찰할 수 있지만 같은 호출 스택에서 전투 흐름을 다시 진행시킬 수 없다.

탐험 Mapping Context는 전투 전체에서 비활성화되고, 전투 명령 Mapping Context는 `PlayerCommand`에서만, 방어 Mapping Context는 `DefenseWindow`에서만 활성화된다. UI Button과 Enhanced Input Action은 상태를 직접 변경하지 않고 동일한 명령 API를 호출한다.

방어 명령 승인 시에는 별도의 native `OnDefenseCommandAccepted(Command, TimeUntilHit)` 이벤트로 연출을 시작한다. 방어 결과와 피해 적용은 기존 타격 시점에 남겨 둔다. 플레이어 방어 연출은 이 이벤트와 결과/상태 이벤트를 구독할 뿐 상태를 전이하거나 피해를 적용하지 않는다. 회피의 메시 이동 역시 방어 성공 여부나 전투 참가자의 실제 위치에 영향을 주지 않는다.

## 구현 시 검증할 예외

패링 성공 시 유효한 Counter Magic이 있으면 `DefenseWindow` 내부에서 반격 행동을 시작한다. 반격 발사체가 적에게 도착할 때 피해를 한 번 적용하고 적 공격 모션을 정지한 뒤 피격 모션을 재생한다. 적 모션이 먼저 끝나도 반격 완료 전에는 `TurnEnd`로 전환하지 않는다. 반격 중단/시간 초과는 미적용 피해를 취소하고 적 모션이 남아 있으면 그 완료를 기다린다. 전투 정리 시 반격 Timer·Montage·활성 VFX도 정리한다. 설정 누락/재생 실패는 기존 즉시 15 피해 경로를 유지한다. 세부 설정은 `docs/player-defense-presentation.md`를 참조한다.

반격 행동의 첫 구간은 성공 섬광과 슬로모션이다. 초기 설정은 실제 0.15초 동안 이전 게임 속도의 0.15배이며 이 구간에는 반격을 발사하지 않는다. 실시간 기준으로 이전 속도를 복원한 뒤 반격을 시작한다. 상태는 계속 DefenseWindow이며 성공 판정·추가 입력 차단·명중 시 피해 규칙은 동일하다. 중단과 전투 종료도 원래 속도를 복원한다.

강한 마법의 `PlayerAction` 내부에는 Preparing/Executing 연출 단계가 있다. Preparing의 2.5초 동안 타이밍 입력을 한 번 받고 마지막 0.3초이면 성공한다. 준비는 입력 결과와 관계없이 끝까지 진행하며, Executing의 타격에서 기본 40 또는 성공 60을 적용한다. 실패·미입력은 공격을 취소하지 않는다. 중단과 시간 초과는 열려 있는 시간창과 미적용 피해를 취소한다. UI가 새 전투 상태를 만들거나 판정을 소유하지 않는다.

- Action 도중 참가자가 제거되거나 무효화된 경우
- Animation이나 Effect 완료 Event가 오지 않는 경우
- DefenseWindow 경계 프레임에 입력이 들어오는 경우
- 동일한 명령이나 완료 Event가 두 번 전달되는 경우
- 전투 종료 중 새로운 조우가 발생하는 경우
- 패배 후 재시도 정책과 Level 재설정 범위
