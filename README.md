# venom

언리얼 엔진(UE 5.6)을 **MCP(Model Context Protocol)** 로 제어해서 게임을 만드는 프로젝트.

AI 어시스턴트(Claude Code)가 자연어로 액터 생성, 블루프린트 작성, 레벨 편집 등을
언리얼 에디터에 직접 지시할 수 있습니다.

기반: [chongdashu/unreal-mcp](https://github.com/chongdashu/unreal-mcp) (MIT)

## 구성

```
MCPGameProject/            언리얼 프로젝트 (UE 5.6)
  Plugins/UnrealMCP/       C++ 플러그인 — 에디터 안에서 TCP 서버(포트 55557) 실행
Python/                    파이썬 MCP 서버 — Claude ↔ 언리얼 에디터 중계
Docs/                      원본 문서
.mcp.json                  Claude Code용 MCP 서버 설정
```

동작 흐름: `Claude Code` ⇄ (MCP) `Python 서버` ⇄ (TCP 55557) `언리얼 에디터 내 플러그인`

## 설치 / 실행 순서

### 1. 사전 준비 (완료된 것)
- [x] Visual Studio Community 2026 (C++ 툴체인)
- [x] Python 3.14 + `uv`
- [x] Epic Games 런처

### 2. 언리얼 엔진 5.6 설치
Epic Games 런처 → Unreal Engine → Library → **5.6** 설치.

### 3. 플러그인 빌드
1. `MCPGameProject/MCPGameProject.uproject` 우클릭 → **Generate Visual Studio project files**
2. 생성된 `.sln` 열기 → 구성 `Development Editor` → **빌드**
   (UnrealMCP 플러그인이 함께 컴파일됨)

### 4. 파이썬 MCP 서버 의존성 설치
```powershell
cd "Python"
uv sync
```

### 5. Claude Code에 MCP 서버 연결
`.mcp.json` 이 이미 준비되어 있음. Claude Code에서 프로젝트를 열면
`unrealMCP` 서버가 인식됨. (승인 프롬프트에서 허용)

### 6. 게임 제작
1. 언리얼 에디터에서 `MCPGameProject` 열기 (플러그인이 TCP 서버 자동 시작)
2. Claude Code에게 자연어로 지시 → 에디터에 반영

## 주의
- 파이썬 MCP 서버는 서드파티 코드입니다. 실행 시 승인 필요.
- 실제 조작은 언리얼 에디터가 **열려 있을 때만** 동작합니다.
