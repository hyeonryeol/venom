# PC 세션 테스트 인수인계 (모바일 → PC)

> 이 파일은 모바일 세션이 남긴 **테스트 요청 메모**입니다. PC에서 확인 후 지워도 됩니다.

## 이번에 바뀐 것 (2026-07-14, `claude/design-review-continuation-k0eho0`)
증강(레벨업 카드) **7종 → 10종**으로 확장. 추가 3종:
- **REGENERATION** (id7) — 빙의 중 숙주 HP **+4/초** 재생
- **VIRULENCE** (id8) — 빙의 쿨다운 **-2초** (하한 2초)
- **CARAPACE** (id9) — 기생충 최대 HP **+15**

건드린 파일: `ParasitePawn.h`, `ParasitePawn.cpp` (헤더 `HostRegenRate` 필드 + Tick 재생 로직 + `ApplyAugment`/`AugmentName`/`AugmentTitle`의 case 7~9, 증강 풀 `{0..9}`). HUD는 제네릭 렌더라 변경 없음.
> ⚠️ 모바일에서 **컴파일 검증 안 됨**. 빌드부터 확인 필요.

## 절차
1. **동기화**
   ```
   cd /c/venom && git pull origin claude/design-review-continuation-k0eho0
   ```
2. **에디터 닫고 빌드** (에디터 열려 있으면 DLL 잠김으로 실패)
   ```
   C:\UE_5.6\Engine\Build\BatchFiles\Build.bat MCPGameProjectEditor Win64 Development -project=C:\venom\MCPGameProject\MCPGameProject.uproject -waitmutex
   ```
   - 빌드 실패하면 에러 로그를 다음 세션에 붙여주세요.
3. **플레이 테스트 (검증 포인트)**
   - 레벨업 카드에 새 증강(REGENERATION / VIRULENCE / CARAPACE)이 **간헐적으로 등장**하는지 (풀 10종 중 3장 랜덤).
   - **REGENERATION**: 몹에 빙의한 상태에서 피격 후 시간이 지나면 하단 체력바가 **서서히 회복**되는지 (최대치 초과 회복 없음).
   - **VIRULENCE**: 뽑은 뒤 빙의 → 이젝트 후 **재빙의 가능 시점이 빨라지는지**.
   - **CARAPACE**: 뽑는 즉시 기생충 상태 최대 HP가 **+15** 되고 현재 HP도 오르는지. 이후 이젝트 시에도 최대치 유지되는지.

## 결과 회신
- 잘 되면: 이 파일 삭제 + 커밋, DESIGN.md의 증강 항목 확정.
- 문제 있으면: 증상/에러 로그를 남겨주면 모바일 세션에서 이어서 수정.
