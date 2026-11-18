# 물약·마법 사운드

현재 물약 파일·음량·싱크는 `Combat/Items/DA_HealingPotion → Use Sound`에서 변경한다. 사용 타이밍은 `Characters/PlayerMage/Combat/DA_PlayerMageItemUse → Apply Time`이다. 평타·마법·반격은 현재 캐릭터의 행동 DA, 패링 성공음은 공통 `DA_ParryFeedback`을 사용한다. 아래 이전 구축 단계의 DA 경로는 보관용이며 [현재 소유권과 편집 위치](shared-combat-ownership.md)를 우선한다.

현재 캐릭터의 평타·기술·반격과 공통 패링 설정은 [캐릭터별 전투 설정](character-actions.md)으로 분리됐다. 아래의 이전 DA 경로 예시보다 새 프로필의 연결을 우선한다.


## 피격·회피·이동·얼음 명중

| 동작 | DA / 프로퍼티 | 최초 연결 |
|---|---|---|
| 실제 적 공격 피격(방어 실패·미입력) | Animation/Defense/DA_PlayerDefensePresentation → Hit Reaction / Audio → Hit Sound | SFX_Hit_Magic_Generic_Medium_01 |
| 회피 모션 시작 | 같은 DA → Dodge / Audio → Dodge Sound | F_ENM_Swordman_Run_02 |
| 평타 접근 시작 | Animation/Commands/DA_PlayerUtilityPresentation → Melee / Audio → Approach Sound | F_ENM_Swordman_Run_08 |
| 평타 복귀 시작 | 같은 DA → Melee / Audio → Return Sound | F_ENM_Swordman_Run_09 |
| 얼음마법 실제 명중 | Animation/Magic/DA_StrongMagic → Magic / Audio → Impact Sound | SFX_FB_Effect_Frozen_Deactivate_01 |

새 방어·이동 항목을 펼치면 Sound / Volume / Delay / Start Time을 편집한다. 강한 마법 명중음은 기존 Impact Sound / Impact Sound Volume / Impact Sound Delay / Impact Sound Start Time을 사용한다. None은 무음이다. 방어 성공에는 피격음을 재생하지 않고, 거절된 회피 입력은 회피음을 추가하지 않는다. 평타를 복귀 전에 중단하면 복귀음도 발생하지 않는다. 이미 발생한 이벤트의 소리와 지연은 기존 공통 사운드 수명 정책을 따른다. Tools/SetupDefenseTravelSounds.py는 최초 연결 후 사용자 수치·파일 변경을 보존한다.

검증: Editor Development 빌드 및 기존 전투 테스트 49개 통과. PIE 5개 흐름에서 AudioComponent의 실제 재생 상태를 추적했다. 평타+미입력은 접근/복귀/피격 각각 1회, 회피 성공은 회피 1회·피격 0회, 패링 성공도 피격 0회, 강한 마법은 얼음 명중음 1회였다. 접근 중 Montage를 중단하면 접근음만 발생하고 복귀음·평타 피해는 없음을 확인했다. 중복 방어 입력은 추가 사운드를 내지 않았다. 사용자 직접 청음으로 음량·싱크 체감을 확인하는 단계는 남아 있다.

## 메뉴·턴·아군 대상 선택

뒤로가기 사운드는 아이템 대상 선택 → 아이템 목록에서 Item Menu(`UI_Battle_Item_Menu_Open`), 아이템 목록/기술 목록/기술 대상/평타 대상에서 이전 메뉴로 돌아갈 때 Skill Menu(`UI_Skill_NavigatePage`)를 사용한다. Backspace·Escape·뒤로 버튼은 같은 경로를 사용한다. 실제 복귀 성공 때만 1회 재생하며 첫 메뉴, 확정 대기, 명령 턴 밖에서는 재생하지 않는다. 키를 누르고 있을 때의 자동 반복도 무시한다.

뒤로가기 변경 검증: Editor Development 빌드 성공. PIE에서 실제 Backspace로 아이템 대상 선택을 취소하고 아이템 목록 복귀를 확인했다. 나머지 네 경로도 실행하여 복귀 성공마다 지정된 사운드가 정확히 1회 기록되는지 검사했다. 첫 메뉴에서 뒤로가기를 두 번 호출해도 무음이며 물약·턴을 소비하지 않음을 확인했다. 최종 청음은 사용자 플레이 확인 대상이다.

`Presentation/DA_BattleUIAudio`에서 Item Menu, Skill Menu, No Uses, Ally Target Start, Ally Target Move, Ally Turn, Enemy Turn의 Sound / Volume / Delay / Start Time을 조절한다. PC의 BattlePresentation 컴포넌트 → UI Audio Data로 연결한다. 첫 메뉴에서 아이템 목록을 열면 Item Menu, 기술 목록을 열면 Skill Menu를 재생한다. 횟수 소진 기술을 클릭하면 No Uses만 재생하고 행동은 거절한다. 사용 불가 기술 버튼은 어둡게 표시하되 거절음 입력을 받는다.

대상 진입음 조정: 위 설명의 물약·기술·평타 선택 시 메뉴음 중복은 제거했다. 첫 화면에서 아이템 목록을 열 때만 Item Menu, 기술 목록을 열 때만 Skill Menu를 재생한다. 물약을 골라 아군 선택에 진입하면 Ally Target Start만, 기술·평타를 골라 적 선택에 진입하면 기존 DA_BattlePresentation의 Target Selection Sound만 재생한다. 승인된 Space 확정은 아군일 때 Ally Target Move, 적일 때 Target Selection Sound를 1회 재생한다. 확정 후 전환 중 중복 입력은 무음이다. 뒤로가기 음과 횟수 소진 No Uses는 유지한다.

물약은 A/D로 아군을 선택하고 Space로 확정한다. 진입은 Ally Target Start, 유효한 A/D 입력은 Ally Target Move를 재생한다(후보 1명일 때도 재생). 취소는 아이템 목록으로 돌아가며 물약과 턴을 소비하지 않는다. 풀피인 생존 아군도 물약 후보에 포함한다. 쓰러진 대상은 제외하며, 풀피 사용은 HP를 바꾸지 않고 기존 효과·사운드를 재생하고 물약 1개와 턴을 소비한다. 추가 동료는 전투 명령 단계에서 Register Battle Ally로 등록할 수 있다. 파티 턴 시스템을 추가한 것은 아니다.

풀피 사용 검증: Editor Development 빌드와 전투 테스트 49개 통과. 선택 취소 시 미소비, 확정 중복 거부, HP 변경 이벤트 없음, 물약 1개·턴 소비와 다음 적 턴을 검사했다. PIE에서 HP 100/100의 자신 선택 화면, 취소 후 재선택·사용, HP 유지·수량 차감, SFX_FB_Heal_Tint_Light_01의 1회 재생과 1초 턴 대기 후 적 턴 진입을 확인했다. 직접 플레이의 최종 청음은 남아 있다.

플레이어 명령 턴 진입은 Ally Turn, 적 공격 예고 진입은 Enemy Turn을 1회 재생한다. 메뉴 복귀·대상 취소는 턴 시작으로 취급하지 않는다. 모든 UI 사운드는 2D이며 None으로 비울 수 있다. 기본 연결과 Space 키 설정은 Tools/SetupBattleUIAudio.py가 담당하며 재실행해도 기존 사운드 튜닝을 초기화하지 않는다.

메뉴 변경 검증: Editor Win64 Development 빌드 및 전투 테스트 47개 통과. 등록된 추가 아군의 회복, 선택/취소 중 미소비, 확정 중 대상 제거, 단일 후보 A/D 알림과 중복 확정 차단을 검사했다. 실제 PIE에서 물약 취소·재선택·네이티브 Space 확정·1회 회복/소비, 강한 마법 사용 후 NoAP 거절, 평타의 좌클릭 확정 거부와 Space 확정·1회 피해, 모든 요청 사운드의 재생 로그를 확인했다. 사람이 직접 플레이하며 청음·입력감 확인은 남아 있다.

콘텐츠 브라우저에서 해당 DA를 더블클릭하고 Details의 검색창에 `Sound`를 입력한다. 소리 선택은 에셋 드롭다운 또는 콘텐츠 브라우저에서 선택한 사운드를 화살표 버튼으로 지정한다. `None`으로 비우면 재생하지 않는다.

| 동작 | Data Asset | 에디터 항목 | 최초 연결 |
|---|---|---|---|
| 회복 물약 | Animation/Commands/DA_PlayerUtilityPresentation | Potion → Audio → Potion Sound / Potion Sound Volume | SFX_FB_Heal_Tint_Light_01 |
| 약한 불마법 | Animation/Magic/DA_BasicMagic | Magic → Audio → Attack Sound / Attack Sound Volume | SFX_FB_Effect_Burn_Activate_01 |
| 강한 마법 | Animation/Magic/DA_StrongMagic | Magic → Audio → Attack Sound / Attack Sound Volume | SFX_FB_Effect_Burn_Deactivate_01 |

위 경로의 공통 시작점은 `Content/ExpeditionPortfolio`다. 볼륨 1은 원래 크기, 0은 무음, 최대 2다. 사운드를 직접 바꾼 뒤 DA를 저장한다. 기존 패링 사운드는 DA_ParryCounter의 Parry Impact Sound 설정을 유지한다.

물약은 Potion Apply Time에 사용 효과가 승인됐을 때 재생한다. 풀피 사용도 재생하며 수량이 없거나 유효한 생존 대상이 없어 명령이 거절되면 재생하지 않는다. 약한 마법은 왼손 발사 Notify, 강한 마법은 준비·타이밍 게이지 이후 공격 발동 Notify에 맞춰 한 번 재생한다. 대상 선택과 준비 구간에서는 이 발사 사운드를 재생하지 않는다. 중복 Notify/중복 타격으로 소리를 겹쳐 틀지 않는다. 정상 발사 후 짧은 사운드는 자연스럽게 끝나며 공격 모션 종료에 맞춰 자르지 않는다.

사운드는 시전자 위치에서 재생하며 감쇠 설정은 사운드 에셋의 기존 설정을 따른다. 짧은 일회성 효과음을 사용한다. `Tools/SetupCombatSounds.py`는 최초 연결 후 메타데이터로 사용자 교체·볼륨 설정을 보존한다. 원본 사운드 에셋을 변경하거나 복사하지 않는다. 해당 Audio 폴더의 기존 Git 제외 규칙도 유지한다.

PIE에서 물약·두 마법 모두 지정한 SoundWave를 가진 AudioComponent의 실제 재생 상태와 행동당 1회 시작을 확인했다. 사용자 청음과 볼륨 체감은 직접 플레이 확인 대상이다.

## 발사·명중과 싱크 조절

| DA | 위치 | 사운드 기준 / 초기 연결 |
|---|---|---|
| DA_BasicMagic | Magic → Audio → Attack Sound | 손에서 발사 / Burn_Activate_01 |
| DA_BasicMagic | Magic → Audio → Impact Sound | 적에게 도착하여 피해·폭발 / Burn_Feedback_01 |
| DA_ParryCounter | Magic → Audio → Attack Sound / Impact Sound | 반격 발사 / 명중. 약한 불마법과 같은 두 사운드 |
| DA_PlayerUtilityPresentation | Melee → Audio → Melee Hit Sound | 평타의 실제 타격 / SFX_Hit_GestralPunch_Medium_01 |
| DA_PlayerUtilityPresentation | Potion → Audio → Potion Sound | 실제 회복 / 기존 물약음 |
| DA_StrongMagic | Magic → Audio → Attack Sound | 강한 마법 발동 / 기존 Burn_Deactivate_01 |
| DA_ParryCounter | Magic → Counter Impact → Parry Impact Sound | 패링 성공 섬광 / 기존 패링 성공음 |
| DA_BattlePresentation | Target Selection → Audio → Target Selection Sound | 대상 선택 진입·실제 선택 변경 / UI_Battle_Select_Enemy_01 (2D) |

각 항목의 Sound(파일), Volume(0~2), Delay(0~5초), Start Time(초)을 직접 바꿀 수 있다. Details 검색창에 `Sound`를 입력하면 함께 찾기 쉽다. Parry Impact Sound에 대응하는 나머지 항목 이름은 Parry Sound Volume / Delay / Start Time이다.

- Delay: 기준 이벤트 이후 재생을 늦춘다. 0이면 즉시, 0.1이면 0.1초 뒤에 재생한다. 전투 피해·모션·VFX 시점은 바꾸지 않는다.
- Start Time: 음원 파일의 앞부분을 건너뛰고 시작한다. 예를 들어 앞에 무음 0.08초가 있으면 0.08로 잘라 싱크를 맞춘다. 이벤트 발생 이전에 소리를 미리 트는 기능은 아니다. 음원 전체 길이 이상이면 들리지 않을 수 있다.
- 딜레이는 게임 시간 기준이므로 패링 슬로모션의 영향을 받는다. 패링 성공음은 기본 Delay 0으로 섬광과 함께 재생한다.

승인된 발사·명중·회복 이벤트당 한 번만 예약한다. 실제로 발생한 이벤트의 사운드와 재생 대기는 행동이 끝나도 유지하며, 시전자 제거/PIE 종료 후에는 재생하지 않는다. 반격 취소나 발사체 중단으로 명중이 없으면 폭발음도 예약하지 않는다. 지연 시간·볼륨·시작 위치·재생 위치는 이벤트 시점 값을 복사해 사용한다. 사운드를 비우면 무음이다. 원본 사운드 에셋과 기존 사용자 튜닝은 유지한다.

검증: Editor Development 빌드와 전투 테스트 44개 통과. PIE에서 약한 마법의 발사/폭발 1회씩, 평타 타격 1회, 패링 반격의 발사/폭발 1회씩 재생을 AudioComponent 재생 상태로 확인했다. 약한 마법의 Delay를 0.2/0.25초, Start Time을 0.01초로 임시 변경한 재생도 확인하고 원래 값으로 복원했다. 최종 청음과 볼륨·싱크 체감은 사용자 직접 플레이 확인 대상이다.
