#include "TimeDilationHelper.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#if WITH_EDITOR
#include "Debug/DebugUtils.h"
#endif

#define LOCTEXT_NAMESPACE "TimeDilationHelper"

DEFINE_LOG_CATEGORY_STATIC(LogTimeDilationHelper, Log, All);

const float FTimeDilationHelper::TargetTimeDilation = 1.f;
const float FTimeDilationHelper::TimeDilationMag = 0.05f;
const float FTimeDilationHelper::TinyTimeDilationMag = 0.0f;
const float FTimeDilationHelper::StableAccumulationSeconds = 0.5f;

FTimeMagAverager::FTimeMagAverager(int32 DataNum)
	: CurIndex(0)
	, CurNum(0)
	, MaxNum(DataNum)
	, Average(-FLOAT_NORMAL_THRESH)
{
	datas.Init(-1, MaxNum);
}

void FTimeMagAverager::Reset(int DataNum)
{
	CurIndex = 0;
	CurNum = 0;
	MaxNum = DataNum;
	Average = -0.00001;
	datas.Init(-1, MaxNum);
}

void FTimeMagAverager::Push(float value)
{
	datas[CurIndex] = value;
	CurIndex = (CurIndex + 1) % MaxNum;
	CurNum += CurNum < MaxNum ? 1 : 0;

	float sum = 0;
	float mid = datas[CurNum / 2];
	float ValidNum = 0;
	for (float& i : datas)
	{
		if (i > 0 && i <= mid * 1.5)
		{
			sum += i;
			++ValidNum;
		}
	}
	if (ValidNum > 0)
	{
		Average = sum / ValidNum;
	}
	else
	{
		Average = -0.00001;
	}

}

void FTimeDilationHelper::Update(UWorld* WorldContext, bool bFault)
{
	if (bFault)
	{
		TimeDilationAdaptStage = ETimeDilationAdaptStage::Dilate;
		AdjustClientTimeDilation(WorldContext, TargetTimeDilation + TimeDilationMag);

		AccumulatedStableSeconds = 0.0f;

		TimeMagAverager.Reset(10);
	}
}

void FTimeDilationHelper::Update(UWorld* WorldContext, uint32 ServerInputBufferNum, bool bFault)
{
	LastServerCommandBufferNum = ServerInputBufferNum;

	if (ServerInputBufferNum == 0 || bFault)
	{
		UE_LOG(LogTimeDilationHelper, Log, TEXT("ServerInputBufferNum[%d] bFault[%s]"), ServerInputBufferNum, bFault ? TEXT("true") : TEXT("false"));

		TimeDilationAdaptStage = ETimeDilationAdaptStage::Dilate;
		AdjustClientTimeDilation(WorldContext, TargetTimeDilation + TimeDilationMag);

		AccumulatedStableSeconds = 0.0f;
		
		TimeMagAverager.Reset(10);
	}
}

FTimeDilationHelper::FTimeDilationHelper()
	: MinCommandBufferNum(4)
	, MaxCommandBufferNum(16)
	, FixedFrameRate(30.0f)
	, TimeMagAverager(0)
	, TimeDilationAdaptStage(ETimeDilationAdaptStage::Default)
	, CurTimeDilation(1.0f)
	, AccumulatedStableSeconds(0.0f)
	, LastServerCommandBufferNum(0.0f)
{

}

FTimeDilationHelper::FTimeDilationHelper(int32 DataNum, uint32 InMinCommandBufferNum, uint32 InMaxCommandBufferNum, float InFixedFrameRate)
	: MinCommandBufferNum(InMinCommandBufferNum)
	, MaxCommandBufferNum(InMaxCommandBufferNum)
	, FixedFrameRate(InFixedFrameRate)
	, TimeMagAverager(DataNum)
	, TimeDilationAdaptStage(ETimeDilationAdaptStage::Default)
	, CurTimeDilation(1.0f)
	, AccumulatedStableSeconds(0.0f)
	, LastServerCommandBufferNum(0.0f)
{
	TimeDilationAdaptStage = ETimeDilationAdaptStage::Default;
	CurTimeDilation = 1.0f;
	AccumulatedStableSeconds = 0.0f;
	LastServerCommandBufferNum = 0;

}

void FTimeDilationHelper::FixedTick(UWorld* WorldContext, float DeltaTime)
{
	float LocalPing = GetLocalPing(WorldContext);

	if (LocalPing <= FLOAT_NORMAL_THRESH)
	{
		return;
	}

#if WITH_EDITOR
	if (WorldContext->GetNetMode() == NM_Client)
	{
		static FVector2f DefaultScale = FVector2f(1.f, 1.f);
		static FColor DefaultColor = FColor::Black;
		static FText Title = LOCTEXT("DebugTittle", "TimeDilationHelper");
		static FVector2f TitleScale = FVector2f(1.5f, 1.5f);

		if (!DebugProxy_StateText.IsValid())
		{
			TArray<FCFrameSimpleDebugText::FDebugTextItem> TextItems;
			TextItems.Emplace(LOCTEXT("Debug", "State[Unkown]"), DefaultScale, DefaultColor);
			DebugProxy_StateText = FCFrameSimpleDebugText::CreateDebugProxy(FName(TEXT("TimeDilationHelper")), TextItems, 20);
		}

		const UEnum* Enum = StaticEnum<ETimeDilationAdaptStage>();
		//UE_LOG(LogTimeDilationHelper, Log, TEXT("LocalPing[%f] LastServerCommandBufferNum[%f] State[%s]"), LocalPing, LastServerCommandBufferNum, *(Enum->GetNameStringByValue((int64)TimeDilationAdaptStage)));
		TArray<FCFrameSimpleDebugText::FDebugTextItem> TextItems;
		TextItems.Emplace(Title, TitleScale, DefaultColor);
		TextItems.Emplace(FText::FromString(FString::Printf(TEXT("State[%s]"), *(Enum->GetNameStringByValue((int64)TimeDilationAdaptStage)))), DefaultScale, DefaultColor);
		TextItems.Emplace(FText::FromString(FString::Printf(TEXT("Pings: %f"), LocalPing)), DefaultScale, DefaultColor);
		TextItems.Emplace(FText::FromString(FString::Printf(TEXT("预估 ServerCommandBuffer Num: %f"), LastServerCommandBufferNum)), DefaultScale, DefaultColor);
		
		DebugProxy_StateText->UpdateText(TextItems);
	}
#endif

	switch (TimeDilationAdaptStage)
	{
	case ETimeDilationAdaptStage::Default: 
	{
		if (LastServerCommandBufferNum <= FLOAT_NORMAL_THRESH)
		{
			// 从稳定状态升为膨胀状态（缓冲区的膨胀）
			TimeDilationAdaptStage = ETimeDilationAdaptStage::Dilate;
			AdjustClientTimeDilation(WorldContext, TargetTimeDilation + TimeDilationMag);
		}
		else if (LastServerCommandBufferNum > MinCommandBufferNum)
		{
			// 从稳定状态退位收缩状态（缓冲区的收缩）
			TimeDilationAdaptStage = ETimeDilationAdaptStage::Shrink;
			AdjustClientTimeDilation(WorldContext, TargetTimeDilation - TimeDilationMag);
		}

		break;
	}
	case ETimeDilationAdaptStage::Dilate:
	{
		LastServerCommandBufferNum += FixedFrameRate * DeltaTime;
		if (LastServerCommandBufferNum + GetPredictedBufferNumDelta(LocalPing) >= MaxCommandBufferNum)
		{
			// We predict it will reach Max and wait to see if it is stable.
			TimeDilationAdaptStage = ETimeDilationAdaptStage::PredictMax;
			AdjustClientTimeDilation(WorldContext, TargetTimeDilation + TinyTimeDilationMag);
		}

		break;
	}
	case ETimeDilationAdaptStage::PredictMax:
	{
		//if (LastServerCommandBufferNum >= MaxCommandBufferNum)
		{
			// If the Buffer can still reach Max without frame number compensation
			// the Buffer begins to accumulate stability time for shrinking
			TimeDilationAdaptStage = ETimeDilationAdaptStage::RealMax;
			AdjustClientTimeDilation(WorldContext, 1.0f);
		}

		break;
	}
	case ETimeDilationAdaptStage::RealMax:
	{
		AccumulatedStableSeconds += DeltaTime;
		TimeMagAverager.Push(LocalPing * 0.001);

		if (AccumulatedStableSeconds >= StableAccumulationSeconds)
		{
			// 当0.5s后，平均延迟的波动稳定则开始收缩（这里并不会关心当前延迟是否过高）
			if (TimeMagAverager.GetAverage() * 1.5 >= LocalPing * 0.001)
			{
				AccumulatedStableSeconds = 0.0f;
				TimeDilationAdaptStage = ETimeDilationAdaptStage::Shrink;
				AdjustClientTimeDilation(WorldContext, TargetTimeDilation - TimeDilationMag);
			}
			else
			{
				// re-accumulate
				AccumulatedStableSeconds = 0.0f;
			}

		}

		break;
	}
	case ETimeDilationAdaptStage::Shrink:
	{
		LastServerCommandBufferNum -= FixedFrameRate * DeltaTime;
		if (LastServerCommandBufferNum - GetPredictedBufferNumDelta(LocalPing) <= MinCommandBufferNum)
		{
			TimeDilationAdaptStage = ETimeDilationAdaptStage::Default;
			AdjustClientTimeDilation(WorldContext, TargetTimeDilation);
		}

		break;
	}
	}
}

void FTimeDilationHelper::AdjustClientTimeDilation(UWorld* World, float TimeDilation)
{
	check(World->GetNetMode() == ENetMode::NM_Client);

	if (AWorldSettings* WorldSettings = World->GetWorldSettings())
	{
		// 注意区分 DemoPlayTimeDilation、CinematicTimeDilation、TimeDilation
		//WorldSettings->DemoPlayTimeDilation = TimeDilation;
		WorldSettings->SetTimeDilation(TimeDilation);
		CurTimeDilation = World->GetWorldSettings()->GetEffectiveTimeDilation();

	}
}

int32 FTimeDilationHelper::GetPredictedBufferNumDelta(float LocalPing)
{
	// 预测未来整个RTT的服务器的缓冲区的大小变化，因为从客户端收到服务器最新数据，到服务器接到客户端最新的Input，是一个完整的RTT
	double PredictedDelta = (LocalPing * 0.001) * FixedFrameRate * FMath::Abs(CurTimeDilation - TargetTimeDilation);

	UE_LOG(LogTimeDilationHelper, Log, TEXT("LastServerCommandBufferNum[%f] PredictedBufferNumDelta[%f]"), LastServerCommandBufferNum, PredictedDelta);

	// 进1取整
	return PredictedDelta + 0.5;
}

float FTimeDilationHelper::GetLocalPing(UWorld* WorldContext)
{
	if (APlayerController* FirstPlayerController = WorldContext->GetFirstPlayerController())
	{
		if (APlayerState* FirstPlayerState = FirstPlayerController->PlayerState)
		{
			return FirstPlayerState->ExactPing;
		}
	}

	return 0;
}

#undef LOCTEXT_NAMESPACE