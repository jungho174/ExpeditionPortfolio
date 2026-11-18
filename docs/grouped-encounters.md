# 다수 적 조우와 파티 턴

현재 WhiteCastle의 성문 조우는 골렘 2마리다. 탐험 중부터 전원이 보이며, 하나의 조우 영역과 고유 조우 ID를 공유한다. 현재 정식 아군은 기존 마법사 한 명을 유지한다.

현재 성문 묶음은 단차 경계에서 양발이 걸치지 않도록 배치를 교정한 맵 전용 `DA_WhiteCastleGateEncounter`를 참조한다. 기본 `DA_Encounter_2Enemies`와 `DA_Encounter_3Enemies`는 유지한다. 성문 배치를 수정할 때는 배치된 묶음의 Composition에 연결된 DA를 연다.

마법사의 얼음마법은 이제 현재 조우의 생존 적 전체를 공격한다. 전체 표식과 Space 확정, DA 편집 위치는 [캐릭터별 전투 설정](character-actions.md)의 적 전체 공격 절을 참조한다. 아래 개별 얼음마법 피해 검증은 광역화 이전 기록이다.

## 에디터에서 배치하기

콘텐츠 브라우저의 `ExpeditionPortfolio → Exploration → Encounters`에서 다음 Blueprint를 레벨로 드래그한다.

| 프리셋 | 기본 구성 |
|---|---|
| BP_Encounter_2Enemies | 돌 골렘 2마리 |
| BP_Encounter_3Enemies | 돌 골렘 3마리 |

두 Blueprint는 같은 C++ `BattleEncounterGroup`을 사용하며 각각 `DA_Encounter_2Enemies`, `DA_Encounter_3Enemies`를 참조한다. 코드에서 인원수를 2나 3으로 제한하지 않는다. 조우 액터의 위치·회전으로 전체 묶음을 옮긴다. 기본 배치는 조우 영역에서 로컬 X 방향으로 750cm 앞이며 몬스터는 진입 방향을 마주본다.

Details → Encounter / Group → **Composition**에서 구성 DA를 연다. **Members**의 각 항목에서 **Enemy Class**를 바꾸면 해당 자리가 다른 몬스터로 바뀐다. `Relative Transform`에서 개별 위치·회전·크기를 편집한다. 새 적은 공통 `BattleEnemyActor`를 상속한 종족 Blueprint를 사용한다. 동일한 골렘 Blueprint를 여러 자리에 써도 각각 별도의 Actor와 HP를 가진다.

DA를 편집한 뒤 배치된 조우 액터의 **Rebuild Preview**를 눌러 미리보기를 갱신한다. 액터의 Composition을 바꾸면 Construction에서도 갱신한다. PIE 중에는 재생성하지 않는다. 같은 DA를 공유하는 조우들은 같은 구성을 사용하므로 특정 묶음만 다르게 하려면 DA를 복제해 연결한다. 인원수도 Members 배열에서 바꿀 수 있다.

조우 영역 크기는 상속된 Encounter Volume의 Box Extent에서 조절한다. 조우 액터를 복제하면 새 조우 ID가 생성된다. 기존 WhiteCastle 조우 ID와 후퇴 지점은 유지했다. 앞으로 이 맵에서 적 묶음을 바꿀 때는 전체 맵 초기 제작 도구 대신 이 액터의 Composition을 편집한다.

## 턴과 대상

등록 순서대로 **아군 1 → 아군 2 → … → 적 1 → 적 2 → … → 다음 라운드**를 진행한다. 화면상의 좌우 순서로 정렬되는 공격 대상 선택과, DA의 Members 등록 순서로 정해지는 적 턴 순서는 구분한다. 현재 플레이는 아군 1 → 골렘 1 → 골렘 2다.

각 적은 자기 턴 시작에 생존 아군 중 한 명을 무작위로 선택하고 그 패턴 전체에서 같은 대상을 공격한다. 방어 입력·피격·반격·카메라는 공격받는 아군을 따른다. 완전 패링 반격은 해당 아군의 캐릭터 전용 행동 DA를 사용한다. 명령은 현재 행동하는 아군의 평타·기술·사용 횟수를 사용하고 물약 재고는 파티 공용이다.

전투불능 참가자는 다음 턴과 일반 대상 선택에서 제외한다. 공격 도중 아군이 전투불능이 되면 남은 타격을 종료하고 다음 참가자로 진행한다. 적이 반격으로 쓰러져도 남은 적 턴을 계속 진행한다. 모든 적 전투불능 시 승리, 모든 행동 가능한 아군 전투불능 시 패배다. 생존 아군이 있으면 HP가 0인 탐험 주인공과도 조우할 수 있다. 일반 물약은 부활 기능을 제공하지 않는다.

미래 아군은 각자 고유 ID·행동 프로필·실행기를 가진 Pawn으로 준비하고 조우의 **Additional Allies**에 순서대로 등록한다. 현재 작업에서는 영구 동료 캐릭터·필드 추종·파티 편성 화면을 추가하지 않았다. 런타임 시작 API는 `RequestGroupBattleStart(Source, ExplorationPlayer, Enemies, AdditionalAllies)`다. `GetActingCombatant`는 턴 주체, `GetActivePlayerPawn`은 현재 명령 아군 또는 적의 공격 대상이며 탐험 조작 Pawn과 구분한다. 옛 1대1 시작 API도 한 명짜리 목록으로 연결된다.

기존 기본 1초의 턴 간격은 모든 참가자 사이에 적용하며 `Combat/Rules/DA_BattleRules → Turn Transition Delay`에서 조절한다. A/D·Space 확정·Backspace 취소, QTE, 기술별 카메라·사운드와 명령 메뉴는 유지한다. 하단 대상 이름·HP·조작키 안내 패널은 제거했고 대상 표식은 유지한다.

## HP 표시 편집

상단은 현재 전투 단계만 표시한다. 적 HP는 각 적 머리 위에 숫자 없이 바만 표시하며 카메라 전환을 따라 위치를 투영한다. 위치는 해당 몬스터 Visual Data의 **Health Bar** 앵커에서 조절한다. 기본은 Bounds Top + 24cm다.

아군은 사진을 참고해 우측 하단에 캐릭터별 패널을 가로로 배치한다. 이름, 현재/최대 HP, 붉은 HP 바와 현재 행동자 강조를 표시한다. 초상화는 캐릭터 Visual Data의 **Portrait**에 Texture를 지정한다. 없는 캐릭터는 중립 실루엣을 표시한다. 현재 마법사 위치는 `Characters/PlayerMage/Combat/DA_PlayerMageVisual`이다.

공통 외형은 `UI/DA_CombatHealthHUD`에서 Panel Width/Height/Gap, Bottom Right Margin, 색과 폰트, Enemy Bar Size를 편집한다. `UI/WBP_CombatHUD`의 Class Defaults → Health HUD Data가 이를 참조한다. 기존 상단 HP 위젯은 Blueprint 바인딩을 보존한 채 접어 두었다. 단계 텍스트는 같은 WBP의 `TXT_BattleState`다.

대상 선택 카메라에는 **Fit Target Bounds**를 적용해 큰 몬스터와 머리 위 HP 바를 화면에 포함한다. 설정은 `Presentation/DA_BattlePresentation → Target Selection Shot`의 Fit Target Bounds / Target Framing Padding이다. 다른 기술의 카메라 설정은 유지한다. 벽에 막혀 카메라가 당겨지는 구간은 맵별 구도 검토가 필요하다.

## 승패와 재사용

승리 화면의 계속을 누르면 묶음의 적 전원을 Destroy한다. 같은 실행 중 맵을 다시 방문해도 조우 ID의 처치 상태에 따라 전원 제거된다. 재도전은 모든 참가자의 진입 HP·배치와 공용 아이템·캐릭터별 기술 횟수를 복원하고 첫 생존 아군부터 시작한다. 후퇴는 같은 상태를 복원한 뒤 기존 안전 지점으로 탐험 주인공을 돌려보내며 조우를 재활성화한다.

개별 적은 HP 0 즉시 턴·대상에서 제외되고, 사망 모션 후 화면에서 소멸한다. 숨긴 인스턴스는 재도전·후퇴 복원을 위해 승리 계속까지 보관한다. 사망 중 일반 공격 정리가 호출되어도 사망 모션·소멸 Timer를 취소하거나 메시를 원위치로 되돌리지 않는다. 반격으로 공격자를 처치하면 취소된 공격 모션의 완료 신호를 기다리지 않고 남은 적 턴으로 진행한다.

`Tools/SetupGroupedEncounters.py`는 최초 프리셋·HUD 설정과 기존 성문 조우 이전 도구다. 기존 구성 DA의 사용자 편집값을 보존한다. 임의의 맵에 있는 다른 조우를 자동으로 묶지 않는다. 검증용 임시 Actor와 설정은 실제 콘텐츠와 분리한다.

## 검증 기록 (2026-11-10)

`ExpeditionPortfolioEditor Win64 Development` 빌드와 `ExpeditionPortfolio` 자동화 테스트 67개를 통과했다. 그룹 테스트는 아군 2명·적 3명의 등록 순서, 무작위 생존 대상, 전투불능 턴 건너뛰기, 일부 적 처치 후 계속 진행, 대상 변경·중복 확정 거부, 반격으로 공격자 처치, 전투불능 주인공 대신 생존 동료부터 시작, 전체 패배·재도전을 검사한다.

WhiteCastle PIE에서 아군 1명·골렘 2마리로 불·얼음 마법의 개별 대상 피해, 두 적의 반복 턴과 다단 패링·QTE를 확인했다. 별도 실제 플레이 검증으로 일부 적 처치 후 남은 적 턴, 최종 승리와 계속 후 전원 Destroy, 실제 World 재생성 후 같은 조우 ID·처치 상태·아군 HP 유지, 재도전의 양쪽 진입 HP 복원, 후퇴 후 일시정지·시간 배율 해제와 즉시 재조우 방지를 확인했다.

최종 빌드의 화면에서 W/S 메뉴 이동·Space 기술/아이템 선택·A/D 두 적 전환·Backspace 취소와 대상 안내 패널 제거를 확인했다. 명령 화면과 대상 확대 화면에서 두 적의 머리 위 바, 우측 하단 아군 HP와 행동자 강조를 확인했다. 새 프로세스에서 2/3마리 프리셋을 로드하고 임시 3마리 조우를 배치한 뒤 구성 수·종족 교체·미리보기 반복 갱신을 검증했다. 임시 조우는 정리했고 기존 조우 ID, 배경 Nanite·재질·충돌과 조명을 보존했다.

화면 검증 중 D3D12 장치 제거 오류로 에디터가 한 번 종료됐다. 재실행 후 위 승패 흐름은 통과했으며 프로젝트 설정이나 그래픽 드라이버는 변경하지 않았다. 사용자 직접 플레이에 의한 가독성·카메라·난이도·청음의 최종 확인은 별도다.
