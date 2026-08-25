# 아키텍처 방향

## 현재 상태와 설계 원칙

현재 프로젝트에는 Third Person 템플릿의 `AExpeditionPortfolioCharacter`, `AExpeditionPortfolioPlayerController`, `AExpeditionPortfolioGameMode`와 여러 템플릿 Variant가 있다. 전투 상태를 소유하는 `UBattleCoordinator`는 `UWorldSubsystem`으로 확정됐고, `ABattleEncounterTrigger`가 필드 조우 감지를 담당한다. `UCombatantComponent`는 최소 HP, Damage와 전투 불능 상태를 담당하며, 스킬 데이터 클래스는 아직 확정되거나 구현되지 않았다.

데이터 클래스 이름은 책임을 설명하기 위한 후보명이다. 실제 클래스명, 상속 구조와 배치 위치는 구현 전 작은 실험과 테스트를 거쳐 확정한다.

## Exploration과 Combat 영역 분리

Exploration은 캐릭터 이동, 카메라, 필드 상호작용, 조우 감지를 담당한다. Combat은 참가자 등록, 턴 순서, 명령 실행, 피해와 방어 판정, 승패 판정을 담당한다. 두 영역은 서로의 내부 구현을 직접 조작하지 않고 전투 시작 요청과 전투 종료 결과처럼 좁은 인터페이스로 연결한다.

- Exploration 입력은 전투 진입 시 비활성화하고 필드 복귀 시 복원한다.
- Combat 입력은 현재 전투 상태가 허용하는 Action만 활성화한다.
- 맵 전환, 같은 맵 안의 전투 공간 이동, 별도 전투 Level 사용 여부는 아직 확정하지 않는다.
- 전투 중 필드 Actor를 숨기거나 정지시키는 정책도 Vertical Slice의 전환 방식이 결정된 뒤 확정한다.

`AExpeditionPortfolioPlayerController`는 상태 변경 Event를 구독하고 `Exploration`을 벗어날 때 탐험용 Enhanced Input Mapping Context와 이동/시점 입력을 비활성화한다. 전투 중에는 Game and UI 입력 모드와 Mouse Cursor를 사용하며, `PlayerCommand`에서만 전투 명령 Mapping Context를 활성화한다. `ReturnToField -> Exploration`이 완료되면 탐험 입력과 Game Only 입력 모드를 복원한다.

## BattleCoordinator의 책임

`UBattleCoordinator`는 `UWorldSubsystem`으로서 Game/PIE World마다 하나씩 생성되고 전투 흐름을 조정한다. 이 객체의 책임은 다음 범위로 제한한다.

- 전투 시작 요청을 검증하고 참가자를 등록한다.
- 현재 전투 상태와 턴 주체를 소유한다.
- 허용된 상태 전이만 수행하고 전이 Event를 발행한다.
- 플레이어 명령을 검증한 뒤 Action 실행을 요청한다.
- 적 패턴 선택과 Telegraph/DefenseWindow의 진행을 조정한다.
- 행동 종료 시 생존 상태를 확인하고 Victory 또는 Defeat를 판정한다.
- 전투 종료 시 임시 상태를 정리하고 Exploration 복귀를 알린다.

현재 1대1 Vertical Slice에서는 `RequestBattleStart()`가 조우 출처, Player Pawn, Enemy Actor의 유효성, 양쪽 `UCombatantComponent` 존재 여부와 전투 가능 상태를 검증한다. 승인된 Actor와 Component는 Coordinator가 생명주기를 소유하지 않도록 `TWeakObjectPtr`로 보관한다.

`BattleIntro`에 비동기 배치나 연출이 아직 없으므로 승인 직후 `PlayerCommand`까지 전환한다. Intro 연출이 구현되면 이 임시 즉시 전환을 연출 완료 Event 기반 전환으로 교체한다.

외부 호출자는 상태를 직접 전이하지 않고 `RequestBattleStart()`와 `RequestPlayerCommand()` 같은 명령 API를 사용한다. `EBattlePlayerCommand`는 현재 `BasicAttack`과 `PowerStrike`를 제공하며, Coordinator가 상태와 사용 가능 여부를 검증한 뒤 Action을 실행한다. 실제 상태 전이 함수는 Coordinator 내부에 있으며, 상태 Event 방송 중 재진입과 Action 실행 중 중복 명령은 거부한다.

현재 명령은 데이터 구조가 확정되기 전의 프로토타입으로 `BasicAttack`은 25 Damage, 전투당 한 번 사용할 수 있는 `PowerStrike`는 40 Damage를 사용한다. 적 기본 공격은 20 Damage다. 플레이어 Action이 끝나면 `TurnEnd`에서 승패를 확인하고, 적이 생존하면 `EnemyTelegraph -> EnemyAction -> TurnEnd`를 동기식으로 진행한다. 방어 입력이 없는 임시 적 Action이므로 `DefenseWindow`는 사용하지 않는다.

적이 전투 불능이면 `Victory -> ReturnToField -> Exploration`으로 전환하면서 조우와 참가자 참조를 정리한다. 플레이어가 전투 불능이면 후속 정책이 결정될 때까지 `Defeat`에 머물며 활성 전투 Context를 유지한다.

이 객체는 Widget의 표시 방식, Animation 세부 연출, 개별 캐릭터의 이동 구현을 소유하지 않는다.

## 필드 조우 감지

`ABattleEncounterTrigger`는 `UBoxComponent`의 Overlap Event로 로컬 플레이어 Pawn을 감지한다. Level에 배치된 각 Trigger는 `EditInstanceOnly`인 `EnemyActor` 참조로 해당 조우의 적을 명시한다. Trigger는 Player와 Enemy를 `RequestBattleStart()`에 전달해 조우 의도만 알리며 `TryTransitionTo()`를 직접 호출하지 않는다. 요청이 성공한 경우에만 Collision을 비활성화해 같은 조우가 중복 실행되지 않도록 한다. Trigger는 전투 상태, 참가자 데이터, 전투 연출을 소유하지 않는다.

## CombatantComponent의 책임

`UCombatantComponent`는 전투에 참여할 Actor가 공통 전투 기능을 제공하도록 하는 `UActorComponent`다. 현재는 다음 최소 책임을 구현한다.

- Max HP, Current HP와 생존 여부 관리
- 양수 Damage 적용과 HP의 0 하한 처리
- HP 변경과 전투 불능 Delegate 발행

회복, 방어 결과, Action/Skill 목록, 팀 또는 진영 정보는 필요한 단계에서 확장한다. 턴 전체 흐름, UI Widget 참조, 카메라 연출은 Component에 넣지 않는다. 스탯 확장과 상태 이상은 Vertical Slice 이후 필요성이 확인될 때 추가한다.

## UI와 게임 로직의 분리

UI는 전투 상태의 소비자이자 사용자 명령의 입력 경계다.

- Widget은 Coordinator나 View Model 성격의 중간 객체가 발행한 상태 변경 Event를 구독한다.
- Widget은 공격/스킬 선택 의도를 명령 형태로 전달하고, 유효성 검사와 실제 상태 전이는 게임 로직이 수행한다.
- HP, 현재 턴, 선택 가능 Action은 게임 로직의 데이터를 표시하며 Widget 내부에 별도 원본 상태를 만들지 않는다.
- Widget Animation 종료가 핵심 전투 로직의 유일한 진행 조건이 되지 않도록 완료 신호와 실패 시 대체 경로를 둔다.

`UCombatHUDWidget`은 정식 전투 HUD Blueprint의 C++ 기반 클래스다. Coordinator와 양쪽 `UCombatantComponent`의 native Event를 구독하고 `Battle View Updated` Blueprint Event로 현재 상태, HP와 명령 사용 가능 여부를 전달한다. Widget의 버튼과 PlayerController의 Enhanced Input은 모두 `RequestPlayerCommand()`로 같은 명령 경계를 사용한다. Widget은 표시를 위한 별도 원본 상태를 만들지 않으며 `Exploration`에서는 자동으로 숨겨진다.

에디터에서는 다음 에셋을 조립한다.

- `IA_BasicAttack`, `IA_PowerStrike` Input Action
- 위 Action을 키 또는 Gamepad 입력에 연결한 `IMC_PlayerCommand`
- `UCombatHUDWidget`을 부모로 하는 `WBP_CombatHUD`
- PlayerController Blueprint의 `Player Command Mapping Context`, 두 Input Action, `Combat HUD Widget Class` 지정

`WBP_CombatHUD`는 `Battle View Updated` Event의 값으로 HP, 상태 Text와 버튼 활성화를 갱신한다. 기본 공격 버튼은 `Request Basic Attack`, 대표 스킬 버튼은 `Request Power Strike`를 호출한다.

## Data Asset 기반 스킬과 몬스터 패턴

스킬과 적 패턴의 정적 정의는 `UPrimaryDataAsset` 또는 `UDataAsset` 기반으로 분리하는 방향을 우선 검토한다. 데이터 클래스의 최종 이름과 필드는 구현 전 확정하지 않는다.

스킬 데이터 후보 항목:

- 표시 이름, 설명, 아이콘
- 비용, 기본 위력, 대상 규칙
- 실행 Animation/VFX/SFX 참조
- 실행 정책 또는 C++ Action 클래스 참조

몬스터 패턴 데이터 후보 항목:

- 패턴 식별자와 선택 가중치
- Telegraph 시간과 DefenseWindow 구간
- 공격 횟수 및 각 타격 타이밍
- 피해량과 회피/패링 허용 정책
- Animation/VFX/SFX 참조

Data Asset은 설정값과 콘텐츠 참조를 제공하고, 판정 공식과 상태 전이 규칙은 C++에 둔다. 디자이너가 Blueprint로 확장할 지점은 열어 두되 Blueprint가 전투 상태를 독자적으로 소유하지 않게 한다.

## 의존성 방향

의도한 의존성은 다음과 같다.

`Input/UI -> 전투 명령 인터페이스 -> 전투 흐름 조정 객체 -> Combatant/Action 로직 -> Event -> UI/연출`

UI와 연출 계층은 Event를 관찰하고, 핵심 로직은 구체적인 Widget이나 효과 에셋을 직접 알지 않는 방향을 유지한다.
