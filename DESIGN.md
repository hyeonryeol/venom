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

## 진행 상황 (2026-07-14 기준)
- [x] 1. 플레이어(기생충) 이동 + 탑다운 카메라(고정, 거리 800/-60°, 월드 기준 WASD). 속도 300
- [x] 2. 몹(고블린) 스폰·추적·분리·플레이어 겹침 방지. **임포트된 고블린 스켈레탈 메시** + Walk/Attack/Death 애니 (`/Game/Goblin_low_for_bake*`)
- [x] 3. 빙의: Q 순환 선택 + Space, 사거리 제한(빨간 원), **쿨타임 10초**, 숙주 **현재 HP 상속**. 빙의한 숙주 = **심비오트 틴트 고블린 1.3배**
- [x] 4. 전투 루프: 몹 접촉 데미지 → 숙주 HP 0 → 이젝트(무적 1.5초) → 기생충 HP 0 → 게임오버(R 재시작). HUD 체력바(하단 중앙)
- [x] 5. 자동공격 = **자동 타겟팅 부채꼴**(가장 가까운 적 방향, 반각 55°). 기생충=넉백만/고블린=데미지25. 숙주는 공격 시 타겟 방향으로 회전+스윙 애니
- [x] 경험치→레벨업→**ARAM 스타일 카드 3장 UI**(AVenomHUD 캔버스, 클릭 또는 1/2/3, 일시정지+커서)
- [x] 비주얼: 기생충 = **임포트 심비오트 메시**(`/Game/baisegongshengti_battle`, 스케일 60, M_Symbiote 검정+빨강 맥동, 절차적 bob/squash). 적 고블린 원본 텍스처
- [x] 플레이어 속도 300(고블린 250보다 약간 빠름). 카메라 = 고정 탑다운(800/-60°), 월드 기준 WASD
- [x] **사수 고블린**(`AMobRangedGoblin`): 파란 발광(M_RangedTint), 접근하며 투사체(`AVenomProjectile`) 발사. 스폰의 22%. 빙의하면 플레이어 기본공격이 **투사체 발사**로 바뀜(청록, 자동타겟). 관통은 없음 → **PIERCING SHOT 증강**(id5)으로만
- [x] 이펙트/사운드: 몹 피격 흰 플래시(M_HitFlash 오버레이, 0.14초) + 합성 효과음 9종(`/Game/Sounds`, 빙의음은 공포 버전), 카메라 흔들림(빙의/이젝트/피격)
- [x] 빙의 사거리 빨간 원 = Space 눌렀는데 범위(450)에 몹 없을 때만 1.2초 표시
- [x] **웨이브 디렉터**(`AVenomGameMode`): 웨이브별 스폰 간격/생존 캡/허용 몹 타입을 점진 상향, 주기적 **호드**(한 방향에서 무리 돌진). HUD에 웨이브 번호 + 다음 웨이브 카운트다운 표시.
- [x] **새 몹 이동 변종**(근접/원거리와 독립적으로 롤): **러너**(RunSpeedMultiplier 1.8배 질주) / **점퍼**(주기적으로 플레이어에게 도약). `AMobEnemy::SetVariant`로 스폰 시 지정.
- [x] **증강 11종**(id 0~10): BRUTE FORCE(+뎀)/FRENZY(+공속)/SWIFT OOZE(+이속)/LONG REACH(+빙의사거리)/REPULSION(+넉백)/PIERCING SHOT(관통)/MULTISHOT(투사체+1)/**REGENERATION(숙주 재생 +4HP/s)**/**VIRULENCE(빙의 쿨 -2s)**/**CARAPACE(기생충 최대HP +15)**/**RICOCHET(투사체가 엄폐물에 1회 반사)**. 레벨업마다 3장 중 택1.
- [x] **엄폐물 필러**(`AVenomObstacle`): GameMode가 아레나에 7개 흩어 스폰(황금각 분포). 오버랩 전용 콜리전 → **폰은 자유 통과, 투사체만 차단**(navmesh 불필요). 아군/적 투사체 모두 차단돼 **양방향 엄폐**. RICOCHET 증강 시 플레이어 투사체는 원기둥 법선(중심→투사체) 기준 1회 반사.
- [ ] 맵/아레나 지형 확장(현재는 코드 스폰 필러만 — 조명/데모맵은 미해결), 증강 추가 확장, 보스/특수 웨이브

### 핵심 튜닝 수치 (MobEnemy/ParasitePawn 기본값)
몹 HP 125(사수 90), 접촉뎀 4/쿨1.0s, MinPlayerDistance 100. 사수: ShootRange 750, 쿨2.5s, 투사체 데미지 5/속도 600. 플레이어: HostDamage 25, AttackInterval 0.6, 부채꼴 반각 55°, 빙의 쿨 10s, 이젝트 무적 1.5s, 기생충 HP 30. 투사체: 반경 30, 총구 Z+15/전방 10.

### 주의 (겪은 함정)
- 스켈레탈 FBX 임포트 시 **Skeleton 항목 반드시 None** (기존 스켈레톤 지정하면 본 병합으로 손상 → git checkout 복구).
- 투사체는 **발사자 몸에 겹쳐 생성** → `Launch()`로 친구/적 설정 전 오버랩이 먼저 발생해 자해/오작동. `bLaunched` 전 오버랩 무시로 해결. 비관통은 `bConsumed`+오버랩 비활성화로 한 프레임 다중히트 방지.
- 투사체가 몹 콜리전(반경 45)에 안 맞고 위로 지나가면 관통처럼 보임 → 발사 높이/반경 맞출 것.

### 에셋 생성 팁 (중요)
엔진 기본 도형 머티리얼엔 색 파라미터가 없음. 색/머티리얼 등 에셋이 필요하면 **파이썬 에디터 스크립트로 생성**:
`PythonScriptPlugin` 활성(.uproject) 후 `UnrealEditor-Cmd.exe <proj> -ExecutePythonScript="<py>" -unattended -nullrhi`. 예시 스크립트로 `/Game/Materials/M_VenomTint`(Color 벡터파라미터→BaseColor+Emissive) 생성함. 폰마다 MID로 `Color` 설정.

## 제작 방식 (실제)
- **전부 C++** 로 구현 (GAS/블루프린트 아님). 클래스: `AParasitePawn`(플레이어), `AMobEnemy`/`AMobRangedGoblin`(몹), `AVenomProjectile`, `AVenomGameMode`(스포너), `AVenomPlayerController`(정지 중 입력), `AVenomHUD`(체력바+증강카드).
- 반복 루프: 에디터 닫기 → 코드 수정 → `Build.bat`로 빌드(에디터가 열려 있으면 DLL 잠김으로 실패) → 커밋/푸시 → 에디터 실행해 테스트.
- 머티리얼/사운드 등 에셋은 **파이썬 에디터 스크립트**(`-ExecutePythonScript`)로 생성/임포트.
- MCP(`unrealMCP`)는 연결돼 있으나 런타임 C++ 폰 제어엔 안 씀. 레벨 배치 등에 선택적으로 활용 가능.

## 환경 메모 (빠른 참조)
- 엔진: `C:\UE_5.6`. 빌드: `C:\UE_5.6\Engine\Build\BatchFiles\Build.bat MCPGameProjectEditor Win64 Development -project=C:\venom\MCPGameProject\MCPGameProject.uproject -waitmutex`
- 컴파일러: Visual Studio Community 2026 (MSVC 14.51) — "preferred version 아님" 경고는 무시.
- 파이썬 MCP 서버: Python 3.12 고정 (3.14 불가). venv: `C:\venom\Python\.venv`. uv: `C:\Users\이현렬\AppData\Roaming\Python\Python314\Scripts\uv.exe`.
- MCP 설정: `C:\venom\.mcp.json`의 `unrealMCP`.
- 경로에 한글·공백 금지 (그래서 프로젝트가 C:\venom에 있음).

## 모바일 작업 (편집은 모바일, 빌드·테스트는 PC)
UE 5.6은 Windows 전용이라 모바일/클라우드에선 **코드 편집·커밋만** 하고, **빌드·플레이 테스트는 PC**에서 한다. GitHub 레포([hyeonryeol/venom](https://github.com/hyeonryeol/venom))가 둘 사이의 동기화 통로.

**모바일 편집 수단**
- Claude 모바일 앱 / `claude.ai/code` — AI로 코드 수정 후 GitHub에 커밋·푸시 (클라우드엔 UE 없어 빌드 검증 불가).
- `github.dev` (레포 URL의 github.com → github.dev) — 브라우저 VS Code, 빠른 수동 편집.
- GitHub Codespaces — 좀 더 완전한 클라우드 편집기.

**동기화 규칙 (충돌 방지 — 반드시)**
1. PC 작업 **시작 전** 항상 `cd /c/venom && git pull`.
2. 작업 **끝나면** 항상 `git commit` + `git push` (매 단계 커밋·푸시가 기본).
3. 모바일 편집은 컴파일 검증 없이 반영됨 → 작게 나눠서 하고, 다음 PC 세션에서 `pull → 빌드 → 테스트`로 마무리.

**주의**: 플레이 테스트는 PC에서만. 모바일 세션은 이 문서를 먼저 읽고 위 규칙대로 이어서 진행할 것.
