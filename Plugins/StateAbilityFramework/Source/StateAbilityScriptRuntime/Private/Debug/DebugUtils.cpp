#include "Debug/DebugUtils.h"

#include "DrawDebugHelpers.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"

#include "Debug/DebugDrawQueue.h"

DEFINE_LOG_CATEGORY_STATIC(LogCFrameDebug, Log, All)

PRIVATE_DEFINE_NAMESPACE(FCFDebugHelper, UCanvas, DrawColor)

int DebugDraw_MaxElements = 20000;
FAutoConsoleVariableRef CVarCFrame_DebugDraw_MaxElements(TEXT("CFrame.DebugDraw.MaxLines"), DebugDraw_MaxElements, TEXT("Set the maximum number of debug draw lines that can be rendered (to limit perf drops)"));

float DebugDraw_Radius = 3000.0f;
FAutoConsoleVariableRef CVarCFrame_DebugDraw_Radius(TEXT("CFrame.DebugDraw.Radius"), DebugDraw_Radius, TEXT("Set the radius from the camera where debug draw capture stops (0 means infinite)"));


// SingleActor
bool bDebugDraw_SingleActor = false;
FAutoConsoleVariableRef CVarCFrameDebugDraw_SingleActor(TEXT("CFrame.DebugDraw.SingleActor"), bDebugDraw_SingleActor, TEXT("If true, then we draw for the actor the camera is looking at."));
float DebugDraw_SingleActorTraceLength = 2000.0f;
FAutoConsoleVariableRef CVarCFrameDebugDraw_SingleActorTraceLength(TEXT("CFrame.DebugDraw.SingleActorTraceLength"), DebugDraw_SingleActorTraceLength, TEXT("Set the trace length from the camera that is used to select the single actor."));
float DebugDraw_SingleActorMaxRadius = 1000.0f;
FAutoConsoleVariableRef CVarCFrameDebugDraw_SingleActorMaxRadius(TEXT("CFrame.DebugDraw.SingleActorMaxRadius"), DebugDraw_SingleActorMaxRadius, TEXT("Set the max radius to draw around the single actor."));


float CommandLifeTime(const FCFrameLatentDrawCommand& Command, const bool bIsPaused)
{
	// The linebatch time handling is a bit awkward and we need to translate
	// Linebatcher Lifetime < 0: eternal (regardless of bPersistent flag)
	// Linebatcher Lifetime = 0: default lifetime (which is usually 1 second)
	// Linebatcher Lifetime > 0: specified duration
	// Whereas in our Command, 
	// Command lifetime <= 0: 1 frame
	// Command lifetime > 0: specified duration
	if (Command.LifeTime <= 0)
	{
		// One frame - must be non-zero but also less than the next frame time which we don't know
		// NOTE: this only works because UChaosDebugDrawComponent ticks after the line batcher
		return UE_SMALL_NUMBER;
	}

	return Command.LifeTime;
}

void DebugDrawChaos(const UWorld* World, const TArray<FCFrameLatentDrawCommand>& DrawCommands, const bool bIsPaused)
{
	using namespace Chaos;

	if (World == nullptr)
	{
		return;
	}

	if (World->IsPreviewWorld())
	{
		return;
	}

	for (const FCFrameLatentDrawCommand& Command : DrawCommands)
	{
		const uint8 DepthPriority = Command.DepthPriority;
		switch (Command.Type)
		{
		case FCFrameLatentDrawCommand::EDrawType::Point:
			DrawDebugPoint(World, Command.LineStart, Command.Thickness, Command.Color, Command.bPersistentLines, CommandLifeTime(Command, bIsPaused), DepthPriority);
			break;
		case FCFrameLatentDrawCommand::EDrawType::Line:
			DrawDebugLine(World, Command.LineStart, Command.LineEnd, Command.Color, Command.bPersistentLines, CommandLifeTime(Command, bIsPaused), DepthPriority, Command.Thickness);
			break;
		case FCFrameLatentDrawCommand::EDrawType::DirectionalArrow:
			DrawDebugDirectionalArrow(World, Command.LineStart, Command.LineEnd, Command.ArrowSize, Command.Color, Command.bPersistentLines, CommandLifeTime(Command, bIsPaused), DepthPriority, Command.Thickness);
			break;
		case FCFrameLatentDrawCommand::EDrawType::Sphere:
			DrawDebugSphere(World, Command.LineStart, Command.Radius, Command.Segments, Command.Color, Command.bPersistentLines, CommandLifeTime(Command, bIsPaused), DepthPriority, Command.Thickness);
			break;
		case FCFrameLatentDrawCommand::EDrawType::Box:
			DrawDebugBox(World, Command.Center, Command.Extent, Command.Rotation, Command.Color, Command.bPersistentLines, CommandLifeTime(Command, bIsPaused), DepthPriority, Command.Thickness);
			break;
		case FCFrameLatentDrawCommand::EDrawType::String:
			DrawDebugString(World, Command.TextLocation, Command.Text, Command.TestBaseActor, Command.Color, CommandLifeTime(Command, bIsPaused), Command.bDrawShadow, Command.FontScale);
			break;
		case FCFrameLatentDrawCommand::EDrawType::Circle:
		{
			FMatrix M = FRotationMatrix::MakeFromYZ(Command.YAxis, Command.ZAxis);
			M.SetOrigin(Command.Center);
			DrawDebugCircle(World, M, Command.Radius, Command.Segments, Command.Color, Command.bPersistentLines, CommandLifeTime(Command, bIsPaused), DepthPriority, Command.Thickness, Command.bDrawAxis);
			break;
		}
		case FCFrameLatentDrawCommand::EDrawType::Capsule:
			DrawDebugCapsule(World, Command.Center, Command.HalfHeight, Command.Radius, Command.Rotation, Command.Color, Command.bPersistentLines, CommandLifeTime(Command, bIsPaused), DepthPriority, Command.Thickness);
		default:
			break;
		}
	}

}



FCFDebugHelper::FCFDebugHelper()
	: WeakWorld(nullptr)
{
	DebugInfoDelegateHandle = AHUD::OnShowDebugInfo.AddRaw(this, &FCFDebugHelper::ShowDebugInfo);
	LoadMapDelegateHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddLambda([this](UWorld* LoadedWorld) {
		OnPostLoadMap(LoadedWorld);
	});
}

FCFDebugHelper::~FCFDebugHelper()
{
	if (DebugInfoDelegateHandle.IsValid())
	{
		AHUD::OnShowDebugInfo.Remove(DebugInfoDelegateHandle);
	}

	if (LoadMapDelegateHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(LoadMapDelegateHandle);
	}
}

void FCFDebugHelper::OnPostLoadMap(UWorld* LoadedWorld)
{
	WeakWorld = LoadedWorld;
}

// Debug
void FCFDebugHelper::ShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	FDebug2DContext Context_2D(Canvas, YL, YPos, HUD, DisplayInfo);

	for (auto& Category : DebugExternalObjects)
	{
		if (Canvas && HUD->ShouldDisplayDebug(Category.Value.DebugName))
		{
			Category.Value.DebugFunc(Context_2D);
		}
	}

	for (auto& ProxysPair : DebugProxys)
	{
		if (Canvas && HUD->ShouldDisplayDebug(ProxysPair.Key))
		{
			for (TWeakPtr<FCFrameDebugProxyBase>& Proxy : ProxysPair.Value)
			{
				if (Proxy.IsValid())
				{
					Proxy.Pin()->ShowDebug(Context_2D);
				}
			}
		}
	}

	if (WeakWorld.IsValid() && HUD->ShouldDisplayDebug(FName(TEXT("CFrame3D"))))
	{
		UWorld* World = WeakWorld.Get();

		if (World->ViewLocationsRenderedLastFrame.Num() > 0)
		{
			if (bDebugDraw_SingleActor)
			{
				if (const APlayerController* Controller = GEngine->GetFirstLocalPlayerController(World))
				{
					FVector CamLoc;
					FRotator CamRot;
					Controller->GetPlayerViewPoint(CamLoc, CamRot);
					FVector CamForward = CamRot.Vector();
					CamForward *= DebugDraw_SingleActorTraceLength;

					FVector TraceStart = CamLoc;
					FVector TraceEnd = TraceStart + CamForward;

					FHitResult HitResult(ForceInit);
					FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(CFrameDebugVisibilityTrace), true, Controller->GetPawn());
					bool bHit = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, TraceParams);
					if (bHit && HitResult.GetActor() != nullptr)
					{
						FVector Origin, BoxExtent;
						HitResult.GetActor()->GetActorBounds(true, Origin, BoxExtent);
						const float Radius = BoxExtent.Size();
						if (Radius <= DebugDraw_SingleActorMaxRadius)
						{
							FCFrameDebugDrawQueue::GetInstance().SetRegionOfInterest(Origin, Radius);
						}
					}
				}
			}
			else
			{
				FCFrameDebugDrawQueue::GetInstance().SetRegionOfInterest(World->ViewLocationsRenderedLastFrame[0], DebugDraw_Radius);
			}
		}

		FCFrameDebugDrawQueue::GetInstance().SetMaxCost(DebugDraw_MaxElements);

		const bool bIsPaused = World->IsPaused();
		if (!bIsPaused)
		{
			FCFrameDebugDrawQueue::GetInstance().ExtractAllElements(DrawCommands);

			DebugDrawChaos(World, DrawCommands, World->IsPaused());
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// 3D
//////////////////////////////////////////////////////////////////////////
void FCFDebugHelper::DrawDebugPoint_3D(const FVector& Position, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
	FCFrameDebugDrawQueue::GetInstance().DrawDebugPoint(Position, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
}

void FCFDebugHelper::DrawDebugLine_3D(const FVector& LineStart, const FVector& LineEnd, const FColor& Color, bool bPersistentLines /* = false */, float LifeTime /* = -1.f */, uint8 DepthPriority /* = 0 */, float Thickness /* = 0.f */)
{
	FCFrameDebugDrawQueue::GetInstance().DrawDebugLine(LineStart, LineEnd, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
}

void FCFDebugHelper::DrawDebugDirectionalArrow_3D(const FVector& LineStart, const FVector& LineEnd, float ArrowSize, const FColor& Color, bool bPersistentLines /* = false */, float LifeTime /* = -1.f */, uint8 DepthPriority /* = 0 */, float Thickness /* = 0.f */)
{
	FCFrameDebugDrawQueue::GetInstance().DrawDebugDirectionalArrow(LineStart, LineEnd, ArrowSize, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
}

void FCFDebugHelper::DrawDebugCoordinateSystem_3D(const FVector& Position, const FRotator& AxisRot, Chaos::FReal Scale, bool bPersistentLines /* = false */, float LifeTime /* = -1.f */, uint8 DepthPriority /* = 0 */, float Thickness /* = 0.f */)
{
	FCFrameDebugDrawQueue::GetInstance().DrawDebugCoordinateSystem(Position, AxisRot, Scale, bPersistentLines, LifeTime, DepthPriority, Thickness);
}

void FCFDebugHelper::DrawDebugSphere_3D(FVector const& Center, Chaos::FReal Radius, int32 Segments, const FColor& Color, bool bPersistentLines /* = false */, float LifeTime /* = -1.f */, uint8 DepthPriority /* = 0 */, float Thickness /* = 0.f */)
{
	FCFrameDebugDrawQueue::GetInstance().DrawDebugSphere(Center, Radius, Segments, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
}

void FCFDebugHelper::DrawDebugBox_3D(FVector const& Center, FVector const& Extent, const FQuat& Rotation, FColor const& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
	FCFrameDebugDrawQueue::GetInstance().DrawDebugBox(Center, Extent, Rotation, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
}

void FCFDebugHelper::DrawDebugString_3D(FVector const& TextLocation, const FString& Text, class AActor* TestBaseActor, FColor const& Color, float Duration, bool bDrawShadow, float FontScale)
{
	FCFrameDebugDrawQueue::GetInstance().DrawDebugString(TextLocation, Text, TestBaseActor, Color, Duration, bDrawShadow, FontScale);
}

void FCFDebugHelper::DrawDebugCircle_3D(const FVector& Center, Chaos::FReal Radius, int32 Segments, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness, const FVector& YAxis, const FVector& ZAxis, bool bDrawAxis)
{
	FCFrameDebugDrawQueue::GetInstance().DrawDebugCircle(Center, Radius, Segments, Color, bPersistentLines, LifeTime, DepthPriority, Thickness, YAxis, ZAxis, bDrawAxis);
}

void FCFDebugHelper::DrawDebugCapsule_3D(const FVector& Center, Chaos::FReal HalfHeight, Chaos::FReal Radius, const FQuat& Rotation, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
	FCFrameDebugDrawQueue::GetInstance().DrawDebugCapsule(Center, HalfHeight, Radius, Rotation, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
}

//////////////////////////////////////////////////////////////////////////
// 2D
//////////////////////////////////////////////////////////////////////////

void FCFDebugHelper::RegisterDebugProxy(FName DebugName, TSharedPtr<struct FCFrameDebugProxyBase> DebugProxy)
{
	DebugProxys.FindOrAdd(DebugName).Add(DebugProxy.ToWeakPtr());
}

void FCFDebugHelper::DrawDebugString_2D(FDebug2DContext& Context, const FText& InText, float X, FVector2f Scale, FColor Color, UFont* InFont /* = nullptr */)
{
	if (!InFont)
	{
		InFont = GEngine->GetMediumFont();
	}

	Context.YPos += Context.Canvas->DrawText(InFont, InText, X, Context.YPos, Scale.X, Scale.Y);
}

void FCFDebugHelper::DrawDebugChart_2D(FDebug2DContext& Context, FText Title, TArray<FVector2D>& Points, FVector2D YRange, float PivotX, FVector2f Size, FColor Color, bool bIntValueText, UFont* InFont /* = nullptr */)
{
	// 默认Points是有序的

	if (Points.IsEmpty())
	{
		return;
	}

	if (!InFont)
	{
		InFont = GEngine->GetMediumFont();
	}

	FColor PrevColor = PRIVATE_GET_NAMESPACE(FCFDebugHelper, Context.Canvas, DrawColor);

	float MaxYValue = -UE_FLOAT_NON_FRACTIONAL;
	float MinYValue = UE_FLOAT_NON_FRACTIONAL;
	if (YRange.IsNearlyZero())
	{
		for (FVector2D& P : Points)
		{
			MaxYValue = FMath::Max(MaxYValue, P.Y);
			MinYValue = FMath::Min(MinYValue, P.Y);
		}
	}
	else
	{
		MaxYValue = YRange.Y;
		MinYValue = YRange.X;
	}

	bool bAllSameYValue = (MaxYValue - MinYValue) < FLOAT_NORMAL_THRESH;
	float ValueHeight = MaxYValue - MinYValue;
	float WidthStep = Size.X / (float)Points.Num();
	float HeightStep = Size.Y / (float)Points.Num();

	float XStartPos = PivotX;
	float YStartPos = Context.YPos + Size.Y;
	float XEndPos = PivotX + Size.X;
	float YEndPos = Context.YPos;

	auto GetPoint = [WidthStep, MaxYValue, MinYValue, ValueHeight, bAllSameYValue, XStartPos, YStartPos, XEndPos, YEndPos, Size](FVector2D Point, int32 ID) -> FVector2D {
		
		FVector2D Result(XStartPos + ID * WidthStep, 0.0);
		if (bAllSameYValue)
		{
			Result.Y = (YEndPos + YStartPos) / 2.0;
		}
		else
		{
			Result.Y = YStartPos - (Point.Y - MinYValue) / ValueHeight * Size.Y;
		}
		return Result;
	};

	auto GetValueText = [bIntValueText](double Value) -> FText {
		if (bIntValueText)
		{
			return FText::FromString(FString::FromInt(Value));
		}
		else
		{
			return FText::FromString(FString::SanitizeFloat(Value));
		}
	};

	auto GetValuePoint = [XStartPos, YStartPos, XEndPos, YEndPos, HeightStep, Size](int32 ID) -> FVector2D {
		return FVector2D(XEndPos, YEndPos + HeightStep * ID);
	};

	Context.Canvas->SetDrawColor(FColor::Black);

	// 坐标轴
	Context.Canvas->K2_DrawLine(FVector2D(XStartPos, YStartPos), FVector2D(XEndPos, YStartPos), 1.0f, FColor::Black);	// X 轴
	Context.Canvas->K2_DrawLine(FVector2D(XStartPos, YStartPos), FVector2D(XStartPos, YEndPos), 1.0f, FColor::Black);	// Y 轴

	// 连线、标点、数据列
	FVector2D PointLocation = GetPoint(Points[0], 0);
	FVector2D TextLocation = GetValuePoint(0);
	Context.Canvas->K2_DrawBox(PointLocation, FVector2D(2.0f , 2.0f), 1.0f, Color);
	Context.Canvas->DrawText(InFont, GetValueText(Points[0].Y), TextLocation.X, TextLocation.Y, 0.5f, 0.5f);
	for (int32 i = 1; i < Points.Num(); ++i)
	{
		Context.Canvas->K2_DrawBox(GetPoint(Points[i], i), FVector2D(2.0f, 2.0f), 1.0f, Color);

		PointLocation = GetPoint(Points[i], i);
		TextLocation = GetValuePoint(i);
		Context.Canvas->K2_DrawLine(GetPoint(Points[i-1], i-1), PointLocation, 1.0f, Color);
		Context.Canvas->DrawText(InFont, GetValueText(Points[i].Y), TextLocation.X, TextLocation.Y, 0.5f, 0.5f);
	}

	Context.YPos += Size.Y;

	if (!Title.IsEmpty())
	{
		Context.YPos += 3.f;
		Context.YPos += Context.Canvas->DrawText(InFont, Title, XStartPos, Context.YPos, 1.f, 1.f);
	}

	Context.Canvas->SetDrawColor(PrevColor);
}

//////////////////////////////////////////////////////////////////////////
FCFrameSimpleDebugText::FCFrameSimpleDebugText(FText InInitText, float PivotX, FVector2f Scale, FColor Color)
	: Text(InInitText)
	, TextPivotX(PivotX)
	, TextScale(Scale)
	, TextColor(Color)
{
	
}

TSharedPtr<FCFrameSimpleDebugText> FCFrameSimpleDebugText::CreateDebugProxy(FName DebugName, FText InInitText, float PivotX, FVector2f Scale, FColor Color)
{
	TSharedPtr<FCFrameSimpleDebugText> DebugProxy = MakeShared<FCFrameSimpleDebugText>(InInitText, PivotX, Scale, Color);
	FCFDebugHelper::Get().RegisterDebugProxy(DebugName, DebugProxy);
	return DebugProxy;
}

void FCFrameSimpleDebugText::ShowDebug(FCFDebugHelper::FDebug2DContext& Context)
{
	FCFDebugHelper::Get().DrawDebugString_2D(Context, Text, TextPivotX, TextScale, TextColor, nullptr);
}

void FCFrameSimpleDebugText::UpdateText(FText NewText)
{
	Text = NewText;
}


FCFrameSimpleDebugChart::FCFrameSimpleDebugChart(FText Title, int32 PointCount, FVector2D YRange, float InChartPivotX, FVector2f InChartSize, FColor InColor, bool InbIntValueText /* = false */)
	: ChartTitle(Title)
	, ChartPivotX(InChartPivotX)
	, ChartSize(InChartSize)
	, ChartColor(InColor)
	, ChartYRange(YRange)
	, bIntValueText(InbIntValueText)
	, MaxIndex(PointCount - 1)
{
	Points.SetNum(PointCount);
}

TSharedPtr<FCFrameSimpleDebugChart> FCFrameSimpleDebugChart::CreateDebugProxy(FName DebugName, FText Title, int32 PointCount, FVector2D YRange, float InChartPivotX, FVector2f InChartSize, FColor InColor, bool InbIntValueText)
{
	TSharedPtr<FCFrameSimpleDebugChart> DebugProxy = MakeShared<FCFrameSimpleDebugChart>(Title, PointCount, YRange, InChartPivotX, InChartSize, InColor, InbIntValueText);
	FCFDebugHelper::Get().RegisterDebugProxy(DebugName, DebugProxy);
	return DebugProxy;
}

void FCFrameSimpleDebugChart::ShowDebug(FCFDebugHelper::FDebug2DContext& Context)
{
	if (Num == 0)
	{
		return;
	}

	TArray<FVector2D> DrawPoints;
	DrawPoints.Empty(Num);
	for (int32 i = 0; i < Num; ++i)
	{
		int32 index = (i + Head) & MaxIndex;
		DrawPoints.Add(Points[index]);
	}

	FCFDebugHelper::Get().DrawDebugChart_2D(Context, ChartTitle, DrawPoints, ChartYRange, ChartPivotX, ChartSize, ChartColor, bIntValueText, nullptr);
}

void FCFrameSimpleDebugChart::Push(FVector2D NewPoint)
{
	Head = (Head + 1) & MaxIndex;
	Num = FMath::Min(Num + 1, MaxIndex + 1);

	int32 NewIndex = (Head + Num) & MaxIndex;

	Points[NewIndex] = NewPoint;
}

void FCFrameSimpleDebugChart::Pop()
{
	if (Num == 0)
	{
		return;
	}

	Head = (Head + 1) & MaxIndex;
	--Num;
}