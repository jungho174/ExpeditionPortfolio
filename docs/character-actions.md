# 캐릭터별 전투 설정

공통 규칙·아이템·대상별 뼈와 위치·기술별 카메라·아군 식별자도 분리했다. 현재 전체 편집 위치와 새 캐릭터 연결 방법은 [공통 규칙과 캐릭터 콘텐츠의 소유권](shared-combat-ownership.md)을 참조한다.

## 편집 위치

현재 마법사 캐릭터의 설정은 `/Game/ExpeditionPortfolio/Characters/PlayerMage/Combat`에 있다.

| Data Asset | 역할 |
|---|---|
| `DA_PlayerMageCombat` | 평타, 기술 배열, 패링 반격 연결 |
| `DA_PlayerMageBasicAttack` | 평타 피해·접근/타격/복귀 모션·이동 시간·사운드 |
| `DA_PlayerMageFireSkill` | 불마법 피해·QTE 패턴·준비/발사/명중 연출 |
| `DA_PlayerMageIceSkill` | 얼음마법 피해·횟수 제한·QTE·준비/발사/명중 연출 |
| `DA_PlayerMageCounter` | 현재 마법사의 불마법 반격 피해·모션·VFX·발사/명중 소리 |

2026-11-18 밸런스 조정: 현재 마법사의 `DA_PlayerMageFireSkill`과 `DA_PlayerMageCounter`의 Base Damage는 각각 **25**다. 파이어볼의 QTE 패턴·성공 배율은 유지하므로 현재 1.5배 Perfect 피해는 37.5이며, 반격은 25를 한 번 적용한다. 아래 초기 이전 검증의 20/15 피해 기록은 당시 설정값이다.

이번 조정 후 Editor Win64 Development 빌드를 통과했고 새 프로세스의 실제 전투에서 파이어볼 Regular 피해 25, Perfect 피해 37.5, 골렘 두 명의 3연타를 각각 전부 패링한 뒤 반격 피해 각 25를 HP 차이와 피해 이벤트로 대조했다. 사용자 직접 플레이 검증은 남아 있다.

`BP_ThirdPersonCharacter`의 `CombatantComponent > Combat > Character > Character Actions`가 프로필을 참조한다. 다른 캐릭터는 자신의 프로필과 공격 DA를 연결한다. 프로필에는 공격 연출을 직접 넣지 않고 각 행동 DA의 참조를 넣는다. `Skills` 배열 순서가 메뉴 순서이며 기술 수는 두 개로 고정되지 않는다. 메뉴는 DA의 이름·설명·피해·남은 횟수를 표시한다.

`Uses Per Encounter`가 0이면 무제한이다. 양수이면 그 캐릭터의 해당 Action ID에 대해서만 횟수를 소비한다. 대상 선택 취소는 소비하지 않고, 명령 수락 이후 중단은 소비한다. 재도전은 전투 진입 당시 횟수로 복원한다. 스킬에 QTE를 쓰려면 해당 DA에서 `Enable Timing Bonus`, 공통 패턴 DA인 `QTE Pattern`, `Post Timing Hold Duration`을 설정한다. `Preparation Duration`은 종전 `Charge Duration`의 표시 이름이다. 패턴을 연결하면 게이지 시간은 패턴에서 읽는다.

## 적 전체 공격 (2026-11-10)

평타·기술 DA의 **Action / Combat / Target Scope**에서 `Single Enemy` 또는 `All Enemies`를 선택한다. 공통 C++는 대상 목록과 피해 배치를 처리하고, 캐릭터 전용 DA가 범위를 결정한다. 현재 `DA_PlayerMageIceSkill`만 All Enemies로 설정했다. 불마법·평타는 기존 단일 대상이며, 패링 반격은 기존 공격자 대상 경로를 유지한다.

All Enemies는 현재 조우의 생존 적 전원에 표식을 띄우고 Space로 확정한다. A/D로 개별 대상을 변경하지 않으며 Backspace로 취소할 수 있다. 확정 후 숄더뷰 복귀와 기존 준비/QTE를 거쳐 한 번 시전한다. 확정 전에는 HP·횟수·턴을 소비하지 않는다. `Base Damage`는 적 한 마리당 피해다. 현재 얼음마법 30, QTE Perfect 배율 1.5 기준이면 각 적에게 45씩 적용한다. 기본 피해는 명령 수락 시 복사하고, 명중 시점에 이미 쓰러진 적·다른 조우의 적·아군은 제외한다. QTE와 사용 횟수는 대상 수와 무관하게 한 번 처리한다. 피해 숫자·피격과 전투 기록은 실제 각 적의 HP 감소량을 사용하며, 전체 배치 적용 후 승리를 판정한다.

선택 구도는 기술의 Camera Data에서 **Override Target Selection / Target Selection Shot**으로 조절한다. 얼음마법은 자신의 `Cameras/DA_PlayerMageIceSkillCamera`에서 설정한다. 전체 대상의 Bounds와 HP 앵커를 포함하도록 프레이밍하며, 실행 구도도 해당 기술의 Execution Shot을 사용한다. 공통 전환 시간과 준비 모션 설정은 유지한다.

기존 얼음 Niagara를 각 대상 방향으로 같은 타격 시점에 재생한다. 시전 도중 전투불능이 된 적은 피해와 새 명중 연출에서 제외한다. 시전·명중 SFX는 적 수만큼 중첩하지 않고 한 번 재생하며 기존 DA의 파일·음량·지연·시작 시간 설정을 따른다. 연출용 대상 목록은 Coordinator의 `GetSelectedTargets` / `GetActiveActionTargets`로 조회하며 UI나 실행기가 피해를 직접 적용하지 않는다. 기존 한 명 대상 API도 유지한다.

`Tools/SetupIceAreaAttack.py`는 현재 마법사의 범위와 기술별 선택·시전 구도만 설정한다. 피해·횟수·QTE·모션·VFX·SFX 수치를 보존한다.

검증: Editor Win64 Development 빌드와 자동화 테스트 68개 통과. 새 광역 테스트는 전체 선택·취소·A/D 무시·중복 확정, 피해/범위 스냅샷, 시전자별 1회 소비, 두 적의 원자적 피해와 실제 HP 기준 통계, QTE Perfect/Regular/Miss, 0 피해, 중단·중복 Notify, 시전 중 전투불능 대상 제외와 다음 동료 턴을 검사한다. 기존 단일 공격·아이템·반격·파티 턴 회귀도 통과했다.

실제 WhiteCastle PIE에서 Space 확정, 두 골렘 각각 Perfect 45 피해와 얼음 효과 2개·노란 숫자 2개, Miss 시 각각 30 피해, 동시 처치 후 승리·계속 후 전원 제거, 준비 도중 한 적이 쓰러져도 남은 적에만 피해·이펙트 1개 적용을 확인했다. 최종 선택/시전 구도를 화면 확인하고 기술 전용 Execution Shot을 적 그룹 기준으로 조정했다. 진단 일시정지와 콜백을 정리했고 최종 t.MaxFPS는 0이다. 자동화 중 GPU 장치 오류가 한 번 재발해 자동화는 별도 NullRHI 프로세스로, 화면 검증은 새 그래픽 에디터로 실행했다. 사용자 직접 플레이의 최종 체감 확인은 남아 있다.

## 공통 패링 설정

`/Game/ExpeditionPortfolio/Presentation/DA_ParryFeedback`을 연다.

- `Intermediate`: 중간 타격 패링 성공의 소리·섬광·슬로모션.
- `Complete`: 전 타격 패링 성공의 소리·섬광·슬로모션·카메라 흔들림.
- 각 Sound의 `Sound / Volume / Delay / Start Time`으로 파일·크기·싱크를 조절한다.
- `Slow Duration`은 실제 시간 기준이다. 0이면 슬로모션을 생략한다.

컨트롤러의 `BattlePresentation > Parry Feedback`이 이 공통 DA를 참조한다. 전 타격 성공 시 공통 피드백을 먼저 재생하고, 종료 후 현재 캐릭터 프로필의 `Parry Counter`를 실행한다. 공통 설정은 불·얼음·검 같은 공격 종류를 알지 못한다. 캐릭터의 방어 자세·피격·회피 모션은 기존 `DefensePresentation`에 남는다.

## 코드의 책임

`UCharacterActionData`는 피해, 이름, 사용 횟수, QTE 등 행동 공통 규칙을 정의한다. `UMagicActionData`는 마법 연출을, `UMeleeActionData`는 근접 접근·타격·복귀 연출을 추가한다. 둘 다 프로필의 평타·기술·반격 슬롯에 연결할 수 있다. 반격은 별도의 입력 QTE가 없는 행동을 사용한다.

`CharacterActionExecutor`는 지원하는 DA 확인, 예상 소요 시간, 실행, 중단 인터페이스다. 현재 `MagicPresentation`과 `UtilityPresentation`이 구현한다. `Executor Component Name`을 비우면 지원하는 첫 실행기를 찾고, 지정하면 그 이름의 컴포넌트만 사용한다. 이후 검 전용 실행기 등도 같은 인터페이스를 구현한다.

실행기는 명중과 종료를 Coordinator에 알릴 뿐 HP를 직접 바꾸지 않는다. 일반 행동은 `NotifyPlayerActionHit / NotifyPlayerActionFinished`, 반격은 `NotifyParryCounterHit / NotifyParryCounterFinished`를 사용한다. Coordinator가 상태·행동 ID·중복·피해·QTE·횟수를 검증한다. UI는 기술 목록을 표시하고 선택 명령만 전달한다.

## 이전 에셋과 호환 경로

기존 `.uasset`은 삭제하거나 이름을 변경하지 않았다. 기존 `DA_BasicMagic`, `DA_StrongMagic`, `DA_ParryCounter`, `DA_PlayerCommands`, `DA_PlayerUtilityPresentation`은 이전값 보관용이다. 이전 DA를 고쳐도 현재 공격·물약·턴 규칙은 바뀌지 않는다. 현재 물약 정의는 `Combat/Items/DA_HealingPotion`, 공용 수량과 턴 대기는 `Combat/Rules/DA_BattleRules`에 있다.

실제 게임은 프로필이나 필요한 실행기가 없으면 전투·명령을 거부한다. 콘텐츠 없는 핵심 로직 검사는 명시적인 자동화 테스트 전용 경로를 사용한다. 예전 Blueprint 명령 API는 현재 프로필의 슬롯으로 연결되며 기본 마법 피해를 생성하지 않는다. 현재 이전 도구는 `Tools/SetupSharedCombatOwnership.py`이며, 기존 목적지 DA의 사용자 조정값을 덮어쓰지 않는다. `SetupCharacterActions.py`는 이전 단계의 역사적 스크립트다.

## 검증

아래는 캐릭터 액션을 처음 분리한 단계의 검증 기록이다. 추가 소유권 분리의 검증 범위는 [공통 규칙과 캐릭터 콘텐츠의 소유권](shared-combat-ownership.md)의 검증 절을 따른다.

`ExpeditionPortfolioEditor Win64 Development` 빌드와 자동화 테스트 60개를 통과했다. 명령 수락 시 피해 스냅샷, 기술별 독립 횟수, 대상 선택 취소·반복 입력·다른 캐릭터의 행동 거부, 마법/근접 실행기 구분을 포함한다. 이전 스크립트 재실행으로 새 DA의 피해와 공통 슬로모션 값이 유지되는 것도 확인했다.

실제 PIE에서 평타 15, 불마법 20과 QTE 보너스 30, 얼음마법 30과 QTE 보너스 45 피해, 얼음마법 사용 횟수 소진, 대상 확정 후 카메라 대기, 세 번째 기술의 W/S 선택·실행, 마법 평타, 근접 반격과 기존 마법 반격을 확인했다. 공통 피드백 후 두 반격 모두 15 피해를 한 번 적용했다. 중간 성공음 `SFX_Hits_FreeAim_Normal_03`, 전 타격 성공음 `SFX_Hit_Sciel_EndSlice_01`, 마지막 슬로모션 1초를 보존했다.

재도전·후퇴와 횟수 복원, 패링 슬로모션 중 치명상으로 결과창 전환 후 재도전 시 시간 배율 1 복원을 확인했다. 두 기술 설명의 표시와 선택 크기, 세 번째 기술의 탐색을 화면으로 확인했다. 임시 행동·프로필 변경은 원래 마법사 구성으로 복원해 저장했다. 별도 동료나 새 검 반격 콘텐츠를 추가한 작업은 아니다. 사용자의 직접 플레이에 의한 최종 체감 검증은 남아 있다.
