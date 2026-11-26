// DragChainActor.h — 工业拖链（电缆链）动画 Actor
// 样条路径 + 链长守恒运动学：弯心 x_b = (L + X - PI*R) / 2
// 支持 头 / 中间节 / 尾 三段独立模型；参数均可由 Sequencer 属性轨道驱动。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DragChainActor.generated.h"

class USplineComponent;
class UInstancedStaticMeshComponent;
class UStaticMesh;

/** 常用拖链总长度预设（物理世界：1米 = 100 UE 单位 cm；数值即拖链总长度） */
UENUM(BlueprintType)
enum class EDragChainTravelPreset : uint8
{
	Custom  UMETA(DisplayName = "自定义"),
	Len060  UMETA(DisplayName = "0.6 米（600mm）"),
	Len080  UMETA(DisplayName = "0.8 米（800mm）"),
	Len130  UMETA(DisplayName = "1.3 米（1300mm）"),
	Len250  UMETA(DisplayName = "2.5 米（2500mm）"),
	Len300  UMETA(DisplayName = "3 米（3000mm）"),
	Len320  UMETA(DisplayName = "3.2 米（3200mm）"),
	Len400  UMETA(DisplayName = "4 米（4000mm）"),
	Len600  UMETA(DisplayName = "6 米（6000mm）"),
	Len800  UMETA(DisplayName = "8 米（8000mm）"),
	Len1000 UMETA(DisplayName = "10 米（10000mm）"),
	Len1200 UMETA(DisplayName = "12 米（12000mm）"),
	Len1300 UMETA(DisplayName = "13 米（13000mm）"),
	Len1400 UMETA(DisplayName = "14 米（14000mm）"),
	Len2600 UMETA(DisplayName = "26 米（26000mm）")
};

/** 拖链参数预设（整链配置快照，用于跨场景复用） */
USTRUCT(BlueprintType)
struct FChainPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "预设名称"))
	FName Name = TEXT("预设");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "节数"))
	int32 LinksNum = 44;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "节距"))
	float LinkPitch = 5.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "弯曲半径"))
	float BendRadius = 22.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "车架位置"))
	float CarriageLocation = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "固定端位置"))
	FVector FixedEnd = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "走向方向"))
	FVector AxisDir = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "整体翻转"))
	bool bFlipWholeChain = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "头节模型"))
	TObjectPtr<UStaticMesh> HeadMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "中间节模型"))
	TObjectPtr<UStaticMesh> MidMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "尾节模型"))
	TObjectPtr<UStaticMesh> TailMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "启用头尾模型"))
	bool bUseHeadTail = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "头尾翻转"))
	bool bFlipHeadTail = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "链节偏航角"))
	float LinkYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "链节俯仰角"))
	float LinkTilt = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "链节滚转角"))
	float LinkRoll = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "链节缩放"))
	float LinkScale = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "头节缩放"))
	float HeadScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "尾节缩放"))
	float TailScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "头节翻转"))
	bool bFlipHead = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "尾节翻转"))
	bool bFlipTail = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "自动枢轴匹配"))
	bool bAutoPivot = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "头节偏航角"))
	float HeadYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "头节俯仰角"))
	float HeadTilt = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "头节滚转角"))
	float HeadRoll = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "头节位置偏移"))
	FVector HeadOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "尾节偏航角"))
	float TailYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "尾节俯仰角"))
	float TailTilt = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "尾节滚转角"))
	float TailRoll = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "尾节位置偏移"))
	FVector TailOffset = FVector::ZeroVector;
};

UCLASS(meta = (DisplayName = "拖链Actor", ToolTip = "工业拖链（电缆链）动画 Actor：样条路径 + 链长守恒运动学，头/中/尾三段模型，Sequencer 参数驱动，双向挂接（固定父级 / 带动对象 + 偏移）"))
class DRAGCHAIN_API ADragChainActor : public AActor
{
	GENERATED_BODY()

public:
	ADragChainActor();

	/* ===== 参数（Sequencer 可自定义） ===== */

	/** 节数（总链节数，含头尾） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|参数", Interp, meta = (ClampMin = "2", UIMin = "2", DisplayName = "节数"))
	int32 LinksNum = 44;

	/** 总长度 cm = 节数 × 节距；修改时自动反算节数（取整） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|参数", meta = (ClampMin = "2.0", DisplayName = "总长度"))
	float TotalLength = 257.4f;

	/** 节距 cm：与链节模型长度一致 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|参数", Interp, meta = (ClampMin = "0.1", DisplayName = "节距"))
	float LinkPitch = 5.85f;

	/** 弯曲半径 cm（须 >= 拖链最小弯曲半径，见厂商手册） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|参数", Interp, meta = (ClampMin = "0.1", DisplayName = "弯曲半径"))
	float BendRadius = 22.85f;

	/** 车架位置（沿 AxisDir），Sequencer 打关键帧 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|参数", Interp, meta = (DisplayName = "车架位置"))
	float CarriageLocation = 0.0f;

	/** 固定端局部坐标（下排起点） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|参数", meta = (DisplayName = "固定端位置"))
	FVector FixedEnd = FVector::ZeroVector;

	/** 拖链走向（单位向量；设了固定父级并开启"跟随固定父级旋转"时按固定父级本地坐标解释，否则按拖链自身本地坐标） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|参数", meta = (DisplayName = "走向方向"))
	FVector AxisDir = FVector::ForwardVector;

	/** 整体翻转：整条链绕固定端水平翻转 180°（走向镜像、驱动/固定端互换侧），等效旧版把 Actor 旋转 180°；设了固定父级并开启"跟随固定父级旋转"时同样生效（实际装配中链条无固定头尾、需反向铺装时使用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|参数", meta = (DisplayName = "整体翻转"))
	bool bFlipWholeChain = false;

	/* ===== 三段模型 ===== */

	/** 头节模型（固定端第一片），留空则回退到中间节模型 */
	UPROPERTY(EditAnywhere, Category = "拖链|模型", meta = (DisplayName = "头节模型"))
	TObjectPtr<UStaticMesh> HeadMesh = nullptr;

	/** 中间节模型（必填才有显示） */
	UPROPERTY(EditAnywhere, Category = "拖链|模型", meta = (DisplayName = "中间节模型"))
	TObjectPtr<UStaticMesh> MidMesh = nullptr;

	/** 尾节模型（车架端最后一片），留空则回退到中间节模型 */
	UPROPERTY(EditAnywhere, Category = "拖链|模型", meta = (DisplayName = "尾节模型"))
	TObjectPtr<UStaticMesh> TailMesh = nullptr;

	/** 是否启用头/尾节（关闭则整条链统一用中间节模型） */
	UPROPERTY(EditAnywhere, Category = "拖链|模型", meta = (DisplayName = "启用头尾模型"))
	bool bUseHeadTail = true;

	/** 半圆弧采样点数（越大越圆滑） */
	UPROPERTY(EditAnywhere, Category = "拖链|模型", meta = (ClampMin = "8", ClampMax = "64", DisplayName = "圆弧采样数"))
	int32 ArcSamples = 32;

	/** 链节偏航角：绕样条上向（本地 Z）旋转（度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|模型", Interp, meta = (DisplayName = "链节偏航角"))
	float LinkYaw = 0.0f;

	/** 链节俯仰角：绕样条右向（本地 Y）旋转（度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|模型", Interp, meta = (DisplayName = "链节俯仰角"))
	float LinkTilt = 0.0f;

	/** 链节滚转角：绕链向（本地 X）旋转（度），旧"链节朝向角" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|模型", Interp, meta = (DisplayName = "链节滚转角"))
	float LinkRoll = 0.0f;

	/** 链节统一缩放（模型尺寸与节距不匹配时调整）；默认 0.1 = 补偿 C4D(mm)→Datasmith→UE(cm) 的 10 倍尺寸放大 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|模型", meta = (ClampMin = "0.01", DisplayName = "链节缩放"))
	float LinkScale = 0.1f;

	/** 头节缩放：额外倍率（叠加于链节缩放），头节模型长度与节距不一致时调整 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|模型", meta = (ClampMin = "0.01", DisplayName = "头节缩放"))
	float HeadScale = 1.0f;

	/** 尾节缩放：额外倍率（叠加于链节缩放），尾节模型长度与节距不一致时调整 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|模型", meta = (ClampMin = "0.01", DisplayName = "尾节缩放"))
	float TailScale = 1.0f;

	/** 头节翻转：绕铰链轴旋转 180°（端盖镜像，与尾节朝向相反） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|模型", meta = (DisplayName = "头节翻转"))
	bool bFlipHead = false;

	/** 尾节翻转：绕铰链轴旋转 180°（端盖镜像，与头节朝向相反） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|模型", meta = (DisplayName = "尾节翻转"))
	bool bFlipTail = false;

	/** 自动枢轴匹配：读取网格包围盒中心，把几何中心对齐到链节衔接点（补偿网格原点偏离几何中心的偏移），翻转时位置不再漂移 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|模型", meta = (DisplayName = "自动枢轴匹配"))
	bool bAutoPivot = true;

	/** 头节偏航角：仅头节，绕样条上向（本地 Z）旋转（度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|模型", meta = (DisplayName = "头节偏航角"))
	float HeadYaw = 0.0f;

	/** 头节俯仰角：仅头节，绕样条右向（本地 Y）旋转（度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|模型", meta = (DisplayName = "头节俯仰角"))
	float HeadTilt = 0.0f;

	/** 头节滚转角：仅头节，绕链向（本地 X）旋转（度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|模型", meta = (DisplayName = "头节滚转角"))
	float HeadRoll = 0.0f;

	/** 头节位置偏移（cm，沿头节本地坐标，用于微调关节与铰链对齐） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|模型", meta = (DisplayName = "头节位置偏移"))
	FVector HeadOffset = FVector::ZeroVector;

	/** 尾节偏航角：仅尾节，绕样条上向（本地 Z）旋转（度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|模型", meta = (DisplayName = "尾节偏航角"))
	float TailYaw = 0.0f;

	/** 尾节俯仰角：仅尾节，绕样条右向（本地 Y）旋转（度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|模型", meta = (DisplayName = "尾节俯仰角"))
	float TailTilt = 0.0f;

	/** 尾节滚转角：仅尾节，绕链向（本地 X）旋转（度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|模型", meta = (DisplayName = "尾节滚转角"))
	float TailRoll = 0.0f;

	/** 尾节位置偏移（cm，沿尾节本地坐标，用于微调关节与铰链对齐） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|模型", meta = (DisplayName = "尾节位置偏移"))
	FVector TailOffset = FVector::ZeroVector;

	/* ===== 挂接（插槽） ===== */

	/** 头节插槽：固定父级 actor/模型，固定端自动跟随其位置（此时"固定端位置"变为相对偏移） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|挂接", meta = (DisplayName = "固定父级"))
	TObjectPtr<AActor> FixedParent = nullptr;

	/** 固定父级位置偏移（cm）：相对固定父级位置，沿拖链本地坐标 X=链向 / Y=侧向 / Z=竖直 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|挂接", meta = (DisplayName = "固定父级偏移"))
	FVector FixedOffset = FVector::ZeroVector;

	/** 跟随固定父级旋转：设定了固定父级后（默认开启），拖链等效为固定父级的子物体——链条姿态 = 父级姿态 × 自身相对旋转，整机/父级旋转移动时链条整体跟随，同时 Actor 自身旋转作为相对父级的附加姿态生效（可在 Sequencer K 旋转帧微调链条朝向）；关闭后仅位置跟随父级，方向完全由自身旋转决定 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|挂接", meta = (DisplayName = "跟随固定父级旋转"))
	bool bAlignWithFixedParent = true;

	/** 固定父级旋转（度）：叠加旋转链条水平朝向（绕竖直轴，仅 Yaw 生效），零则不干预 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|挂接", meta = (DisplayName = "固定父级旋转"))
	FRotator FixedRotation = FRotator::ZeroRotator;

	/** 尾节插槽：拖链的子级——链条的尾节带着该 actor/网格体运动（K 帧"车架位置"驱动链条，尾节带动此对象跟随） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|挂接", meta = (DisplayName = "带动对象"))
	TObjectPtr<AActor> DrivenActor = nullptr;

	/** 拖链末端对齐偏移（cm）：整机 0 点约束下的链条侧对齐工具，带动对象绝对坐标零写入。沿拖链本地坐标 X=链向 / Y=侧向 / Z=竖直。
	 *  **运行时叠加**：X 在 GetEffectiveCarriage() 里并入车架投影（不写回 CarriageLocation，故不会被 Sequencer 的「车架位置」K 帧覆盖），Y/Z 由「链条平移贴合」闭环吸收。用于把链条末端对准横梁挂接点且不碰装配件 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|挂接", meta = (DisplayName = "拖链末端对齐偏移"))
	FVector DrivenOffset = FVector::ZeroVector;

	/** 末端刚接带动对象：勾选后链条移动端（尾节）视为固定在带动对象的安装点上，随对象运动——
	 *  车架位置每帧由对象当前位置解析求出（纯当前帧、无历史基准、不写回任何 Sequencer 接管的属性），
	 *  编辑器视口 / 播放 / Movie Render Queue / 重开工程结果一致。此时「车架位置」不再作为驱动源；
	 *  安装点相对对象原点的位置用「拖链末端对齐偏移」表达。默认关闭 = 车架位置完全手动权威。
	 *  两种模式下\"指定带动对象\"都只建立关联、不改动任何链条参数；带动对象本身坐标零写入 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|挂接", meta = (DisplayName = "带动对象驱动车架"))
	bool bDrivenDrivesCarriage = false;

	/** 链条平移贴合带动对象：带动对象绝对0点坐标永不被本插件改写（装配基准只读）。勾选后，当链条尾挂点与带动对象当前位置(扣除偏移)不一致时，平移拖链 Actor 本体使尾端贴合对象（对象不动、链条动）；默认关闭——只做参数反投影，链条够不到时停在极限、保持固定端稳定 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|挂接", meta = (DisplayName = "链条平移贴合带动对象"))
	bool bDriveDriven = false;

	/** 头尾翻转：上/下排"被车架位置驱动"的角色互换——整条链绕固定端在竖直面内镜像（圆弧保持在固定端同一侧）。默认：固定端在下排起点、车架端在上排末端（车架位置驱动上方）；勾选后：固定端原位不动、车架端改到下排末端（车架位置驱动下方）。只翻转上/下排的几何运动控制逻辑：头/尾模型、端盖位置、走向方向、圆弧朝向均不变。可与"整体翻转"(水平镜像)组合 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|挂接", meta = (DisplayName = "头尾翻转"))
	bool bFlipHeadTail = false;

	/* ===== 预设 ===== */

	/** 总长度预设：预设值直接作为拖链总长度（cm），节数自动反算；弯曲半径始终可自定义 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|预设", meta = (DisplayName = "总长度预设"))
	EDragChainTravelPreset TravelPreset = EDragChainTravelPreset::Custom;

	/** 预设列表（整链配置快照，含模型与校准参数） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|预设", meta = (DisplayName = "预设列表"))
	TArray<FChainPreset> Presets;

	/** 当前操作的预设序号（保存/应用针对该条目） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拖链|预设", meta = (DisplayName = "预设序号", ClampMin = "0"))
	int32 PresetIndex = 0;

	/** 把当前参数保存为新预设并追加到列表 */
	UFUNCTION(CallInEditor, Category = "拖链|预设")
	void AddPreset();

	/** 把当前参数覆盖写入选中的预设条目 */
	UFUNCTION(CallInEditor, Category = "拖链|预设")
	void SavePreset();

	/** 把选中的预设条目应用到当前参数 */
	UFUNCTION(CallInEditor, Category = "拖链|预设")
	void ApplyPreset();

	/** 手动刷新（编辑器面板按钮 / 蓝图调用） */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "DragChain")
	void RefreshChain();

	/** 把头/中/尾模型资产的枢轴强制重设为几何中心（顶点整体平移，几何中心落到原点 0,0,0），并保存资产。
	 *  用于修复 Datasmith 等导入模型枢轴偏离（几何中心不在原点）导致的链节不贴样条问题。 */
	UFUNCTION(CallInEditor, Category = "拖链|工具", meta = (DisplayName = "重设模型枢轴到几何中心"))
	void ResetMeshPivots();

	/** 把单个 StaticMesh 资产的枢轴重设为几何中心（顶点整体平移，几何中心落到原点 0,0,0），并保存资产。
	 *  static 版本供蓝图 / Python 对任意网格资产直接调用（自动化导入流程使用）。返回是否执行了重设。 */
	UFUNCTION(BlueprintCallable, Category = "拖链|工具", meta = (DisplayName = "重设模型枢轴到几何中心(资产)"))
	static bool ResetMeshPivotAsset(UStaticMesh* Mesh);

	/** 一键把本 Actor 附加（Attach）到"固定父级"，随后清空插件固定父级并取消"跟随固定父级旋转"：
	 *  跟随完全交给引擎变换层级（渲染前必正确、无帧滞后），同时避免父级变换被层级与插件各应用一次。
	 *  会沿父级整条组件链逐级校验 Mobility，列出所有非 Movable 的层级（Static 处会断开跟随）后再询问。 */
	UFUNCTION(CallInEditor, Category = "拖链|工具", meta = (DisplayName = "自动附加到固定父级"))
	void AttachToFixedParentAndClearAnchor();

	/** 当前弯心沿轴位置（调试/蓝图读取） */
	UFUNCTION(BlueprintPure, Category = "DragChain")
	float GetBendCenterLocation() const;

	/** 有效车架位置：手动"车架位置"参数（带动对象反向投影驱动见 Tick） */
	UFUNCTION(BlueprintPure, Category = "DragChain")
	float GetEffectiveCarriage() const;

	/** 有效固定端（本地空间）：固定父级位置（+固定端位置偏移），未设置时取手动"固定端位置" */
	UFUNCTION(BlueprintPure, Category = "DragChain")
	FVector GetEffectiveFixedEnd() const;

	/** 插件级"固定父级"是否真正生效：本 Actor 已被原生「附加到」其它 Actor 时，父级变换由变换层级提供，
	 *  插件必须让位——否则父级旋转会被层级与插件各应用一次（表现为转双倍 / 姿态错乱）。 */
	bool IsPluginFixedParentActive() const;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	// 编辑器视口（Sequencer 预览、无游戏运行）下也持续 Tick，保证 K 帧即时重建
	virtual bool ShouldTickIfViewportsOnly() const override;

	/** 驱动源（固定父级 / 带动对象）变换事件：MRQ 队列渲染用的是当前编辑器世界，
	 *  此时编辑器世界按 LEVELTICK_TimeOnly 推进（EditorEngine.cpp），Actor 的 Tick 完全不执行，
	 *  只靠 Tick 重建会让几何冻结在开渲瞬间 → 渲染里头尾节错位。故重建必须挂在变换事件上。*/
	void OnDriverTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateFlags, ETeleportType Teleport);
	/** 重新绑定驱动源变换事件（固定父级 / 带动对象根组件变化时调用，重复调用无副作用） */
	void RefreshDriverBindings();
	TWeakObjectPtr<USceneComponent> BoundFixedComp;
	TWeakObjectPtr<USceneComponent> BoundDrivenComp;
	bool bRebuildingFromDriver = false;

#if WITH_EDITOR
	// Sequencer K 帧 / 详情面板改属性时立即重建（无需等待 Tick）
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// 视口 gizmo 拖拽（旋转 / 移动）结束时立即重建——该路径不触发 PostEditChangeProperty
	virtual void PostEditMove(bool bFinished) override;
#endif

protected:
	void ApplyMeshes();
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USplineComponent> PathSpline;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> HeadLinks;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> MidLinks;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> TailLinks;

	void RebuildSpline(TArray<FVector>& OutPathPoints, float& OutSplineLength);
	void DistributeLinks(float SplineLength);
	/** 同步参数缓存（PostEditChangeProperty 后调用，避免 Tick 重复重建） */
	void SyncCachedParams();
	/** 链条平移贴合带动对象：勾选 bDriveDriven 时把拖链 Actor 整体平移使尾挂点对齐对象当前位置（对象坐标永不写入；固定父级锚定时自动跳过） */
	void DriveDrivenActor();
	/** 挂点偏移（世界空间）：DrivenOffset 沿链条本地坐标展开（X=链向 / Y=侧向 / Z=分层方向） */
	FVector GetDrivenMountOffsetWorld() const;
	/** 链条设计姿态基准（世界空间旋转）：设了固定父级且开启"跟随固定父级旋转"时 = 固定父级自身姿态（链条等效为父级子物体），否则 = actor 自身姿态 */
	FRotator GetDesignFrameWorld() const;
	/** 走向轴（世界空间，链条行程方向；跟随固定父级时 = 固定父级姿态 × 走向方向，否则完全跟随 actor 旋转：水平/竖直/任意倾斜） */
	FVector GetAxisWorld() const;
	/** 分层方向（世界空间）：上下排分离方向 = 世界 Up 在垂直于链向平面上的投影（重力分层），链向竖直时水平兜底 */
	FVector GetLiftWorld() const;
	FChainPreset MakeSnapshot() const;
	void ApplySnapshot(const FChainPreset& InPreset);

	TArray<FVector> PathPointsCache;

	FVector CachedFixedEnd = FVector(MAX_flt, MAX_flt, MAX_flt);
	FVector CachedActorLoc = FVector(MAX_flt, MAX_flt, MAX_flt);
	FRotator CachedActorRot = FRotator(MAX_flt, MAX_flt, MAX_flt);
	TObjectPtr<AActor> CachedFixedParent = nullptr;
	FRotator CachedFixedParentRot = FRotator(MAX_flt, MAX_flt, MAX_flt);
	FVector CachedFixedOffset = FVector(MAX_flt, MAX_flt, MAX_flt);
	bool bWarnedAttachConflict = false;   // 「附加到」与插件固定父级冲突只提示一次
	FRotator CachedFixedRotation = FRotator(MAX_flt, MAX_flt, MAX_flt);
	TObjectPtr<AActor> CachedDrivenActor = nullptr;
	bool   CachedFlipHeadTail = false;
	bool   CachedDriveDriven = false;
	FTransform CachedTailWorld = FTransform::Identity;  // 链条末端 M 点世界变换（车架附着点，带动对象跟随）
	bool bTailSlotValid = false;                        // 尾节插槽是否有效（链条已构建）
	bool bHasBuilt = false;                             // 首次重建完成标记

	float CachedCarriage = -MAX_flt;
	int32  CachedLinks = -1;
	float  CachedPitch = -1.0f;
	float  CachedRadius = -1.0f;
	TObjectPtr<UStaticMesh> CachedHeadMesh = nullptr;
	TObjectPtr<UStaticMesh> CachedMidMesh = nullptr;
	TObjectPtr<UStaticMesh> CachedTailMesh = nullptr;
	bool   CachedUseHeadTail = true;
	float  CachedYaw = -MAX_flt;
	float  CachedPitchRot = -MAX_flt;
	float  CachedRoll = -MAX_flt;
	float  CachedScale = -1.0f;
	float  CachedHeadScale = -1.0f;
	float  CachedTailScale = -1.0f;
	bool   CachedFlipHead = false;
	bool   CachedFlipTail = false;
	bool   CachedAutoPivot = true;
	bool   CachedFlipWhole = false;
	float  CachedHeadYaw = -MAX_flt;
	float  CachedHeadTilt = -MAX_flt;
	float  CachedHeadRoll = -MAX_flt;
	FVector CachedHeadOffset = FVector(MAX_flt, MAX_flt, MAX_flt);
	float  CachedTailYaw = -MAX_flt;
	float  CachedTailTilt = -MAX_flt;
	float  CachedTailRoll = -MAX_flt;
	FVector CachedTailOffset = FVector(MAX_flt, MAX_flt, MAX_flt);
	bool   bDirty = true;
};
