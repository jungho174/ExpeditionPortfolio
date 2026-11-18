# 공격 대상 선택

근접 공격 또는 기술 목록의 마법을 고르면 PlayerCommand 상태 안에서 대상 선택을 시작한다. 명령 메뉴가 숨고 선택한 적을 확대하며 마름모 표식과 적 머리 위 HP 바를 표시한다. 하단 이름·HP 숫자·조작키 안내 패널은 제거했다. 적이 한 명이어도 확정 입력이 필요하다. 물약은 등록된 아군 중 회복 가능한 대상을 선택한 뒤 사용한다.

- A / D: 이전 / 다음 대상. 목록 끝에서는 반대쪽으로 순환한다.
- Space: 선택한 공격 또는 아이템의 대상을 확정한다. 마우스 좌클릭은 확정에 사용하지 않는다.
- Backspace / Escape: 취소. 근접은 첫 메뉴, 마법은 기술 목록으로 돌아간다.

선택·취소에는 피해, 턴, 강한 마법 횟수를 소비하지 않는다. 확정할 때 대상 생존과 명령 사용 가능 여부를 다시 검사한다. 중복 확정은 무시한다. 강한 마법은 확정 뒤 기존 준비 모션·타이밍 게이지를 진행한다.

대상 확정 직후에는 먼저 Command Shot(플레이어 숄더뷰)으로 돌아온다. Blend Duration 동안 전환하고 Target Confirm Hold Duration만큼 숄더뷰를 유지한 뒤 공격을 시작한다. 초기값은 0.35초 + 0.12초 = 0.47초다. 두 값 모두 DA_BattlePresentation에서 조절한다. 강한 마법도 이 단계를 거친 뒤 기존 정면 준비 카메라·게이지로 넘어간다. 전환 중에는 메뉴·대상 표식을 숨기고 추가 명령·대상 변경·취소를 차단하며 HP는 유지한다. 전환 완료 전에는 피해나 강한 마법 사용 횟수를 소비하지 않는다. 참가자가 무효화되면 예약을 취소하고 필드로 복귀한다. 물약에도 같은 확정 대기를 적용한다.

복귀 대기 변경 검증: Editor Win64 Development 빌드와 전투 테스트 44개 통과. PIE에서 약한·강한 마법 모두 Command Shot 유지 중 피해가 없고 이후 공격/준비 단계로 진행함을 확인했다. 숄더뷰 대기 화면을 캡처해 메뉴·표식 숨김과 상단 HP 유지도 확인했다. 물약과 두 마법의 지정 사운드 재생도 재확인했다. 최종 전환 속도와 청음 체감은 사용자 직접 플레이 확인 대상이다.

`BP_ThirdPersonPlayerController`의 Class Defaults → Input / Combat / Target Selection에서 Target Confirm Key, Target Previous Key, Target Next Key를 변경한다. 대상 안내 문구는 현재 표시하지 않는다. 메뉴 클릭은 버튼을 놓을 때, 확정은 별도 Space 입력에서 처리하므로 메뉴를 고른 클릭이 그대로 확정되지는 않는다.

`DA_BattlePresentation`의 Target Selection Shot에서 확대 구도를 조절한다. Origin At Enemy는 적을 위치 기준으로 사용한다. Offset X는 플레이어→적 방향, Y는 그 오른쪽, Z는 위쪽이다. Fit Target Bounds가 켜져 있으면 적 Bounds와 HP 바 앵커·화면 비율에 맞춰 거리와 시선 높이를 보정한다. Target Framing Padding으로 여유 공간을 조절한다. 기존 장애물 보정과 Blend Duration도 적용한다. 적 이름은 적 Blueprint의 Combatant Component → Combat / Presentation / Combat Display Name에서 조절한다.

현재 후보는 이 조우 묶음에 등록된 생존 적 전체다. A/D로 실제 적을 변경하며 사망·제거된 후보는 제외한다. 월드의 다른 묶음에 속한 적은 후보에 넣지 않는다. 다수 적 등록·턴·승패는 [다수 적 조우](grouped-encounters.md)를 참조한다.

## 대상 선택 UI 사운드

`Content/ExpeditionPortfolio/Presentation/DA_BattlePresentation`을 열고 Details에서 `Target Selection Sound`를 검색한다. Target Selection → Audio 아래 Sound / Volume / Delay / Start Time으로 파일과 음량·싱크를 조절한다. 최초 연결은 `UI_Battle_Select_Enemy_01`, 볼륨 1, Delay와 Start Time은 0이다. None이면 무음이다.

근접 또는 기술의 대상 선택 단계에 처음 진입하면 한 번 재생한다. 이후 A/D로 실제 선택 액터가 바뀌거나 Space로 확정할 때 재생한다. 적이 한 명이어서 같은 대상을 유지하는 A/D와 중복 갱신에는 추가 재생하지 않는다. 취소 후 다시 들어가면 새 진입이므로 한 번 재생한다. 카메라 확대/전환 횟수나 Tick에 연결하지 않으며 2D UI 사운드로 재생해 적과의 거리와 무관하다. 물약 대상 선택은 별도 아군 진입·변경 사운드를 사용한다.

초기 사운드 연결 검증: Editor Development 빌드와 당시 전투 테스트 44개 통과. PIE에서 약한 마법·강한 마법 재진입·평타 진입 각각 1회(총 3회) AudioComponent 재생을 확인했다. 당시 단일 대상의 A/D 네 번에서는 추가 재생이 없음을 확인했다. 이후 대상 변경·확정 사운드와 다수 적을 연결했다. 최신 검증은 [다수 적 조우](grouped-encounters.md)를 참조하며 최종 청음은 사용자 직접 플레이 확인 대상이다.

UI는 BeginCommandSelection, MoveTargetSelection, ConfirmTargetSelection, CancelTargetSelection 명령만 전달한다. 보류 중 명령과 후보는 C++ Coordinator가 소유한다. 기존 RequestPlayerCommand는 내부/헤드리스 즉시 실행 호환 API이며 선택 중에는 우회 실행을 거부한다.

직접 플레이에서는 근접·두 마법의 선택/취소/재선택, 단일 대상 A/D, 좌클릭 연타, 확정 후 게이지 및 적 턴 진행, 강한 마법 취소 후 재사용, 행동 중 메뉴 숨김과 HP 유지, 물약의 기존 동작을 확인한다. 사용자 직접 플레이 전에는 최종 검증 완료로 처리하지 않는다.

검증: Editor Win64 Development 빌드와 전투 자동화 테스트 43개 통과. 신규 테스트는 확정 전 피해 없음, 강한 마법 취소 후 사용 가능, 이벤트 재진입·중복 확정 거부, 대상 제거 후 정리, 세 후보 좌우 순환·사망/제거 제외, 적 중심 카메라 위치를 검사한다. PIE에서 기술 취소 시 목록 복원, 두 마법 확정 후 행동·게이지·적 턴 진행, 키보드 대상 선택과 Backspace 취소, 실제 좌클릭 평타 확정 후 1회 피해를 확인했다. 최종 체감은 사용자 플레이 확인 대상이다.
