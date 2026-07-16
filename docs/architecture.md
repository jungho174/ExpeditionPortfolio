# 아키텍처 방향

## 현재 상태와 설계 원칙

현재 프로젝트에는 Third Person 템플릿의 `AExpeditionPortfolioCharacter`, `AExpeditionPortfolioPlayerController`, `AExpeditionPortfolioGameMode`와 여러 템플릿 Variant가 있다. 프로젝트 고유의 전투 관리자, 전투 참가자 Component, 스킬 데이터 클래스는 아직 확정되거나 구현되지 않았다.

이 문서의 `BattleCoordinator`, `BattleManager`, `CombatantComponent` 등의 이름은 책임을 설명하기 위한 후보명이다. 실제 클래스명, 상속 구조와 배치 위치는 첫 Vertical Slice 구현 전에 작은 실험과 테스트를 거쳐 확정한다.

## Exploration과 Combat 영역 분리

Exploration은 캐릭터 이동, 카메라, 필드 상호작용, 조우 감지를 담당한다. Combat은 참가자 등록, 턴 순서, 명령 실행, 피해와 방어 판정, 승패 판정을 담당한다. 두 영역은 서로의 내부 구현을 직접 조작하지 않고 전투 시작 요청과 전투 종료 결과처럼 좁은 인터페이스로 연결한다.

- Exploration 입력은 전투 진입 시 비활성화하고 필드 복귀 시 복원한다.
- Combat 입력은 현재 전투 상태가 허용하는 Action만 활성화한다.
- 맵 전환, 같은 맵 안의 전투 공간 이동, 별도 전투 Level 사용 여부는 아직 확정하지 않는다.
- 전투 중 필드 Actor를 숨기거나 정지시키는 정책도 Vertical Slice의 전환 방식이 결정된 뒤 확정한다.

## BattleCoordinator 또는 BattleManager의 책임

전투 흐름을 조정하는 단일 객체가 필요하다. 후보 구현은 전투 전용 `Actor`, `WorldSubsystem`, 또는 다른 수명 관리 객체이며 아직 확정하지 않는다. 이 객체의 책임은 다음 범위로 제한한다.

- 전투 시작 요청을 검증하고 참가자를 등록한다.
- 현재 전투 상태와 턴 주체를 소유한다.
- 허용된 상태 전이만 수행하고 전이 Event를 발행한다.
- 플레이어 명령을 검증한 뒤 Action 실행을 요청한다.
- 적 패턴 선택과 Telegraph/DefenseWindow의 진행을 조정한다.
- 행동 종료 시 생존 상태를 확인하고 Victory 또는 Defeat를 판정한다.
- 전투 종료 시 임시 상태를 정리하고 Exploration 복귀를 알린다.

이 객체는 Widget의 표시 방식, Animation 세부 연출, 개별 캐릭터의 이동 구현을 소유하지 않는다.

## CombatantComponent의 책임

`CombatantComponent`는 전투에 참여할 Actor가 공통 전투 기능을 제공하도록 하는 후보 Component다. 다음 책임을 고려한다.

- Max HP, Current HP와 생존 여부 관리
- 피해, 회복, 방어 결과 적용
- 사용 가능한 Action/Skill 목록 제공
- 전투 참가자 식별과 팀 또는 진영 정보 제공
- HP 변경, 사망, Action 완료 등의 Delegate 발행

턴 전체 흐름, UI Widget 참조, 카메라 연출은 Component에 넣지 않는다. 스탯 확장과 상태 이상은 Vertical Slice 이후 필요성이 확인될 때 추가한다.

## UI와 게임 로직의 분리

UI는 전투 상태의 소비자이자 사용자 명령의 입력 경계다.

- Widget은 Coordinator나 View Model 성격의 중간 객체가 발행한 상태 변경 Event를 구독한다.
- Widget은 공격/스킬 선택 의도를 명령 형태로 전달하고, 유효성 검사와 실제 상태 전이는 게임 로직이 수행한다.
- HP, 현재 턴, 선택 가능 Action은 게임 로직의 데이터를 표시하며 Widget 내부에 별도 원본 상태를 만들지 않는다.
- Widget Animation 종료가 핵심 전투 로직의 유일한 진행 조건이 되지 않도록 완료 신호와 실패 시 대체 경로를 둔다.

## Data Asset 기반 스킬과 몬스터 패턴

스킬과 적 패턴의 정적 정의는 `UPrimaryDataAsset` 또는 `UDataAsset` 기반으로 분리하는 방향을 우선 검토한다. 데이터 클래스의 최종 이름과 필드는 구현 전 확정하지 않는다.

스킬 데이터 후보 항목:

- 표시 이름, 설명, 아이콘
- 비용, 기본 위력, 대상 규칙
- 실행 Animation/VFX/SFX 참조
- 실행 정책 또는 C++ Action 클래스 참조

몬스터 패턴 데이터 후보 항목:

- 패턴 식별자와 선택 가중치
- Telegraph 시간과 DefenseWindow 구간
- 공격 횟수 및 각 타격 타이밍
- 피해량과 회피/패링 허용 정책
- Animation/VFX/SFX 참조

Data Asset은 설정값과 콘텐츠 참조를 제공하고, 판정 공식과 상태 전이 규칙은 C++에 둔다. 디자이너가 Blueprint로 확장할 지점은 열어 두되 Blueprint가 전투 상태를 독자적으로 소유하지 않게 한다.

## 의존성 방향

의도한 의존성은 다음과 같다.

`Input/UI -> 전투 명령 인터페이스 -> 전투 흐름 조정 객체 -> Combatant/Action 로직 -> Event -> UI/연출`

UI와 연출 계층은 Event를 관찰하고, 핵심 로직은 구체적인 Widget이나 효과 에셋을 직접 알지 않는 방향을 유지한다.
