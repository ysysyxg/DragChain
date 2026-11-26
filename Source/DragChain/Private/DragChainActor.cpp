// Copyright (c) 2026 ysysyxg (https://github.com/ysysyxg)
// SPDX-License-Identifier: MIT
//
// DragChainActor.cpp — 工业拖链（电缆链）动画实现

#include "DragChainActor.h"
#include "Components/SplineComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "MeshDescription.h"
#include "StaticMeshOperations.h"
#include "UObject/SavePackage.h"
#include "Misc/MessageDialog.h"

/** 总长度预设对应的拖链总长度（cm，1米=100） */
static float TravelPresetCm(EDragChainTravelPreset Preset)
{
	switch (Preset)
	{
	case EDragChainTravelPreset::Len060:  return 60.0f;
	case EDragChainTravelPreset::Len080:  return 80.0f;
	case EDragChainTravelPreset::Len130:  return 130.0f;
	case EDragChainTravelPreset::Len250:  return 250.0f;
	case EDragChainTravelPreset::Len300:  return 300.0f;
	case EDragChainTravelPreset::Len320:  return 320.0f;
	case EDragChainTravelPreset::Len400:  return 400.0f;
	case EDragChainTravelPreset::Len600:  return 600.0f;
	case EDragChainTravelPreset::Len800:  return 800.0f;
	case EDragChainTravelPreset::Len1000: return 1000.0f;
	case EDragChainTravelPreset::Len1200: return 1200.0f;
	case EDragChainTravelPreset::Len1300: return 1300.0f;
	case EDragChainTravelPreset::Len1400: return 1400.0f;
	case EDragChainTravelPreset::Len2600: return 2600.0f;
	default:                               return 0.0f;
	}
}

ADragChainActor::ADragChainActor()
{
	PrimaryActorTick.bCanEverTick = true;
	// 驱动模式必须在最晚的 tick 组读取带动对象本帧的最终变换：默认 PrePhysics 下
	// 对象由 Sequencer / 自身 Tick 在本帧后段写入位置时，链条会读到上一帧 → 视觉滞后
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	PathSpline = CreateDefaultSubobject<USplineComponent>(TEXT("PathSpline"));
	PathSpline->SetupAttachment(Root);

	HeadLinks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HeadLinks"));
	HeadLinks->SetupAttachment(Root);

	MidLinks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MidLinks"));
	MidLinks->SetupAttachment(Root);

	TailLinks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TailLinks"));
	TailLinks->SetupAttachment(Root);

	// 插件预置模型（/DragChain/Models，源自"拖链模型260813"）：放置即显示三段模型，可再手动替换
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HeadMeshAsset(TEXT("/DragChain/Models/HeadLink.HeadLink"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MidMeshAsset(TEXT("/DragChain/Models/MidLink.MidLink"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TailMeshAsset(TEXT("/DragChain/Models/TailLink.TailLink"));
	// 预置模型原头/尾角色弄反：固定端(头)实际外形是 TailLink、车架端(尾)实际外形是 HeadLink，按对调修正（仍可在细节面板手动替换）
	if (TailMeshAsset.Succeeded()) { HeadMesh = TailMeshAsset.Object; }
	if (MidMeshAsset.Succeeded())  { MidMesh = MidMeshAsset.Object; }
	if (HeadMeshAsset.Succeeded()) { TailMesh = HeadMeshAsset.Object; }
}

void ADragChainActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!PathSpline || !HeadLinks || !MidLinks || !TailLinks)
	{
		return;
	}

	ApplyMeshes();
	RefreshDriverBindings();
	RefreshChain();
}

void ADragChainActor::BeginPlay()
{
	Super::BeginPlay();
	// 游戏世界（含 MRQ 的 PIE 路径）里关卡 Actor 不一定跑 OnConstruction，这里补一次绑定
	RefreshDriverBindings();
}

void ADragChainActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BoundFixedComp.IsValid())
	{
		BoundFixedComp->TransformUpdated.RemoveAll(this);
	}
	if (BoundDrivenComp.IsValid())
	{
		BoundDrivenComp->TransformUpdated.RemoveAll(this);
	}
	BoundFixedComp.Reset();
	BoundDrivenComp.Reset();
	Super::EndPlay(EndPlayReason);
}

void ADragChainActor::RefreshDriverBindings()
{
	USceneComponent* NewFixed = (FixedParent && FixedParent != this) ? FixedParent->GetRootComponent() : nullptr;
	USceneComponent* NewDriven = (DrivenActor && DrivenActor != this && DrivenActor != FixedParent)
		? DrivenActor->GetRootComponent() : nullptr;
	if (BoundFixedComp.Get() == NewFixed && BoundDrivenComp.Get() == NewDriven)
	{
		return;   // 引用未变：无需重复绑定
	}
	if (BoundFixedComp.IsValid())
	{
		BoundFixedComp->TransformUpdated.RemoveAll(this);
	}
	if (BoundDrivenComp.IsValid())
	{
		BoundDrivenComp->TransformUpdated.RemoveAll(this);
	}
	BoundFixedComp = NewFixed;
	BoundDrivenComp = NewDriven;
	if (NewFixed)
	{
		NewFixed->TransformUpdated.AddUObject(this, &ADragChainActor::OnDriverTransformUpdated);
	}
	if (NewDriven)
	{
		NewDriven->TransformUpdated.AddUObject(this, &ADragChainActor::OnDriverTransformUpdated);
	}
}

void ADragChainActor::OnDriverTransformUpdated(USceneComponent* /*UpdatedComponent*/, EUpdateTransformFlags /*UpdateFlags*/, ETeleportType /*Teleport*/)
{
	// 驱动源的新变换已生效（广播发生在 UpdateComponentToWorld 之后）：立即按当前世界状态重建几何。
	// 这条路径与 Tick 无关，因此 MRQ（编辑器世界不 Tick）里同样逐帧生效；全程不写任何属性，
	// 不会与 Sequencer 轨道互相覆盖。重入保护：DriveDrivenActor 平移本 Actor 时不再触发重建。
	if (bRebuildingFromDriver || !PathSpline || !HeadLinks || !MidLinks || !TailLinks)
	{
		return;
	}
	TGuardValue<bool> Guard(bRebuildingFromDriver, true);
	ApplyMeshes();
	RefreshChain();
	SyncCachedParams();
}

bool ADragChainActor::ShouldTickIfViewportsOnly() const
{
	// 编辑器视口（Sequencer 预览、无游戏运行）下也持续 Tick，
	// 与 PostEditChangeProperty 双保险：Sequencer 拖动时间轴时几何即时更新
	return true;
}

void ADragChainActor::ApplyMeshes()
{
	// 防御: 存档反序列化早期组件引用可能未就绪（旧版 DLL 存档 + 新版类）
	if (!HeadLinks || !MidLinks || !TailLinks)
	{
		return;
	}
	HeadLinks->SetStaticMesh(bUseHeadTail && HeadMesh ? HeadMesh : MidMesh);
	MidLinks->SetStaticMesh(MidMesh);
	TailLinks->SetStaticMesh(bUseHeadTail && TailMesh ? TailMesh : MidMesh);
}

#if WITH_EDITOR
void ADragChainActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// 组件未就绪（创建 / 重新实例化早期，PathSpline 可能为空）时跳过，
	// 重建由 OnConstruction / Tick 兜底
	if (!PathSpline || !HeadLinks || !MidLinks || !TailLinks)
	{
		return;
	}
	// 行程预设 / 总长度 ↔ 节数联动：改总长度 → 反算节数（取整）；改节数/节距 → 刷新总长度显示
	const FName Prop = PropertyChangedEvent.GetPropertyName();
	if (Prop == GET_MEMBER_NAME_CHECKED(ADragChainActor, TravelPreset))
	{
		if (TravelPreset != EDragChainTravelPreset::Custom)
		{
			const float Target = TravelPresetCm(TravelPreset);  // 预设值 = 拖链总长度（cm）
			const float P = FMath::Max(LinkPitch, 0.1f);
			LinksNum = FMath::Clamp(FMath::RoundToInt(Target / P), 2, 10000);
			TotalLength = (float)LinksNum * P;
			TravelPreset = EDragChainTravelPreset::Custom;  // 应用后回到自定义，半径可继续随意改
		}
	}
	else if (Prop == GET_MEMBER_NAME_CHECKED(ADragChainActor, TotalLength))
	{
		const float P = FMath::Max(LinkPitch, 0.1f);
		LinksNum = FMath::Clamp(FMath::RoundToInt(TotalLength / P), 2, 10000);
		TotalLength = (float)LinksNum * P;              // 吸附到实际节数对应长度
		TravelPreset = EDragChainTravelPreset::Custom;
	}
	else if (Prop == GET_MEMBER_NAME_CHECKED(ADragChainActor, LinksNum)
		|| Prop == GET_MEMBER_NAME_CHECKED(ADragChainActor, LinkPitch)
		|| Prop == GET_MEMBER_NAME_CHECKED(ADragChainActor, BendRadius))
	{
		TotalLength = (float)FMath::Max(LinksNum, 2) * FMath::Max(LinkPitch, 0.1f);
		TravelPreset = EDragChainTravelPreset::Custom;
	}

	// 指定带动对象只建立关联：不改动任何链条参数/形态（参数权威 + 整机 0 点约束）。
	// 刚接模式下链条末端就是对象的安装点，车架位置每帧由对象位置解析求出
	// （见 GetEffectiveCarriage），不需要也不允许任何运行时基准。
	// 注意：「拖链末端对齐偏移」(DrivenOffset) 不走属性写回 —— 它由 GetEffectiveCarriage()
	// 在几何层运行时叠加，因此不会与 Sequencer 的「车架位置」K 帧互相覆盖；改动后由下方统一重建生效。

	// Sequencer 求值 / 详情面板改动都会走这里：立即重建，拖时间轴即见效果
	RefreshDriverBindings();   // 固定父级/带动对象刚被改时，先把变换事件重新挂上
	ApplyMeshes();
	RefreshChain();
	SyncCachedParams();   // 关键：同步缓存，否则 Tick 下一帧检测到"变化"再重建一次（每帧双重建→闪烁）
}
#endif

#if WITH_EDITOR
void ADragChainActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	// 视口 gizmo 拖拽 actor（旋转 / 移动）不触发 PostEditChangeProperty，
	// 且拖拽过程中逐帧调用本函数（bFinished=false），须每帧重建，链条才能实时跟随 gizmo
	if (PathSpline && HeadLinks && MidLinks && TailLinks)
	{
		ApplyMeshes();
		RefreshChain();
		SyncCachedParams();
	}
}
#endif

void ADragChainActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 互斥保护提示：本 Actor 既被原生「附加到」其它 Actor、又设置了插件「固定父级」时，
	// 父级变换已由变换层级提供，插件让位（否则父级旋转会被层级与插件各应用一次 → 转双倍 / 姿态错乱）
	if (!bWarnedAttachConflict && FixedParent && GetAttachParentActor())
	{
		bWarnedAttachConflict = true;
		UE_LOG(LogTemp, Warning, TEXT("DragChain: %s 已「附加到」%s —— 插件「固定父级」自动让位（避免父级变换被应用两次）。要回到插件参数路径请先分离(Detach)再设固定父级。"),
			*GetName(), *GetAttachParentActor()->GetName());
	}

	// 防御: 组件未就绪时不重建（见 ApplyMeshes 注释）
	if (!PathSpline || !HeadLinks || !MidLinks || !TailLinks)
	{
		return;
	}

	// 仅参数变化时重建，避免每帧开销
	if (bDirty
		|| CachedLinks != LinksNum
		|| !FMath::IsNearlyEqual(CachedCarriage, GetEffectiveCarriage())
		|| !CachedFixedEnd.Equals(GetEffectiveFixedEnd())
		|| !CachedActorLoc.Equals(GetActorLocation())
		|| !CachedActorRot.Equals(GetActorRotation())
		|| CachedFixedParent != FixedParent
		|| !CachedFixedParentRot.Equals(FixedParent ? FixedParent->GetActorRotation() : FRotator::ZeroRotator)
		|| !CachedFixedOffset.Equals(FixedOffset)
		|| !CachedFixedRotation.Equals(FixedRotation)
		|| CachedDrivenActor != DrivenActor
		|| CachedFlipHeadTail != bFlipHeadTail
		|| CachedDriveDriven != bDriveDriven
		|| !FMath::IsNearlyEqual(CachedPitch, LinkPitch)
		|| !FMath::IsNearlyEqual(CachedRadius, BendRadius)
		|| CachedHeadMesh != HeadMesh
		|| CachedMidMesh != MidMesh
		|| CachedTailMesh != TailMesh
		|| CachedUseHeadTail != bUseHeadTail
		|| !FMath::IsNearlyEqual(CachedYaw, LinkYaw)
		|| !FMath::IsNearlyEqual(CachedPitchRot, LinkTilt)
		|| !FMath::IsNearlyEqual(CachedRoll, LinkRoll)
		|| !FMath::IsNearlyEqual(CachedScale, LinkScale)
		|| !FMath::IsNearlyEqual(CachedHeadScale, HeadScale)
		|| !FMath::IsNearlyEqual(CachedTailScale, TailScale)
		|| CachedFlipHead != bFlipHead
		|| CachedFlipTail != bFlipTail
		|| CachedAutoPivot != bAutoPivot
		|| CachedFlipWhole != bFlipWholeChain
		|| !FMath::IsNearlyEqual(CachedHeadYaw, HeadYaw)
		|| !FMath::IsNearlyEqual(CachedHeadTilt, HeadTilt)
		|| !FMath::IsNearlyEqual(CachedHeadRoll, HeadRoll)
		|| !CachedHeadOffset.Equals(HeadOffset)
		|| !FMath::IsNearlyEqual(CachedTailYaw, TailYaw)
		|| !FMath::IsNearlyEqual(CachedTailTilt, TailTilt)
		|| !FMath::IsNearlyEqual(CachedTailRoll, TailRoll)
		|| !CachedTailOffset.Equals(TailOffset))
	{
		TotalLength = (float)FMath::Max(LinksNum, 2) * FMath::Max(LinkPitch, 0.1f);  // 旧存档同步
		bDirty = false;
		RefreshDriverBindings();
		SyncCachedParams();

		ApplyMeshes();
		float SplineLength = 0.0f;
		RebuildSpline(PathPointsCache, SplineLength);
		DistributeLinks(SplineLength);
		bHasBuilt = true;
	}

	// 刚接模式（bDrivenDrivesCarriage）：车架位置不再被写回 CarriageLocation ——
	// GetEffectiveCarriage() 每帧从带动对象当前位置解析，dirty 门（上方 CachedCarriage 比对）
	// 自动捕捉对象移动并重建；因此 Sequencer 轨道值、渲染求值顺序都不会影响末端位置。
	// 链条平移贴合（勾选 bDriveDriven 时）：吸收侧向/分层偏差（DistributeLinks 末尾与 Tick 各对齐一次）；
	// 未勾选时本函数直接返回，对象坐标永不被写（见 DriveDrivenActor）
	DriveDrivenActor();
}

void ADragChainActor::SyncCachedParams()
{
	CachedLinks = LinksNum;
	CachedCarriage = GetEffectiveCarriage();
	CachedFixedEnd = GetEffectiveFixedEnd();
	CachedActorLoc = GetActorLocation();
	CachedActorRot = GetActorRotation();
	CachedFixedParent = FixedParent;
	CachedFixedParentRot = FixedParent ? FixedParent->GetActorRotation() : FRotator::ZeroRotator;
	CachedFixedOffset = FixedOffset;
	CachedFixedRotation = FixedRotation;
	CachedDrivenActor = DrivenActor;
	CachedFlipHeadTail = bFlipHeadTail;
	CachedDriveDriven = bDriveDriven;
	CachedPitch = LinkPitch;
	CachedRadius = BendRadius;
	CachedHeadMesh = HeadMesh;
	CachedMidMesh = MidMesh;
	CachedTailMesh = TailMesh;
	CachedUseHeadTail = bUseHeadTail;
	CachedYaw = LinkYaw;
	CachedPitchRot = LinkTilt;
	CachedRoll = LinkRoll;
	CachedScale = LinkScale;
	CachedHeadScale = HeadScale;
	CachedTailScale = TailScale;
	CachedFlipHead = bFlipHead;
	CachedFlipTail = bFlipTail;
	CachedAutoPivot = bAutoPivot;
	CachedFlipWhole = bFlipWholeChain;
	CachedHeadYaw = HeadYaw;
	CachedHeadTilt = HeadTilt;
	CachedHeadRoll = HeadRoll;
	CachedHeadOffset = HeadOffset;
	CachedTailYaw = TailYaw;
	CachedTailTilt = TailTilt;
	CachedTailRoll = TailRoll;
	CachedTailOffset = TailOffset;
}

void ADragChainActor::DriveDrivenActor()
{
	// 硬规则：带动对象是整机装配的零部件，其绝对0点坐标永不被本插件改写（含位置/旋转/移动性）。
	// 默认（bDriveDriven=false）完全不碰带动对象，链条末端靠 GetEffectiveCarriage() 的解析解贴合。
	// 勾选后启用"平移拖链本体补偿"：链条 Actor 整体平移使尾挂点对齐带动对象当前位置——对象不动、链条动。
	if (!bDriveDriven)
	{
		return;
	}
	if (!bTailSlotValid || !DrivenActor || DrivenActor == this || DrivenActor == FixedParent)
	{
		return;
	}
	// 挂点偏移（沿链条本地坐标：X=链向 / Y=侧向 / Z=分层方向）→ 期望尾挂点 = 对象绝对位置 - 偏移
	// 符号与手动模式一致（GetEffectiveCarriage 里 Xc -= Offset.X）：正偏移把链端往链向负方向拉
	const FVector OffsetW = GetDrivenMountOffsetWorld();
	const FVector Delta = (DrivenActor->GetActorLocation() - OffsetW) - CachedTailWorld.GetLocation();
	if (Delta.SizeSquared() < 0.0001f)
	{
		return; // 已贴合/静止（<0.01cm）：零平移，防抖；1.0f 会造成每 1cm 跳一次的追赶式卡顿
	}
	// 平移拖链本体 + 同步尾挂点缓存（链节 ISMC 实例相对 Actor，平移后视觉整体跟随，无需重建）
	AddActorWorldOffset(Delta);
	CachedTailWorld.AddToTranslation(Delta);
}

void ADragChainActor::RefreshChain()
{
	float SplineLength = 0.0f;
	RebuildSpline(PathPointsCache, SplineLength);
	DistributeLinks(SplineLength);
	bDirty = false;
}

float ADragChainActor::GetBendCenterLocation() const
{
	const float L = (float)FMath::Max(LinksNum, 2) * FMath::Max(LinkPitch, 0.1f);
	const float R = FMath::Max(BendRadius, 0.1f);
	// 链长守恒: 下排 Xb + 弧 πR + 上排 (Xb-Xc) = L ⇒ Xb = (L+Xc-πR)/2
	// 对称行程（固定端在行程中间）：下排 ≥ 0 ⇒ Xc ≥ -(L-πR)，上排 ≥ 0 ⇒ Xc ≤ L-πR，
	// 车架可向前走过固定端（Xc<0）继续折叠，下排链节随之被吸收进弧区/上排
	const float MaxX = FMath::Max(0.0f, L - R * PI);
	const float Xc = FMath::Clamp(GetEffectiveCarriage(), -MaxX, MaxX);
	// 下排长度钳制 ≥ 0（Xc 已被 [-MaxX, MaxX] 钳制，此处为防御）
	return FMath::Max(0.0f, (L + Xc - PI * R) * 0.5f);
}

FRotator ADragChainActor::GetDesignFrameWorld() const
{
	// 设定了固定父级并开启"跟随固定父级旋转"时，链条等效为固定父级的子物体：
	// 姿态 = 父级姿态 × 自身相对旋转（标准父子合成）——整机/父级旋转、移动时链条整体跟随，
	// 同时 Actor 自身旋转仍作为"相对父级的附加姿态"生效，可在 Sequencer 里 K 旋转帧微调链条
	// （例如把链条转到与横梁平行以避免穿模）。自身旋转为零时等价旧行为（完全由父级决定）。
	// 关闭或未设父级时沿用 actor 自身姿态（旧行为：旋转 actor 即改变链条走向）。
	FRotator Frame;
	if (IsPluginFixedParentActive() && bAlignWithFixedParent)
	{
		const FRotator ParentRot = FixedParent->GetActorRotation();
		const FRotator OwnRot = GetActorRotation();
		// 父级姿态 × 自身相对旋转（自身旋转在父级本地空间内叠加）：
		// 旧版直接丢弃自身旋转，导致"K 拖链自身旋转无任何视觉变化、像被重置"。
		Frame = (ParentRot.Quaternion() * OwnRot.Quaternion()).Rotator();
	}
	else
	{
		Frame = GetActorRotation();
	}
	// 整体翻转：绕世界竖直轴旋转 180°（链条水平镜像、固定端不动）——
	// 用于实际装配中链条无固定头尾、需反向铺装（头尾对调）的场景；跟随固定父级时同样生效。
	if (bFlipWholeChain)
	{
		Frame = (FRotator(0.0f, 180.0f, 0.0f).Quaternion() * Frame.Quaternion()).Rotator();
	}
	return Frame;
}

FVector ADragChainActor::GetAxisWorld() const
{
	const FRotator Frame = GetDesignFrameWorld();
	FVector Axis = Frame.RotateVector(AxisDir.GetSafeNormal());
	Axis = Axis.GetSafeNormal();
	if (Axis.IsNearlyZero())
	{
		Axis = FVector::ForwardVector;                  // AxisDir 退化为零时兜底
	}
	// 固定父级旋转：叠加绕世界竖直轴的偏航（仅 Yaw 生效，零则不干预）
	if (!FixedRotation.IsZero())
	{
		Axis = Axis.RotateAngleAxis(FixedRotation.Yaw, FVector::UpVector);
	}
	return Axis;
}

FVector ADragChainActor::GetLiftWorld() const
{
	// 上下排分层方向 = 设计姿态本地 Up 在垂直于链向平面上的投影：
	// 跟随固定父级时以父级姿态为基准（整机旋转/侧倾时弧区朝向随之），否则沿用 actor 姿态（旧行为）。
	const FRotator Frame = GetDesignFrameWorld();
	const FVector A = GetAxisWorld();
	FVector LocalUp = Frame.RotateVector(FVector::UpVector);
	FVector H = LocalUp - (LocalUp | A) * A;
	if (H.SizeSquared() < 1e-6f)
	{
		// 设计姿态本地 Up 与链向平行（竖直行程，如 AxisDir 沿 Z）：分层方向退化为水平。
		// 用设计姿态本地 X 的水平投影（跟随父级/actor 旋转），本地 X 也竖直时依次回退本地 Y / 世界右向
		H = Frame.RotateVector(FVector::ForwardVector);
		H.Z = 0.0f;
		H = H.GetSafeNormal();
		if (H.IsNearlyZero())
		{
			H = Frame.RotateVector(FVector::RightVector);
			H.Z = 0.0f;
			H = H.GetSafeNormal();
			if (H.IsNearlyZero())
			{
				H = FVector::RightVector;
			}
		}
	}
	// 头尾翻转：上/下排"被车架位置驱动"的角色互换——整条链绕固定端(分层面)竖直镜像，
	// 车架端(车架位置的移动端)从上方改到下方、固定端及头/尾模型位置不变（只翻转上下排的几何运动控制）
	return bFlipHeadTail ? -H.GetSafeNormal() : H.GetSafeNormal();
}

FVector ADragChainActor::GetDrivenMountOffsetWorld() const
{
	// 挂点偏移（沿链条本地坐标：X=链向 / Y=侧向 / Z=分层方向）——带动对象原点到链条末端的安装偏移
	const FVector AxisW = GetAxisWorld();
	const FVector HingeW = (GetLiftWorld() ^ AxisW).GetSafeNormal();
	return AxisW * DrivenOffset.X + HingeW * DrivenOffset.Y + GetLiftWorld() * DrivenOffset.Z;
}

float ADragChainActor::GetEffectiveCarriage() const
{
	// 刚接模式：带动对象就是链条移动端（尾节）的安装点，末端随对象运动 ⇒ 车架位置必须每帧
	// 从对象当前位置解析求出，绝不能写回 CarriageLocation。「车架位置」一旦被 Sequencer 轨道
	// 接管，任何写回都会被逐帧求值覆盖；而增量基准又是运行时字段（不序列化、重开/渲染即丢），
	// 于是出现"编辑器视口对、播放与 MRQ 渲染不对"。此处为纯当前帧解析：无历史基准、无写回，
	// 因此编辑器、播放、MRQ、重开工程四者结果必然一致。
	if (bDrivenDrivesCarriage && DrivenActor && DrivenActor != this && DrivenActor != FixedParent)
	{
		const FVector AxisW = GetAxisWorld();
		const FVector LiftW = GetLiftWorld();
		const float R = FMath::Max(BendRadius, 0.1f);
		const FVector P0W = GetActorTransform().TransformPosition(GetEffectiveFixedEnd());
		// 末端 M = P0 + Axis*Xc + Lift*2R，令 M 落在挂点(对象位置 - 偏移)上 ⇒ 沿链向取投影
		const FVector MountW = DrivenActor->GetActorLocation() - GetDrivenMountOffsetWorld();
		return FVector::DotProduct(MountW - P0W - LiftW * (2.0f * R), AxisW);
	}
	// 手动模式：车架位置 = 手动/K 帧参数。「拖链末端对齐偏移」的 X 在此运行时叠加，同样不写回
	// （写回会被 K 帧覆盖）。侧向(Y)/分层(Z) 分量不参与车架投影，由「链条平移贴合」闭环吸收。
	return CarriageLocation - DrivenOffset.X;
}

bool ADragChainActor::IsPluginFixedParentActive() const
{
	// 已被原生「附加到」时父级变换由变换层级负责，插件的固定父级让位（见 AttachToFixedParentAndClearAnchor）
	return FixedParent != nullptr && GetAttachParentActor() == nullptr;
}

FVector ADragChainActor::GetEffectiveFixedEnd() const
{
	if (IsPluginFixedParentActive())
	{
		// 固定端跟随父级（"固定端位置"变为相对偏移），偏移沿链条本地坐标：X=链向 / Y=侧向 / Z=分层方向
		const FVector AxisW = GetAxisWorld();
		const FVector HingeW = (GetLiftWorld() ^ AxisW).GetSafeNormal();
		const FVector OffsetW = AxisW * FixedOffset.X + HingeW * FixedOffset.Y + GetLiftWorld() * FixedOffset.Z;
		return GetActorTransform().InverseTransformPosition(FixedParent->GetActorLocation() + OffsetW) + FixedEnd;
	}
	return FixedEnd;
}

FChainPreset ADragChainActor::MakeSnapshot() const
{
	FChainPreset P;
	P.Name = Presets.IsValidIndex(PresetIndex) ? Presets[PresetIndex].Name : TEXT("预设");
	P.LinksNum = LinksNum;
	P.LinkPitch = LinkPitch;
	P.BendRadius = BendRadius;
	P.CarriageLocation = CarriageLocation;
	P.FixedEnd = FixedEnd;
	P.AxisDir = AxisDir;
	P.bFlipWholeChain = bFlipWholeChain;
	P.HeadMesh = HeadMesh;
	P.MidMesh = MidMesh;
	P.TailMesh = TailMesh;
	P.bUseHeadTail = bUseHeadTail;
	P.bFlipHeadTail = bFlipHeadTail;
	P.LinkYaw = LinkYaw;
	P.LinkTilt = LinkTilt;
	P.LinkRoll = LinkRoll;
	P.LinkScale = LinkScale;
	P.HeadScale = HeadScale;
	P.TailScale = TailScale;
	P.bFlipHead = bFlipHead;
	P.bFlipTail = bFlipTail;
	P.bAutoPivot = bAutoPivot;
	P.HeadYaw = HeadYaw;
	P.HeadTilt = HeadTilt;
	P.HeadRoll = HeadRoll;
	P.HeadOffset = HeadOffset;
	P.TailYaw = TailYaw;
	P.TailTilt = TailTilt;
	P.TailRoll = TailRoll;
	P.TailOffset = TailOffset;
	return P;
}

void ADragChainActor::ApplySnapshot(const FChainPreset& P)
{
	LinksNum = P.LinksNum;
	LinkPitch = P.LinkPitch;
	BendRadius = P.BendRadius;
	CarriageLocation = P.CarriageLocation;
	FixedEnd = P.FixedEnd;
	AxisDir = P.AxisDir;
	bFlipWholeChain = P.bFlipWholeChain;
	HeadMesh = P.HeadMesh;
	MidMesh = P.MidMesh;
	TailMesh = P.TailMesh;
	bUseHeadTail = P.bUseHeadTail;
	bFlipHeadTail = P.bFlipHeadTail;
	LinkYaw = P.LinkYaw;
	LinkTilt = P.LinkTilt;
	LinkRoll = P.LinkRoll;
	LinkScale = P.LinkScale;
	HeadScale = P.HeadScale;
	TailScale = P.TailScale;
	bFlipHead = P.bFlipHead;
	bFlipTail = P.bFlipTail;
	bAutoPivot = P.bAutoPivot;
	HeadYaw = P.HeadYaw;
	HeadTilt = P.HeadTilt;
	HeadRoll = P.HeadRoll;
	HeadOffset = P.HeadOffset;
	TailYaw = P.TailYaw;
	TailTilt = P.TailTilt;
	TailRoll = P.TailRoll;
	TailOffset = P.TailOffset;
	TotalLength = (float)FMath::Max(LinksNum, 2) * FMath::Max(LinkPitch, 0.1f);
}

void ADragChainActor::AddPreset()
{
	FChainPreset P = MakeSnapshot();
	P.Name = FName(FString::Printf(TEXT("预设%d"), Presets.Num() + 1));
	Presets.Add(P);
	PresetIndex = Presets.Num() - 1;
}

void ADragChainActor::SavePreset()
{
	if (Presets.IsValidIndex(PresetIndex))
	{
		Presets[PresetIndex] = MakeSnapshot();
	}
}

void ADragChainActor::ApplyPreset()
{
	if (Presets.IsValidIndex(PresetIndex))
	{
		ApplySnapshot(Presets[PresetIndex]);
		if (PathSpline && HeadLinks && MidLinks && TailLinks)
		{
			ApplyMeshes();
			RefreshChain();
		}
	}
}

void ADragChainActor::AttachToFixedParentAndClearAnchor()
{
	if (!FixedParent || FixedParent == this)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("未设置「固定父级」，无需附加。")));
		return;
	}
	if (GetAttachParentActor() == FixedParent)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("本 Actor 已附加到该父级，插件「固定父级」已让位。")));
		return;
	}
	if (!FixedParent->GetRootComponent())
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("固定父级没有有效根组件，无法附加。")));
		return;
	}
	// Mobility 逐级校验：沿父级整条组件链往上走 —— 中间任何一级是 Static，"附加后跟随"就在那一级断开
	FString ChainNote = TEXT("父级链 Mobility：全部 Movable");
	int32 StaticCount = 0;
	FString StaticList;
	int32 Guard = 0;
	for (USceneComponent* Comp = FixedParent->GetRootComponent(); Comp && Guard < 64; Comp = Comp->GetAttachParent(), ++Guard)
	{
		if (Comp->Mobility != EComponentMobility::Movable)
		{
			++StaticCount;
			if (StaticCount <= 8)
			{
				StaticList += FString::Printf(TEXT("\n  %d) %s → 组件 %s"),
					StaticCount, *Comp->GetOwner()->GetActorNameOrLabel(), *Comp->GetName());
			}
		}
	}
	if (StaticCount > 0)
	{
		ChainNote = FString::Printf(TEXT("父级链 Mobility：有 %d 级不是 Movable"), StaticCount);
		const FText Msg = FText::FromString(FString::Printf(
			TEXT("固定父级链上有 %d 级不是 Movable —— Static 那一级会断开跟随（运行时 / Sequencer 播放中链条不会跟）：%s%s\n\n仍要附加吗？\n\n要真正跟随：把这条链上的组件批量改成 Movable；不想动整机零件的话，改用一个 Movable 空锚点、把链条附加到它。"),
			StaticCount, *StaticList, StaticCount > 8 ? TEXT("\n  …") : TEXT("")));
		if (FMessageDialog::Open(EAppMsgType::YesNo, Msg) != EAppReturnType::Yes)
		{
			return;
		}
	}

	// 保持世界变换附加：链条不会跳位，之后由变换层级跟随父级
	AActor* NewParent = FixedParent;
	Modify();
	if (Root)
	{
		Root->Modify();
		if (Root->Mobility != EComponentMobility::Movable)
		{
			Root->SetMobility(EComponentMobility::Movable);   // 只改本 Actor 的根组件，不碰任何零部件
		}
	}
	AttachToActor(NewParent, FAttachmentTransformRules::KeepWorldTransform);

	// 让位：清空插件固定父级 + 取消跟随旋转，父级变换只由变换层级应用一次
	FixedParent = nullptr;
	bAlignWithFixedParent = false;
	bWarnedAttachConflict = false;
	SyncCachedParams();
	RefreshChain();
	MarkPackageDirty();

	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(
		TEXT("已附加到「%s」：插件固定父级已清空、跟随旋转已取消，跟随改由引擎变换层级驱动（无帧滞后）。\n%s"),
		*NewParent->GetActorNameOrLabel(), *ChainNote)));
}

bool ADragChainActor::ResetMeshPivotAsset(UStaticMesh* Mesh)
{
	if (!Mesh) { return false; }
	const FVector Center = Mesh->GetBounds().Origin;
	if (Center.IsNearlyZero(0.01f))
	{
		UE_LOG(LogTemp, Warning, TEXT("DragChain: %s 几何中心已在原点，无需重设"), *Mesh->GetName());
		return false;
	}

	Mesh->Modify();
	FMeshDescription* MD = Mesh->GetMeshDescription(0);
	if (!MD) { return false; }
	// 顶点整体平移，使几何中心落到原点 (0,0,0)；法线/UV/包围盒随 Build 自动重建
	FStaticMeshOperations::ApplyTransform(*MD, FTransform(-Center));
	Mesh->CommitMeshDescription(0);
	Mesh->Build();

	UPackage* Pkg = Mesh->GetOutermost();
	if (Pkg)
	{
		Pkg->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Pkg->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		UPackage::SavePackage(Pkg, Mesh, *Filename, Args);
	}
	UE_LOG(LogTemp, Warning, TEXT("DragChain: 枢轴已重设 %s（偏移 %.1f, %.1f, %.1f）"),
		*Mesh->GetName(), Center.X, Center.Y, Center.Z);
	return true;
}

void ADragChainActor::ResetMeshPivots()
{
	TArray<UStaticMesh*> Meshes;
	if (HeadMesh) { Meshes.AddUnique(HeadMesh); }
	if (MidMesh) { Meshes.AddUnique(MidMesh); }
	if (TailMesh) { Meshes.AddUnique(TailMesh); }

	int32 Fixed = 0;
	for (UStaticMesh* Mesh : Meshes)
	{
		if (Mesh && ResetMeshPivotAsset(Mesh))
		{
			++Fixed;
		}
	}

	// 枢轴修正后 bAutoPivot 的包围盒补偿自动失效（Origin≈0），直接刷新即可
	if (PathSpline && HeadLinks && MidLinks && TailLinks)
	{
		ApplyMeshes();
		RefreshChain();
	}
	if (Fixed == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("DragChain: 无需要重设枢轴的模型（几何中心均已在原点）"));
	}
}

void ADragChainActor::RebuildSpline(TArray<FVector>& OutPathPoints, float& OutSplineLength)
{
	const int32 N = FMath::Max(LinksNum, 2);
	const float Pitch = FMath::Max(LinkPitch, 0.1f);
	const float L = (float)N * Pitch;
	const float R = FMath::Max(BendRadius, 0.1f);
	// 链长守恒: 下排 Xb + 弧 πR + 上排 (Xb-Xc) = L ⇒ Xb = (L+Xc-πR)/2
	// 对称行程（固定端在行程中间）：下排 ≥ 0 ⇒ Xc ≥ -(L-πR)，上排 ≥ 0 ⇒ Xc ≤ L-πR，
	// 车架可向前走过固定端（Xc<0）继续折叠，下排链节随之被吸收进弧区/上排
	const float MaxX = FMath::Max(0.0f, L - R * PI);
	const float Xc = FMath::Clamp(GetEffectiveCarriage(), -MaxX, MaxX);
	// 下排长度钳制 ≥ 0（Xc 已被 [-MaxX, MaxX] 钳制，此处为防御）
	const float Xb = FMath::Max(0.0f, (L + Xc - PI * R) * 0.5f);

	// 世界空间计算：走向 = 本地 AxisDir 转到世界（任意方向，可水平/竖直/倾斜）；
	// 分层方向 = 世界 Up 在垂直于链向平面上的投影（重力分层，水平行程保持 Z 朝上）。
	// 固定端 = 固定父级（如有）或手动值
	const FTransform& AW = GetActorTransform();
	const FTransform InvAW = AW.Inverse();
	const FVector Axis = GetAxisWorld();
	const FVector Lift = GetLiftWorld();               // 上下排分层方向（弯曲平面法向）
	const FVector P0 = AW.TransformPosition(GetEffectiveFixedEnd());  // 固定端（世界）
	const FVector P1 = P0 + Axis * Xb;                  // 下排终点 = 圆弧起点
	const FVector C = P1 + Lift * R;                    // 弯心
	const FVector T = C + Lift * R;                     // 圆弧顶点（上排起点）
	const FVector M = P0 + Axis * Xc + Lift * (2.0f * R); // 移动端（上排，车架附着点）

	// 世界路径点：P0 → 下排各铰链点 → P1 → 圆弧采样 → T → 上排各铰链点 → M
	// 每节链节在直段上对应一个样条点（铰链），头/尾两点决定链长，中间节可直观核对
	TArray<FVector> WorldPts;
	TArray<bool> bArcPt;
	WorldPts.Reset();
	WorldPts.Reserve(N + ArcSamples + 8);
	bArcPt.Reserve(N + ArcSamples + 8);
	const auto AddPt = [&WorldPts, &bArcPt](const FVector& Pt, bool bArc)
	{
		WorldPts.Add(Pt);
		bArcPt.Add(bArc);
	};

	const float ArcLen = PI * R;
	AddPt(P0, false);
	for (int32 k = 1; k < N && (float)k * Pitch < Xb - 0.01f; ++k)
	{
		AddPt(P0 + Axis * ((float)k * Pitch), false);   // 下排铰链（每节一个样条点）
	}
	if (Xb > 0.01f)
	{
		AddPt(P1, true);                                // 圆弧起点（完全折叠 Xb≈0 时 P1=P0，跳过重复点）
	}
	for (int32 i = 1; i < ArcSamples; ++i)              // 半圆弧采样：-90° -> +90°
	{
		const float A = -PI * 0.5f + PI * (float)i / (float)ArcSamples;
		AddPt(C + Lift * (R * FMath::Sin(A)) + Axis * (R * FMath::Cos(A)), true);
	}
	AddPt(T, true);                                     // 圆弧终点（上排起点）
	for (int32 k = 1; k < N; ++k)
	{
		const float dk = (float)k * Pitch;
		if (dk > Xb + ArcLen + 0.01f)
		{
			AddPt(T - Axis * (dk - Xb - ArcLen), false);  // 上排铰链（从 T 往 M 走）
		}
	}
	AddPt(M, false);

	OutPathPoints.Reset();
	OutPathPoints.Reserve(WorldPts.Num());
	for (const FVector& W : WorldPts)
	{
		OutPathPoints.Add(InvAW.TransformPosition(W));
	}

	// 精确切线（CurveCustom）：直线段切线=段向量，圆弧点切线=圆周切向 * R*dθ，
	// 避免自动切线在直-弧交接处因长直线段产生巨大幅度导致 Hermite 过冲/回环
	PathSpline->ClearSplinePoints(false);
	const int32 Samples = FMath::Max(ArcSamples, 8);
	const float dTheta = PI / (float)Samples;           // 相邻弧点角度步长
	const float ArcTan = R * dTheta;                    // 圆弧段 Hermite 切线长度
	for (int32 i = 0; i < OutPathPoints.Num(); ++i)
	{
		PathSpline->AddSplinePoint(OutPathPoints[i], ESplineCoordinateSpace::Local, false);
		PathSpline->SetSplinePointType(PathSpline->GetNumberOfSplinePoints() - 1, ESplinePointType::CurveCustomTangent, false);

		FVector Arrive = FVector::ZeroVector;
		FVector Leave = FVector::ZeroVector;
		if (i > 0)
		{
			Arrive = InvAW.TransformVectorNoScale(WorldPts[i] - WorldPts[i - 1]);  // 段向量（直线精确）
		}
		if (i < WorldPts.Num() - 1)
		{
			Leave = InvAW.TransformVectorNoScale(WorldPts[i + 1] - WorldPts[i]);
		}
		if (bArcPt[i])                                  // 圆弧点：圆周切向覆盖（幅度 R*dθ）
		{
			const FVector D = WorldPts[i] - C;
			const float A = FMath::Atan2(FVector::DotProduct(D, Lift), FVector::DotProduct(D, Axis));
			const FVector Dir = -FMath::Sin(A) * Axis + FMath::Cos(A) * Lift;
			Arrive = InvAW.TransformVectorNoScale(Dir * ArcTan);
			Leave = InvAW.TransformVectorNoScale(Dir * ArcTan);
		}
		PathSpline->SetTangentsAtSplinePoint(PathSpline->GetNumberOfSplinePoints() - 1, Arrive, Leave, ESplineCoordinateSpace::Local, false);
	}
	PathSpline->UpdateSpline();                         // 批量改点后重建一次
	OutSplineLength = PathSpline->GetSplineLength();
}

void ADragChainActor::DistributeLinks(float SplineLength)
{
	const int32 N = FMath::Max(LinksNum, 2);

	// 理论路径参数化（世界）：链节位置 = 固定端起 k×理论节距，与样条弧长无关。
	// 关键：下排链节位置 = P0 + Axis×(k·Pitch) 仅取决于 k，不随车架位置变化——
	// 车架运动时只有弧区/上排链节移动，下排（头到弯曲起点）链节完全固定，
	// 符合真实拖链"下排铺设在固定轨道上"的行为。
	const float L = (float)N * FMath::Max(LinkPitch, 0.1f);
	const float R = FMath::Max(BendRadius, 0.1f);
	const float ChainPitch = L / (float)N;
	const float MaxX = FMath::Max(0.0f, L - R * PI);
	const float Xc = FMath::Clamp(GetEffectiveCarriage(), -MaxX, MaxX);
	const float Xb = FMath::Max(0.0f, (L + Xc - PI * R) * 0.5f);
	const float ArcLen = PI * R;
	const FTransform& AW = GetActorTransform();
	const FTransform InvAW = AW.Inverse();
	const FVector AxisW = GetAxisWorld();
	const FVector LiftW = GetLiftWorld();                 // 分层方向（世界，重力投影）
	const FVector P0W = AW.TransformPosition(GetEffectiveFixedEnd());
	const FVector P1W = P0W + AxisW * Xb;                 // 弧起点（世界）
	const FVector CW = P1W + LiftW * R;                   // 弯心（世界）
	const FVector TW = CW + LiftW * R;                    // 弧终点（世界）
	// 本地理论节距（FitScale 用）：以样条弧长估计本地→世界缩放
	const float EstScale = L / FMath::Max(SplineLength, 1e-3f);
	const float ChainPitchLocal = ChainPitch / EstScale;

	TArray<FTransform> HeadTransforms;
	TArray<FTransform> MidTransforms;
	TArray<FTransform> TailTransforms;
	MidTransforms.Reserve(N);

	// 铰链轴 = 弯曲平面法线（世界分层方向 × 世界走向），全程恒定。
	// 样条坐标系在圆弧顶点（切线∥分层方向）处退化，Y 轴会在弧顶两侧翻转 180°，
	// 导致弧区链节扭转错位；这里用恒定铰链轴重建每节坐标系（直段与当前外观一致）。
	const FVector Hinge = GetActorTransform().InverseTransformVectorNoScale(GetLiftWorld() ^ GetAxisWorld()).GetSafeNormal();

	// 铰链点沿理论路径按节距分布；每节链节 = 相邻两铰链之间的"弦"（直段弦=切线，弧段弦≠切线）。
	// 相邻链节共享铰链点 → 接头无错位（按切线定向时弧区接头错位约 p^2/2R）。
	// 弧区用解析圆弧定位（不依赖 Hermite 样条弧长——弧段插值偏离圆弧，链节会跳出路径）。
	TArray<FVector> Hinges;
	Hinges.Reserve(N + 1);
	for (int32 k = 0; k <= N; ++k)
	{
		const float s = (float)k * ChainPitch;            // 理论弧长（世界，从固定端起）
		FVector HW;
		if (s <= Xb)
		{
			HW = P0W + AxisW * s;                         // 下排直段（固定，不随车架运动）
		}
		else if (s <= Xb + ArcLen)
		{
			const float a = -PI * 0.5f + (s - Xb) / R;    // 弧区：半圆解析（弧长/半径 = 角度）
			HW = CW + LiftW * (R * FMath::Sin(a)) + AxisW * (R * FMath::Cos(a));
		}
		else
		{
			HW = TW - AxisW * (s - Xb - ArcLen);          // 上排直段（随车架运动）
		}
		Hinges.Add(InvAW.TransformPosition(HW));
	}

	// 头/中/尾网格包围盒中心（网格本地空间，用于自动枢轴匹配）。
	// 注意：Datasmith 导入模型的几何中心通常远离原点（C4D 原点不在模型中心），
	// 补偿后实例原点会大幅偏离弦中点，但几何中心精确落在样条上——这是正确行为
	const FVector HeadOrigin = bUseHeadTail && HeadMesh ? HeadMesh->GetBounds().Origin : FVector::ZeroVector;
	const FVector MidOrigin = MidMesh ? MidMesh->GetBounds().Origin : FVector::ZeroVector;
	const FVector TailOrigin = bUseHeadTail && TailMesh ? TailMesh->GetBounds().Origin : FVector::ZeroVector;

	for (int32 i = 0; i < N; ++i)
	{
		// 端盖角色固定：固定端(P0/i=0)=头节、车架端(M/i=N-1)=尾节，不受"头尾翻转"影响——
		// 翻转只改变上/下排哪一侧被"车架位置"驱动（见 GetLiftWorld 的竖直镜像），端盖模型位置永远不变
		const bool bIsHead = bUseHeadTail && HeadMesh && (i == 0);
		const bool bIsTail = bUseHeadTail && TailMesh && (i == N - 1);

		// 分节缩放：全局链节缩放 × 头/尾独立倍率（模型长度与节距不一致时补正）
		const float SecScale = bIsHead ? FMath::Max(LinkScale, 0.01f) * FMath::Max(HeadScale, 0.01f)
			: (bIsTail ? FMath::Max(LinkScale, 0.01f) * FMath::Max(TailScale, 0.01f) : FMath::Max(LinkScale, 0.01f));
		// 弧区适配：按实际铰链间距（弦长 < 节距）微调缩放，接头精确贴合、无重叠/空隙
		const float HingeDist = (Hinges[i + 1] - Hinges[i]).Size();
		const float FitScale = (HingeDist > 0.01f && ChainPitchLocal > 0.01f)
			? FMath::Clamp(HingeDist / ChainPitchLocal, 0.9f, 1.1f) : 1.0f;
		const float EffScale = SecScale * FitScale;

		FVector ChordDir = (Hinges[i + 1] - Hinges[i]).GetSafeNormal();
		if (ChordDir.IsNearlyZero())
		{
			ChordDir = PathSpline->GetTangentAtDistanceAlongSpline(
				FMath::Clamp((i + 0.5f) * ChainPitchLocal, 0.0f, SplineLength), ESplineCoordinateSpace::Local).GetSafeNormal();
		}
		FTransform T;
		if (!Hinge.IsNearlyZero())
		{
		FVector Z = ChordDir ^ Hinge;
		if (Z.IsNearlyZero())
		{
			Z = GetActorTransform().InverseTransformVectorNoScale(GetLiftWorld()).GetSafeNormal(); // 罕见：弦向∥铰链轴时兜底
		}
			T = FTransform(
				FRotationMatrix::MakeFromXZ(ChordDir, Z.GetSafeNormal()).ToQuat(),
				(Hinges[i] + Hinges[i + 1]) * 0.5f,
				FVector::OneVector);
		}
		else
		{
			T = FTransform(
				FRotationMatrix::MakeFromX(ChordDir).ToQuat(),
				(Hinges[i] + Hinges[i + 1]) * 0.5f,
				FVector::OneVector);
		}

		// 自动枢轴匹配：把网格几何中心对齐到弦中点（补偿资产原点偏离几何中心），
		// 之后翻转/旋转都绕几何中心进行，链节不会漂移，关节端自然落在铰链上。
		// 覆盖全部链节（含中间节）：否则中间节模型渲染时偏离样条线分布
		if (bAutoPivot)
		{
			const FVector Origin = bIsHead ? HeadOrigin : (bIsTail ? TailOrigin : MidOrigin);
			if (!Origin.IsNearlyZero())
			{
				T.AddToTranslation(T.GetRotation().RotateVector(-Origin * EffScale));
			}
		}

		// 头/尾端盖镜像：绕铰链轴（本地Y）翻转 180°，使两端朝向相反
		if ((bIsHead && bFlipHead) || (bIsTail && bFlipTail))
		{
			T.SetRotation(T.GetRotation() * FRotator(180.0f, 0.0f, 0.0f).Quaternion());
		}

		// 头/尾独立三维角度（仅对应段生效）
		if (bIsHead && (!FMath::IsNearlyZero(HeadYaw) || !FMath::IsNearlyZero(HeadTilt) || !FMath::IsNearlyZero(HeadRoll)))
		{
			T.SetRotation(T.GetRotation() * FRotator(HeadTilt, HeadYaw, HeadRoll).Quaternion());
		}
		else if (bIsTail && (!FMath::IsNearlyZero(TailYaw) || !FMath::IsNearlyZero(TailTilt) || !FMath::IsNearlyZero(TailRoll)))
		{
			T.SetRotation(T.GetRotation() * FRotator(TailTilt, TailYaw, TailRoll).Quaternion());
		}

		// 头/尾独立位置偏移（cm，沿本段旋转后的本地坐标）
		const FVector SecOff = bIsHead ? HeadOffset : (bIsTail ? TailOffset : FVector::ZeroVector);
		if (!SecOff.IsNearlyZero())
		{
			T.AddToTranslation(T.GetRotation().RotateVector(SecOff));
		}

		// 全局方向校准：偏航(绕Z)/俯仰(绕Y)/滚转(绕X) 三轴附加旋转
		if (!FMath::IsNearlyZero(LinkYaw) || !FMath::IsNearlyZero(LinkTilt) || !FMath::IsNearlyZero(LinkRoll))
		{
			T.SetRotation(T.GetRotation() * FRotator(LinkTilt, LinkYaw, LinkRoll).Quaternion());
		}
		T.SetScale3D(FVector(EffScale));

		if (bIsHead)
		{
			HeadTransforms.Add(T);
		}
		else if (bIsTail)
		{
			TailTransforms.Add(T);
		}
		else
		{
			MidTransforms.Add(T);
		}
	}

	// ISMC 更新：实例数不变时批量更新变换（避免 Clear+Add 全量重建导致的闪烁），
	// 实例数变化（节数/头尾开关改动）时才清空重建
	auto UpdateInstances = [](UInstancedStaticMeshComponent* Comp, const TArray<FTransform>& Transforms)
	{
		if (!Comp)
		{
			return;
		}
		if (Transforms.Num() == Comp->GetInstanceCount())
		{
			Comp->BatchUpdateInstancesTransforms(0, Transforms, /*bWorldSpace=*/false, /*bMarkRenderStateDirty=*/false, /*bTeleport=*/true);
		}
		else
		{
			Comp->ClearInstances();
			if (Transforms.Num() > 0)
			{
				Comp->AddInstances(Transforms, /*bShouldReturnIndices=*/false);
			}
		}
	};
	UpdateInstances(HeadLinks, HeadTransforms);
	UpdateInstances(MidLinks, MidTransforms);
	UpdateInstances(TailLinks, TailTransforms);

	// 尾节锚点 = 链条末端 M 点（上排终点 = 车架附着点），世界空间：
	// M = P0 + Axis*Xc + Lift*2R。带动对象精确挂在此点，双向驱动自洽
	// （链条→车架 与 车架→链条投影 都收敛到同一位置，无半节距漂移）。
	CachedTailWorld = FTransform(AW.GetRotation(), P0W + AxisW * Xc + LiftW * (2.0f * R));
	bTailSlotValid = true;

	// 尾节位置已更新，立即执行链条平移贴合（勾选 bDriveDriven 时；Sequencer 每帧求值→刷新→此处对齐）
	DriveDrivenActor();

	HeadLinks->MarkRenderStateDirty();
	MidLinks->MarkRenderStateDirty();
	TailLinks->MarkRenderStateDirty();
}
