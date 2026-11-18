# 약한 마법 발사체 설정

콘텐츠 브라우저에서 `Content/ExpeditionPortfolio/Animation/Magic/DA_BasicMagic`을 연다. 약한 마법은 왼손을 뻗는 약 1.0초의 Notify에서 불꽃을 발사하고, 적에게 도착할 때 폭발과 피해 25를 한 번 적용한다. 비행과 공격 모션이 모두 끝나면 다음 턴으로 넘어간다.

| 에디터 프로퍼티 | 현재 설정 | 의미 |
|---|---|---|
| Use Projectile | 켬 | Notify를 즉시 피해 대신 발사 신호로 사용 |
| Attack Effect | NS_Magma_Shot_Projectile | 이동하는 불꽃과 궤적 |
| Cast Socket | hand_l | 발사 위치 |
| Attack Effect Transform | 크기 0.5 | 손 기준 발사 위치·회전·크기 보정 |
| Projectile Speed | 500cm/s | 낮추면 천천히 날아감 |
| Max Flight Duration | 3초 | 먼 표적에서도 비행이 끝나는 최대 시간 |
| Target Socket | spine_03 | 발사 순간 포착할 적의 몸통 위치 |
| Impact Effect | NS_Magma_Shot_Impact | 도착 시에만 생성할 폭발 |
| Impact Effect Transform | 크기 0.5 | 도착 지점 기준 폭발 위치·회전·크기 보정 |
| Attack Effect Tail Duration | 1초 | 행동 완료 후 폭발 효과의 추가 재생 시간 |

비행 시간은 거리/속도이며 최소 0.05초부터 Max Flight Duration까지 제한한다. 최대 시간에 걸리는 긴 거리에서는 실제 속도가 설정 속도보다 빨라진다. 표적 뼈가 없으면 적 Actor 위치를 사용한다. 폭발 보정의 X는 시전자에서 표적을 향하는 방향이며 적 Actor의 회전이나 카메라를 따르지 않는다.

발사 시 목적지를 고정하는 직선 연출이다. 유도탄, 물리 충돌, 장애물 차단은 포함하지 않는다. 복사본의 불꽃·연기·불티는 로컬 공간에서 이동 컴포넌트를 함께 따른다. 도착 시 새 방출을 멈추고 잔여 입자는 자연스럽게 소멸한다. Tail Duration은 모든 입자의 전체 수명이 아니다.

Notify가 누락된 정상 모션은 경고 후 종료 시점에 발사한다. 중단·시간 초과·참가자 제거 시 아직 적용하지 않은 피해와 비행을 취소한다. 이미 도착하여 적용한 피해는 되돌리지 않는다. 안전 Timer는 모션 길이와 최대 비행 시간에 여유 1초를 더한다. 발사 프레임의 전체 DeltaTime을 비행에 더하지 않으므로 첫 VFX 준비가 느려져도 같은 프레임에 바로 명중하지 않는다.

두 VFX는 `Content/ExpeditionPortfolio/Animation/Magic/VFX`의 프로젝트 복사본이다. 원본 팩은 보존한다. 발사체 Main의 Collision/Kill Particles는 비활성화하여 시전자 캡슐 등과의 VFX 내부 충돌이 불꽃을 조기에 제거하지 않게 한다. 명중은 C++ 비행 완료가 담당한다. 발사체 System의 Warmup Time은 0.2초, Warmup Tick Delta는 1/30초로 초기 불꽃 상태를 준비한다. 이는 실제 발사를 0.2초 늦추는 설정이 아니다.

`Tools/SetupMagicProjectile.py`는 복사본과 연결을 만들며 기존 속도·최대 시간·위치 보정을 보존한다. 편집기 전용 C++ API로 불꽃 전체가 컴포넌트를 따르도록 좌표 공간을 맞추고 코어의 중복 충돌/제거 모듈을 끈다. 전체 `SetupMagicActions.py`는 마법 기본값도 설정하므로 기존 튜닝을 보존하려면 발사체 전용 스크립트를 사용한다.

## 직접 플레이 확인

새 PIE에서 조우 후 기술 → 약한 마법을 선택한다. 왼손을 뻗기 전에는 불꽃·폭발·피해가 없어야 한다. 손에서 나온 불꽃이 적의 몸통으로 날아가고 도착 시 HP가 25 줄며 폭발해야 한다. 행동 중 명령 메뉴는 숨고 HP는 유지된다. 다음 턴 메뉴 복귀와 기존 강한 마법도 확인한다. 비행을 더 또렷하게 보고 싶으면 Projectile Speed를 낮춰 새 공격에서 확인한다.

값은 비행 시작 때 읽고 비행 중에는 목적지와 시간을 유지한다. 최종 튜닝은 PIE를 종료한 뒤 저장한다. 카메라 전환과 리듬 입력은 별도 후속 작업이다.
