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

struct FCFDebugChartPoint
{
	enum EPointValueType : uint8
	{
		Int,
		Float,
	};

	FCFDebugChartPoint() {}
	FCFDebugChartPoint(FVector2D InValue) : Value(InValue) {}

	FVector2D Value;
	FColor PointColor = FColor::Green;
	EPointValueType ValueType = EPointValueType::Int;
};

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
	void DrawDebugChart_2D(FDebug2DContext& Context, FText Title, TArray<FCFDebugChartPoint>& Points, FVector2D ChartY_ValueRange, float ChartPivotX, FVector2f ChartSize, UFont* InFont = nullptr);


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
	struct FDebugTextItem
	{
		FDebugTextItem() {}
		FDebugTextItem(const FText& InText) : Text(InText) {}
		FDebugTextItem(const FText& InText, FVector2f InTextScale, FColor InTextColor) : Text(InText), TextScale(InTextScale), TextColor(InTextColor) {}

		FText Text;
		FVector2f TextScale = FVector2f(1.f, 1.f);
		FColor TextColor = FColor::White;
	};

	FCFrameSimpleDebugText(TArray<FDebugTextItem>& InInitTexts, float PivotX);
	static TSharedPtr<FCFrameSimpleDebugText> CreateDebugProxy(FName DebugName, TArray<FDebugTextItem>& InInitTexts, float PivotX);


	virtual void ShowDebug(FCFDebugHelper::FDebug2DContext& Context) override;
	void UpdateText(TArray<FDebugTextItem>& NewTextItem);
protected:
	TArray<FDebugTextItem> TextData;
	float TextPivotX;
};

// 利用循环缓冲区实现Chart的数据记录
// 循环缓冲区要求数量必须为2的幂次
struct FCFrameSimpleDebugChart : public FCFrameDebugProxyBase
{
	FCFrameSimpleDebugChart(const FText& Title, int32 PointCount, FVector2D InChartY_ValueRange, float InChartPivotX, FVector2f InChartSize);

	static TSharedPtr<FCFrameSimpleDebugChart> CreateDebugProxy(FName DebugName, const FText& Title, int32 PointCount, FVector2D InChartY_ValueRange, float InChartPivotX, FVector2f InChartSize);

	virtual void ShowDebug(FCFDebugHelper::FDebug2DContext& Context) override;
	void Push(const FCFDebugChartPoint& NewPoint);
	void Pop();

protected:
	FText ChartTitle;
	float ChartPivotX;
	FVector2f ChartSize;
	FVector2D ChartY_ValueRange;

	int32 Head = 0;
	int32 MaxIndex;
	int32 Num = 0;
	TArray<FCFDebugChartPoint> Points;
};

// 在需要返回到旧的时间点时，会过渡到新的时间线
// 循环缓冲区要求数量必须为2的幂次
struct FCFrameSimpleDebugMultiTimeline : public FCFrameDebugProxyBase
{
	struct FTimePoint
	{
		FTimePoint() {}
		FTimePoint(double InValue, FColor InPointColor) : Value(InValue), PointColor(InPointColor) {}
		double Value = 0;
		FColor PointColor = FColor::White;
	};
	struct FTimelineItem
	{
		FTimelineItem(int32 InTimePointCount)
		{
			Points.SetNum(FMath::RoundUpToPowerOfTwo(InTimePointCount));
			MaxIndex = Points.Num() - 1;
		}

		void Push(const FTimePoint& NewPoint);
		void Reset();
		FTimePoint* GetLastPoint();

		int32 Head = 0;
		int32 MaxIndex;
		int32 Num = 0;

		TArray<FTimePoint> Points;
	};

	FCFrameSimpleDebugMultiTimeline(const FText& InTitle, int32 InTimePointCount, int32 InTimelineCount, float InPivotX, FVector2f InTimelineSize);
	static TSharedPtr<FCFrameSimpleDebugMultiTimeline> CreateDebugProxy(FName DebugName, const FText& InTitle, int32 InTimePointCount, int32 InTimelineCount, float InPivotX, FVector2f InTimelineSize);

	virtual void ShowDebug(FCFDebugHelper::FDebug2DContext& Context) override;
	void Push(const FTimePoint& NewPoint);

protected:
	FText TimelineTitle;
	int32 TimePointCount;
	float TimelinePivotX;
	FVector2f TimelineSize;

	int32 Head = 0;
	int32 MaxIndex;
	int32 Num = 0;
	TArray<FTimelineItem> TimelineItems;
};