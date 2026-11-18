# 피해·회복 숫자

평타·불마법·얼음마법·패링 반격·적 공격·물약의 실제 효과 적용 시점에 대상을 따라 숫자를 표시한다. 기본 피해는 흰색, 회복은 초록색, QTE 배율이 1보다 큰 피해는 노란색이며 기본보다 1.12배 크다. 패링·회피 자체는 피해 숫자를 만들지 않는다.

숫자는 실제 줄어든/회복한 HP를 소수점 없이 반올림하며 `5,932`처럼 쉼표로 구분한다. 회복은 초록색 `+30`처럼 앞에 `+`를 붙인다. 남은 HP보다 큰 공격은 실제 HP 손실만 표시하고, 초과 회복도 실제 회복량만 표시한다. 풀피 물약은 `+0`, 설정 피해량 0의 정상 적중은 `0`을 표시한다. QTE 실패/미입력은 기본 피해 색을 사용한다. 여러 단계 QTE에서도 최종 피해 배율을 기준으로 구분한다.

방어 성공 문구는 패링 시 `쳐내기`, 회피 시 `회피`를 표시한다. 피격 시 HIT 문구는 표시하지 않으며 피해 숫자와 피격 모션·사운드는 유지한다.

문구 변경 검증: Editor Development 빌드 통과. 실제 PIE에서 패링 `쳐내기`의 한글 표시, 회피 `회피`, 미입력 피격 시 방어 문구 Collapsed, 물약의 초록색 `+30` 표시와 정상 종료를 확인했다. 사용자의 직접 플레이 확인은 남아 있다.

## 에디터 설정

콘텐츠 브라우저에서 `Content/ExpeditionPortfolio/Presentation/DA_CombatNumbers`를 연다.

| 항목 | 기본값 / 설명 |
|---|---|
| Appearance → Font | `/Game/ExpeditionPortfolio/UI/Fonts/LibreCaslonDisplay-Regular_Font`, Default, 크기 54. 머리 위 피해·회복 숫자는 UI 글꼴 통일 대상에서 제외하며 Font Object·Typeface·Size·Outline Settings에서 편집한다. |
| Damage Color / Healing Color / Bonus Color | 기본 피해·회복·QTE 보너스 색 |
| Bonus Scale | 보너스 숫자 크기 1.12배 |
| Shadow Color / Shadow Offset | 어두운 그림자와 위치 (2, 3) |
| 대상 Combatant → Visual Data → Damage Number | 대상별 Socket / World Offset. 현재 캐릭터 데이터로 기존 `spine_03`, 높이 +35cm를 이전했다. |
| Screen Offset | 공통 스타일의 화면 위쪽 25 UI 단위. 월드 위치는 대상별 Visual Data에서 편집한다. |
| Motion → Duration | 표시 시간 1.1초 |
| Rise Distance | 표시 중 위로 이동할 거리 50 UI 단위 |
| Initial Scale / Pop Duration | 처음 1.2배 → 0.12초 동안 원래 크기 |
| Fade Duration | 마지막 0.35초 동안 투명해짐 |

연결은 `BP_ThirdPersonPlayerController → Class Defaults → Combat / UI → Combat Number Data`다. 추가 위젯 배치나 Blueprint 이벤트 연결은 필요 없다. 현재 사용하는 카메라에서 대상 위치를 매 프레임 투영하므로 숄더뷰·정면뷰·카메라 전환과 DPI 변경을 따른다. 같은 대상의 짧은 연속 적중은 가로로 분산하고 화면 밖 가장자리에서 잘리지 않게 보정한다.

## 수명과 판정 경계

Coordinator가 검증된 효과를 적용한 뒤 읽기 전용 이벤트를 발행한다. UI는 피해 계산이나 HP 변경을 하지 않는다. 행동 ID의 기존 중복 방지가 숫자 중복도 막는다. 빗나간/취소된 미적용 효과, 재도전의 HP 복원, 전투 진입·필드 복귀의 HP 복원은 숫자를 만들지 않는다.

숫자가 떠 있을 때만 위젯을 표시하고 프레임별 위치·애니메이션을 갱신한다. 수명은 게임 시간을 따르므로 승패 슬로모션에서는 같이 느려진다. 치명타 숫자도 표시하되 결과창이 준비되어 월드가 정지하면 제거한다. 필드 복귀·재도전·새 조우·PIE 종료에도 남지 않는다.

## 폰트 출처

2026-11-18 UI 글꼴 통일 후 사용자 요청으로 머리 위 피해·회복 숫자만 기존 Libre Caslon Display로 복원했다. 크기 54, 외곽선, 색상과 애니메이션은 유지한다. 글꼴 출처는 [Google Fonts](https://github.com/google/fonts/tree/main/ofl/librecaslondisplay), SIL Open Font License 1.1이며 `SourceArt/Fonts/LibreCaslonDisplay`와 `ThirdPartyNotices/LibreCaslonDisplay/OFL.txt`에 보관한다.

`Tools/SetupCombatNumbers.py`는 DA를 최초 생성할 때 Libre Caslon Display의 Default를 지정하고 PlayerController에 연결한다. 재실행 시 이미 저장된 사용자 스타일과 연결은 유지한다. `Tools/UnifyGameUIFonts.py`는 HP·QTE의 폰트 설정을 기준 글꼴로 맞추며 머리 위 피해·회복 숫자는 변경하지 않는다.

## 검증

2026-11-18 숫자 글꼴 복원: `ExpeditionPortfolioEditor Win64 Development` 빌드 통과. 새 PIE에서 실제 파이어볼 피해 `25`와 물약 회복 `+30`을 캡처해 Libre Caslon Display 표시를 확인했다. 런타임 Font는 Default·크기 54·외곽선 1이며, HP 이름/수치는 Roboto Bold·15/23을 유지한다. 물약 검증에는 회복 공간을 만들기 위한 진단용 비치명 피해 30을 사용했다. 검증 콜백 정리와 정상 PIE 종료를 확인했으며 사용자 직접 플레이의 최종 확인은 남아 있다.

2026-11-08: `ExpeditionPortfolioEditor Win64 Development` 빌드와 전투 자동화 테스트 54개 통과. 실제 피해·최대 HP 제한 회복·풀피 회복 0·QTE 배율 구분·중복 타격 거절·회피 무피해·치명타와 HP 복원 제외를 검사했다. 새 프로세스에서 폰트와 DA 참조를 재로드했다.

실제 PIE에서 불마법 피해 20(흰색), 적 공격 피해 35 두 번, 물약 회복 30(초록색), QTE 보너스 강한 마법 피해 45(노란색)를 확인했다. 흰색/초록색/노란색의 글꼴·화면 위치를 각각 확인했고, 승리 슬로모션 뒤 결과창 표시 시 활성 숫자 0개와 정상 PIE 종료도 검증했다. 테스트 중 스타일 DA와 원본 전투 수치는 변경하지 않았다. 사용자의 직접 플레이로 크기·속도·원하는 폰트 느낌을 최종 확인해야 하며 패키징 빌드의 배포 검증은 별도다.
