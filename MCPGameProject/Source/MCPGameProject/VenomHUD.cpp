// venom — On-screen HUD. Player health bar + ARAM-style augment cards.

#include "VenomHUD.h"
#include "ParasitePawn.h"
#include "VenomGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FLinearColor CardPanel(0.030f, 0.028f, 0.045f, 0.97f);
	const FLinearColor CardPanelHover(0.070f, 0.045f, 0.075f, 0.97f);
	const FLinearColor CardBorder(0.45f, 0.06f, 0.08f, 1.f);
	const FLinearColor CardBorderHover(1.0f, 0.15f, 0.15f, 1.f);
	const FLinearColor AccentRed(0.85f, 0.10f, 0.12f, 1.f);
	const FLinearColor TitleColor(1.f, 0.96f, 0.92f, 1.f);
	const FLinearColor DescColor(0.75f, 0.72f, 0.78f, 1.f);
}

void AVenomHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	AParasitePawn* Player = Cast<AParasitePawn>(GetOwningPawn());
	if (!Player)
	{
		return;
	}

	DrawHealthBar(Player);
	DrawWaveBanner();

	if (Player->IsChoosingAugment())
	{
		DrawAugmentCards(Player);
	}

	if (Player->IsDead())
	{
		DrawText(TEXT("=== GAME OVER ===  press R"), FLinearColor::Red,
			(Canvas->SizeX * 0.5f) - 130.f, (Canvas->SizeY * 0.5f) - 10.f, nullptr, 1.6f);
	}
}

void AVenomHUD::DrawHealthBar(AParasitePawn* Player)
{
	const float MaxHP = Player->GetMaxHealth();
	const float Ratio = (MaxHP > 0.f) ? FMath::Clamp(Player->GetHealth() / MaxHP, 0.f, 1.f) : 0.f;

	const float BarW = 460.f;
	const float BarH = 26.f;
	const float X = (Canvas->SizeX - BarW) * 0.5f;
	const float Y = Canvas->SizeY - 70.f;
	const float Pad = 3.f;

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), X - Pad, Y - Pad, BarW + Pad * 2.f, BarH + Pad * 2.f);
	DrawRect(FLinearColor(0.12f, 0.12f, 0.12f, 1.f), X, Y, BarW, BarH);

	FLinearColor Fill = FMath::Lerp(FLinearColor(0.85f, 0.12f, 0.12f), FLinearColor(0.15f, 0.85f, 0.2f), Ratio);
	if (Player->IsInvulnerable())
	{
		Fill = FLinearColor(0.2f, 0.8f, 0.9f);
	}
	DrawRect(Fill, X, Y, BarW * Ratio, BarH);

	const FString Label = FString::Printf(TEXT("%s  %.0f / %.0f"),
		Player->IsPossessing() ? TEXT("HOST") : TEXT("PARASITE"),
		FMath::Max(0.f, Player->GetHealth()), MaxHP);
	DrawText(Label, FLinearColor::White, X + 6.f, Y - 22.f, nullptr, 1.1f);
}

void AVenomHUD::DrawWaveBanner()
{
	const AVenomGameMode* GM = Cast<AVenomGameMode>(UGameplayStatics::GetGameMode(this));
	if (!GM)
	{
		return;
	}
	const int32 Wave = GM->GetCurrentWave();
	if (Wave <= 0)
	{
		return;
	}

	UFont* BigFont = GEngine ? GEngine->GetLargeFont() : nullptr;
	const FString Text = FString::Printf(TEXT("WAVE %d"), Wave);

	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, BigFont, 1.5f);

	const float PadX = 22.f;
	const float PadY = 8.f;
	const float BoxW = W + PadX * 2.f;
	const float BoxH = H + PadY * 2.f;
	const float X = (Canvas->SizeX - BoxW) * 0.5f;
	const float Y = 24.f;

	// Dark pill + red underline, matching the augment-card look.
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X, Y, BoxW, BoxH);
	DrawRect(AccentRed, X, Y + BoxH - 3.f, BoxW, 3.f);
	DrawText(Text, TitleColor, X + PadX, Y + PadY, BigFont, 1.5f);
}

void AVenomHUD::DrawAugmentCards(AParasitePawn* Player)
{
	const TArray<int32>& Options = Player->GetAugmentOptions();
	if (Options.Num() == 0)
	{
		return;
	}

	UFont* BigFont = GEngine ? GEngine->GetLargeFont() : nullptr;
	UFont* MidFont = GEngine ? GEngine->GetMediumFont() : nullptr;

	// Dim the game behind the picker.
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.72f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);

	// Card layout: centred row.
	const float CardW = 280.f;
	const float CardH = 360.f;
	const float Gap = 44.f;
	const int32 N = Options.Num();
	const float TotalW = N * CardW + (N - 1) * Gap;
	const float X0 = (Canvas->SizeX - TotalW) * 0.5f;
	const float Y0 = (Canvas->SizeY - CardH) * 0.5f;

	// Header.
	{
		const FString Header = TEXT("LEVEL UP  -  CHOOSE AN AUGMENT");
		float W = 0.f, H = 0.f;
		GetTextSize(Header, W, H, BigFont, 1.6f);
		DrawText(Header, AccentRed, (Canvas->SizeX - W) * 0.5f, Y0 - 80.f, BigFont, 1.6f);
	}

	for (int32 i = 0; i < N; ++i)
	{
		const float X = X0 + i * (CardW + Gap);
		const FName BoxName(*FString::Printf(TEXT("Augment%d"), i));
		const bool bHovered = (HoveredBox == BoxName);

		// Border frame + panel.
		const float B = bHovered ? 4.f : 2.f;
		DrawRect(bHovered ? CardBorderHover : CardBorder, X - B, Y0 - B, CardW + B * 2.f, CardH + B * 2.f);
		DrawRect(bHovered ? CardPanelHover : CardPanel, X, Y0, CardW, CardH);

		// Top accent strip.
		DrawRect(AccentRed, X, Y0, CardW, 6.f);

		// Key number badge (top centre).
		{
			const FString Key = FString::Printf(TEXT("%d"), i + 1);
			const float BadgeS = 44.f;
			const float BX = X + (CardW - BadgeS) * 0.5f;
			const float BY = Y0 + 26.f;
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), BX, BY, BadgeS, BadgeS);
			DrawRect(AccentRed, BX, BY + BadgeS - 3.f, BadgeS, 3.f);
			float W = 0.f, H = 0.f;
			GetTextSize(Key, W, H, BigFont, 1.8f);
			DrawText(Key, TitleColor, BX + (BadgeS - W) * 0.5f, BY + (BadgeS - H) * 0.5f, BigFont, 1.8f);
		}

		const int32 AugmentId = Options[i];

		// Title.
		{
			const FString Title = Player->AugmentTitle(AugmentId);
			float W = 0.f, H = 0.f;
			GetTextSize(Title, W, H, BigFont, 1.25f);
			DrawText(Title, bHovered ? FLinearColor::White : TitleColor,
				X + (CardW - W) * 0.5f, Y0 + 140.f, BigFont, 1.25f);
		}

		// Divider.
		DrawRect(FLinearColor(AccentRed.R, AccentRed.G, AccentRed.B, 0.55f),
			X + CardW * 0.2f, Y0 + 185.f, CardW * 0.6f, 2.f);

		// Description.
		{
			const FString Desc = Player->AugmentName(AugmentId);
			float W = 0.f, H = 0.f;
			GetTextSize(Desc, W, H, MidFont, 1.15f);
			DrawText(Desc, DescColor, X + (CardW - W) * 0.5f, Y0 + 215.f, MidFont, 1.15f);
		}

		// Bottom hint.
		{
			const FString Hint = bHovered ? TEXT("CLICK TO TAKE") : FString::Printf(TEXT("PRESS %d"), i + 1);
			float W = 0.f, H = 0.f;
			GetTextSize(Hint, W, H, MidFont, 0.95f);
			DrawText(Hint, bHovered ? CardBorderHover : FLinearColor(0.5f, 0.47f, 0.52f, 1.f),
				X + (CardW - W) * 0.5f, Y0 + CardH - 40.f, MidFont, 0.95f);
		}

		// Clickable area.
		AddHitBox(FVector2D(X, Y0), FVector2D(CardW, CardH), BoxName, /*bConsumesInput=*/true);
	}
}

void AVenomHUD::NotifyHitBoxClick(FName BoxName)
{
	Super::NotifyHitBoxClick(BoxName);

	const FString Name = BoxName.ToString();
	if (Name.StartsWith(TEXT("Augment")))
	{
		const int32 Index = FCString::Atoi(*Name.Mid(7));
		if (AParasitePawn* Player = Cast<AParasitePawn>(GetOwningPawn()))
		{
			Player->ChooseAugment(Index);
			HoveredBox = NAME_None;
		}
	}
}

void AVenomHUD::NotifyHitBoxBeginCursorOver(FName BoxName)
{
	Super::NotifyHitBoxBeginCursorOver(BoxName);
	HoveredBox = BoxName;
}

void AVenomHUD::NotifyHitBoxEndCursorOver(FName BoxName)
{
	Super::NotifyHitBoxEndCursorOver(BoxName);
	if (HoveredBox == BoxName)
	{
		HoveredBox = NAME_None;
	}
}
