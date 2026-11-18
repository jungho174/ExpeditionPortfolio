# 플레이어 방어 모션 설정

현재 캐릭터의 평타·기술·반격과 공통 패링 설정은 [캐릭터별 전투 설정](character-actions.md)으로 분리됐다. 아래의 이전 DA 경로 예시보다 새 프로필의 연결을 우선한다.


다단 공격에서는 타격마다 패링·회피를 새로 입력한다. 중간 패링은 성공 피드백만, 전 타격 패링은 마지막에 반격을 한 번 실행한다. 반격 종류는 캐릭터별 `ParryCounterProvider`로 지정하며 아래 불마법은 현재 플레이어의 구현이다. 골렘 설정과 다른 반격 실행기 연결은 `docs/stone-golem.md`를 참조한다.

`Content/ExpeditionPortfolio/Animation/Defense/DA_PlayerDefensePresentation`에서 방어 연출을 설정한다. 판정 시간은 기존 `DA_EnemyBasicAttack`의 방어/패링/타격 보정값을 사용하며, 이 문서의 값은 화면 연출만 변경한다.

| 설정 | 기본값 | 의미 |
|---|---:|---|
| Parry Montage | AM_ParryGuard | AS_MagicDefend 복사본 |
| Parry Start Time | 0.3초 | 방어 모션을 시작할 원본 위치 |
| Parry Play Rate | 2 | 패링 모션 재생 속도 |
| Parry Duration | 0.65초 | 방어 자세 재생 후 대기 자세로 돌아갈 시간 |
| Dodge Montage | AM_DefenseDodge | 전방 루트 이동을 잠근 MM_Dash 복사본 |
| Dodge Distance | 140cm | 화면 속 캐릭터의 후퇴 거리 |
| Dodge Retreat Duration | 0.14초 | 뒤로 빠지는 시간. 타격 직전 입력이면 남은 시간에 맞춰 단축 |
| Dodge Hold After Hit | 0.08초 | 예상 타격 후 뒤쪽 자세를 더 유지할 시간 |
| Dodge Return Duration | 0.22초 | 원래 자리로 돌아오는 시간 |
| Dodge Pose Time | 0.05초 | 후퇴 후 잠깐 유지할 MM_Dash 자세의 원본 위치 |
| Hit Reaction Montage | AM_UnarmedHitReaction | Front_Med_01의 가산 피격 모션 |
| Hit Reaction Play Rate | 1 | 피격 모션 재생 속도 |

E 패링과 Q 회피는 Coordinator가 승인한 첫 입력에서만 모션을 시작한다. 성공 여부는 기존 타격 순간에 판정·표시한다. 너무 이른 패링 또는 미입력 피격은 타격 순간 피격 모션으로 전환한다. 피격은 무기 없는 AnimBP의 기본 자세에 변화량만 더하므로 총을 든 기본 자세로 바뀌지 않는다.

회피는 실제 Pawn, 충돌 캡슐과 카메라의 기준 위치를 이동시키지 않는다. 메시만 후퇴·유지·복귀한다. MM_Dash의 앞부분 자세를 사용하고 후퇴 후 잠시 멈췄다가 복귀 중 기본 자세로 블렌딩한다. 원본은 공중 전방 대시이므로 전용 백스텝과 같은 움직임은 아니며, 추후 Dodge Montage만 교체할 수 있다.

회피 입력 승인 시 공통 `BattleGrounding`으로 캡슐 크기의 바닥·몸통 경로를 한 번 계산한다. 단차는 지면 높이를 따르고, 벽·낭떠러지·너무 높은 단차 앞에서는 마지막으로 확인한 안전 지점까지만 후퇴한다. 복귀에는 같은 경로를 역순으로 사용한다. 안전한 이동 공간이 없으면 제자리에서 회피 모션과 원래 후퇴/유지/복귀 시간을 유지한다. 이동 거리와 관계없이 회피 성공은 기존 Coordinator 판정을 따른다. 상시 지형 검색이나 새 발 IK·장애물 우회 탐색을 추가하지 않는다.

각 캐릭터의 DefensePresentation에 연결된 DA에서 **Dodge / Grounding → Grounding**을 펼쳐 Enabled, Max Step Height(기본 60cm), Max Drop Height(150cm), Sample Spacing(5cm), Max Slope Angle(50도)을 조절한다. 회피는 작은 캡슐로 단차를 빠르게 왕복하므로 경로를 촘촘하게 계산한다. 기존 Dodge Distance는 최대 후퇴 거리이며 실제 거리는 공간에 따라 짧아질 수 있다. Enabled를 끄면 기존의 높이·충돌 미보정 직선 이동을 사용한다. 현재 마법사는 위 `DA_PlayerDefensePresentation`을 사용한다. 평타·골렘의 지면 허용치는 각각의 행동/종족 DA에 유지한다.

2026-11-12 검증: Editor Win64 Development 빌드와 자동화 73개 통과. 단차 상승·하강, 벽 앞의 안전 경로 축소, 이동 공간 없음, 낭떠러지, 과도한 단차, 시작 지면 없음, 0 거리 설정과 중단/정상 복원의 회귀 검사를 포함한다. WhiteCastle 실제 골렘 전투에서 평지 140cm, 단차 ±38.5cm, 난간 앞 약 45cm 제한, 이동 불가 시 0cm의 제자리 회피, 회피 몽타주 중단까지 6개 흐름을 확인했다. 후퇴·복귀 전 구간의 지면 높이를 대조했고 최종 진단의 최대 차이는 약 3.48cm였다. 중복 회피 입력 거부, 캐릭터 캡슐 고정, 원래 메시 위치 복원·Tick 해제와 기존 회피 무피해 판정을 확인했다. 임시 위치·카메라는 PIE 인스턴스에서만 사용했고 실제 맵과 DA의 기존 튜닝은 저장·변경하지 않았다. 사용자의 직접 플레이에 의한 최종 체감 확인은 별도다.

후퇴·복귀 시간과 거리는 입력 승인 시 복사한다. PIE 중 설정을 바꾸면 다음 방어 입력부터 사용하며 이미 진행 중인 회피 궤적은 유지한다. 다음 공격, 명령 턴 복귀, 전투 종료, 모션 중단 또는 캐릭터 EndPlay 시에는 메시 위치와 회전을 복원한다. 후퇴/유지/복귀 동안에만 Tick을 켠다. 모션이 없거나 설정이 잘못돼도 전투 판정과 턴 진행은 계속된다.

원본 에셋은 보존한다. `Tools/SetupDefensePresentation.py`가 프로젝트 복사본, Montage와 Data Asset을 만들고 플레이어 Blueprint의 `DefensePresentation.PresentationData`에 연결한다. 다시 실행해도 기존 연출 수치는 덮어쓰지 않는다.

## 패링 성공 반격

패링 성공 시 짧은 손바닥 시전 모션으로 작은 불꽃을 발사한다. 적 몸통에 도착할 때 반격 피해와 적의 가산 피격 모션을 적용한다. 성공 판정 즉시 HP를 차감하지 않는다. 발사체와 반격 모션이 모두 끝날 때까지 `DefenseWindow`를 유지하며 추가 명령과 방어 입력은 거부한다. 기존 적 턴 카메라를 유지한다.

`Content/ExpeditionPortfolio/Animation/Defense/DA_ParryCounter`에서 조절한다.

| 설정 | 초기값 | 의미 |
|---|---:|---|
| Magic / Combat / Base Damage | 15 | 반격 명중 피해 |
| Attack Montage | AM_ParryCounter | AS_SpellAttack 기반 프로젝트 복사본 |
| Attack Start Time | 0.65초 | 원본 모션의 시작 위치 |
| Attack Play Rate | 2 | 짧고 빠른 반격 속도 |
| Projectile Speed | 1800cm/s | 적에게 향하는 불꽃의 속도 |
| Max Flight Duration | 0.6초 | 최대 비행 시간 |
| Counter Hit Reaction | AM_UnarmedHitReaction | 적에게 재생하는 가산 피격 모션 |

발사 Notify는 원본 모션의 1초에 있으므로 초기 설정에서는 반격 시작 약 0.175초 뒤 발사한다. Attack Start Time을 Notify 이후로 옮기면 종료 시 대체 발사가 되므로 발사 이전 범위에서 조절한다. 원본 약한 마법과 기존 패링/회피 수치는 변경하지 않는다. 반격 피해는 시작 시 복사하며 전투당 강한 마법 사용 횟수를 소비하지 않는다.

반격 전용 설정이 없거나 재생에 실패하면 기존 즉시 반격 15 피해로 대체한다. 정상 반격에서 Notify 누락은 모션 종료 시 발사로 대체하고, 모션 중단·시간 초과는 아직 적용하지 않은 피해를 취소한다. 이미 명중한 피해는 유지하며 중복/이전 행동 신호는 거부한다. 반격으로 적이 쓰러져도 반격 행동 완료 후 승리와 탐험 복귀를 처리한다.

`Tools/SetupParryCounter.py`는 전용 Data Asset·Montage를 최초 생성하고 플레이어 MagicPresentation의 Counter Magic에 연결한다. 재실행은 기존 반격 수치를 보존한다. 성공 충격 효과 연결은 SetupParryImpact.py를 이어서 실행한다.

### 성공 순간의 충격과 슬로모션

`DA_ParryCounter`의 **Magic / Counter Impact**에서 조절한다. 패링이 성공하면 손 앞의 섬광·충격파, 타격음과 약한 카메라 흔들림을 재생한다. 이때 반격 모션은 아직 시작하지 않고 기존 방어 자세를 보여준다. 실제 시간 기준으로 잠깐 느려진 뒤 이전 게임 속도로 복원하고 반격을 시작한다.

| 설정 | 초기값 | 의미 |
|---|---:|---|
| Parry Slow Duration | 0.15초 | 현실 시간 기준 슬로모션 길이. 0이면 생략 |
| Parry Slow Factor | 0.15 | 직전 게임 속도에 곱할 배율 |
| Parry Impact Effect / Scale | NS_ParryImpact / 0.35 | 손 위치의 짧은 SF 섬광·충격 효과 |
| Parry Impact Sound / Sound Volume | SW_ParryImpact_Tight / 0.5 | 패링 성공 타격음과 음량 |
| Parry Camera Shake / Scale | BP_ParryImpactShake / 0.25 | 성공 시 짧은 카메라 흔들림 |

게임 시간 Timer로 슬로모션을 종료하지 않는다. 짧은 충격 구간에만 실시간 Core Ticker를 등록하고, 만료·중단·참가자 무효화·컴포넌트 종료 시 등록을 제거하고 기존 Time Dilation을 복원한다. 성공 효과의 Niagara는 시간 배율을 보정해 느린 게임 속도에서도 즉시 보이도록 한다. 실패 패링·회피에는 적용하지 않으며 피해는 계속 반격 명중 순간에만 들어간다. `Tools/SetupParryImpact.py`는 기존 패링/반격 수치를 보존하면서 새 효과 연결을 최초 한 번만 저장한다.

패링 소리는 SourceArt/Audio/ParryImpact_Tight.wav의 0.22초 합성 충돌음으로 교체했다. 짧은 고역 충돌과 저역 타격을 겹치고 긴 음정성 금속 잔향을 제거한다. Tools/SetupParrySound.py가 WAV 생성·SoundWave 가져오기와 기존 기본음 교체를 담당하며 사용자가 별도로 지정한 다른 소리는 보존한다. 음색과 실제 체감 음량은 직접 청취로 최종 확인한다.
