# PC 세션 테스트 인수인계 (모바일 → PC)

> 이 파일은 모바일 세션이 남긴 **테스트 요청 메모**입니다. PC에서 확인 후 지워도 됩니다.

## 이번에 바뀐 것 (엄폐물 + RICOCHET 증강)
브랜치: `claude/design-review-continuation-k0eho0`

1. **엄폐물 필러**(`AVenomObstacle`, 신규 파일) — GameMode가 플레이 시작 시 아레나에 **7개** 흩어 스폰.
   - 오버랩 전용 콜리전: **폰(플레이어/몹)은 자유 통과, 투사체만 차단** = 엄폐물.
   - 아군/적 투사체 모두 막힘 → 뒤에 숨으면 적 사격을 피할 수 있음.
2. **RICOCHET 증강**(id10) — 플레이어(사수 숙주) 투사체가 필러에 **1회 반사**. 증강 없으면 필러에 맞고 소멸.

건드린 파일: `VenomObstacle.h/.cpp`(신규), `VenomProjectile.h/.cpp`(Launch에 Bounce 파라미터 + 오버랩 시 필러 반사/소멸), `ParasitePawn.h/.cpp`(`BounceCharges` + 증강 case10), `VenomGameMode.h/.cpp`(`SpawnObstacles`).
> ⚠️ 모바일에서 **컴파일 검증 안 됨**. 빌드부터 확인. 신규 .cpp/.h는 UBT가 자동 인식(Build.cs 수정 불필요).

## 절차
1. **동기화**: `cd /c/venom && git pull origin claude/design-review-continuation-k0eho0`
2. **에디터 닫고 빌드**:
   ```
   C:\UE_5.6\Engine\Build\BatchFiles\Build.bat MCPGameProjectEditor Win64 Development -project=C:\venom\MCPGameProject\MCPGameProject.uproject -waitmutex
   ```
3. **플레이 테스트 (검증 포인트)**
   - 시작하면 맵에 **어두운 돌기둥 7개**가 흩어져 보이는지. (Z 높이/스케일이 어색하면 `AVenomObstacle` 생성자의 `SetRelativeScale3D`/`SetRelativeLocation`, GameMode의 `NumObstacles`·`ObstacleMin/MaxRadius`로 튜닝)
   - 플레이어·몹이 기둥을 **그냥 통과**하는지(막히면 안 됨).
   - **사수 몹의 투사체가 기둥에 막히는지**(기둥 뒤로 숨어 확인).
   - 사수 고블린에 빙의해 **기본 투사체가 기둥에 맞고 사라지는지**(증강 전).
   - **RICOCHET** 증강을 뽑은 뒤, 투사체가 기둥에 맞고 **한 번 튕겨서** 다른 방향으로 날아가는지. 두 번째 기둥에선 소멸.
   - 반사각이 이상하면(원기둥이라 중심 기준 근사) 알려주세요.

## 결과 회신
- 잘 되면: 이 파일 삭제 + 커밋. 안 되면 증상/에러 로그 남겨주면 이어서 수정.
