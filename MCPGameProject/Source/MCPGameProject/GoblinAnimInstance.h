// venom — Layered anim instance for the possessed host goblin.
// Base = locomotion (legs/hips); the attack is layered onto the upper body
// (Spine and above), so the host can walk/backpedal while it swings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "GoblinAnimInstance.generated.h"

class UAnimSequenceBase;

/** Worker-thread proxy that does the per-bone layered blend. */
struct FGoblinAnimProxy : public FAnimInstanceProxy
{
public:
	FGoblinAnimProxy() = default;
	explicit FGoblinAnimProxy(UAnimInstance* InInstance) : FAnimInstanceProxy(InInstance) {}

	// Inputs, copied from the UAnimInstance each game-thread update.
	UAnimSequenceBase* LocomotionAnim = nullptr;
	UAnimSequenceBase* AttackAnim = nullptr;
	float LocomotionPlayRate = 1.f;
	float AttackTime = 0.f;   // elapsed seconds since the current swing began
	bool bAttacking = false;
	FName UpperBodyRootBone = FName(TEXT("Spine"));

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	float LocoTime = 0.f;
	float UpperBodyWeight = 0.f;
	TArray<bool> UpperBodyMask; // per compact-pose bone: true = drive from the attack

	void RebuildMask(const FBoneContainer& BoneContainer);
};

UCLASS()
class MCPGAMEPROJECT_API UGoblinAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// Set by the pawn each frame.
	UPROPERTY(Transient)
	UAnimSequenceBase* LocomotionAnim = nullptr;

	UPROPERTY(Transient)
	UAnimSequenceBase* AttackAnim = nullptr;

	float LocomotionPlayRate = 1.f;
	float AttackTime = 0.f;
	bool bAttacking = false;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
