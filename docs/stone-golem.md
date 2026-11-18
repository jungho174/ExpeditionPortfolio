# 돌 골렘과 다단 패링

## 에디터에서 조절할 위치

콘텐츠 브라우저의 `ExpeditionPortfolio/Combat/Patterns/StoneGolem`에 패턴 3개가 있다.

| Data Asset | 구성 | 초기 타격별 피해 |
|---|---|---|
| DA_GolemDouble | 묵직한 2연타 | 10 / 10 |
| DA_GolemTriple | 빠른 3연타 | 8 / 8 / 12 |
| DA_GolemDelayed | 2연타 후 지연된 강타 | 7 / 7 / 18 |

각 DA의 `Hits` 배열을 펼쳐 타격별 `Damage`, `Defense Window Duration`, `Parry Success Duration`, `Hit Time Offset`과 명중 사운드·VFX를 조절한다. 초기 방어 구간은 0.45초, 마지막 패링 성공 구간은 0.2초다. `Selection Weight`는 패턴 선택 확률의 상대 가중치이며 0이면 제외한다. `Telegraph Duration`은 접근 전 예고 대기다.

`Animation/StoneGolem/AM_GolemDouble`, `AM_GolemTriple`, `AM_GolemDelayed`의 타격마다 `EnemyHit` Notify를 배치했다. `Hit Index`는 0부터 시작하며 `Hits[0]`, `Hits[1]` 등에 대응한다. Notify를 옮기면 기준 타격이 바뀌고, DA의 보정값은 그 기준에서 초 단위로 더한다. Notify가 없을 때만 해당 항목의 `Fallback Hit Time`을 사용한다. 시간은 접근 완료 후 공격 Montage 시작 기준이다.

타격 순서가 뒤집히거나 방어 구간이 앞 타격과 겹치는 설정, 중복된 타격 번호는 무효다. Output Log에 이유를 남기고 패턴 선택에서 제외한다. 패턴을 고르는 시점에 전체 타격을 값으로 복사하므로 PIE 중 수정은 다음 적 행동부터 적용한다. `Legacy Single Hit`의 고급 항목은 Hits가 빈 기존 패턴용이며 골렘 패턴에서는 사용하지 않는다.

## 골렘 연출

접근·복귀 경로는 `DA_GolemPresentation → Movement / Grounding`에서 설정한다. Enabled 기본 켬, Max Step Height 60cm, Max Drop Height 150cm, Sample Spacing 25cm, Max Slope Angle 50도다. 접근 시작 때 실제 지형 충돌에 골렘 캡슐 크기로 바닥을 검사하고 몸통이 통과할 공간까지 확인한다. 계산한 높이 경로로 메시를 움직이고 복귀에는 같은 경로를 역순으로 사용한다. 충돌 캡슐과 조우 기준점은 유지하며, 매 프레임 지형을 다시 검색하지 않는다. 바닥이 없거나 벽·큰 단차로 경로가 막히면 해당 공격을 피해 없이 종료하고 다음 턴을 진행한다. 바닥을 타는 이동 보정이며 새로운 발 IK나 장애물 우회 경로 탐색을 추가한 것은 아니다.

WhiteCastle 성문 묶음은 원래 2/3마리 공용 프리셋을 보존하고 `Exploration/Encounters/DA_WhiteCastleGateEncounter`를 사용한다. 현재 두 골렘 중심은 월드 X ±450cm의 인도로 옮기고 지면 높이에 맞췄다. 배치는 해당 DA의 Members에서 편집하며 공통 이동 허용치와 몬스터별 애니메이션 설정은 위 Presentation DA에서 바꾼다. `Tools/SetupGroundedGolemEncounter.py`는 최초 배치 교정 도구이며 기존 조우 ID·후퇴 지점·종족·HP·연출 설정을 보존한다.

`Animation/StoneGolem/DA_GolemPresentation`에서 접근·복귀 시간, 공격 거리, 플레이어 평타가 골렘에게 접근할 거리, 걷기·피격·사망 Montage와 반응 사운드를 바꾼다. 이동 중에만 Tick을 켜서 메시를 이동하며 Pawn과 조우 영역의 기준 위치는 유지한다. 정상 복귀·중단·재도전·후퇴에서 원래 메시 위치를 복원한다. 치명타에서는 타격 당시 위치를 결과창 배경까지 유지한다.

적 한 마리의 HP가 0이 되면 충돌·공격을 중단하고 사망 Montage를 재생한다. 마지막 자세를 유지한 뒤 서서히 사라지며, 살아 있는 다른 적의 턴과 전투는 계속된다. `DA_GolemPresentation → Reaction / Death`의 **Death Play Rate**(기본 1), **Death Disappear Delay**(모션 종료 후 기본 0.2초), **Death Fade Duration**(페이드 시간, 기본 0.8초), **Death Fallback Duration**(모션을 재생할 수 없을 때 기본 0.75초)을 조절한다. Fade Duration을 0으로 두면 즉시 숨긴다. Death Montage와 Death Sound는 기존 Reaction 항목이다. 공통 실행 코드는 종족별 DA를 읽으므로 다른 몬스터도 자기 사망 모션을 연결할 수 있다.

**Death Fade Materials**는 메시의 재질 슬롯 순서에 맞춘 페이드용 재질이며, **Death Fade Parameter**(기본 DeathOpacity)는 1=완전 표시, 0=완전 제거인 Scalar 이름이다. 원래 재질 자체가 이 파라미터를 지원하면 해당 배열 항목을 비워도 된다. 골렘은 `Rendering/Enemies/StoneGolem`의 프로젝트 복사본에 DitherTemporalAA 기반 Masked 페이드를 추가했다. 생존 중에는 원래 재질을 사용하고 페이드 중에만 개체별 Dynamic Material Instance로 교체한다. 다른 골렘과 원본 에셋의 값은 바뀌지 않으며 재도전·후퇴 시 기존 재질 참조까지 복원한다. 페이드 구간에서만 프레임 갱신하고 끝나면 끈다. 새 종족은 자신의 원래 외형을 보존한 페이드 재질을 같은 슬롯에 연결한다. 미지원 재질은 경고를 남기고 기존 숨김 경로를 사용한다. `Tools/SetupEnemyDeathFade.py`는 골렘 페이드 최초 구성 도구로 이후 사용자 조정을 보존한다.

소멸은 현재 전투에서 외형을 숨기는 처리다. 재도전·후퇴 시 전투 진입 HP와 함께 외형·충돌·메시 위치를 복원해야 하므로 인스턴스는 유지한다. 승리 후 계속을 누르면 기존 정책대로 실제 Destroy한다. 사망 Timer와 페이드는 게임 시간을 사용하므로 마지막 처치에서는 기존 승리 슬로모션과 결과창 정지가 적용된다. 별도 상시 Tick이나 공통 Montage 에셋 변경은 하지 않는다.

골렘 메시·AnimBP·HP·패턴은 `Combat/Enemies/BP_StoneGolem`이 소유하고 공통 C++ `BattleEnemyActor`를 상속한다. 현재 맵의 기존 `BP_PrototypeEnemy`는 배치 Actor와 조우 ID를 보존하기 위한 골렘의 호환 자식이다. 새 적 종족은 `BattleEnemyActor`에서 파생시킨다. 초기 골렘 HP는 200이며 원본 메시·Skeleton은 `/Game/Stone_Golem`을 참조한다. 조준·숫자·표식 위치는 `Combat/Enemies/DA_StoneGolemVisual`에서 편집한다.

`Animation/StoneGolem/RTG_Manny_StoneGolem`과 두 IK Rig를 통해 기존 맨손 모션을 골렘에 리타기팅했다. 원본 애니메이션과 구매한 원본 에셋은 보존한다. 현재 공격은 사람의 주먹 모션을 조합한 것으로, 전용 골렘 내려찍기·포효 애니메이션을 새로 제작한 것은 아니다. 리타기팅 모션의 무게감과 자연스러움은 직접 플레이로 최종 판단한다.

## 방어와 반격 규칙

- 각 타격마다 새 입력 한 번을 받는다. 첫 입력 후 같은 타격에 대한 추가 입력은 거부한다.
- 이른 패링 실패와 미입력은 해당 타격의 피해만 적용한다. 다음 타격은 다시 방어할 수 있다.
- 회피도 타격별로 입력해야 한다. 입력 Context 재활성화 시 이미 누른 키는 떼기 전까지 무시한다.
- 전 타격 패링에 성공해야 마지막에 반격을 한 번 실행한다. 피격이나 회피가 섞이면 반격하지 않는다.
- 중간 패링과 전 타격 성공 피드백은 공통 `DA_ParryFeedback`의 `Intermediate / Complete`에서 조절한다. 반격 공격 DA와 분리했다.
- 전 타격 성공은 공통 피드백이 끝난 다음 현재 캐릭터 프로필의 `Parry Counter`를 실행한다.
- 체력이 0이 되면 남은 타격과 판정 Timer를 취소하고 결과 연출로 전환한다.

## 캐릭터별 고유 반격 연결

현재 캐릭터의 `CombatantComponent.CharacterActions`가 `DA_PlayerMageCombat`을 참조한다. 이 프로필의 `Parry Counter`에 연결된 `DA_PlayerMageCounter`에만 현재 마법사의 불마법 반격 연출·피해가 있다. 평타와 기술도 같은 프로필에서 각각 독립적으로 연결한다. 기술 수와 공격 종류는 공통 전투 규칙에 고정하지 않는다.

실행기는 `CharacterActionExecutor` 인터페이스로 행동 DA, 행동 ID, 대상, 반격 여부를 받는다. `MagicPresentation`은 마법, `UtilityPresentation`은 근접 공격을 실행하며 근접 공격도 기술·반격 슬롯에 연결할 수 있다. 명중과 완료는 Coordinator에 통지하고 HP는 직접 변경하지 않는다. 상세 편집 위치와 이전 에셋 보존 정책은 [character-actions.md](character-actions.md)를 참조한다.

프로필이 없는 이전 테스트 참가자만 `ParryCounterProvider`와 즉시 피해 호환 경로를 유지한다. 새 캐릭터는 자기 프로필과 실행기를 연결한다.

## 검증

2026-11-10 지면 이동: Editor Win64 Development 빌드와 자동화 71개 통과. 높이 40cm 단차의 내려가기·올라가기, 벽·지면 단절·허용치 초과의 경로 거절을 검사했다. WhiteCastle PIE에서 두 골렘의 실제 접근·복귀 중 각각 약 38.5cm 높이 변화를 측정했고 캡슐 지면 검사와 보간 경로의 차이는 최대 0.61cm였다. 불마법 처치, 남은 적 턴, 전 타격 패링 반격 처치, 소멸, 재도전·후퇴의 원위치 복원, 재조우와 광역 승리까지 통과했다. 사용자 조정 페이드 1.5초와 HP·피해·카메라·조우 ID는 보존했다. 최종 걸음과 접지의 체감은 사용자 직접 플레이 확인이 남아 있다.

2026-11-10 점진 소멸: Editor Win64 Development 빌드와 자동화 70개 통과. 개체별 MID 분리, 중간 투명도 값, 다른 적·공유 재질의 값 보존, 행동 정리 중 페이드 유지, 진행 중/종료 후 재도전의 정확한 재질 참조 복원, 0초 옵션을 검사했다. 골렘 원본 M/MI 파일의 해시는 작업 전후 동일하다. 페이드 전용 MI에도 Masked 표시 모드를 명시해 불투명 모드가 남는 문제를 보정했으며, 고정 투명도와 실제 사망 중간 캡처에서 배경이 비쳐 보이는 것을 확인했다. 기본 페이드 시간은 0.8초이고 최종 속도 체감은 사용자 직접 플레이 확인이 남아 있다.

2026-11-10 개별 사망·소멸: Editor Win64 Development 빌드와 자동화 테스트 69개를 통과했다. 사망 전 충돌 해제·턴/대상 제외, 대기 자세로 되돌아가지 않는 마지막 자세, 콘텐츠 누락 시 소멸 대체 시간, 사망 중 행동 정리와 메시 위치 유지, 재도전 HP·외형·충돌 복원, 복원 후 이전 Timer의 재소멸 방지를 검사했다. 실제 WhiteCastle PIE에서 불마법 및 완전 패링 반격으로 첫 골렘을 처치하고 사망 중간/마지막 자세와 소멸 화면을 확인했다. 남은 골렘 턴 진행, 재도전·후퇴 복원, 재조우 후 얼음 광역 동시 처치와 승리 계속의 전체 Destroy까지 통과했다. 검증은 설정 DA를 변경하지 않고 실행 인스턴스의 HP로 처치 상황을 만들었으며 진단 콜백·PIE를 종료했다. 모션과 소멸 속도의 최종 체감은 사용자 직접 플레이 확인이 남아 있다.

Editor Development 빌드와 자동화 테스트 58개를 통과했다. 실제 PIE에서 세 패턴의 전 타격 패링과 반격 1회, 3연타 중간 피격 8 피해, 전 타격 회피, 패링·회피 혼합, 이른 패링 10 피해와 다음 타격 방어, 중복 입력 거부를 확인했다. 패턴마다 종료 후 메시 위치가 복원됐고 재도전·후퇴, 승리 결과의 Space 계속과 적 Destroy·처치 상태 기록도 확인했다. 새 PIE에서 대기 모션의 발·머리 방향과 액터 정면을 대조해 옆을 향하는 90도 불일치가 없음을 확인했다.

생성된 몽타주의 공격 시작 기준 타격 시각은 2연타 0.929/2.357초, 3연타 0.773/1.682/2.591초, 지연 공격 0.773/1.682/3.857초다. 이후 에디터 조정에 따라 달라질 수 있다. 검증 중 가중치를 바꿔 패턴을 지정했고 종료 시 원래 값으로 복원했다.

구현 검증과 사람의 최종 플레이 검증은 구분한다. 리타기팅 모션의 무게감·연결, 공격 간격과 사운드 체감은 사용자의 직접 플레이로 최종 조정해야 한다. 다른 동료의 검 반격 콘텐츠는 아직 만들지 않았으며 연결용 인터페이스를 제공한 상태다.
