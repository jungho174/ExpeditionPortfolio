# 아키텍처 방향

현재 조우는 `BattleEncounterGroup`과 구성 DA를 사용하며 Coordinator의 참가자 목록으로 아군 전원 → 적 전원 순서의 턴을 진행한다. 현재 행동자·방어 대상·탐험 조작 Pawn을 구분한다. HP HUD와 승패·복원도 전체 목록을 사용한다. 최신 구조와 편집 위치는 [다수 적 조우와 파티 턴](grouped-encounters.md)을 참조한다. 아래 1대1 설명은 초기 구현 기록이다.

맵 조명은 `EnvironmentLightingProfile`과 `EnvironmentLightingRig`, VFX 발광 보정은 공통 Material Function이 담당한다. 캐릭터·전투 규칙과 분리하며 현재 적용과 템플릿·검증은 [공통 조명과 VFX](rendering-and-vfx-exposure.md)를 참조한다.

공통 규칙과 콘텐츠의 현재 경계는 [소유권 가이드](shared-combat-ownership.md)를 참조한다. 공통 전투 규칙과 파티 아이템 수량은 `BattleRulesData`, 아이템 정의는 `CombatItemData`, 캐릭터의 사용 동작은 `CharacterItemUseData`가 소유한다. 대상의 뼈와 위치는 `CombatantVisualData`, 행동별 구도는 `ActionCameraData`로 분리한다. 아군은 프로필의 단일 Character ID로 HP를 저장하고 중복 생성을 거부하며, 동일 종족의 적들은 임시 개체로 독립 HP를 갖는다.

적 패턴은 단일 타격 호환 경로와 `Hits` 배열의 다단 타격을 지원한다. Coordinator가 행동 ID와 타격 번호를 검증하고 각 타격의 방어 창을 진행한다. 타격 사이에는 `DefenseWindow -> EnemyAction -> DefenseWindow`로 전환하며 전체 패턴과 복귀가 끝나야 턴을 마친다. 평타·기술·전 타격 패링 반격은 `UCharacterCombatData` 프로필에서 선택하고 `CharacterActionExecutor`에 위임한다. 공통 패링 피드백은 `UParryFeedbackData`로 분리한다. 불마법은 현재 마법사 캐릭터의 행동 DA에만 연결한다. 상세 설정은 `docs/character-actions.md`를 참조한다. 설정은 `docs/stone-golem.md`를 참조한다.

승패 결과·실시간 슬로모션·World Pause, 재도전/후퇴와 세션 HP 보관은 `docs/battle-results.md`를 참조한다. 치명타는 행동 완료를 기다리지 않고 결과 상태를 확정하며, 결과창을 닫기 전에는 카메라·참가자 컨텍스트를 유지한다. PartyHealthSubsystem은 GameInstance 수명 동안 캐릭터 ID별 HP를 보관한다.

공통 공격 QTE는 `UActionQTESubsystem`이 세션·입력·타이머·판정을 소유하고, `UQTEPresentationComponent`와 `UAttackTimingWidget`이 공통 연출을 관찰한다. `BattleCoordinator`는 현재 행동에 세션을 연결하여 유효성을 검사하고 타격에 배율을 적용한다. 기술별 시간·판정 설정과 공통 사운드 DA를 분리하며 상세 API·에디터 설정·검증은 `docs/common-qte.md`를 참조한다.

## 현재 상태와 설계 원칙

현재 프로젝트에는 Third Person 템플릿의 `AExpeditionPortfolioCharacter`, `AExpeditionPortfolioPlayerController`, `AExpeditionPortfolioGameMode`와 여러 템플릿 Variant가 있다. 전투 상태를 소유하는 `UBattleCoordinator`는 `UWorldSubsystem`으로 확정됐고, `ABattleEncounterTrigger`가 필드 조우 감지를 담당한다. `UCombatantComponent`는 최소 HP, Damage와 전투 불능 상태를 담당하며, 캐릭터별 행동은 `UCharacterActionData`를 기반으로 마법·근접 연출 DA와 프로필을 구현했다.

데이터 클래스 이름은 책임을 설명하기 위한 후보명이다. 실제 클래스명, 상속 구조와 배치 위치는 구현 전 작은 실험과 테스트를 거쳐 확정한다.

## Exploration과 Combat 영역 분리

Exploration은 캐릭터 이동, 카메라, 필드 상호작용, 조우 감지를 담당한다. Combat은 참가자 등록, 턴 순서, 명령 실행, 피해와 방어 판정, 승패 판정을 담당한다. 두 영역은 서로의 내부 구현을 직접 조작하지 않고 전투 시작 요청과 전투 종료 결과처럼 좁은 인터페이스로 연결한다.

- Exploration 입력은 전투 진입 시 비활성화하고 필드 복귀 시 복원한다.
- Combat 입력은 현재 전투 상태가 허용하는 Action만 활성화한다.
- 맵 전환, 같은 맵 안의 전투 공간 이동, 별도 전투 Level 사용 여부는 아직 확정하지 않는다.
- 전투 중 필드 Actor를 숨기거나 정지시키는 정책도 Vertical Slice의 전환 방식이 결정된 뒤 확정한다.

`AExpeditionPortfolioPlayerController`는 상태 변경 Event를 구독하고 `Exploration`을 벗어날 때 탐험용 Enhanced Input Mapping Context와 이동/시점 입력을 비활성화한다. 전투 중에는 Game and UI 입력 모드와 Mouse Cursor를 사용한다. `PlayerCommand`에서는 전투 명령 Mapping Context만, `DefenseWindow`에서는 방어 Mapping Context만 활성화한다. `ReturnToField -> Exploration`이 완료되면 탐험 입력과 Game Only 입력 모드를 복원한다.

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

`BattleIntro`에서는 양쪽 참가자의 자동 회전을 잠그고 서로 마주보게 정렬한다. PlayerController의 `UBattlePresentationComponent`가 카메라 전환을 시작하고 완료 신호로 `PlayerCommand`에 진입한다. 전투 일련번호로 이전 진입의 신호를 거부하며 안전 Timer로 누락 신호를 대체한다. 카메라가 없는 핵심 테스트와 프로토타입 Pawn은 즉시 명령 턴으로 전환한다. 종료 시 이전 회전 설정과 탐험 뷰를 복원한다.

외부 호출자는 상태를 직접 전이하지 않고 `RequestBattleStart()`와 `RequestPlayerCommand()` 같은 명령 API를 사용한다. `EBattlePlayerCommand`는 현재 `BasicAttack`과 `PowerStrike`를 제공하며, Coordinator가 상태와 사용 가능 여부를 검증한 뒤 Action을 실행한다. 실제 상태 전이 함수는 Coordinator 내부에 있으며, 상태 Event 방송 중 재진입과 Action 실행 중 중복 명령은 거부한다.

플레이어 `BasicAttack`과 전투당 한 번의 `PowerStrike`는 각각 `DA_BasicMagic`과 `DA_StrongMagic`의 `Magic / Combat / Base Damage`에서 기본 피해를 설정한다. 초기값은 25와 40이며 명령 시작 때 복사하고 타격 시 강한 마법의 타이밍 성공 배율을 적용한다. 시전 중 변경은 다음 명령부터 반영된다. 0 피해를 허용하며 음수·비유한 값 또는 에셋 누락은 기존 기본값으로 대체한다. HUD의 피해 설명도 같은 설정을 조회한다. 패링 반격은 DA_ParryCounter의 Base Damage를 사용하며 초기값과 설정 누락 시 대체 피해는 15다. 적 피해량, 공격 예고 시간, 방어 시간과 패링 성공 시간은 `UEnemyAttackPatternData`로 분리했다. 기본값은 각각 20 Damage, 1초, 0.6초, 0.2초다.

플레이어 Action이 끝나면 `TurnEnd`에서 승패를 확인하고, 적이 생존하면 `EnemyTelegraph` Timer 후 `EnemyAction`에서 공격 모션을 시작한다. 타격 0.6초 전에 `DefenseWindow`가 열리고, 타격 직전 0.2초의 패링만 성공한다. 회피는 방어 구간 전체에서 성공하고 너무 이른 패링은 실패한다. 첫 입력을 보관한 뒤 실제 타격 순간에 피해 또는 반격을 한 번 적용하며 모션 완료까지 다음 턴을 보류한다. 시간과 피해량은 선택된 패턴의 복사본을 사용하고 Tick 없이 Notify와 Timer로 진행한다.

`EBattleDefenseCommand`와 `EBattleDefenseResult`는 입력 의도와 판정 결과를 분리한다. 외부 호출자는 `RequestDefenseCommand()`, `RequestParry()`, `RequestDodge()`로 의도만 전달하고, Coordinator가 상태, 참가자, Timer 유효성과 경과 시간을 검증한다. 판정 결과는 native/Blueprint Delegate로 발행하며 상태 전이 또는 결과 Event 처리 중의 재진입 명령은 거부한다. 전투 Context를 정리할 때는 Telegraph와 DefenseWindow Timer도 함께 취소한다.

적이 전투 불능이면 Victory에서 결과창 확인을 기다린 뒤 `ReturnToField -> Exploration`으로 복귀한다. 플레이어가 전투 불능이면 Defeat 결과창의 재도전/후퇴 선택을 기다린다. 두 경로 모두 전투 진입 스냅숏을 복원하며 후퇴만 조우 영역 바깥으로 복귀한다.

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

### 전투 카메라와 타이밍 보너스

Coordinator는 플레이어 행동의 Preparing/Executing 단계를 발행한다. 강한 마법의 준비 시작 시 C++ 시간창에 지속 시간·성공 구간·보너스 배율을 복사하며 첫 입력만 판정한다. 준비 완료 전에는 피해 신호를 거부하고, 실제 타격 시 성공 배율을 한 번 적용한다. 미입력·이른 입력은 기본 피해, 중단은 미적용 피해 취소다. 위젯은 현재 시간창의 진행률·결과를 표시하고 PlayerController는 키 입력만 전달한다.

`UBattlePresentationComponent`는 전투 상태와 행동 단계를 관찰해 `DA_BattlePresentation`의 구도를 `BP_BattleCameraRig`에 전달한다. Rig는 Blueprint에서 구도 계산을 재정의할 수 있으며 기본 계산은 플레이어→적의 축을 기준으로 한다. 번갈아 사용하는 두 카메라를 블렌딩하고 매 프레임 뼈대를 추적하지 않는다. 종료 시 원래 뷰 대상을 복원하고 Rig를 제거한다. 게이지는 별도 `WBP_AttackTiming`이며 명령 HUD의 상태 소유권을 늘리지 않는다. 조절 방법은 `docs/battle-camera-and-timing.md`를 참조한다.

### 플레이어 마법 연출

`UMagicPresentationComponent`는 캐릭터에 붙는 연출 실행기다. `UMagicActionData`에서 일반/강한 마법의 Montage, Niagara, 부착 위치와 크기를 지정한다. 특정 마네킹이나 콘텐츠 경로는 C++ 전투 로직에 넣지 않는다. 원본 모션은 보존하고 프로젝트 폴더의 복사본으로 Montage를 생성한다. 동일한 89개 뼈 구조를 확인한 뒤 플레이어 Skeleton에 마법 팩 Skeleton 호환을 등록했다. 기존 AnimBP의 DefaultSlot을 사용한다.

일반 마법은 `AS_SpellAttack`과 분리된 `NS_Magma_Shot_Projectile`/`NS_Magma_Shot_Impact`의 프로젝트 복사본을 사용한다. 강한 마법은 `AS_LevitatingFireballCharge`의 첫 2.5초 + 버블 시전 효과 후 `AS_LevitatingMagicAttack` + `NS_Ice_Mist`로 전환한다. 준비 구간을 끝낼 때 버블 컴포넌트를 제거한다. 마법 설정 에셋은 `/Game/ExpeditionPortfolio/Animation/Magic/DA_BasicMagic`과 `DA_StrongMagic`이다. `Tools/SetupMagicActions.py`가 복사본, Montage, 타격 Notify와 캐릭터 연결을 생성한다.

일반 마법은 `UseProjectile`을 켜 약 1.0초의 발사 Notify에서 `hand_l` 위치에 발사체를 생성한다. 발사 시 적 `spine_03`의 위치를 포착하고 거리/속도로 정한 시간 동안 직선 이동한 뒤, 도착 지점에 폭발을 생성하며 피해를 한 번 적용한다. 발사체 복사본은 불꽃·연기·불티를 로컬 공간에서 함께 이동시키며 도착 시 방출을 멈춘다. 중복 Notify는 추가 발사를 만들지 않는다. 비행 중에만 Tick을 켜며 발사 프레임의 DeltaTime은 비행에 포함하지 않는다. 비행과 공격 모션이 모두 끝나야 행동을 완료한다. 속도·최대 비행 시간·표적 뼈·폭발 보정은 Data Asset에서 설정한다. 자세한 내용은 `docs/player-magic-projectile.md`를 참조한다.

강한 마법의 버블은 `ChargeEffectBetweenHands`로 `hand_l`/`hand_r` 월드 위치의 중간을 따른다. 실제로 움직이는 두 손 중심을 추적해야 하므로 준비 효과가 존재하는 구간에만 Tick을 켜고 메시의 뼈대 갱신 이후 위치를 업데이트한다. 준비 종료/중단/정리 시 Tick을 끈다. `ChargeEffectTransform`은 양손 중심 기준의 보정값이다. 강한 마법 발동의 IceMist는 기존처럼 공격 단계 시작 시 재생한다.

Coordinator는 명령 승인 후 `PlayerAction`에 머물며 고유 행동 ID, 피해량과 타격 여부를 보관한다. `UAnimNotify_MagicHit`는 Montage branching point의 인스턴스 ID를 연출 컴포넌트에 전달한다. 활성 메시/인스턴스가 일치할 때 발사형은 비행을 시작하고 도착 시, 그 외 마법은 Notify 시 Coordinator에 행동 ID로 타격을 알린다. 중복/이전 행동/재진입 신호는 거부하며 피해 적용 후에도 모션 종료까지 기다린다. UI는 기존 상태 이벤트만으로 명령 패널을 숨긴다.

정상 종료에서 타격 Notify가 누락되면 경고 후 발사형은 그 시점에 비행을 시작하고, 그 외 마법은 피해를 한 번 적용한다. 중단이나 예상 길이+1초의 안전 Timer 만료는 미적용 피해를 취소하고 다음 턴으로 진행하며, 이미 들어간 피해는 유지한다. 발사형의 예상 길이에는 최대 비행 시간을 포함한다. 승인된 PowerStrike 사용 횟수는 환불하지 않는다. 참가자 무효화 시 전투를 정리한다. Montage 재생 실패는 경고 후 즉시 명령을 처리하며, 연출 컴포넌트가 없는 프로토타입 Pawn/핵심 테스트는 기존 즉시 실행 경로를 사용한다. 완료/중단/전투 종료/EndPlay에서 Montage, VFX와 Timer를 정리한다.

일반 마법의 `AttackEffectTailDuration`은 정상 행동 완료 후 폭발 효과의 추가 재생 시간이며 현재 1초다. 이 시간이 설정된 공격 VFX는 행동 참조에서 분리하여 다음 턴 동안에도 재생하고, Timer가 끝나면 `Deactivate`로 새 방출을 멈춘 뒤 남은 입자가 자연스럽게 완료되어 자동 제거되도록 한다. 비행 VFX는 도착 시 즉시 방출을 멈추고 잔여 입자만 남긴다. 모션 중단 시 활성 효과는 즉시 정리하며, 캐릭터 EndPlay에서는 잔여 효과와 관련 Timer를 모두 정리한다. 강한 마법의 값은 0으로 유지한다. 피해 판정과 행동 완료 시점은 추가 VFX 시간에 영향받지 않는다.

UI는 전투 상태의 소비자이자 사용자 명령의 입력 경계다.

- Widget은 Coordinator나 View Model 성격의 중간 객체가 발행한 상태 변경 Event를 구독한다.
- Widget은 공격/스킬 선택 의도를 명령 형태로 전달하고, 유효성 검사와 실제 상태 전이는 게임 로직이 수행한다.
- HP, 현재 턴, 선택 가능 Action은 게임 로직의 데이터를 표시하며 Widget 내부에 별도 원본 상태를 만들지 않는다.
- Widget Animation 종료가 핵심 전투 로직의 유일한 진행 조건이 되지 않도록 완료 신호와 실패 시 대체 경로를 둔다.

`UCombatHUDWidget`은 정식 전투 HUD Blueprint의 C++ 기반 클래스다. Coordinator와 양쪽 `UCombatantComponent`의 native Event를 구독하고 `Battle View Updated` Blueprint Event로 현재 상태, HP와 플레이어 명령 사용 가능 여부를 전달한다. 방어 판정은 별도의 `Defense Result Updated` Blueprint Event로 전달한다. Widget의 버튼과 PlayerController의 Enhanced Input은 각각 `RequestPlayerCommand()`와 `RequestDefenseCommand()`라는 동일한 C++ 명령 경계를 사용한다. Widget은 표시를 위한 별도 원본 상태를 만들지 않으며 `Exploration`에서는 자동으로 숨겨진다.

에디터에서는 다음 에셋을 조립한다.

- `IA_BasicAttack`, `IA_PowerStrike` Input Action
- 위 Action을 키 또는 Gamepad 입력에 연결한 `IMC_PlayerCommand`
- `IA_Parry`, `IA_Dodge` Input Action
- 위 방어 Action을 키 또는 Gamepad 입력에 연결한 `IMC_DefenseWindow`
- `UCombatHUDWidget`을 부모로 하는 `WBP_CombatHUD`
- PlayerController Blueprint의 전투 명령/방어 Mapping Context, 네 Input Action, `Combat HUD Widget Class` 지정

`WBP_CombatHUD`는 `Battle View Updated` Event의 값으로 HP, 상태 Text와 버튼 활성화를 갱신한다. 기본 공격과 대표 스킬 버튼은 `Request Basic Attack`, `Request Power Strike`를 호출한다. 방어는 E/Q 단축키를 사용하며 별도 버튼은 없다. `TXT_DefenseResult`는 타격 결과 Event를 받아 PARRY/DODGE/HIT를 약 1.2초 표시한다. Widget은 결과를 표시하기만 하며 판정이나 상태 전이에 관여하지 않는다.

## Data Asset 기반 스킬과 몬스터 패턴

적 공격은 `UDataAsset` 기반 `UEnemyAttackPatternData`로 정의한다. 기존 패턴 필드에 AttackMontage, HitTimeOffset, FallbackHitTime을 추가했다. 타격 시점은 Montage의 EnemyHit Notify와 보정값으로 계산하고 방어·패링 시간과 함께 선택 시 복사한다. `CombatantComponent.EnemyAttackPatterns`는 콘텐츠 목록만 제공하며 Coordinator가 선택과 판정을 담당한다. `UEnemyAttackPresentationComponent`는 적을 표적 방향으로 돌려 Montage를 재생하고 메시/인스턴스/행동 ID를 검증한 신호만 전달한다. 누락된 타격 Notify는 예약 Timer가 처리하고, 모션 중단/완료 신호 시간 초과는 미적용 피해 없이 행동을 취소한다. 모션 종료에서 타격 신호와 Timer가 모두 누락된 경우에만 정상 종료 대체 판정을 한 번 수행한다. 전투 정리와 EndPlay는 남은 모션과 Timer를 취소한다. 자세한 조절 방법은 `docs/enemy-attack-tuning.md`를 참조한다.

선택은 `EnemyTelegraph` 상태 이벤트를 알리기 전에 이루어진다. 유효한 패턴의 양수 가중치를 합산해 선택하며 0 가중치는 비활성화한다. null, 빈 ID, 비정상 수치, 0 이하 피해/시간, 방어 시간보다 긴 패링 시간, 음수 가중치를 제외한다. 유효한 후보가 없으면 적 이름과 원인을 경고하고 기본 공격으로 대체한다.

`FEnemyAttackPatternSnapshot`은 선택 시점의 값 복사다. Timer와 피해 판정은 원본 에셋 대신 이 값을 사용한다. `HasActiveEnemyPattern`과 `GetActiveEnemyPattern`으로 Blueprint에서 조회하며 적 행동 완료/전투 정리 시 해제한다. 선택 함수는 패턴 실행과 분리되어 향후 적응형 AI로 확장할 수 있다. 현재는 가중치 무작위 선택이고 학습/적응 기능은 없다.

`Tools/SetupEnemyAttackPatterns.py`는 기본 Data Asset을 생성하고 `BP_PrototypeEnemy`의 컴포넌트에 연결하는 에디터 Python 스크립트다. 기존 에셋 수치는 보존하고 다른 패턴 목록이 있으면 덮어쓰지 않는다. 실행 시 PythonScriptPlugin과 EditorScriptingUtilities를 일시적으로 활성화하며 프로젝트 플러그인 설정을 변경하지 않는다.

플레이어 스킬 데이터와 아래 확장 항목은 후속 설계 후보이며 현재 구현 항목이 아니다.

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

강한 마법의 방향성 IceMist는 `AttackEffectAimAtTarget`을 사용한다. `spine_03` 앞에서 생성한 뒤 최종 생성 위치에서 적 액터 중심을 향하는 회전을 계산하고, 에셋의 회전 보정값을 적용한다. 적의 배치 회전은 발사 방향에 사용하지 않는다. `AttackEffectAtTarget`은 표적 위치에서 재생하는 별도의 충격 효과용 설정으로 유지한다.

### 플레이어 방어 연출

패링 반격은 Coordinator가 적 행동 ID에 묶어 별도로 추적한다. `UMagicPresentationComponent.CounterMagic`의 전용 `DA_ParryCounter`를 기존 발사체 실행기로 재생하되 명중/완료 신호는 플레이어 명령 경로와 분리한다. Coordinator가 반격 피해를 시작 시 복사하고 명중을 한 번 검증하며 적 공격 완료와 반격 완료를 함께 기다린다. 반격은 새 플레이어 턴이나 명령을 만들지 않는다. 입력 성공 통지 즉시 HP가 감소하던 기존 동작은 설정이 없는 대체 경로에만 남긴다.

반격 시작 전에는 성공 충격 구간을 둔다. MagicPresentation이 섬광·음향·카메라 흔들림과 짧은 World Time Dilation을 관리하고, 이 구간에만 Core Ticker와 플랫폼 실시간 시계를 사용한다. 슬로모션에 영향을 받는 게임 Timer로 복귀를 예약하지 않는다. 충격 종료 후 이전 배율을 복원하고 기존 반격 실행기로 넘어간다. StopPresentation/EndPlay와 전투 정리 모두 같은 복원 경로를 사용해 배율·Ticker·충격 VFX·카메라 흔들림을 남기지 않는다. 시간 제한과 피해 소유권은 Coordinator에 남는다.

`UDefensePresentationComponent`는 Coordinator의 `OnDefenseCommandAccepted`와 기존 방어 결과/상태 이벤트를 구독한다. 승인된 첫 입력에서만 패링/회피 모션을 재생하며 피해나 상태 전이는 수행하지 않는다. 실패 또는 미입력의 타격 결과는 가산 HitReaction Montage로 표시한다. 입력 승인 이벤트는 Action Guard 안에서 방송하여 연출 소비자의 재진입 명령도 차단한다.

`UDefensePresentationData`는 Montage와 연출 수치를 제공한다. 회피는 Actor/캡슐을 이동시키지 않고 메시의 상대 위치만 보간하며, 원본 Transform을 저장해 완료·중단·다음 행동·전투 종료·EndPlay에 복원한다. 회피 동안에만 Tick을 켜고 후퇴 후 자세를 잠시 유지한 뒤 복귀한다. 원본 MM_Dash는 보존하고 루트 이동을 잠근 복사본을 사용한다. HitReact는 가산 속성을 유지한 Montage를 기존 AnimBP 슬롯에 재생하여 무기 없는 기본 자세에 더한다. 설정 경로와 조절 방법은 `docs/player-defense-presentation.md`를 참조한다.

## 의존성 방향

### 계층형 명령 메뉴와 근접/물약

명령 메뉴의 호흡 연출은 현재 카메라 투영과 액터 로컬의 몸통/골반 모션 변화로 계산하며, 카메라 종류를 참조하지 않는다. 작은 움직임만 보간하고 컷/재표시 때 초기화한다. 미래 카메라 연출은 HUD의 표시 게이트를 사용하고, 준비 모션의 타이밍 게이지는 별도 행동 UI/명령 경계로 연결한다. 메뉴가 전투 상태나 카메라를 제어하지 않는다.

`WBP_CombatHUD`에 캐릭터 투영 좌표를 따르는 기울어진 메뉴를 배치했다. UI의 메뉴 페이지는 표시 상태이며 전투 상태와 분리된다. PlayerController의 기존 두 명령 단축키도 HUD의 현재 메뉴 선택으로 전달한다. `BasicAttack` enum은 기존 에셋 호환을 위해 약한 마법의 의미로 유지하며 실제 평타는 `MeleeAttack`, 물약은 `UsePotion`이다. 기존 두 명령은 기술 목록에서만 선택한다.

`UPlayerCommandData`의 근접 피해/물약 회복/초기 수량은 조우 시작 시 복사한다. Coordinator가 상태·HP·수량을 검증하고 승인 시 아이템을 차감하며 행동 ID당 한 번 회복 또는 피해를 적용한다. 정상 행동은 다음 적 턴으로 이어지고 중단 시 미적용 효과를 취소하며 사용한 물약은 반환하지 않는다. `UCombatantComponent::RestoreHealth`는 생존자에게 양수 유한 회복만 적용하고 최대 HP로 제한한다.

`UPlayerUtilityPresentationComponent`는 설정 Data Asset으로 맨손 공격과 접근/복귀, 물약 VFX를 실행한다. 연출과 핵심 판정의 완료/중복/시간 초과 보호는 기존 행동 경계를 사용한다. UI 레이아웃 생성 도구의 UMGEditor 의존성은 Editor 빌드에만 포함한다. 상세 경로와 조절 방법은 `docs/combat-command-menu.md`를 참조한다.

의도한 의존성은 다음과 같다.

`Input/UI -> 전투 명령 인터페이스 -> 전투 흐름 조정 객체 -> Combatant/Action 로직 -> Event -> UI/연출`

UI와 연출 계층은 Event를 관찰하고, 핵심 로직은 구체적인 Widget이나 효과 에셋을 직접 알지 않는 방향을 유지한다.
