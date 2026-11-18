# 공통 조명과 VFX 노출 보정

새 맵은 **파일 → 새 레벨 → Expedition Exploration**으로 시작한다. 템플릿은 기본 바닥·PlayerStart·게임 모드와 공통 조명만 포함한다. 캐릭터 조우나 적·고유 조우 ID는 복제하지 않는다. 검증 맵은 `/Game/ExpeditionPortfolio/Maps/L_VFXValidation`이다.

## 조명 설정

`Rendering/Lighting/BP_LevelLightingRig`의 `Profile`에 조명 Data Asset을 연결한다. 현재 프로필은 `Rendering/Lighting/Profiles`에 있다.

| 프로필 | 용도 |
|---|---|
| DA_Lighting_Outdoor | 현재 WhiteCastle의 고정 EV100 1.5와 밝기 기준 |
| DA_Lighting_Dim | 어두운 환경에서 효과 가독성과 연기 표현 확인 |
| DA_Lighting_BrightHDR | EV100 11.5·주광 18,000 lux의 강한 조명에서 발광 보정 확인 |

프로필은 노출 방식(Histogram/Basic), Min/Max EV100, Exposure Bias, 주광·보조광·하늘 조명 세기, 블룸과 Local Exposure 대비를 소유한다. Min=Max면 고정 노출이다. 광원 위치·방향·색과 맵 장식용 지역 조명은 맵별로 편집한다. 다른 분위기의 맵은 프로필 복사본을 만들어 선택한다. 공통 DA 자체를 수정하면 그 프로필을 공유하는 맵에 영향을 준다.

조명 관리 액터의 Sun Light, Fill Light, Sky Light, Exposure Volume은 **같은 맵의 액터**를 참조해야 한다. WhiteCastle은 기존 광원을 그대로 연결했다. `Apply Lighting Profile` 버튼으로 절대값을 적용한다. 반복 적용해도 광량이 계속 낮아지지 않는다. 에디터 Construction과 BeginPlay에서 같은 프로필이 적용되며 Actor Tick은 사용하지 않는다.

`Validate Lighting` 버튼은 프로필·광원 누락, 잘못된 수치, 중복 조명 관리 액터, 다른 Post Process Volume 또는 카메라의 노출 덮어쓰기를 검사한다. 결과는 Details의 Validation Messages와 Output Log에서 확인한다. 경고는 검토한 뒤 수정하며 도구가 다른 볼륨이나 카메라를 임의로 변경하지 않는다. HDR/암실 검증 프로필은 실제 배경의 최종 아트 설정을 대신하지 않는다.

## 이펙트 설정

공통 함수는 `Rendering/Materials/MF_VFXExposure`다. 기존 Emissive 계산의 마지막에 연결하며 Base Color·Opacity·Normal·파티클 생성/이동/충돌은 변경하지 않는다.

| Material Instance 파라미터 | 의미 |
|---|---|
| VFXExposureStrength | 0은 기존 노출 반응, 1은 발광의 노출 변화를 보정 |
| VFXBrightness | 효과별 기본 발광 세기 |
| VFXReferenceExposure | 현재 외관을 유지할 기준. 기본값 2^(-1.5) ≈ 0.353553 |

EyeAdaptationInverse와 기준 노출을 결합해 기존 EV100 1.5의 모습을 유지한다. 밝기 기준은 고정된 시각적 기준이며 현재 맵 EV를 매 프레임 전달하지 않는다. 셰이더가 현재 뷰의 노출을 사용하므로 캐릭터·기술에 맵 설정을 넣지 않는다. 일반 불꽃 연기는 강도 0.35로 환경 반응을 일부 유지한다. 얼음·충전·회복의 색이 있는 마법 안개는 발광으로 분류한다. 자체 발광이 없는 표면 재질은 그대로 유지한다. 블룸·컬러 그레이딩·Local Exposure와 조명을 받는 표면은 여전히 환경별로 달라질 수 있다.

현재 캐릭터 프로필의 평타·기술·반격과 공통 패링·아이템 DA에서 참조하는 VFX를 대상으로 처리했다. 공통 패링과 캐릭터별 반격의 소유권은 유지한다. UI 재질은 처리하지 않는다. 기존 노출 보정이 있는 재질은 중복 삽입하지 않는다. 직접 Emissive 출력과 Make Material Attributes의 Emissive 입력을 지원하며, 다른 재질 구조는 검토 대상으로 중단해 확인한다.

외부 에셋의 Niagara·Material·Material Instance 파생본은 `Rendering/PrivateVFX`에 둔다. 원본 텍스처/함수는 참조하되 편집하지 않는다. 이 폴더는 구매 원본과 같은 이유로 Git에서 제외한다. 자체 제작한 공통 함수·프로필·Blueprint·템플릿·코드·도구는 Git 관리 대상이다. 새 체크아웃에서는 적법하게 보유한 원본 팩을 설치하고 도구를 실행해야 한다.

## 새 VFX와 재사용

새 행동/아이템을 해당 캐릭터 프로필 또는 공통 데이터에 연결한 뒤, PIE 밖의 에디터에서 `Tools/PrepareActiveVFX.py`를 실행한다. 이는 활성 데이터의 VFX만 준비하며 맵이나 조명 프로필을 재생성하지 않는다. 생성된 Material Instance의 VFX Exposure 파라미터는 에디터에서 조절할 수 있다. 원본/파생본 대응은 RenderingSource 메타데이터로 기록한다. 재실행 시 기존 프로필과 인스턴스 튜닝을 보존한다. 새 외부 VFX를 검증 없이 모두 지원한다는 의미는 아니다.

전체 최초 생성은 `Tools/SetupCommonRendering.py`다. 공통 프로필과 Blueprint, 템플릿·검증 맵을 생성하고 WhiteCastle을 연결한다. 기존 템플릿의 사용자 배치와 기존 프로필 선택을 보존한다. 여러 에디터/명령행 프로세스에서 에셋 저장을 동시에 실행하지 않는다.

## 직접 검증

`L_VFXValidation`을 플레이한다. **1: Outdoor / 2: Dim / 3: Bright HDR / F: 다시 재생 / N: 다음 효과**다. 효과는 4초마다 반복된다. Details의 Preview 버튼도 사용할 수 있다. 환경 전환 후 노출·하늘 갱신을 기다렸다가 비교한다. 바닥과 흑·회·백색 패널로 배경 밝기와 알파 표현을 함께 확인한다.

WhiteCastle에서도 실제 불꽃 발사·명중, 얼음 충전·분사, 중간/전타 패링·반격과 회복을 확인한다. 템플릿에서 만든 새 레벨의 이동, 프로필 반복 적용, 저장·재로드와 Editor Development 빌드를 함께 검증한다. 자동 캡처에서 보이지 않은 것을 즉시 표시 불량으로 단정하지 않고 실제 플레이와 관측 시점을 대조한다. 사용자 직접 플레이에 의한 최종 체감 검증은 별도로 구분한다.

## 2026-11-09 구현·검증 결과

- 공통 프로필 3개, 조명 관리 Blueprint, 검증 Blueprint, 탐험 템플릿과 검증 맵을 저장했다. 새 프로세스에서 다시 열어 3개 맵의 광원 참조·프로필 유효성을 확인했다. 새 레벨 창의 Expedition Exploration 항목도 화면 확인했다.
- 캐릭터/공통 DA의 연결 9곳에서 VFX 6종을 발견했다. 중복을 제외한 렌더러 재질 슬롯 43개(연결별 조회 시 73개)가 공통 보정을 참조한다. 일반 불꽃 연기의 환경 반응과 발광 코어를 구분했다. 원본 VFX·WhiteCastle 파일의 수정 시각과 미저장 패키지에 변경이 없음을 확인했다.
- 템플릿으로 만든 임시 레벨에서 이동, 조우 없음, 반복 프로필 적용, 누락/잘못된 프로필 거부를 PIE 검증했다. Outdoor/Dim/Bright HDR의 18개 조합으로 효과를 재생하고 대표 프레임에서 불꽃·푸른 얼음·패링 발광을 확인했다. 초기 전환 중 편집기 화면이 찍힌 캡처는 시각적 통과 근거로 사용하지 않았다.
- WhiteCastle 실제 전투에서 얼음/불 마법의 QTE·명중, 2타/3타 전부 쳐내기·반격과 명령 복귀, 대상 확정 중복 거부를 검증했다. 물약은 HP 80→100, 재고 1개 소비와 회복 숫자를 확인했다. 회복 효과 자체의 발광은 검증 맵에서도 확인했다. 충전 효과는 검증 맵에서 작은 흰 발광을 확인했으며 기존 원본 크기·시간·손 부착 설정은 바꾸지 않았다.
- ExpeditionPortfolio 자동화 테스트 66개가 모두 통과했다. ExpeditionPortfolioEditor Win64 Development 정식 빌드가 통과했다. 조우 GUID·골렘 참조·기존 이동 충돌과 Nanite 설정을 보존했다. 진단 PIE와 임시 실행 콜백은 정리했다.

사용자가 직접 플레이하며 느끼는 효과 세기·가독성의 최종 확인과 패키징 실행 파일 검증은 남아 있다. 새 맵의 분위기별 아트 조명과 새 외부 VFX의 재질 구조 검토는 계속 필요하다.
