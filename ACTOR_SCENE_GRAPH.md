# Actor / SceneGraph 구조

## 책임 분리

| 파일 | 책임 |
| --- | --- |
| `SimpleGame/Actor.h` | 공통 객체, 로컬 위치, 활성/표시 상태, 갱신 단계, 렌더 레이어, 복제 |
| `SimpleGame/SceneGraph.h/.cpp` | 객체 소유권, ID, 부모·자식 관계, 좌표 합성, 생성·지연 삭제, 갱신과 깊이 정렬 |
| `SimpleGame/LevelActors.h` | 플레이어, 무기, 적/보스, 발사체, 드랍, 바닥/벽, 입구, 사거리, 공격 예고 |
| `SimpleGame/LevelActorUpdate.cpp` | 각 Actor의 이동, 적 AI, 자동 조준/발사, 발사체 충돌, 자석 습득 |
| `SimpleGame/LevelActorDraw.cpp` | 각 Actor의 표현과 공통 캐릭터 도형 |
| `SimpleGame/LevelOne.cpp` | 씬 구성, 단계별 갱신, 카메라, 귀환 |
| `SimpleGame/LevelNavigation.cpp` | 랜덤 맵, 연결성 검사, 충돌/시야, 경로 거리장 |
| `SimpleGame/LevelRules.cpp` | 성장 수치, 스폰 조건, 사망 보상, 클리어 규칙 |
| `SimpleGame/LevelInterface.cpp` | HUD, 안내, 사망 화면 |
| `SimpleGame/LevelSave.cpp` | 기존 `LEVEL_ONE 1` 저장 형식의 읽기/쓰기 |
| `SimpleGame/WorldActors.h/.cpp` | 마을/오픈 월드 객체, 월드 플레이어, 지형, 청크 경계, 안개 |
| `SimpleGame/WorldGeneration.h` | 기존 월드 생성 해시와 도로 규칙 공유 |

`Game`과 `LevelOne`은 각각 SceneGraph를 소유한다. 화면에 배치되는 월드 객체는 Actor이며, 체력바·대화·메뉴 등 화면 UI는 후처리 뒤에 기존 Renderer 인터페이스 패스로 그린다. 메시 캐시와 외부 셰이더는 기존 Renderer가 담당한다.

## 실제 계층

```text
LevelOne scene
├─ terrain
│  ├─ TileActor (바닥/벽)
│  └─ EntranceActor
├─ characters
│  ├─ PlayerActor
│  │  ├─ WeaponActor
│  │  └─ RangeActor
│  └─ EnemyActor (일반 적/보스)
│     └─ WarningActor
└─ effects
   ├─ ProjectileActor
   └─ LootActor

Game scene
├─ village → 건물, 폐허, 모닥불, 불지기, 기념비
├─ heirs → 가까운 전승 NPC
├─ WorldActor (플레이어)
├─ MistActor
└─ chunk roots (7 × 7)
   ├─ WorldTileActor
   ├─ WorldActor (나무, 바위, 기념비)
   └─ ChunkBoundaryActor
```

청크 루트의 위치는 청크 원점이고, 자식은 청크 내부 로컬 좌표를 사용한다. 그리기·월드 충돌·기념비 상호작용은 부모 위치가 합쳐진 월드 좌표를 사용한다. 청크 루트를 삭제하면 자식도 제거된다. 전승 기록은 저장 데이터에 남고 가까운 기록만 씬에 배치된다.

## SceneGraph 사용

```cpp
SceneGraph scene;
ActorId group = scene.Create<Actor>(0); // 0은 부모 없음
scene.Get<Actor>(group).p = {100, 200};
ActorId tile = scene.Create<TileActor>(group, 2, 3, false);
ActorPosition world = scene.WorldPosition(tile); // 102.5, 203.5

scene.Get<Actor>(group).visible = false; // 자식도 숨김, 갱신은 계속
scene.Get<Actor>(group).enabled = false; // 자식의 갱신/그리기도 중지
scene.SetParent(tile, 0);               // 기본값: 월드 위치 유지
scene.SetParent(tile, group, false);    // 로컬 위치 유지
scene.Destroy(group);                  // 자식까지 삭제 예약
scene.FlushDestroyed();                // 안전한 갱신 경계에서 실제 해제
```

- `Parent`, `Children`, `Find`, `Get`, `Query<T>`, `Visit<T>`로 계층과 객체를 조회한다.
- ID는 해당 씬 안에서만 유효하다. 저장 파일에 런타임 ID를 기록하지 않는다.
- `Query<T>`는 살아 있는 객체의 참조 스냅샷이다. `FlushDestroyed()` 이후까지 보관하지 않는다. `Visit<T>`는 활성 객체와 합성된 월드 좌표를 전달한다.
- 로컬/월드 변환은 현재 2D 평행이동이다. 회전·스케일 상속은 아직 구현하지 않았다.
- 전투 Actor는 원점이 이동하지 않는 레벨 그룹에 배치한다. 추후 전투 그룹 자체를 이동시킬 경우 이동/충돌 서비스에도 월드↔로컬 변환을 적용해야 한다.
- 공격 예고의 충돌 위치는 시전 순간의 월드 좌표에 고정한다. 부모 적이 사라지면 예고 노드도 사라진다.

## 갱신·그리기·복사

레벨 갱신 순서는 이동 → 경로/스폰 → 적 → 무기 → 발사체 → 사망 보상 → 드랍 → 실제 삭제이다. 같은 단계 도중 생성된 객체는 그 단계의 다음 호출부터 갱신한다. 뒤쪽 단계 객체는 같은 프레임에 갱신할 수 있어 기존 발사/습득 흐름을 유지한다.

그리기는 부모별로 묶어 출력하지 않고, 활성 노드를 모아 `RenderLayer`와 월드 `x+y` 순으로 정렬한다. 서로 다른 부모에 속한 벽과 캐릭터도 올바른 가림 순서를 갖는다. 순서가 같은 객체는 생성 순서를 유지한다.

씬 복사는 각 Actor의 `Clone()`으로 상태를 깊게 복사한다. `LevelOne` 저장 실패 롤백과 로드 검증용 사본이 원본 객체를 공유하지 않는다. 새 클래스는 `CloneableActor<새클래스>`를 상속하고 `Update`/`Draw`를 구현한다. 씬이나 레벨의 포인터를 Actor 멤버에 보관하지 말고 호출마다 `SceneContext`를 사용한다.

## 확인 상태와 수동 확인 항목

빌드·실행·렌더링 확인은 사용자 요청에 따라 수행하지 않았다. 프로젝트 소스 등록, 선언/정의, 구형 컨테이너 참조 제거, 포맷과 저장 필드 순서를 정적으로 확인한다.

사용자 실행 시 확인할 항목:

1. 기존 저장 파일로 마을/레벨 진입, 체력·경험치·적·드랍·보스 상태 복원.
2. 자동 공격, 벽에 막히는 발사체, 드랍 자석, 레벨업, 보스 공격 예고와 클리어.
3. 사망 전승과 귀환, 전승 NPC 생성, 저장 후 재실행.
4. 음수 좌표 청크를 포함해 이동하며 지형/장애물 위치, 충돌, 기념비 조사, 청크 제거/재생성.
5. 벽/캐릭터 가림, 사거리와 예고 표시, 한글 HUD, HDR/Bloom.

`Tests/SceneGraphTests.cpp`는 좌표 상속, 순환 방지, 재부모화, 자식 삭제, 복사 독립성과 활성 상태 상속을 확인하는 별도 테스트 소스다. 게임 프로젝트에는 포함하지 않았으며 실행하지 않았다.
