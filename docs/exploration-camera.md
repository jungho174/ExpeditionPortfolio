# 탐험 카메라

맵 이동 카메라는 `/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`의 상속 컴포넌트 `CameraBoom`, `FollowCamera`에서 설정한다. 전투 연출 카메라는 별도의 `BattleCameraRig`를 사용한다.

2026-11-18 제공된 원작 참고 화면의 가까운 어깨 구도에 맞춘 값:

| 컴포넌트 | 프로퍼티 | 값 |
| --- | --- | --- |
| CameraBoom | TargetArmLength | 300 cm |
| CameraBoom | SocketOffset | X 0, Y 75, Z 45 cm |
| CameraBoom | TargetOffset | X 0, Y 0, Z 0 cm |
| FollowCamera | FieldOfView | 65° |

16:9에서 캐릭터 중심이 화면 왼쪽 약 31%, 머리 상단이 높이 약 32%에 위치하고 하체 일부가 화면 아래로 잘리는 구도다. 원작 내부 설정값을 복제한 것이 아니라 제공된 스크린샷의 화면 비율을 기준으로 조정했다.

SpringArm의 충돌 검사와 컨트롤 회전 사용은 유지한다. 어깨 위치는 FollowCamera의 상대 위치 대신 SpringArm의 SocketOffset으로 설정해 카메라 끝점 충돌 검사에 반영한다. 시점 회전과 이동 입력 방식은 기존 그대로다. C++ 기본 생성자의 400 cm는 공통 기본값이며, 실제 플레이에는 위 Blueprint 저장값이 적용된다.

PIE에서 이동 입력, 시점 회전, 벽 접근 시 카메라 단축 및 이탈 시 복원, 패배 후 재도전·후퇴와 승리 후 탐험 복귀를 확인했다. 전투 종료 후 ViewTarget, ControlRotation, FOV, 입력 허용과 전투 카메라 정리를 검사했다. 사용자 직접 플레이에 의한 최종 체감 확인은 별도다.

`ExpeditionPortfolioEditor Win64 Development` 빌드와 기존 `ExpeditionPortfolio.Combat.Presentation.FacingAndCamera` 자동화 테스트를 통과했다. 카메라 구도와 충돌 검증은 그래픽 PIE에서, 자동화 테스트는 별도 NullRHI 프로세스에서 수행했다.

에디터를 재시작한 새 PIE에서도 저장값과 구도를 확인했다. 플레이 창에 W 키를 입력해 위치·방향이 변하고, 마우스 입력으로 시점이 회전하는 것도 확인했다. 벽 단축 검사는 `WC_ArenaRightBoundary`에서 수행했으며 맵 전체의 카메라 가림을 검사한 것은 아니다.
