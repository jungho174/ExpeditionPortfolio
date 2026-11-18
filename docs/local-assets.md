# 로컬 외부 에셋과 Git 제외

Fab/외부 스토어에서 받은 에셋은 로컬 프로젝트에서 사용하되 저장소에 원본을 공개하지 않도록 `.gitignore`에서 제외한다. 파일을 삭제하거나 Unreal 경로를 바꾸지 않는다.

## 제외 경로

- Content/Abandoned_Cathedral
- Content/AncientTempleRuins
- Content/Atlantis_Ruins
- Content/Chaotic_Skies
- Content/CombatMagicAnims
- Content/Dark_Knight
- Content/DragonCave
- Content/FuturisticWarrior
- Content/Lizardman_Berserker
- Content/Mixed_Magic_VFX_Pack
- Content/ParagonPhase
- Content/Polyphoria
- Content/SlashTrail_SoftTofu
- Content/Stone_Golem
- Content/WhiteCastle
- Content/ExpeditionPortfolio/Environment/WhiteCastle/PrivateGeometry (WhiteCastle Nanite 파생 모델)
- Content/ExpeditionPortfolio/Rendering/PrivateVFX (외부 Niagara·재질의 노출 보정 파생본)
- Content/ExpeditionPortfolio/Audio
- SourceArt/Audio/ThirdParty
- Content/__ExternalActors__/AncientTempleRuins (외부 팩 맵의 World Partition 액터)
- Content/ExpeditionPortfolio/Rendering/Enemies/StoneGolem/M_StoneGolemDeathFade.uasset
- Content/ExpeditionPortfolio/Rendering/Enemies/StoneGolem/MI_StoneGolemDeathFade.uasset

프로젝트 폴더에 복제한 외부 AS_* 애니메이션 5개와 Magic/VFX, Defense/NS_ParryImpact도 정확한 경로로 제외한다. 직접 만든 코드·Blueprint·Data Asset·Montage 연결과 자체 합성한 SW_ParryImpact_Tight 및 원본 WAV는 계속 Git 관리 대상이다. 기본 Unreal 템플릿 콘텐츠는 이번 제외에 포함하지 않는다.

골렘 사망 페이드 재질 두 개는 외부 Stone_Golem 재질을 복제해 수정한 파일이므로 제외한다. 원본 팩 설치 후 `Tools/SetupEnemyDeathFade.py`로 다시 생성한다. Unreal 기본 마네킹 모션을 복제하거나 골렘에 리타기팅한 애니메이션과 직접 작성한 Blueprint·IK Rig·Retargeter·공격 패턴은 계속 관리한다. Libre Caslon Display 글꼴과 원본 TTF는 함께 보관한 OFL 고지문을 포함한다.

## 앞으로 에셋을 추가할 때

받은 음원은 Unreal 콘텐츠 브라우저의 ExpeditionPortfolio/Audio 아래에 임포트한다. 원본을 프로젝트 안에 보관하려면 SourceArt/Audio/ThirdParty 아래에 둔다. Unity 패키지의 .meta 파일은 가져올 필요가 없다. 새 캐릭터/몬스터/배경 팩을 다른 최상위 폴더에 추가하면 그 폴더를 `.gitignore`에도 추가해야 한다. 기존 팩의 파일을 다른 경로로 복사해도 제외 규칙이 자동으로 따라가지는 않는다.

## 저장소에서 프로젝트를 다시 열 때

외부 에셋이 빠진 새 체크아웃에서는 관련 메시·애니메이션·VFX·소리 참조가 누락될 수 있다. 각 상품을 적법하게 보유한 계정에서 원래 경로에 다시 설치하고 필요한 설정 스크립트를 실행한다. 이 저장소만으로 모든 콘텐츠를 재현할 수 있다는 의미는 아니다. 원래 에셋 목록과 라이선스 증빙은 별도로 보관한다.

`.gitignore`는 이미 추적되거나 과거 커밋에 들어간 파일을 제거하지 않는다. 이번에 지정한 현재 외부 에셋 경로는 수정 시점에 모두 추적 전이었다. 이미 원격에 업로드한 파일이나 과거 이력을 삭제하는 작업은 이번 변경에 포함하지 않는다. Git LFS 역시 업로드 제외 기능이 아니다.
