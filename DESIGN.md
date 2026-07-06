# venom — 게임 설계 & 제작 계획

> 이 문서는 세션 간 인수인계용입니다. 새 Claude Code 세션(C:\venom에서 시작)은 이 문서를 먼저 읽고 이어서 진행하세요.

## 컨셉
**뱀파이어 서바이버즈류 생존 게임.** 주인공은 **기생충**.

- 혼자선 나약함. 사방에서 몰려오는 몹에 접촉해 **숙주로 빙의**한다.
- **한 번에 딱 한 마리만 빙의** (확정). 빙의 중엔 그 숙주 몸을 조종하며 전투.
- 숙주 종류마다 공격/능력이 다름 (근접 브루트 = 광역, 원거리 몹 = 사격 등).
- 숙주 HP가 0이 되면 알몸 기생충으로 **튕겨나옴** → 제한시간 안에 새 숙주를 못 찾으면 **사망**.
- 몹 처치 → 경험치 → 레벨업 시 **기생충 특성** 선택 (감염 속도↑, 숙주 재생, 감염 전이 등 — 숙주가 바뀌어도 유지).
- 시간이 갈수록 웨이브가 거세짐. 얼마나 오래 버티나가 목표.

## 시점/그래픽
- **3D 탑다운** 기본 제안 (엔진 기본 도형·메시로 MCP 프로토타이핑이 가장 빠름).
- 2D 탑다운은 미확정 — 사용자 재확인 필요.

## 제작 로드맵 (사용자 지시: "플레이어부터")
1. **플레이어** — 탑다운 카메라 + 기생충 폰 이동 (Enhanced Input)
2. **몹** — 스폰 + 플레이어 추적
3. **빙의** — 몹 접촉 → 숙주로 전환, 숙주 몸으로 조종
4. **방출/사망** — 숙주 HP 0 → 기생충으로 방출 → 제한시간 내 새 숙주 못 찾으면 사망
5. **성장/난이도** — 처치→경험치→레벨업 특성 선택→웨이브 강화

## 진행 상황 (2026-07-06 기준)
- [x] 1. 플레이어(기생충) 이동 + 탑다운 카메라 (`AParasitePawn`)
- [x] 2. 몹(고블린) 스폰·추적·분리 (`AMobEnemy`, `AVenomGameMode` 스포너)
- [x] 3. 빙의: Q로 사거리 내 몹 순환 선택(하이라이트) → Space 빙의. 빨간 원 = 사거리
- [x] 5(일부). 자동공격(기생충=넉백/사거리 짧음, 고블린=데미지25/사거리 김), 몹 HP 125(5대), 경험치→레벨업→**일시정지 증강 택1**(1/2/3키). `AVenomPlayerController`가 정지 중 입력 처리
- [x] 비주얼 1차: 색상(기생충 보라/고블린 초록/숙주 청록/선택 노랑/피격 흰번쩍) via `M_VenomTint`
- [ ] 4. 숙주 HP·피격·이젝트·사망(게임오버) — 아직 몹이 플레이어를 못 때림
- [ ] 웨이브/난이도 상승, 파티클 이펙트, 캐릭터 모델

### 에셋 생성 팁 (중요)
엔진 기본 도형 머티리얼엔 색 파라미터가 없음. 색/머티리얼 등 에셋이 필요하면 **파이썬 에디터 스크립트로 생성**:
`PythonScriptPlugin` 활성(.uproject) 후 `UnrealEditor-Cmd.exe <proj> -ExecutePythonScript="<py>" -unattended -nullrhi`. 예시 스크립트로 `/Game/Materials/M_VenomTint`(Color 벡터파라미터→BaseColor+Emissive) 생성함. 폰마다 MID로 `Color` 설정.

## 제작 방식
- 사용자는 **MCP 워크플로우** 선택 (C++ 직접 작성 아님).
- 따라서 반드시 **언리얼 에디터를 열어둔 채**, `unrealMCP` MCP 도구(액터/블루프린트/노드 생성 등)로 제작.
- 에디터가 켜져 있어야 플러그인 TCP 서버(포트 55557)가 살아있고 MCP가 동작함.

## 환경 메모 (빠른 참조)
- 엔진: `C:\UE_5.6`. 빌드: `C:\UE_5.6\Engine\Build\BatchFiles\Build.bat MCPGameProjectEditor Win64 Development -project=C:\venom\MCPGameProject\MCPGameProject.uproject -waitmutex`
- 컴파일러: Visual Studio Community 2026 (MSVC 14.51) — "preferred version 아님" 경고는 무시.
- 파이썬 MCP 서버: Python 3.12 고정 (3.14 불가). venv: `C:\venom\Python\.venv`. uv: `C:\Users\이현렬\AppData\Roaming\Python\Python314\Scripts\uv.exe`.
- MCP 설정: `C:\venom\.mcp.json`의 `unrealMCP`.
- 경로에 한글·공백 금지 (그래서 프로젝트가 C:\venom에 있음).
