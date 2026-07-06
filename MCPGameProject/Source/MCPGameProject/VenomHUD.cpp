// venom — On-screen HUD. Draws the player's health bar.

#include "VenomHUD.h"
#include "ParasitePawn.h"
#include "Engine/Canvas.h"

void AVenomHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	const AParasitePawn* Player = Cast<AParasitePawn>(GetOwningPawn());
	if (!Player)
	{
		return;
	}

	const float MaxHP = Player->GetMaxHealth();
	const float Ratio = (MaxHP > 0.f) ? FMath::Clamp(Player->GetHealth() / MaxHP, 0.f, 1.f) : 0.f;

	// Bar geometry: bottom-centre.
	const float BarW = 460.f;
	const float BarH = 26.f;
	const float X = (Canvas->SizeX - BarW) * 0.5f;
	const float Y = Canvas->SizeY - 70.f;
	const float Pad = 3.f;

	// Backing frame.
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), X - Pad, Y - Pad, BarW + Pad * 2.f, BarH + Pad * 2.f);
	// Empty track.
	DrawRect(FLinearColor(0.12f, 0.12f, 0.12f, 1.f), X, Y, BarW, BarH);
	// Fill (green -> red as it drops); cyan tint while invulnerable.
	FLinearColor Fill = FMath::Lerp(FLinearColor(0.85f, 0.12f, 0.12f), FLinearColor(0.15f, 0.85f, 0.2f), Ratio);
	if (Player->IsInvulnerable())
	{
		Fill = FLinearColor(0.2f, 0.8f, 0.9f);
	}
	DrawRect(Fill, X, Y, BarW * Ratio, BarH);

	// Label.
	const FString Label = FString::Printf(TEXT("%s  %.0f / %.0f"),
		Player->IsPossessing() ? TEXT("HOST") : TEXT("PARASITE"),
		FMath::Max(0.f, Player->GetHealth()), MaxHP);
	DrawText(Label, FLinearColor::White, X + 6.f, Y - 22.f, nullptr, 1.1f);

	if (Player->IsDead())
	{
		DrawText(TEXT("=== GAME OVER ===  press R"), FLinearColor::Red,
			(Canvas->SizeX * 0.5f) - 130.f, (Canvas->SizeY * 0.5f) - 10.f, nullptr, 1.6f);
	}
}
