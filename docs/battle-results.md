# 전투 결과와 HP 유지

마지막 적의 HP가 0이 되는 타격에서 Victory, 현재 조작 플레이어의 HP가 0이면 Defeat를 확정한다. 정상 행동은 기존 모션 완료를 기다리지만, 치명타는 완료를 기다리지 않는다. 현재 조우는 적 한 명이며 다수 적 행동 순서나 동료 턴은 이번 변경에 포함하지 않는다.

## 결과 화면

- 약 3초의 실제 시간 동안 슬로모션을 적용하고 World Pause로 애니메이션·VFX·게임 타이머를 정지한다. 0에 가까운 배율로 정지를 흉내내지 않는다.
- 카메라는 마지막 전투 구도를 유지한다. 명령 HUD·방어 안내·QTE는 숨긴다.
- 승리: 왼쪽 위 제목, 오른쪽 아군 이름·HP·체력바, 아래 중앙 준 피해·받은 피해·패링 성공·회피 성공. Space 또는 계속 버튼으로 탐험 복귀한다.
- 승리 제목·통계 이름/수치·계속 버튼·아군 이름/HP 수치는 순백색(RGBA 1,1,1,1)이다. 기존 `WBP_BattleResult`와 `WBP_BattleResultMember`의 해당 TextBlock 색만 조정하며, 배치·글꼴·결과 체력바와 장식선은 각 기존 설정을 따른다.
- 패배: 어두운 배경과 중앙의 붉은 '패배', 아래 재도전/후퇴 버튼만 표시한다. A/D 또는 방향키로 선택하고 Space/Enter로 확정하거나 마우스로 클릭한다. 전투 기록과 HP는 표시하지 않는다.
- 결과창 직후 0.25초와 키 반복 입력을 무시해 마지막 QTE 입력이 결과창을 바로 닫지 않도록 한다.

`Content/ExpeditionPortfolio/Presentation/DA_BattleResults`:

| 항목 | 기본값 | 의미 |
|---|---|---|
| Slow Duration | 3초 | 실제 시간 기준, 0이면 슬로 구간 생략 |
| Slow Factor | 0.15 | 직전 게임 속도에 곱하는 배율 |
| Widget Class | WBP_BattleResult | 승리/패배 레이아웃 |

PC Blueprint의 BattleResultPresentation → Presentation Data에서 연결한다. `WBP_BattleResult`의 CN_Victory / CN_Defeat에서 각 레이아웃을 편집한다. CN_Defeat는 기본 Collapsed이며 Designer 미리보기 후 기본값을 복원한다. 파티 행은 `WBP_BattleResultMember`에서 편집한다. `Tools/SetupBattleResults.py`는 최초 레이아웃을 만들고 이후 기존 배치·시간·클래스 조정을 보존한다.

3초 변경 검증: Editor Development 빌드와 `Tools/UpdateBattleResultSlowDuration.py`의 저장 완료 로그를 확인했다. 새 에디터 PIE에서 패배와 재도전 후 승리를 실행해 각각 실제 3.0초 뒤 결과창·World Pause가 활성화되는 것을 측정했고, 승리 계속으로 정상 복귀했다. 사용자 직접 플레이의 최종 체감 확인은 남아 있다.

승리 버튼은 '계속'만 표시하고 패배창 하단 조작키 안내는 비웠다. 입력 키 동작은 유지한다. 직접 문구를 수정하려면 콘텐츠 브라우저에서 `Content/ExpeditionPortfolio/UI/WBP_BattleResult`를 열고 Designer의 Hierarchy에서 `CN_Victory → BTN_Continue → TXT_Continue`를 선택한 뒤 Details → Content → Text를 편집한다. 패배 안내는 `CN_Defeat → TXT_DefeatHint`의 Text이며 현재 빈 값이다. 패배 제목은 TXT_Defeat, 버튼 글자는 BTN_Retry → TXT_Retry / BTN_Retreat → TXT_Retreat다. 글꼴·크기·색은 선택한 Text 위젯의 Appearance, 위치·크기는 버튼/패널의 Canvas Panel Slot에서 조절하고 Compile/Save한다. 이름과 HP 수치는 게임에서 갱신하므로 오른쪽 행의 외형은 WBP_BattleResultMember에서 편집한다.

## 통계와 복귀

승리의 계속을 누르면 전투 정리 후 적 Actor를 Destroy하고 조우 완료 상태를 기록한다. 결과창과 슬로모션 동안에는 적을 유지한다. 같은 게임 실행 중 맵을 재진입해도 완료된 조우의 적은 다시 나타나지 않는다. 고유 ID, 재도전/후퇴 정책과 세션 범위는 `docs/encounter-persistence.md`를 참조한다.

통계 원본은 C++ Coordinator가 소유한다. 등록된 전투 참가자의 실제 HP 감소만 합산하므로 과잉 피해는 남아 있던 HP까지만 기록한다. 평타·마법·QTE 보너스·반격을 포함하고 회복과 전투 진입 이전 피해는 제외한다. 성공한 방어만 한 번 집계하며 입력 시도나 중복 Notify는 추가하지 않는다. 승패 확정 때 파티 결과를 복사하고 이후 전투 입력·타격·다음 턴 예약은 거부한다.

재도전은 아군/적 HP와 Actor 배치를 전투 진입 당시로 복원하고 물약·강한 마법 사용 가능 횟수와 통계를 초기화해 같은 조우를 다시 시작한다. 추가 등록한 아군도 등록 시점의 HP와 배치를 복원한다. 일반 회복 API와 달리 이 복원 경로는 쓰러진 캐릭터를 되살릴 수 있다.

후퇴는 같은 진입 스냅숏을 복원하되 전투를 시작하지 않는다. 조우 영역 바깥에서 접근 방향의 뒤쪽으로 이동한 후 트리거를 재활성화한다. 다시 걸어 들어가면 조우할 수 있다. Level의 BattleEncounterTrigger → Encounter / Retreat:

- Retreat Distance: 조우 경계와 캐릭터 반경에 더하는 거리(cm), 기본 100.
- Retreat Point: 특정 맵에서 자동 위치 계산 대신 사용할 고정 지점 Actor. 비어 있으면 접근 방향으로 자동 계산한다.

후퇴 후보는 바닥 기울기, 캐릭터 캡슐 공간과 영역 밖 여부를 확인한다. 안전한 지점을 찾지 못하면 결과창을 유지하고 실패 안내를 표시하므로 해당 조우에 고정 지점을 지정한다. 승리 후에는 쓰러진 적 조우를 다시 활성화하지 않는다.

접근 방향은 전투 진입의 대치 회전 정렬보다 먼저 기록한다. 최종 빌드의 PIE에서도 후퇴 위치 (1214, 1446.56, 292.15), 진입 HP 20 복원, 영역 밖 탐험 유지와 재진입 전투를 확인했다. 이 좌표는 현재 테스트 맵의 관측값이며 다른 조우의 고정 설정값이 아니다.

## HP 유지 범위

아군 Combatant Component의 Persistence를 `Unique Party Member`로 지정하고 Character Actions 프로필의 Character ID 하나를 HP 저장 키로 사용한다. 현재 주인공 ID는 `PlayerMage`다. 동료는 서로 다른 고유 ID를 사용하며 같은 ID가 동시에 생성되면 중복 등록을 거부한다. 같은 실행 중 맵 전환·캐릭터 재생성에는 해당 ID로 HP를 복원한다. 적은 `Transient`로 지정해 같은 골렘을 여러 개 배치해도 개별 HP를 유지한다. 이전 Persistent Character Id 필드는 더 이상 저장 키로 사용하지 않는다.

표시 이름은 Combat / Presentation / Combat Display Name에서 변경한다. 최대 HP는 현재 캐릭터 설정을 사용하며 저장 HP를 그 범위로 제한한다. 전투 진입마다 HP를 회복하지 않는다. 새 실행/새 PIE는 새 세션이므로 초기 HP로 시작하며 디스크 저장·불러오기는 포함하지 않는다.

구조: BattleCoordinator가 승패·통계·복원 명령을 소유하고 GameInstance의 PartyHealthSubsystem이 HP를 유지한다. BattleResultPresentationComponent는 실시간 슬로모션·정지·입력 모드·위젯을 관리하며 종료/중단 시 원래 속도를 복원한다. 위젯은 결과를 표시하고 명령만 전달한다.

## 실행 검증

2026-11-18 승리 글자색 변경: Editor Win64 Development 빌드 후 새 프로세스의 실제 PIE에서 승리 결과창을 띄워 12개 TextBlock의 RGBA `(1,1,1,1)`과 실제 화면의 흰색 표시를 확인했다. 이 색상 검증에서는 피해 확인을 마친 뒤 남은 적 HP를 진단 API로 소진해 결과창에 진입했으며, 결과창 총 피해를 일반 플레이만으로 달성한 기록으로 해석하지 않는다. 사용자 직접 플레이의 최종 색상 확인은 남아 있다.

이전 결과 기능 구현 당시 `ExpeditionPortfolioEditor Win64 Development` 빌드와 전투 자동화 테스트 53개가 통과했다. 결과 확인 대기, 실제 HP 감소/과잉 피해, 진입 전 피해 제외, 등록된 파티 결과, 다음 전투 HP 유지, 반복 재도전·기술/물약 복원, 저장 HP ID 분리와 비유한 값 거절을 검사했다. 바닥이 없어 후퇴할 수 없는 경우에도 패배 결과와 HP를 그대로 유지하고 재도전할 수 있음을 검사했다.

실제 PIE에서 불마법 치명타 → 약 2초 슬로모션 → World Pause와 승리창, Space 확인 → 원래 속도·탐험 카메라·HP 70 유지를 확인했다. 패배창은 HP·통계 없이 중앙 제목/두 버튼만 표시하며 마우스 재도전에서 진입 HP 20·적 HP 100·물약 수량·통계 초기화를 검사했다. 다시 패배한 뒤 D/Space 후퇴에서 HP 20과 시간 복원, 조우 영역 바깥 복귀, 1초 이상 재조우 없음과 재진입 조우를 확인했다.

캐릭터 재생성(RestartPlayer)과 OpenLevel을 통한 현재 맵 재로딩에서 HP 37 유지를 확인했다. 서로 다른 플레이용 맵 사이를 이동하는 화면은 현재 해당 콘텐츠 연결이 없어 미검증이다. 저장소의 다른 템플릿 맵은 별도 캐릭터/규칙이므로 임의로 연결하지 않았다.

회피 1회 후 패링 반격으로 승리하는 PIE에서 기존 패링 슬로모션 종료 → 반격 명중 → 결과 슬로모션/정지를 확인했다. 결과창의 회피/패링 각 1, 실제 준 피해 31, 받은 피해 0과 HP 100을 대조했다. 수치는 현재 저장된 피해 설정을 읽고 과잉 피해를 제한해 계산했다. 결과 대기 중 게임 시간 정지와 전투 입력 거절, 확인 후 시간 배율 1 복원도 검사했다. 추가 동료의 행은 현재 실제 파티 콘텐츠가 없어 화면 미검증이며 등록된 2명 결과 스냅숏은 자동화 검사한다.

사람의 직접 플레이를 통한 UI 배치·슬로모션 체감과 후퇴 거리의 최종 검증은 남아 있다.
