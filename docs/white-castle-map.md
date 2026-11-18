# WhiteCastle 다리와 안뜰

성문 조우는 현재 `WC_GateEncounter_Group`의 골렘 2마리다. 기존 단일 `WC_GateEncounter`와 `WC_GatekeeperGolem` 배치를 이 묶음으로 이전했고 조우 GUID·후퇴 지점·배경을 보존했다. 적 구성을 바꾸려면 [다수 적 조우](grouped-encounters.md)의 Composition을 편집한다. 전체 맵 초기 제작 도구는 그룹 조우가 있는 맵에서 중단한다.

성문 묶음의 현재 Composition은 `DA_WhiteCastleGateEncounter`다. 골렘은 도로/인도 경계 X ±245에서 인도 중앙 쪽 X ±450으로 이동하고 바닥 높이에 맞췄다. 접근과 복귀에서는 실제 충돌 지면의 높이를 따라 이동한다. 맵 충돌이나 배경 메시를 수정하지 않았으며 기존 공용 2/3마리 프리셋도 보존했다.

현재 조명은 공통 `EP_LightingRig`와 `DA_Lighting_Outdoor`에 연결돼 있다. 새 맵과 VFX의 공통 기준은 [공통 조명과 VFX 노출 보정](rendering-and-vfx-exposure.md)을 참조한다.

플레이 맵은 `/Game/ExpeditionPortfolio/Maps/L_WhiteCastle`이다. 기존 `ThirdPerson/Lvl_ThirdPerson`과 구매한 WhiteCastle 원본은 보존한다.

## 플레이 흐름

1. 성을 정면으로 바라보는 다리에서 시작한다.
2. 다리 끝의 골렘에게 접근하면 기존 전투 시스템으로 조우한다. 조우 영역이 다리 폭 전체를 덮어 우회 진입을 막는다.
3. 승리 결과에서 계속을 선택하면 골렘이 제거된다. 같은 실행 중 맵을 다시 불러와도 해당 조우의 처치 상태를 유지한다.
4. 성문을 통과하고 정면 계단 앞에서 오른쪽으로 꺾어, 기둥 사이의 열린 통로로 안뜰에 이동한다. 정면 계단 위는 원본 모델의 막힌 장식벽이며 안뜰 출입문은 아니다. 흰 성벽·금색 창 장식과 바닥 테두리로 약 22.5m × 22.5m의 열린 전투 공간을 구분한다.
5. 향후 보스는 안뜰의 예약 위치에 연결한다. 이번 작업에는 보스 캐릭터나 보스 전투 규칙을 추가하지 않는다.

패배의 후퇴는 다리 위의 고정 지점으로 돌아가며 진입 HP와 전투 소모품을 복원한다. 재도전은 같은 골렘 조우를 다시 시작한다.

## 에디터에서 수정할 위치

Outliner의 `WhiteCastle` 폴더를 펼친다.

| 폴더 / Actor | 역할 |
|---|---|
| Architecture / WC_CastleExterior | 성·다리·절벽의 외관 |
| Gameplay / WC_PlayerStart | 플레이 시작 위치와 방향 |
| Gameplay / WC_GatekeeperGolem | 성문 앞 골렘. HP·패턴은 BP_StoneGolem의 기존 설정 사용 |
| Gameplay / WC_GateEncounter | Enemy Actor, 조우 범위와 고유 Encounter Id |
| Gameplay / WC_GateRetreatPoint | 후퇴 복귀 위치 |
| Courtyard | 안뜰 바닥, 흰 벽, 창, 기둥과 화로 |
| Details | 안뜰의 금색 바닥 테두리 |
| FutureBoss / WC_FutureBossSpawn | 미래 보스 위치·방향을 표시하는 Target Point |
| FutureBoss / WC_FutureBossPlayerEntry | 미래 보스전의 플레이어 배치 참고점 |
| Lighting / WC_Exposure | 노출, 블룸과 모션 블러 |
| Lighting / WC_CourtyardSkyFill | 그늘의 밝기를 보완하는 조명 |
| Collision | 바닥 지지와 보이는 외곽 경계의 단순 충돌. 기존 통로 차단 박스 5개는 충돌 비활성화 |

충돌 Actor는 렌더링되지 않는다. 계단·기둥·성벽의 충돌은 `WC_CastleExterior`의 로컬 메시에서 제공한다. 이 메시의 기존 `Use Complex Collision As Simple` 설정과 `BlockAll`을 사용한다. `WC_PalaceBoundary`, `WC_CrossingLeftBoundary`, `WC_CrossingFrontBoundary`, `WC_CrossingRearBoundary`, `WC_ArenaLeftBoundary`는 열린 공간을 가로막았으므로 보존하되 `NoCollision`으로 둔다. 충돌 수정 시 계단의 실제 등반 높이, 양쪽 출입구, 안뜰까지의 왕복 이동과 후퇴 지점이 함께 맞는지 확인한다.

## 배경 에셋과 렌더링

WhiteCastle의 전시용 성 메시에는 약 412만 개의 삼각형이 있다. 원본에 충돌·Nanite 설정을 덮어쓰지 않고 `Environment/WhiteCastle/PrivateGeometry/SM_WhiteCastleScenery_Nanite`라는 로컬 파생본을 사용한다. 같은 폴더의 Materials에는 Nanite 사용 플래그가 있는 로컬 재질 복사본을 둔다. 텍스처는 원래 팩을 참조한다. 표시는 Nanite, 건축물 충돌은 메시의 준비된 삼각형 충돌과 단순 바닥 지지를 사용한다. 이 배경의 레이트레이싱 가속 구조는 만들지 않는다. 캐릭터 전투 설정이나 프로젝트 전체의 레이트레이싱 설정은 바꾸지 않는다. 삼각형 충돌 활성화에 따른 장시간 성능 측정은 별도 확인이 필요하다.

파생본은 구매한 모델의 복사본이므로 원본 팩과 마찬가지로 Git에서 제외한다. 맵·배치 도구·자체 제작한 단색 금색 머티리얼은 Git 관리 대상이다. 새 체크아웃에는 적법하게 보유한 원본 WhiteCastle·골렘·전투 콘텐츠를 먼저 설치해야 한다.

### 전투 VFX와 조명·노출 기준

기존 전투 VFX의 발광 범위에 맞춰 `WC_Exposure`의 Min/Max EV100을 모두 **1.5**로 고정한다. 이전 값 11.5에서 10스톱 낮춘 만큼 Directional·Point 조명의 세기를 **1/1024**로 함께 낮춰 배경 밝기를 비슷하게 유지한다. 주광은 17.578125 lux, `WC_CourtyardSkyFill`은 2.44140625 lux이다. Skylight의 캡처 배율 1은 유지하고 조정한 하늘을 다시 캡처한다. 색·방향·그림자·블룸·충돌과 전투 콘텐츠의 재질은 이 보정에서 변경하지 않는다.

현재 설정은 `EP_LightingRig`의 `Profile`에 연결된 `DA_Lighting_Outdoor`에서 편집한다. 위 수치는 공통화 이전에 확정한 WhiteCastle의 외관 기준이며 공통 프로필에도 유지했다. 활성 전투 VFX는 별도의 공통 발광 노출 보정을 사용한다. 광원의 방향·색과 장식 조명은 맵에서 편집한다.

## 제작 도구

- `Tools/SetupWhiteCastleMap.py`: 새 맵 생성과 배치. 기존 맵에서 실행해도 새 맵만 수정한다. 재실행하면 이 도구가 관리하는 Actor를 기준 배치로 되돌리므로 수동 레벨 디자인 이후에는 의도적으로 사용한다. 조우 ID는 유지한다.
- `Tools/OptimizeWhiteCastleScenery.py`: 원본에서 로컬 Nanite 파생본을 생성하고 새 맵에 연결한다. 외부 에디터를 여러 개 열지 않고 PIE 밖에서 단독 실행한다.
- `Tools/FixWhiteCastleTraversal.py`: 현재 맵에서 외관 표면 충돌과 통로 차단 박스를 교정한다. 사용자 수동 배치와 시작 위치를 초기화하지 않는다.
- `Tools/TuneWhiteCastleLighting.py`: 공통 조명 액터가 있으면 연결된 프로필을 적용하고 저장한다. 공통화 이전 맵에만 EV 1.5로 맞추는 기존 보정 경로를 사용한다. 배치·시작 위치는 유지한다.

## 검증

Editor Development 빌드를 확인하고 실제 PIE의 이동·조우·전투·승리 후 통과를 검증한다. 이후 재도전·후퇴, 렌더링 경고와 저장 후 재로드를 확인한다. 최종 검증 결과는 작업 완료 시 기록하며, 사용자 직접 플레이 전에는 최종 체감 검증 완료로 처리하지 않는다.

2026-11-09 최초 검증: `ExpeditionPortfolioEditor Win64 Development` 빌드를 통과했다. 실제 CharacterMovement로 골렘 조우, 평타·불/얼음 마법 QTE·회피·다단 패링·반격, 정상 전투 승리와 계속 후 골렘 제거를 확인했다. 지정한 다섯 이동 좌표는 통과했으나, 외관 메시의 충돌이 꺼져 있어 기둥을 관통하는 경로가 허용됐고 실제 계단과 측면 통로의 진행 가능성을 검증하지 못했다. 사용자 플레이에서 이 문제가 확인돼 기존 이동 검증을 충분한 통과 근거로 취급하지 않는다. 별도 PIE에서 패배→재도전→패배→고정 지점 후퇴→재조우 및 진입 HP 복원도 확인했다.

Nanite·재질 복사본과 안뜰 마감 적용 후 새 에디터 프로세스에서 맵을 다시 읽었다. Actor 130개, 기존 조우 GUID, 골렘 참조, Nanite 활성화, 배경 레이트레이싱 비활성화, 로컬 재질 연결과 원본 패키지에 미저장 변경이 없음을 확인했다. 원본 WhiteCastle 파일의 수정 시각도 작업 시작 이후 바뀐 것이 없다. 최종 다리·골렘 명령 화면·안뜰 캡처에서 이전 레이트레이싱 메모리·VSM 작업 큐·노출 클리핑 경고가 보이지 않는 것을 확인했다. 검증용 콜백과 PIE를 종료했다.

구현 및 실행 검증과 사용자 직접 플레이에 의한 최종 체감 검증은 구분한다. 사용자 플레이, 장시간 성능 측정과 패키징 실행 파일 검증은 남아 있다. 프로젝트 기본 시작 맵은 기존 테스트 맵을 유지하므로 콘텐츠 브라우저에서 `ExpeditionPortfolio → Maps → L_WhiteCastle`을 열어 플레이한다.

2026-11-09 충돌 수정 후 검증: 외관 메시의 기존 삼각형 충돌을 활성화하고 통로를 가로막은 큰 경계 박스 5개의 충돌을 비활성화했다. 사용자가 변경한 PlayerStart 위치는 유지했다. 실제 CharacterMovement 입력으로 시작 지점→골렘 조우→평타 접근·공격·복귀와 적 공격 회피를 실행했다. 이동 검증을 위해 다음 명령 턴에서 적을 강제로 처치하고 계속을 누른 뒤, 순간이동 없이 성문 진입·계단 등반/하강·오른쪽 실제 출입구·안뜰 중앙과 세 모서리·왼쪽 실제 출입구·다리 복귀까지 20개 지점을 통과했다. 계단 도착 시 캐릭터 중심 Z가 약 249.6cm로 상승했고 양쪽 통로와 안뜰의 바닥도 유지됐다. 계단과 통로의 실제 플레이 화면을 함께 확인했다. `ExpeditionPortfolioEditor Win64 Development` 빌드도 통과했다. 이 검증은 전투 전체의 정상 처치 회귀 테스트나 사용자 직접 플레이를 대신하지 않는다.

2026-11-09 조명 수정 후 PIE 검증: Min/Max EV100을 1.5로, Directional·Point 조명 13개의 세기를 이전의 1/1024로 저장했다. 기존 검은 불꽃 캡처와 같은 전투 구도에서 배경 밝기는 유사하게 유지되고 불꽃이 주황색으로 표시되는 것을 비교했다. 실제 얼음/불 마법 QTE·발사·명중, 중간 패링과 2타/3타 전타 패링 성공 후 반격·명령 턴 복귀를 실행했다. 불꽃 발사와 명중 폭발, 얼음 분사의 푸른 안개·입자, 패링의 푸른 섬광을 캡처로 확인했으며 플레이어 HP 100 유지와 중복 대상 확정 거부도 확인했다. 테스트 전용 변경과 PIE는 종료했다.

충전 효과 확인: `NS_Magic_Bubbles_Owner_Cast_Spell`의 컴포넌트 활성화·정상 위치의 Bounds는 확인했으나 자동 캡처에서는 표시를 확인하지 못했다. 이후 사용자가 직접 플레이 화면에서 충전 효과가 보인다고 확인했다. 자동 관측의 누락을 표시 불량으로 단정하지 않으며 충전 효과 설정은 변경하지 않았다. 추가 노출·Distance Field 비교 진단도 저장하지 않았다.

조명 수정 후 맵을 재로드해 Min/Max EV100 1.5와 조명 13개의 저장값, 조우 ID·골렘 참조·충돌 설정·Nanite·원본 패키지 보존을 확인했다. `ExpeditionPortfolioEditor Win64 Development` 빌드는 최신 타깃으로 통과했다. 사용자 플레이 중 에디터는 종료하지 않았다. 사용자 최종 플레이 체감과 장시간 성능 확인은 구분한다.
