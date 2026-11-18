# 전투 카메라와 강한 마법 타이밍

명령 선택 중 숄더뷰는 `Presentation/DA_BattlePresentation → Command Shot`에서 조절한다. 현재 Offset (-300, 180, 100), Look Height 69.1, FOV 60으로 설정해 마네킹의 엉덩이 바로 위에서 화면 하단이 잘리도록 맞췄다. 행동별 준비·공격·대상 선택 구도는 변경하지 않았다. 몸통 옆 메뉴 위치는 `Characters/PlayerMage/Combat/DA_PlayerMageVisual → Menu Screen Offset`에서 조절한다.

2026-11-10: Editor Win64 Development 빌드 성공. 에디터 재시작 후 저장값·다른 카메라 구도 보존·프레임 제한 0을 확인했고, 기본 및 확대 PIE 화면에서 상체 구도와 첫 메뉴의 몸통 옆 배치를 확인했다. 기술·아이템 목록, 뒤로가기와 선택 확대도 화면에서 확인했다. 구도의 최종 체감은 사용자 직접 플레이 확인이 남아 있다.

현재 행동별 준비/실행 구도는 각 행동 DA의 `Camera Data`에 연결된 `ActionCameraData`에서 편집한다. 현재 마법사의 카메라 DA는 `Characters/PlayerMage/Combat/Cameras`에 있다. 공통 `DA_BattlePresentation`은 전환·가림 보정과 기본 구도를 제공한다. [공통 규칙과 콘텐츠 소유권](shared-combat-ownership.md)을 참조한다.

전투 진입 시 플레이어와 적이 서로 마주보고, 전투 카메라로 전환한 뒤 명령을 받는다. 강한 마법은 2.5초 준비 중 정면 구도와 마름모 게이지를 보여준다. 준비가 끝나면 공격 구도로 전환하고 기존 마법을 발동한다. 적 턴에는 두 캐릭터와 방어 모션을 볼 수 있는 구도를 사용한다. 승리 후 탐험 카메라와 회전 설정을 복원한다.

## 타이밍 판정

현재 QTE는 공통 판정/연출 시스템으로 분리했다. 하얀 구간은 Regular(기본 피해), 노란 구간은 Perfect(기존 보너스), 미입력은 Miss다. 네 이벤트 사운드는 DA_QTEPresentation에서 공통 설정한다. 상세 구조와 재사용 방법은 `docs/common-qte.md`를 참조한다.

**게이지 종료 후 준비 동작을 더 보여주려면** `Content/ExpeditionPortfolio/Animation/Magic/DA_StrongMagic`을 열고 Details에서 `Post Timing Hold Duration`을 검색한다. 위치는 Magic → Timing이며 최초 연결값은 **0.5초**다. 0이면 추가 유지 없음, 0.8이면 게이지 종료 후 0.8초 동안 준비 모션·준비 VFX·정면 카메라를 더 유지한다. 저장 후 다음 강한 마법에서 확인한다. Charge Duration은 기존 게이지 시간이며 이 값을 바꾸지 않으면 게이지 속도와 성공 구간은 유지된다.

스페이스는 판정 결과만 기록하며 조기 발사를 시키지 않는다. 성공·실패·미입력 모두 게이지 종료 후 같은 추가 유지 시간을 거친다. 추가 유지 구간에는 게이지 입력이 닫히고 피해는 없으며 준비가 모두 끝난 뒤 공격을 시작한다. 매우 긴 준비 시간은 준비 모션 재생 속도를 낮춰 전체 구간을 덮는다. 기본 0.5초 추가가 원본 모션보다 짧은 경우에는 기존 모션 속도를 유지한다.

마법 기본 피해는 `Content/ExpeditionPortfolio/Animation/Magic`의 `DA_BasicMagic` 또는 `DA_StrongMagic`을 열고 **Magic / Combat / Base Damage**에서 조절한다. 초기값은 약한 마법 25, 강한 마법 40이다. 저장 후 다음 명령부터 반영되며 이미 시작한 공격은 기존 피해를 유지한다. 강한 마법의 성공 피해는 Base Damage × Timing Damage Multiplier다. 예를 들어 기본 50, 배율 1.5이면 성공 시 75다. 0 피해도 설정할 수 있다. 근접 평타·패링 반격·적 공격 피해에는 이 설정을 적용하지 않는다.

`Tools/SetupMagicDamage.py`는 두 에셋의 최초 피해량만 초기화하고 메타데이터 표식으로 이후 사용자 조정을 보존한다.

`Content/ExpeditionPortfolio/Animation/Magic/DA_StrongMagic`의 Magic / Timing:

| 프로퍼티 | 초기 설정 | 의미 |
|---|---:|---|
| Enable Timing Bonus | 켬 | 강한 마법 준비 구간의 보너스 판정 사용 |
| Charge Duration | 2.5초 | 기존 준비 모션과 게이지 전체 시간 |
| Timing Success Duration | 0.3초 | 준비 종료 직전 성공 구간 |
| Timing Damage Multiplier | 1.5 | 성공 시 피해 40 → 60 |

게이지는 위 꼭짓점부터 시계 방향으로 줄어들며 마지막 왼쪽 위 구간이 노란 Perfect 영역이다. 노란 구간도 진행률이 도달하면 흰색에 이어 사라진다. 기본 입력은 Space이며 첫 누름만 판정한다. 기본 설정 예시에서 2.2초 이상 2.5초 미만은 Perfect이며 그 전 하얀 구간은 Regular다. 연타나 누르고 있기로 재시도할 수 없다. 미입력(Miss)과 Regular는 기본 피해이고 Perfect만 설정된 보너스를 적용한다. 입력 결과는 발사 시점을 앞당기지 않는다. 추가 준비 유지 시간까지 마친 뒤 모션의 타격 순간에 피해를 한 번 적용한다.

성공 판정 후에도 실제 타격 전 행동이 중단되면 피해를 적용하지 않는다. 이미 적용한 피해는 되돌리지 않는다. 강한 마법의 전투당 1회 제한은 유지한다. 약한 마법·근접 평타·물약에는 이번 타이밍 보너스를 적용하지 않는다.

시간 판정과 보너스는 Coordinator의 행동 ID와 C++ 시간창을 사용한다. UI 애니메이션이나 UI 프레임 수로 판정하지 않는다. 설정은 준비 시작 때 복사하며 진행 중에는 성공 구간과 배율을 유지한다. 준비 중에는 피해를 적용하지 않고, 종료·중단·전투 정리 시 입력을 차단한다.

입력 키는 `BP_ThirdPersonPlayerController`의 Input / Combat / Attack Timing Key에서 바꾼다. 게이지의 중앙 문구도 실제 설정 키를 표시한다. 명령 턴은 마우스·키보드를 받으며, 행동과 방어 구간은 게임 입력 모드로 전환하여 남아 있는 버튼 포커스가 Space를 가로채지 않게 한다.

## 카메라 조절

`Content/ExpeditionPortfolio/Presentation/DA_BattlePresentation`:

| 프로퍼티 | 의미 |
|---|---|
| Camera Rig Class | BP_BattleCameraRig. 구도 계산을 Blueprint에서 재정의 가능 |
| Intro Duration | 진입 후 명령을 받기까지의 대기. 초기 0.55초 |
| Blend Duration | 구도 사이 전환. 초기 0.35초 |
| Command Shot | 플레이어 어깨 너머에서 적을 보는 명령 구도 |
| Target Selection Shot | 선택한 적을 확대하는 구도. Origin At Enemy를 켜서 적 중심 기준 Offset 사용 |
| Preparation Shot | 강한 마법의 얼굴·양손·준비 효과를 보는 정면 구도 |
| Attack Shot | 플레이어 공격을 보여주는 구도 |
| Enemy Shot | 적 공격과 플레이어 방어를 함께 보는 구도 |

각 Shot의 Offset은 플레이어 캡슐 중심 기준이다. X는 플레이어에서 적으로 향하는 축, Y는 그 축의 오른쪽, Z는 월드 위쪽이다. Look At Enemy Weight는 0이면 플레이어, 1이면 적을 바라본다. Look Height는 그 기준점의 높이 보정이며 Field Of View는 화각이다. 배치 방향이 달라도 같은 상대 구도를 계산한다. 두 카메라를 교대로 사용해 이전 카메라 위치를 이동시키며 생기는 순간 이동을 피한다. 뼈대의 잔떨림을 카메라 기준으로 사용하지 않는다.

각 Shot의 Avoid World Obstacles가 켜져 있으면 시선 기준점에서 카메라까지 Camera 채널로 구체 스윕하고 벽 앞쪽으로 카메라를 당긴다. Collision Radius의 초기값은 12cm다. 전투 중 실제 캐릭터 위치를 카메라 때문에 옮기지 않는다. 이번 카메라는 기존 필드 배치를 사용하므로 맵의 벽·높은 장애물에 맞춘 구도 튜닝이 필요할 수 있다. 승리·전투 취소·패배와 컴포넌트 종료 시 원래 뷰 대상을 복원하고 전투용 카메라를 제거한다. 카메라가 없거나 진입 완료 신호가 오지 않아도 대체 경로로 전투가 진행된다.

## 게이지 외형과 대치 방향

`WBP_AttackTiming`의 Class Defaults / Timing / Appearance에서 마름모 반지름, 선 두께, 진행 색상과 성공 영역 색상을 조절한다. 테두리는 안티앨리어싱 선으로 그리며 현재 C++ 시간창의 진행률을 표시한다. 중앙은 어두운 원과 실제 입력 키, 아래는 성공·실패 문구다. Blueprint의 Render Transform / Translation X는 초기 220으로 캐릭터 얼굴·양손 오른쪽에 표시한다. 표시 중에만 UI를 갱신한다. 명령 메뉴와 별도 위젯이며 상단 HP는 유지한다.

적이 옆을 보던 원인은 같은 Quinn 메시를 쓰면서 플레이어 메시 Yaw -90도와 적 메시 Yaw 0도가 달랐던 것이다. `BP_PrototypeEnemy`의 메시 기본 회전을 플레이어와 맞췄다. 전투 진입 시 양쪽 액터가 서로를 바라보고, 이동 방향·컨트롤러에 의한 자동 회전은 잠시 잠근 뒤 전투 종료 시 원래 설정으로 복원한다. 공격 시작 때도 상대 방향을 갱신한다.

`Tools/SetupBattlePresentation.py`는 관련 Blueprint·Data Asset과 연결을 생성한다. 재실행 시 기존 카메라 구도와 타이밍 수치를 덮어쓰지 않는다. 에셋 저장은 다른 Unreal 에디터·명령행 프로세스를 종료한 뒤 단독 실행한다.

## 직접 플레이 확인

조우 후 서로 마주보고 숄더뷰로 전환되는지 확인한다. 기술 → 강한 마법에서 얼굴·양손이 보이고 명령 메뉴가 숨는지, 마지막 노란 테두리에서 Space를 눌러 성공하는지 확인한다. 이른 입력·미입력도 각각 시도하여 피해 40, 성공 시 60인지 확인한다. 준비 완료 전 발사되지 않고 기존 방어·다음 명령 턴·승리 후 탐험 복귀가 유지되어야 한다. 최종 구도와 입력 난이도는 직접 플레이로 확정한다.
