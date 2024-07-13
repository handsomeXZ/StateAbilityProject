#pragma once

#include "CoreMinimal.h"
#include "Chaos/DebugDrawQueue.h"

#include "PrivateAccessor.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#define DEBUG_DECLARE_BEGIN  struct FDebugInfo {
#define DEBUG_ARGUMENT(Type, Variable) Type Variable;
#define DEBUG_ARGUMENT_INIT(Type, Variable, Value) Type Variable = Value;
#define DEBUG_DECLARE_END } DebugInfo;

#define DEBUG_THIS(Name) DebugInfo.Name = Name;
//#define DEBUG_FLOAT_WITH_NAME(Name, Value) DebugInfo.Name = Value;

#define DEBUG_COUNTER(Name) ++DebugInfo.Name;

#define DEBUG_CONDITION_BEGIN(condition) if (condition) {
#define DEBUG_CONDITION_END }

#define DEBUG_REGISTER(Name, Obj, Func) FCFDebugHelper::Get().RegisterDebug(Name, Obj, Func);
#define DEBUG_UNREGISTER(Obj) FCFDebugHelper::Get().UnRegisterDebug(Obj);
#else
#undef DEBUG_DECLARE_BEGIN
#undef DEBUG_ARGUMENT
#undef DEBUG_ARGUMENT_INIT
#undef DEBUG_DECLARE_END
#undef DEBUG_THIS
#undef DEBUG_FLOAT_WITH_NAME
#undef DEBUG_COUNTER
#undef DEBUG_CONDITION_BEGIN
#undef DEBUG_CONDITION_END
#undef DEBUG_REGISTER
#undef DEBUG_UNREGISTER

#define DEBUG_DECLARE_BEGIN
#define DEBUG_ARGUMENT(Type, Variable)
#define DEBUG_ARGUMENT_INIT(Type, Variable, Value)
#define DEBUG_DECLARE_END 
#define DEBUG_THIS(Name)
#define DEBUG_FLOAT_WITH_NAME(Name, Value)
#define DEBUG_COUNTER(Name)
#define DEBUG_CONDITION_BEGIN(condition)
#define DEBUG_CONDITION_END
#define DEBUG_REGISTER(Name, Obj, Func)
#define DEBUG_UNREGISTER(Obj)
#endif



class FCFDebugHelper
{
public:
	PRIVATE_DECLARE_NAMESPACE(UCanvas, FColor, DrawColor);

	FCFDebugHelper();
	virtual ~FCFDebugHelper();

	static FCFDebugHelper& Get()
	{
		static FCFDebugHelper JDebugHelper;
		return JDebugHelper;
	}
	void SetWorld(UWorld* InWorld) { WeakWorld = InWorld; }

	// 3D Debug
	void DrawDebugPoint_3D(const FVector& Position, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness);
	void DrawDebugLine_3D(const FVector& LineStart, const FVector& LineEnd, const FColor& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f);
	void DrawDebugDirectionalArrow_3D(const FVector& LineStart, const FVector& LineEnd, float ArrowSize, const FColor& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f);
	void DrawDebugCoordinateSystem_3D(const FVector& Position, const FRotator& AxisRot, Chaos::FReal Scale, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f);
	void DrawDebugSphere_3D(FVector const& Center, Chaos::FReal Radius, int32 Segments, const FColor& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f);
	void DrawDebugBox_3D(FVector const& Center, FVector const& Extent, const FQuat& Rotation, FColor const& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness);
	void DrawDebugString_3D(FVector const& TextLocation, const FString& Text, class AActor* TestBaseActor, FColor const& Color, float Duration, bool bDrawShadow, float FontScale);
	void DrawDebugCircle_3D(const FVector& Center, Chaos::FReal Radius, int32 Segments, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness, const FVector& YAxis, const FVector& ZAxis, bool bDrawAxis);
	void DrawDebugCapsule_3D(const FVector& Center, Chaos::FReal HalfHeight, Chaos::FReal Radius, const FQuat& Rotation, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness);
	
	// 2D Debug
	struct FDebug2DContext
	{
		FDebug2DContext(UCanvas* InCanvas, float& InYL, float& InYPos, class AHUD* InHUD, const class FDebugDisplayInfo& InDisplayInfo) : Canvas(InCanvas), YL(InYL), YPos(InYPos), HUD(InHUD), DisplayInfo(InDisplayInfo) {}
		UCanvas* Canvas;
		float& YL;
		float& YPos;
		class AHUD* HUD;
		const class FDebugDisplayInfo& DisplayInfo;
	};
	void ShowDebugInfo(class AHUD* HUD, class UCanvas* Canvas, const class FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos);
	void DrawDebugString_2D(FDebug2DContext& Context, const FText& InText, float X, FVector2f Scale, FColor Color, UFont* InFont = nullptr);
	void DrawDebugChart_2D(FDebug2DContext& Context, FText Title, TArray<FVector2D>& Points, FVector2D YRange, float PivotX, FVector2f Size, FColor Color, bool bIntValueText = false, UFont* InFont = nullptr);

	void RegisterExternalDebug(FName DebugName, UObject* obj, TFunction<void(FDebug2DContext& Context)> func)
	{
		FDebugView& view = DebugExternalObjects.FindOrAdd(obj);
		view.DebugName = DebugName;
		view.DebugFunc = func;
	}
	void RegisterDebugProxy(FName DebugName, TSharedPtr<struct FCFrameDebugProxyBase> DebugProxy);
	void UnRegisterDebug(UObject* obj)
	{
		DebugExternalObjects.Remove(obj);
	}
protected:
	void OnPostLoadMap(UWorld* LoadedWorld);

private:
	struct FDebugView
	{
		FName DebugName;
		TFunction<void(FDebug2DContext& Context)> DebugFunc;
	};
	TMap<UObject*, FDebugView> DebugExternalObjects;

	TMap<FName, TArray<TWeakPtr<struct FCFrameDebugProxyBase>>> DebugProxys;

	FDelegateHandle DebugInfoDelegateHandle;
	FDelegateHandle LoadMapDelegateHandle;
	TArray<struct FCFrameLatentDrawCommand> DrawCommands;
	TWeakObjectPtr<UWorld> WeakWorld;
};

struct FCFrameDebugProxyBase : public TSharedFromThis<FCFrameDebugProxyBase>
{
	FCFrameDebugProxyBase() {}
	virtual ~FCFrameDebugProxyBase() {}

	virtual void ShowDebug(FCFDebugHelper::FDebug2DContext& Context) {}
};

struct FCFrameSimpleDebugText : public FCFrameDebugProxyBase
{
	FCFrameSimpleDebugText(FText InInitText, float PivotX, FVector2f Scale, FColor Color);

	static TSharedPtr<FCFrameSimpleDebugText> CreateDebugProxy(FName DebugName, FText InInitText, float PivotX, FVector2f Scale, FColor Color);

	virtual void ShowDebug(FCFDebugHelper::FDebug2DContext& Context) override;
	void UpdateText(FText NewText);
protected:
	FText Text;
	float TextPivotX;
	FVector2f TextScale;
	FColor TextColor;
	
};

// 利用循环缓冲区实现Chart的数据记录
struct FCFrameSimpleDebugChart : public FCFrameDebugProxyBase
{
	FCFrameSimpleDebugChart(FText Title, int32 PointCount, FVector2D YRange, float InChartPivotX, FVector2f InChartSize, FColor InColor, bool InbIntValueText = false);

	static TSharedPtr<FCFrameSimpleDebugChart> CreateDebugProxy(FName DebugName, FText Title, int32 PointCount, FVector2D YRange, float InChartPivotX, FVector2f InChartSize, FColor InColor, bool InbIntValueText = false);

	virtual void ShowDebug(FCFDebugHelper::FDebug2DContext& Context) override;
	void Push(FVector2D NewPoint);
	void Pop();

protected:
	FText ChartTitle;
	float ChartPivotX;
	FVector2f ChartSize;
	FColor ChartColor;
	FVector2D ChartYRange;
	bool bIntValueText;

	int32 Head = -1;
	int32 MaxIndex;
	int32 Num = 0;
	TArray<FVector2D> Points;
};