# 평타 도약·백대시

현재 캐릭터의 평타·기술·반격과 공통 패링 설정은 [캐릭터별 전투 설정](character-actions.md)으로 분리됐다. 아래의 이전 DA 경로 예시보다 새 프로필의 연결을 우선한다.


평타는 AM_Dash와 같은 MM_Dash 모션을 사용해 적 앞으로 도약하고, 기존 맨손 주먹 공격 후 적을 바라본 채 백대시로 원래 자리로 복귀한다. 프로젝트 전용 root-lock 복사본 AS_MeleeDash와 AM_MeleeDashForward / AM_MeleeDashBack을 사용한다. Back은 같은 모션의 역방향 재생이다. 원본 AM_Dash와 회피 설정은 보존한다.

현재 마법사는 `Characters/PlayerMage/Combat/DA_PlayerMageBasicAttack`에서 접근·타격·복귀 항목을 조절한다. 이전 `DA_PlayerUtilityPresentation`은 보관용이며 현재 평타를 변경하지 않는다.

| 항목 | 최초 연결값 | 의미 |
|---|---|---|
| Approach Montage | AM_MeleeDashForward | 접근 도약 모션 |
| Return Montage | AM_MeleeDashBack | 뒤로 복귀하는 모션 |
| Approach Duration | 0.32초 | 적 앞까지 이동하는 전체 시간 |
| Return Duration | 0.30초 | 원위치로 복귀하는 전체 시간 |
| Approach Leap Height | 30cm | 접근 중 추가 도약 높이 |
| Return Leap Height | 18cm | 복귀 중 추가 도약 높이 |
| Target Distance | 기존 설정 유지 | 적 앞에서 멈추는 거리 |
| Hit Time | 기존 설정 유지 | 주먹 공격 Montage 시작 후 타격 시간 |

접근·복귀 모션 재생 속도는 각각의 이동 시간에 맞춰 자동 계산한다. 도약 높이는 원본 모션의 자세 변화에 추가하는 메시 높이이며 0이면 추가 높이가 없다. 주먹 공격과 타격 사운드는 기존 설정을 유지한다. 물약과 마법에는 적용하지 않는다.

이동은 캐릭터 메시를 움직이고 Actor/캡슐의 조우 기준 위치는 유지한다. 공통 `BattleGrounding`으로 접근 전에 실제 캡슐 크기의 지면 높이와 몸통 충돌을 검사하고, 복귀는 같은 높이 경로를 역순으로 사용한다. 기존 도약 높이는 그 경로 위에 추가한다. 각 캐릭터의 근접 공격 DA에서 `Melee / Grounding`의 Enabled, Max Step Height, Max Drop Height, Sample Spacing, Max Slope Angle을 조절한다. 기본 허용 단차는 60cm, 낙차는 150cm다. 바닥이 없거나 벽·과도한 단차로 막히면 피해 없이 행동을 중단한다. 매 프레임 지면 검색, 새 발 IK나 장애물 우회 탐색은 사용하지 않는다. 이동·공격 중단과 전투 종료 때 원래 메시 Transform을 복원한다. 접근 중 중단하면 타격과 타격음이 없고, 이미 적중한 후 복귀를 중단하면 적용한 피해는 유지한다.

Tools/SetupMeleeDash.py는 원본을 보존하여 새 복사본을 만들고 최초 연결 이후의 DA 튜닝을 메타데이터로 보존한다. 기존 SetupCombatCommandMenu.py도 대시 연결 표식이 있으면 예전 Jog 모션으로 덮어쓰지 않는다.

2026-11-10 지면 이동 보정: Editor Win64 Development 빌드와 자동화 72개 통과. 공통 지면 경로를 플레이어 근접 실행기에도 연결하고 캐릭터별 공격 DA에 허용치를 뒀다. 자동화에서 단차 상승·하강에 기존 도약 높이를 더하는 처리와 캡슐 기준점 유지·중단 복원을 검사했다. 실제 WhiteCastle PIE에서 양쪽 골렘에 대한 평타 접근·공격·복귀, 접근/타격 전/복귀의 세 중단 흐름을 확인했다. 두 공격 위치는 시작보다 약 38.35cm 높았고 지면 검사와 공격 중 메시 기준 높이가 일치했다. 정상 타격은 현재 DA의 15 피해를 한 번 적용했으며 타격 전 중단은 무피해, 타격 후 중단은 피해 유지였다. 종료 후 메시 원위치와 Tick 해제를 확인하고 양쪽 발이 인도에 묻히지 않는 화면을 확인했다. 최종 도약·접지의 자연스러움은 사용자 직접 플레이 확인 대상이다.

검증: Editor Win64 Development 빌드와 전투 테스트 44개 통과. 전면 PIE에서 정상 접근·도약·타격·백대시 복귀, 접근/주먹 공격/복귀 각각의 Montage 중단, 직교하는 배치 방향까지 5개 흐름을 확인했다. Actor 위치 유지, 메시 원위치 복원과 Tick 종료, 타격 전 중단 시 무피해 및 적중 후 피해 1회 적용을 확인했다. 접근·복귀 화면을 캡처해 모션도 확인했다. 최종 속도와 자세의 자연스러움은 사용자 직접 플레이 확인 대상이다.
