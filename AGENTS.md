# 개발 규칙

- 전투 규칙, 상태 전이, 판정처럼 테스트가 필요한 핵심 로직은 C++로 구현한다.
- 콘텐츠 조립, 수치 설정, Animation, VFX, SFX, 카메라 연출은 Blueprint와 Data Asset을 사용한다.
- UI는 전투 상태를 직접 소유하거나 전이시키지 않는다. UI는 게임 로직의 상태를 표시하고 명령만 전달한다.
- 매 프레임 처리가 반드시 필요한 경우가 아니면 `Tick`을 사용하지 않는다. Event, Delegate, Timer를 우선한다.
- `Binaries`, `DerivedDataCache`, `Intermediate`, `Saved` 등 생성 폴더의 파일과 기존 `.uasset`, `.umap`을 임의로 삭제하거나 이름 변경하지 않는다.
- 작업 후 영향 범위에 맞는 테스트를 수행하고 `ExpeditionPortfolioEditor Win64 Development` 빌드를 검증한다.
- 요청 범위와 관련 없는 리팩터링, 파일 이동, 포맷 변경을 하지 않는다.
- 상세 목표와 설계는 `docs/project-overview.md`, `docs/architecture.md`, `docs/combat-state-machine.md`, `docs/development-roadmap.md`를 참조한다.
