# 공통 공격 QTE

QTE 화면은 마름모 게이지와 중앙 Space 키 표시만 사용한다. 아래 안내·Perfect/Regular/Miss·피해 보너스·단계 진행 문구와 게이지 위 번호는 표시하지 않는다. 판정·피해 배율·사운드와 단일/순차형/연속형 동작은 유지한다.

마름모는 흰색 잔량이 가득 찬 상태로 시작하고, 시간이 지나면 상단부터 시계 방향으로 사라진다. 지나간 부분에는 희미한 무채색 윤곽만 남는다. 노란 목표 구간도 진행률이 해당 구간에 도달하면 흰색에 이어 같은 방향으로 줄어들며, 게이지 종료 시 흰색·노란색 잔량이 모두 사라진다. Perfect·Regular·Miss에 따라 잔량을 초록색이나 빨간색으로 바꾸지 않는다. 단일·순차형·겹침형에 공통으로 적용하고, 순차형 단계 사이의 입력 불가 대기 중에는 흰색·노란색 잔량을 모두 숨긴다. 겹침형은 각 마름모가 자기 진행률로 줄어들며 입력한 마름모만 사라지는 기존 동작을 유지한다.

잔량색은 `WBP_AttackTiming`의 `Fill Color`에서 설정하며 기본값은 흰색이다. 목표 구간색은 `Window Color`로 조정한다. 표시만 변경하며 판정 시간·노란 구간의 실제 범위·피해 배율·사운드는 변경하지 않는다.

흰색과 노란색은 모두 같은 `Line Width`를 사용한다. 각 색의 구간을 따로 그려 노란색 아래에 흰 선이 겹치지 않으며, 두 색의 경계에서도 동일한 굵기를 유지한다.

2026-11-18 굵기 통일 검증: `ExpeditionPortfolioEditor Win64 Development` 빌드를 통과했다. 새 PIE에서 시작·흰색 소진 중·노란색 소진 중·종료까지 5개 프레임을 캡처해 두 색의 동일한 굵기와 연속 소진을 확인했다. 미입력 종료와 종료 후 입력 거절도 통과했으며 사용자 직접 플레이의 최종 확인은 남아 있다.

중앙 키 글꼴은 `WBP_AttackTiming → Input Font`에서 설정한다. 현재 상단 상태 표시와 같은 `/Engine/EngineFonts/Roboto`, Bold, 크기 16이며, 폰트를 지정하지 않은 경우도 UTextBlock 기본 폰트를 사용한다.

2026-11-18 노란 구간 소진 검증: `ExpeditionPortfolioEditor Win64 Development` 빌드와 `QTE.Patterns`·`QTE.Sessions` 자동화 2개를 통과했다. 새 PIE의 기존 위젯과 임시 패턴으로 Miss·Perfect·Regular·취소·순차형·겹침형 6가지를 실행하고 37개 프레임을 캡처했다. 시작→흰색 잔량→노란색 잔량→완전 소진, 순차 대기 중 색 숨김, 겹침형의 개별 노란 잔량을 화면에서 확인했다. 중복·종료·이전 세션 입력 거절과 기존 배율도 통과했다. 에셋 수치·시간 배율을 변경하지 않았으며 검증 콜백 정리 후 PIE를 종료했다. 사용자 직접 플레이의 최종 체감 확인은 남아 있다.

2026-11-18 흰색 잔량 검증: `ExpeditionPortfolioEditor Win64 Development` 빌드와 `QTE.Patterns`·`QTE.Sessions` 자동화 2개를 통과했다. 실제 파이어볼 QTE와 별도 PIE의 Regular·Perfect·미입력·취소·순차형·겹침형 6가지 실행을 확인했다. 반복 입력 거절, 순차 대기 중 잔량 숨김, 겹침형의 개별 잔량과 입력된 게이지만 제거되는 동작을 검사했다. 시작·중간·판정 이후·단계 대기 화면을 직접 확인했고 사용자 직접 플레이의 최종 체감 검증은 남아 있다.

이전 표시 간소화 검증: Editor Development 빌드 통과. 실제 PIE의 강한 마법 순차형 2단계 패턴에서 마름모와 중앙 SPACE만 남고 안내·단계 문구가 없는 화면을 확인했다. 사용자 직접 플레이 확인은 남아 있다.

강한 마법에 있던 시간창과 입력 판정을 UActionQTESubsystem으로 분리했다. C++ 모듈은 기술 이름·마법 모션·카메라·위젯·사운드를 참조하지 않는다. 현재 활성 행동이 설정에 따라 QTE를 시작하고, 판정 배율을 실제 타격에 적용한다. 현재 불마법과 강한 마법은 공통 패턴 DA를 사용한다. 평타·물약·패링 반격에는 QTE를 추가하지 않는다. 단일·연속형·순차형 설정과 하나라도 Miss면 전체 Miss인 집계 규칙은 [qte-patterns.md](qte-patterns.md)를 참조한다.

| 이벤트 | 결과와 연출 | 기본 사운드 |
|---|---|---|
| 시작 | 게이지 표시 | UI_Battle_QTE_Start |
| 하얀 구간 첫 입력 | Regular · 기본 피해 | UI_Battle_QTE_Success_Regular |
| 노란 구간 첫 입력 | Perfect · 기존 보너스 | UI_Battle_QTE_Success_Perfect |
| 게이지 종료까지 미입력 | Miss · 기본 피해 | UI_Battle_QTE_Miss |
| 행동 중단 | Cancelled · 화면/예약 정리 | 결과음 없음 |

첫 입력만 수신하고 Regular/Perfect 결과는 게이지 종료 때 Miss로 바뀌지 않는다. 결과 입력은 준비 동작을 끝내지 않는다. 게이지가 끝나면 입력은 닫히고 추가 준비 유지 시간이 끝난 뒤 공격한다. Miss는 게이지 종료 순간에 한 번 발생하며 추가 준비 종료까지 늦추지 않는다. UI/음량/화면 FPS로 판정하지 않는다.

## 에디터 설정

`Content/ExpeditionPortfolio/Presentation/DA_QTEPresentation`을 열어 QTE → Audio의 Start Sound, Regular Sound, Perfect Sound, Miss Sound를 편집한다. 각 항목에는 Sound / Volume / Delay / Start Time이 있다. Delay는 이벤트 이후의 게임 시간 대기(0~5초), Start Time은 파일 앞부분을 건너뛰는 초 단위 위치다. 기본 볼륨 1, Delay/Start Time 0이며 None은 무음이다. UI 사운드이므로 카메라·캐릭터 거리의 영향을 받지 않는다. Regular_V2가 아닌 요청한 정확한 Regular 파일을 연결한다.

공통 DA의 연결 위치는 BP_ThirdPersonPlayerController의 QTEPresentation 컴포넌트 → Combat / QTE / Presentation Data다. 강한 마법 DA에 네 사운드를 중복 보관하지 않는다. 이후 기술도 이 공통 연출 설정을 사용한다. 최초 연결 스크립트는 Tools/SetupCommonQTE.py이며 재실행해도 기존 사운드 조정값을 초기화하지 않는다.

기술별 QTE 사용 여부·추가 준비 시간은 마법 DA의 Enable Timing Bonus / Post Timing Hold Duration에서 결정한다. 연결된 QTE Pattern에서 단계별 시간·노란 구간·보너스 배율을 조절한다. 패턴이 없는 마법은 기존 Charge Duration / Timing Success Duration / Timing Damage Multiplier를 호환 설정으로 사용한다. 모션·카메라·게이지 크기·위치는 기존 설정을 보존한다.

## 재사용과 책임

- UActionQTESubsystem의 BeginQTE(Duration, PerfectDuration, PerfectMultiplier)는 세션 ID를 반환한다. 한 월드에 하나의 활성 QTE만 허용하며 실패하면 0이다.
- SubmitInput(SessionId), FinishWindow(SessionId), CancelQTE(SessionId)는 일치하는 세션에만 작용한다. 타이머가 자동으로 게이지를 종료하며 잘못된 설정, 중복 입력과 이벤트 콜백 재진입을 거부한다.
- Started / Resolved / Cancelled / Changed는 C++ 알림이다. 시작과 최종 판정 이벤트는 각각 한 번만 발생하며 UI 새로고침으로 다시 소리 내지 않는다. BeginPattern으로 다단계 패턴을 시작하며 StepStarted / StepResolved로 단계별 UI·사운드를 전달한다.
- UQTEPresentationComponent는 PC에서 사운드만 담당한다. 중단·새 QTE·EndPlay는 남아 있는 재생 예약을 정리한다. 이미 재생된 짧은 사운드의 꼬리는 자연스럽게 끝난다.
- UAttackTimingWidget은 공통 모듈의 뷰를 표시한다. 전투 상태를 소유하거나 결과를 계산하지 않는다. 전투 외 독립 QTE 호출에도 재사용 가능하다.
- BattleCoordinator는 현재 전투 행동에 세션 ID를 연결하고 참가자 유효성과 행동 중단을 검사한다. 위젯 표시나 소리가 피해를 발생시키지 않는다. 다음 기술은 공통 API를 호출하고 반환 결과를 자신의 타격 효과에 반영한다.

기존 Blueprint 저장값의 호환성을 위해 EAttackTimingResult의 내부 Success/Failed 이름과 숫자는 유지했다. 에디터 표시와 의미는 각각 Perfect/Regular다. Missed는 Miss로 표시한다. 기존 GetAttackTimingView / RequestAttackTimingInput API는 전투 연결용 호환 API로 유지한다.

## 검증

Editor Win64 Development 빌드와 전투 자동화 테스트 45개 통과. 시작 이벤트 중 재진입, 중복 입력, 노란 구간 경계, 시간 초과, 이전 세션 입력·취소, 판정 후 중복 Miss 방지와 강한 마법이 아닌 행동의 보너스 적용을 검사했다.

PIE의 전투 없는 독립 QTE에서 Regular / Perfect / Miss, 입력 전 취소, 시작음 0.8초 지연 예약 후 취소를 확인했다. 각 흐름은 시작음과 해당 결과음만 한 번씩 재생하며, 취소는 Miss를 재생하지 않고 지연 예약도 제거했다. Regular/Perfect 결과 화면도 캡처해 확인했다. 기존 강한 마법에서도 Perfect 사운드·저장된 피해 배율·추가 준비 유지·후속 적 턴까지 확인했다. 진단용 설정은 값별로 백업·복원했고 저장하지 않았다. 사용자 직접 플레이·청음 전에는 최종 체감 검증 완료로 처리하지 않는다.
