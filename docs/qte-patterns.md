# 기술별 QTE 패턴과 불마법

각 마법 DA의 Magic → Timing → QTE Pattern에서 공통 패턴 DA를 선택한다. Enable Timing Bonus가 켜져 있어야 사용한다. 패턴이 연결되어 있으면 게이지 시간·노란 구간·Perfect 배율은 패턴에서 읽고, 기존 Charge Duration / Timing Success Duration / Timing Damage Multiplier는 패턴 없는 기술의 호환 설정으로 남는다. Post Timing Hold Duration은 패턴 종료 후 추가 준비 유지 시간이며 계속 마법 DA에서 조절한다.

## 생성한 패턴

Content/ExpeditionPortfolio/Combat/QTE:

| 에셋 | 방식 | 최초 설정 |
|---|---|---|
| DA_QTE_Single_Hard | 단일 입력 | 0.75초, Perfect 마지막 0.18초 |
| DA_QTE_Single_Easy | 단일 입력 | 이전 강한 마법의 시간·구간·배율을 최초 생성 시 복사 |
| DA_QTE_Rhythm_Double | 겹침형 연속 입력 | 독립 마름모 2개를 0.18초 시차로 생성. 각각 0.4초, Perfect 각 마지막 0.12초 |
| DA_QTE_Sequential_Double | 순차형 | 1초 게이지 2개, Perfect 각 마지막 0.25초. 사이 간격 0.15초 |

기본 연결은 불마법 → DA_QTE_Single_Hard, 강한 마법 → DA_QTE_Single_Easy이다. 두 번 입력을 시험하려면 불마법 DA의 QTE Pattern을 Double 패턴 중 하나로 교체하고 저장한다. 해당 기술의 발사는 모든 QTE 단계와 추가 준비 유지가 끝난 뒤 한 번 실행한다. 단계마다 공격하는 다단 타격 기술은 이번 범위에 포함하지 않는다.

## 에디터 조절

- Mode: Sequential은 이전 단계가 끝난 뒤 다음 마름모를 생성한다. Rhythm은 각 마름모를 시차를 두고 생성하며 동시에 여러 마름모가 진행된다. 단계가 하나이면 단일 QTE다.
- Steps: 1~16개 입력 단계. 배열의 +로 단계를 추가한다.
- Duration: 해당 단계의 전체 시간(0.05~10초).
- Perfect Duration: 해당 단계 끝의 노란 구간 길이. Duration 이하여야 한다.
- Spawn Interval: Rhythm에서 마름모 생성 사이의 시간(0.01~5초). 각 Duration보다 짧게 두면 마름모들이 겹쳐 진행된다. 숫자 순서와 종료 시각이 일치해야 하며 같거나 역전되는 종료 시각은 잘못된 설정으로 거부한다.
- Delay After: Sequential에서 이전 단계 종료 후 다음 단계 시작까지 간격(0~5초). 마지막 단계 값과 Rhythm의 값은 사용하지 않는다. 간격 중 입력은 무시한다.
- Perfect Multiplier: 모두 Perfect일 때의 배율. 기본 1.5.

각 단계는 새로운 Space 누름 한 번만 받는다. 누르고 있기·자동 반복·같은 시각 중복 호출로 여러 단계를 통과하지 않는다. Rhythm의 입력은 번호 순서상 아직 판정되지 않은 첫 마름모를 대상으로 한다. 입력한 마름모는 즉시 사라지고, 이미 생성된 다음 마름모는 자기 진행률을 유지한다. 아직 생성되지 않은 마름모에는 입력할 수 없다. 생성 시차와 각 종료 시각은 앞선 입력 때문에 바뀌지 않는다. 입력을 놓친 단계는 Miss로 기록하고 다음 단계는 계속 진행한다.

예를 들어 Duration 0.4초 / Spawn Interval 0.18초이면 1번은 0초에 생성되어 0.4초에 끝나고, 2번은 0.18초에 생성되어 0.58초에 끝난다. 두 마름모가 동시에 진행되며 마지막 노란 구간에 0.18초 간격으로 두 번 입력할 수 있다. 전체 준비 시간은 두 Duration의 합이 아닌 마지막 마름모의 종료 시각이다. 서로 구분되는 위치에 표시하며 하나를 입력해 없애도 다른 마름모의 위치는 움직이지 않는다. 화면의 단계 번호·진행 문구는 숨기고 각 마름모 중앙의 Space만 표시한다.

## 전체 결과: 하나라도 Miss면 전체 Miss

Regular는 일반 성공이고 Perfect는 정확한 성공이다. 미입력은 Miss다. 어느 한 단계라도 Miss면 전체 결과는 Miss이며 모든 보너스를 제거하고 기본 피해를 적용한다. 공격은 취소되지 않는다. Miss가 없으면 단계별 배율을 평균내며, 기본 배율 1.5인 두 단계 예시는 다음과 같다.

| 단계 결과 | 전체 결과 | 피해 배율 |
|---|---|---|
| Perfect + Perfect | Perfect | 1.5 |
| Perfect + Regular | Regular | 1.25 |
| Regular + Regular | Regular | 1 |
| Perfect + Miss 또는 Miss + Perfect | Miss | 1 |

진행 중 이미 Miss가 있으면 화면에 전체 Miss를 표시한다. 사운드는 마름모가 생성될 때 시작음, 판정될 때 해당 결과음을 한 번씩 재생한다. 전체 집계 때문에 결과음을 추가 중복 재생하지 않는다. 공통 사운드 파일·음량·싱크는 기존 DA_QTEPresentation에서 조절한다.

## 불마법 모션 연결

기존 AS_SpellAttack/AM_BasicMagic의 시전 동작과 함께 QTE를 시작한다. Cast QTE Hold Time(최초 0.8초)은 Montage 기준 발사 직전 대기 자세의 위치다. 기존 발사 Notify(약 1초)보다 앞에 둔다. 짧은 패턴은 시전 모션 중 자연스럽게 완료하고, 긴 패턴은 이 자세에서 대기한 뒤 발사 구간으로 이어진다. 프레임 지연으로 Notify가 먼저 와도 QTE 준비 중에는 발사·피해를 차단한다.

불마법 Post Timing Hold Duration의 최초 설정은 0.1초다. 기존 강한 마법의 추가 준비 유지 시간과 피해량, 발사체·폭발·사운드는 보존한다. 패링 반격에는 QTE를 추가하지 않는다. 중단·참가자 제거·PIE 종료는 남은 입력과 발사를 정리한다. 잘못된 패턴은 QTE 시작을 거부하고 기존 무보너스 준비 경로로 처리한다.

실행 중 패턴은 값 복사본을 사용한다. DA 수정은 다음 행동부터 반영된다. Tools/SetupQTEPatterns.py 재실행은 이미 생성한 패턴과 사용자가 바꾼 연결·시간 설정을 덮어쓰지 않는다.

## 이전 구현 검증 기록

ExpeditionPortfolioEditor Win64 Development 빌드와 전투 자동화 테스트 46개 통과. 두 패턴 방식에서 두 단계의 Perfect/Regular/Miss 조합 9가지를 각각 검사했다. 반복 입력, 간격 중 입력, 이전 세션 취소, 프레임 지연으로 여러 단계가 지난 경우, 실행 중 DA 수정의 격리도 검사했다.

PIE 전투 7개 흐름 통과: 불마법 단일 Perfect와 Miss, 연속형 Perfect+Perfect와 Perfect+Miss, 순차형 Miss+Perfect와 Perfect+Perfect, 기존 강한 마법 Perfect. 현재 저장 피해량 기준으로 불마법은 Perfect 30 / Miss 20, 강한 마법은 Perfect 45였다. QTE 진행 중 피해 없음, 순차형 동안 Montage 0.8초 자세 유지, 준비 종료 후 발사 재개, 실제 명중 피해 한 번과 후속 적 턴까지 확인했다. 순차형 2/2에서 전체 Miss 표시 화면도 확인했다. 진단 중 바꾼 패턴 연결은 원래 값으로 복원했고 저장하지 않았다.

별도 PIE에서 연속형 시작음 1회·단계 결과음 2회, 순차형 시작음 2회·단계 결과음 2회, 중도 취소 후 결과음 없음도 확인했다. 검증 당시 기존 DA_QTEPresentation의 Perfect Sound는 Regular Sound와 같은 UI_Battle_QTE_Success_Regular로 연결되어 있었으며 이 사용자 설정은 변경하지 않았다. 소리 구분을 원하면 Perfect Sound의 Sound를 UI_Battle_QTE_Success_Perfect로 바꾸면 된다.

사용자 직접 플레이를 통한 입력감·난이도·모션과 청음의 최종 검증은 남아 있다.

위 기록의 한 게이지에 여러 판정 구간을 그리던 Rhythm은 사용자의 의도와 달라 독립 마름모를 겹쳐 생성하는 방식으로 교체했다. 기존 순차형·마법 DA 연결·개별 Duration과 Perfect Duration은 유지한다. Tools/SetupOverlappingQTE.py는 기존 Rhythm 샘플에 생성 간격을 최초 1회만 저장하며 이후 사용자 조정을 보존한다.

## 겹침형 수정 검증

최종 Editor Win64 Development 빌드 성공, 전투 테스트 46개 통과. 두 마름모의 동시 진행과 다른 진행률, 한 입력당 하나만 소비, 0.18초 간격의 두 Perfect, 어느 한쪽 Miss의 전체 실패, 취소·프레임 지연·잘못된 종료 순서를 검사했다. 기존 순차형의 모든 결과 조합도 유지했다.

PIE에서 기본 Rhythm의 Perfect+Perfect / Perfect+Miss / Miss+Perfect, 화면 확인용 긴 겹침 패턴, 기존 순차형, 강한 마법에 현재 연결된 순차형, 겹친 상태에서 Montage 중단까지 7개 흐름을 확인했다. 기본 불마법 피해는 두 Perfect일 때 30, 한 Miss일 때 20이었고 중단 시 적 HP는 100을 유지했다. 강한 마법은 현재 패턴의 단계 수·배율을 읽어 검증했으며 모두 Perfect일 때 45 피해였다. 두 마름모 표시 화면과 1번만 사라져 2번이 제자리에서 이어지는 화면을 각각 확인했다. 임시 패턴 연결은 복원했으며 저장하지 않았다. 사용자 직접 플레이의 입력감 검증은 남아 있다.
