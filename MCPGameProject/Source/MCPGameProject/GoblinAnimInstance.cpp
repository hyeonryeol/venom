// venom — Layered anim instance for the possessed host goblin.

#include "GoblinAnimInstance.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimNodeBase.h"
#include "BonePose.h"
#include "BoneContainer.h"

void FGoblinAnimProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);
	LocoTime = 0.f;
	UpperBodyWeight = 0.f;
	UpperBodyMask.Reset();
}

void FGoblinAnimProxy::Update(float DeltaSeconds)
{
	FAnimInstanceProxy::Update(DeltaSeconds);

	if (LocomotionAnim)
	{
		LocoTime += DeltaSeconds * LocomotionPlayRate;
		const float Len = LocomotionAnim->GetPlayLength();
		if (Len > 0.f)
		{
			LocoTime = FMath::Fmod(LocoTime, Len);
			if (LocoTime < 0.f)
			{
				LocoTime += Len;
			}
		}
	}

	// Ease the upper body in/out of the attack so it blends smoothly.
	const float Target = bAttacking ? 1.f : 0.f;
	UpperBodyWeight = FMath::FInterpTo(UpperBodyWeight, Target, DeltaSeconds, 14.f);
}

void FGoblinAnimProxy::RebuildMask(const FBoneContainer& BoneContainer)
{
	const int32 Num = BoneContainer.GetCompactPoseNumBones();
	UpperBodyMask.Init(false, Num);

	const USkeleton* Skel = BoneContainer.GetSkeletonAsset();
	if (!Skel)
	{
		return;
	}
	const FReferenceSkeleton& Ref = Skel->GetReferenceSkeleton();
	const int32 RootUpper = Ref.FindBoneIndex(UpperBodyRootBone);
	if (RootUpper == INDEX_NONE)
	{
		return;
	}

	// A bone belongs to the upper body if the split bone is one of its ancestors.
	for (int32 c = 0; c < Num; ++c)
	{
		const FCompactPoseBoneIndex CIdx(c);
		const FSkeletonPoseBoneIndex SkelIdx = BoneContainer.GetSkeletonPoseIndexFromCompactPoseIndex(CIdx);
		int32 b = SkelIdx.GetInt();
		while (b != INDEX_NONE)
		{
			if (b == RootUpper)
			{
				UpperBodyMask[c] = true;
				break;
			}
			b = Ref.GetParentIndex(b);
		}
	}
}

bool FGoblinAnimProxy::Evaluate(FPoseContext& Output)
{
	const FBoneContainer& BC = Output.Pose.GetBoneContainer();

	// Base pose: full-body locomotion.
	if (LocomotionAnim)
	{
		FAnimationPoseData PoseData(Output);
		const FAnimExtractContext Ctx((double)LocoTime, false);
		LocomotionAnim->GetAnimationPose(PoseData, Ctx);
	}
	else
	{
		Output.ResetToRefPose();
	}

	// Layer the attack onto the upper body.
	if (AttackAnim && UpperBodyWeight > KINDA_SMALL_NUMBER)
	{
		if (UpperBodyMask.Num() != Output.Pose.GetNumBones())
		{
			RebuildMask(BC);
		}

		FCompactPose UpperPose;
		UpperPose.SetBoneContainer(&BC);
		FBlendedCurve UpperCurve;
		UpperCurve.InitFrom(BC);
		UE::Anim::FStackAttributeContainer UpperAttr;
		FAnimationPoseData UpperData(UpperPose, UpperCurve, UpperAttr);
		const FAnimExtractContext Ctx((double)AttackTime, false);
		AttackAnim->GetAnimationPose(UpperData, Ctx);

		for (const FCompactPoseBoneIndex BoneIndex : Output.Pose.ForEachBoneIndex())
		{
			const int32 i = BoneIndex.GetInt();
			if (UpperBodyMask.IsValidIndex(i) && UpperBodyMask[i])
			{
				FTransform Blended;
				Blended.Blend(Output.Pose[BoneIndex], UpperPose[BoneIndex], UpperBodyWeight);
				Blended.NormalizeRotation();
				Output.Pose[BoneIndex] = Blended;
			}
		}
	}

	return true;
}

FAnimInstanceProxy* UGoblinAnimInstance::CreateAnimInstanceProxy()
{
	return new FGoblinAnimProxy(this);
}

void UGoblinAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}

void UGoblinAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	FGoblinAnimProxy& Proxy = GetProxyOnGameThread<FGoblinAnimProxy>();
	Proxy.LocomotionAnim = LocomotionAnim;
	Proxy.AttackAnim = AttackAnim;
	Proxy.LocomotionPlayRate = LocomotionPlayRate;
	Proxy.AttackTime = AttackTime;
	Proxy.bAttacking = bAttacking;
}
