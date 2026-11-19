# 33원정대 모작 전투 시스템

Unreal Engine 5.8과 C++로 구현한 개인 프로젝트입니다. 턴제로 공격 대상을 선택하고, 공격 중에는 타이밍 입력으로 피해를 높이며, 적의 턴에는 타격마다 회피하거나 패링할 수 있습니다.

## 주요 구현 코드

| 기능 | 코드에서 볼 수 있는 내용 |
| --- | --- |
| [전투 상태와 턴 진행](Source/ExpeditionPortfolio/Combat/BattleCoordinator.cpp) | [턴 순서](Source/ExpeditionPortfolio/Combat/BattleCoordinatorTurns.cpp)와 공격 대상을 별도로 관리하고 전투불능 참가자를 건너뜁니다. |
| [캐릭터별 액션](Source/ExpeditionPortfolio/Combat/CharacterCombatData.h) | 기본 공격·스킬·반격을 캐릭터 프로필로 구성하고, [실행기 인터페이스](Source/ExpeditionPortfolio/Combat/CharacterActionExecutor.h)를 통해 마법과 근접 동작을 연결합니다. |
| [전투 명령 메뉴](Source/ExpeditionPortfolio/UI/CombatHUDWidget.cpp) | HUD가 메뉴와 선택 항목을 관리하고, 대상 확정과 행동 승인 요청을 전투 코디네이터에 전달합니다. |
| [공격 QTE와 적의 다단 공격](Source/ExpeditionPortfolio/Combat/ActionQTESubsystem.cpp) | 공격 QTE의 단계별 입력과 피해 배율을 계산합니다. [적 공격 패턴](Source/ExpeditionPortfolio/Combat/EnemyAttackPatternData.cpp)은 애니메이션의 타격 번호와 방어 가능 시간을 연결합니다. |
| [마법과 광역 공격](Source/ExpeditionPortfolio/UI/MagicPresentationComponent.cpp) | 발사체의 발사·도착 시점을 구분하고, [광역 공격 테스트](Source/ExpeditionPortfolio/Tests/AreaActionTests.cpp)로 여러 적의 피해 반영 순서와 중복 명중 방지를 검사합니다. |
| [결과 처리와 재도전](Source/ExpeditionPortfolio/Combat/BattleCoordinatorResult.cpp) | [결과 처리 테스트](Source/ExpeditionPortfolio/Tests/BattleResultTests.cpp)에서 전투 종료 시점의 통계, 재도전 시 진입 HP·행동 자원 복원, 안전한 후퇴 위치가 없는 경우를 검사합니다. |

[전체 C++ 소스](Source/ExpeditionPortfolio) · [자동화 테스트](Source/ExpeditionPortfolio/Tests) · [Unreal 프로젝트 설정](Config)

이 저장소는 소스 코드 열람용입니다. 플레이에 필요한 맵·Blueprint·Data Asset은 로컬 Unreal 프로젝트에서 관리합니다.
