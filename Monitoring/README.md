# 로컬 게임 월드 모니터링

## 실행

1. Visual Studio에서 `SimpleGame.sln`을 다시 빌드하고 게임을 실행합니다.
2. Node.js가 설치된 PC에서 이 폴더의 `start.cmd`를 실행합니다.
3. 브라우저에서 http://127.0.0.1:8765 를 엽니다. 종료는 서버 창에서 Ctrl+C입니다.

외부 패키지는 필요 없습니다. HTML 파일을 직접 더블 클릭하는 대신 로컬 서버 주소를 사용합니다.
포트 변경은 `MONITOR_PORT` 환경 변수로 설정합니다.

게임은 실행 파일 옆 `Monitoring/`에 1초마다 JSON 전체 스냅샷을 임시 파일 작성 후 교체합니다.
수신기는 저장소의 Debug, Release, x64/Debug, x64/Release 및 SimpleGame 하위의 같은 경로를 확인합니다.
사용자 지정 빌드 경로는 `node Monitoring/server.cjs "D:\Game\Monitoring"`처럼 추가합니다.
게임 내보내기 파일 쓰기가 실패하면 새 메시지가 중단되고 대시보드는 5초 후 수신 중단으로 표시합니다.

## 현재 수집 범위

- 시작 마을: 실제 Villager NPC의 대기/비활성 상태와 위치.
- 주변 월드: 현재 로드된 Heir NPC. 로드되지 않은 영역의 NPC는 집계하지 않습니다.
- 전투 지역: 살아 있는 EnemyActor 수. 비활성 지역도 마지막 상태를 표시합니다.
- NPC 수는 목록의 전체 엔티티 수이며 플레이어는 제외합니다.
- 월드별 수신 중단, 지역별 활성/일시 정지, NPC별 상태와 위치를 표시합니다.
- 월드 선택은 카드 집계에도 적용되며 이름 검색은 지역 카드와 NPC 목록에 적용됩니다.

현재 게임에는 시작 마을 하나가 있습니다. 존재하지 않는 마을이나 NPC 행동 상태를 만들어 표시하지 않습니다.
추가 월드/마을은 아래 메시지로 연결할 수 있습니다. 이는 통합 관측 계층이며, 월드 간 게임 상태 동기화나 이동 기능은 아닙니다.

## JSON 메시지 계약 (v1)

`POST http://127.0.0.1:8765/api/snapshot`, `Content-Type: application/json`.
각 메시지는 한 월드의 **전체 상태**입니다. 빠진 지역/NPC는 다음 상태에서 제거됩니다.

```json
{
  "schemaVersion": 1,
  "worldId": "world-seoul-session-001",
  "name": "서울 월드",
  "sequence": 1,
  "villages": [
    {
      "id": "village-01",
      "name": "시작 마을",
      "status": "active",
      "monsterCount": 12,
      "npcs": [
        {"id": "npc-01", "name": "불지기", "status": "idle", "x": -0.8, "y": 1.2}
      ]
    }
  ]
}
```

지역 상태: `active`, `paused`, `warning`.
NPC 상태: `idle`, `moving`, `combat`, `dead`, `disabled`, `unknown`.
ID와 이름은 1~160자 문자열, 좌표는 유한 숫자, 몹 수와 sequence는 0 이상의 안전한 정수입니다.
지역 ID는 월드 내, NPC ID는 지역 내에서 중복되면 안 됩니다.
sequence는 같은 worldId에서 계속 증가해야 합니다. 중복/역순 메시지는 409로 거절되며 마지막 수신 시각도 갱신하지 않습니다.
재시작 시 새 세션 ID를 사용하거나 sequence를 유지하세요. 게임은 PID와 시작 시각을 포함한 세션 ID를 사용합니다.
같은 월드 ID를 여러 발행자가 공유하면 안 됩니다. 연결 종료된 월드는 마지막 상태로 남아 있으며 집계에도 포함됩니다.
수신기 재시작 시 HTTP 메시지는 사라지고 파일 스냅샷은 다시 읽습니다. 오래된 파일은 수정 시각 기준으로 수신 중단 처리합니다.

메시지 최대 1 MiB, 월드당 지역 1,000개/NPC 10,000개, 수신기 최대 256개 월드입니다.
다른 PC에서 직접 접속할 수 없도록 127.0.0.1에만 바인딩합니다.
여러 PC를 실제로 연결하려면 인증 및 전송 계층을 추가해야 합니다.

테스트: `node --test Monitoring/server.test.cjs`
