# 공통 규칙과 캐릭터 콘텐츠의 소유권

현재 그룹 조우와 파티 턴은 [다수 적 조우](grouped-encounters.md)에 추가됐다. 종족 구성과 배치는 Encounter Group Data, 참가자·턴·승패는 Coordinator, 개체 HP는 Combatant, 머리 위 바 위치·초상화는 각 Visual Data, 공통 HP HUD 외형은 Combat Health HUD Data가 소유한다.

## 현재 편집 위치

공통 규칙에는 캐릭터의 공격 종류·뼈 이름·공격 모션을 넣지 않는다. 현재 마법사의 공격·반격과 골렘의 외형·패턴은 각각의 콘텐츠에서 연결한다.

| 변경할 내용 | 에셋 / 항목 |
|---|---|
| 턴 사이 대기, 전투 시작 시 파티 공용 아이템 수량 | `Combat/Rules/DA_BattleRules` → Turn Transition Delay / Initial Items |
| 물약 회복량·사용 효과·사운드와 싱크 | `Combat/Items/DA_HealingPotion` → Healing Amount / Effect / Use Sound |
| 현재 캐릭터의 물약 사용 동작·적용 시간·전체 시간 | `Characters/PlayerMage/Combat/DA_PlayerMageItemUse` |
| 캐릭터 고유 ID·평타·기술·반격·아이템 사용 연출 연결 | `Characters/PlayerMage/Combat/DA_PlayerMageCombat` |
| 평타·불마법·얼음마법·반격의 피해와 연출 | 같은 폴더의 `DA_PlayerMageBasicAttack / FireSkill / IceSkill / Counter` |
| 공격별 준비·실행 카메라 구도 | 각 행동 DA의 Camera Data → 같은 폴더의 `Cameras/DA_…Camera` |
| 마법사의 숫자·대상 표식·패링 효과 위치와 메뉴 호흡 기준 뼈 | `Characters/PlayerMage/Combat/DA_PlayerMageVisual` |
| 골렘의 조준점·피해 숫자·대상 표식 위치 | `Combat/Enemies/DA_StoneGolemVisual` |
| 공통 패링 섬광·소리·슬로모션 | `Presentation/DA_ParryFeedback` → Intermediate / Complete |
| 골렘 메시·캡슐·HP·공격 패턴 | `Combat/Enemies/BP_StoneGolem` |

경로의 공통 시작점은 `/Game/ExpeditionPortfolio`다. `BP_ThirdPersonGameMode`의 Class Defaults → Battle Rules가 공통 규칙 DA를 참조한다. 아이템 목록은 이 DA의 Initial Items 배열에서 생성된다. 같은 아이템 ID는 이 배열에 중복 등록할 수 없다. 수량은 파티 공용이며 현재 동작대로 새 전투마다 초기 수량으로 시작한다. 영구 인벤토리를 추가한 것은 아니다. 재도전은 이번 전투 진입 시 수량으로 복원한다.

## 캐릭터 식별자와 적 개체

아군 `CombatantComponent`의 Persistence는 `Unique Party Member`로 지정하고, 연결된 Character Actions 프로필의 Character ID 하나를 HP 저장 키로 사용한다. 다른 아군은 다른 프로필과 ID를 사용한다. 같은 아군이 맵을 이동하거나 다시 생성되면 같은 ID로 HP를 불러온다. 런타임에 같은 ID를 가진 아군이 동시에 생성되면 두 번째 등록을 거부하고 로그를 남기며 저장 HP를 덮어쓰지 않는다. 에셋 이전 도구도 Characters 폴더의 서로 다른 프로필이 같은 ID를 쓰는지 검사한다.

적은 `Transient`로 둔다. 같은 골렘 Blueprint를 여러 개 배치해도 각 Combatant가 HP를 따로 소유하며 아군 HP 저장소에 등록되지 않는다. 종족 콘텐츠의 공유와 개별 적 HP는 구분된다. 기존 맵/조우 ID는 별도 체계로 그대로 유지한다. 이번 변경은 다수 적의 턴 진행이나 파티 턴 시스템을 추가하지 않는다.

## 실행기와 콘텐츠 조립

공통 플레이어 C++ 클래스는 마법·근접 실행기를 강제 생성하지 않는다. 현재 `BP_ThirdPersonCharacter`가 필요한 `MagicPresentation`, `UtilityPresentation`을 추가한다. 행동 DA의 Executor Component Name과 지원하는 DA 종류로 실행기를 고른다. 공통 ItemPresentation은 프로필의 Item Use Presentation과 공통 아이템 정의를 결합한다. 근접 실행기는 물약 설정을 참조하지 않는다.

실제 게임에서 프로필·실행기·공통 규칙·아군 식별자 설정이 빠지면 전투 진입 또는 명령을 거부한다. 누락 설정을 고정 피해로 대체하지 않는다. 콘텐츠 없이 핵심 상태 전이만 검사하는 자동화 테스트는 명시적인 테스트 전용 경로를 사용하며 게임 실행의 기본 동작과 분리한다.

공통 적 C++ 기반은 `BattleEnemyActor`이며 골렘 메시나 패턴을 갖지 않는다. `BP_StoneGolem`이 이를 상속하고 골렘 콘텐츠를 소유한다. 기존 배치와 조우 참조를 보존하기 위해 `BP_PrototypeEnemy`는 `BP_StoneGolem`을 상속하는 호환 자식으로 남긴다. 새 적 종족은 `BattleEnemyActor`를 상속해 별도의 Blueprint를 만든다. `BP_PrototypeEnemy`를 공통 부모로 사용하지 않는다.

## 위치·피격·카메라

공통 UI와 패링 피드백은 머리·손·등뼈 이름을 직접 사용하지 않는다. 각 대상의 Visual Data가 Aim, Damage Number, Target Marker, Parry Intermediate, Parry Complete, Camera라는 용도별 위치를 제공한다. Socket이 없으면 설정된 액터 기준 오프셋이나 Bounds Top Fallback을 사용한다. 메뉴는 같은 데이터의 Motion Bone / Reference Bone / World Offset / Screen Offset을 읽는다. 현재 마네킹과 골렘의 기존 위치를 이 데이터로 옮겼다.

맞는 모션은 공격자가 지정하지 않는다. 적은 자신의 Enemy Presentation, 아군은 자신의 Defense Presentation을 사용한다. 공격 DA의 이전 Counter Hit Reaction 필드는 런타임에서 사용하지 않는다.

각 행동의 Camera Data에서 준비/실행 구도를 독립 설정한다. Override를 끄거나 DA를 연결하지 않은 단계는 공통 Battle Presentation의 기본 구도를 사용한다. 카메라 보간·가림 보정·대상 확정 후 숄더뷰 복귀는 공통 구현을 유지한다. 반격도 자신의 Camera Data를 사용한다.

## 이전 에셋과 도구

기존 `.uasset`, `.umap`은 삭제·이름 변경하지 않았다. `DA_PlayerCommands`, `DA_PlayerUtilityPresentation`과 이전 마법·반격 DA는 이전값 보관용이며 현재 캐릭터의 수치를 수정하는 위치가 아니다. 숨긴 직렬화 필드는 이전 도구가 읽기 위해 남겨 두었다.

`Tools/SetupSharedCombatOwnership.py`는 외부 에디터가 없는 상태에서 단독 실행한다. 기존 목적지 DA의 사용자 튜닝을 보존한다. `SetupCharacterActions.py`는 역사적인 첫 이전 스크립트이며 분리된 프로젝트에서 재실행하면 새 도구로 안내하고 중단한다. `SetupStoneGolem.py`는 분리 이후 골렘 전용 Blueprint를 대상으로 한다. 이전 구축 스크립트를 무작정 재실행하지 말고 현재 연결된 프로필과 이 문서의 경로를 기준으로 편집한다.

## 검증 기준

Editor Development 빌드와 전투 자동화 테스트에 더해 실제 PIE에서 공격·방어·아이템·재도전·카메라와 기존 조우 연결을 확인한다. 자동화에는 누락 콘텐츠 거부, 공용 아이템과 진입 스냅샷 복원, 중복 아군 ID와 동일 종족 적의 독립 HP, 대상별 위치와 기술별 구도, 콘텐츠 없는 공통 기반 검사를 포함한다. 사용자 직접 플레이 전에는 최종 체감 검증 완료로 처리하지 않는다.

2026-11-09 구현 검증: `ExpeditionPortfolioEditor Win64 Development` 최종 빌드와 전투 자동화 테스트 66개가 통과했다. 새 프로세스에서 이전 도구를 재실행하여 피해 15/20/30/15, 골렘 HP 200과 패턴 참조, 물약 회복 30·수량 3, 턴 대기 1초를 보존하는 것을 확인했다.

실제 PIE에서는 평타, 두 마법의 QTE와 보너스 피해 30/45, 세 번째 기술, 마법 평타, 근접·마법 반격, 다단 회피·패링, 풀피 물약, 대상 취소·중복 확정, 재도전·후퇴를 검증했다. 불마법과 얼음마법의 준비 카메라에 서로 다른 FOV를 임시 적용해 각각의 구도가 사용되는 것을 검사하고 원래 값으로 복원했다. 패링 슬로모션 중 패배 후 재도전 시 시간 배율 1과 진입 HP 복원을 확인했다.

승리 확인 후 적 제거, 실제 World 재생성과 맵 재진입 후 HP 72 유지·동일 조우 ID·처치 상태 복원을 확인했다. 임시 두 번째 아이템의 W/S 선택, 취소·사용, 아이템 ID별 독립 수량, 사용 모션 중단 시 회복 취소와 정상 사용의 HP 60→72를 확인했다. 임시 아이템·프로필·모션·카메라 설정은 복원해 저장했다. 여러 동일 종족 적의 독립 HP와 아군 ID 중복 거부는 자동화에서 검증했으며 다수 적의 실전 턴 진행을 구현한 것은 아니다. 사용자의 직접 플레이와 청음에 의한 최종 체감 검증은 남아 있다.
