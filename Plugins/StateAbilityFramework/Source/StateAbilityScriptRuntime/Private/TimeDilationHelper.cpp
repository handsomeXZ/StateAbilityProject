#include "TimeDilationHelper.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

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

void FTimeDilationHelper::Update(UWorld* WorldContext, uint32 ServerInputBufferNum, bool bFault)
{
	LastServerCommandBufferNum = ServerInputBufferNum;

	if (TimeDilationAdaptStage == ETimeDilationAdaptStage::Default && (ServerInputBufferNum == 0 || bFault))
	{
		TimeDilationAdaptStage = ETimeDilationAdaptStage::Dilate;
		AdjustClientTimeDilation(WorldContext, TargetTimeDilation + TimeDilationMag);

		AccumulatedStableSeconds = 0.0f;
		
		TimeMagAverager.Reset(10);
	}
}

void FTimeDilationHelper::FixedTick(UWorld* WorldContext, float DeltaTime)
{
	float LocalPing = GetLocalPing(WorldContext);
	if (LocalPing <= FLOAT_NORMAL_THRESH)
	{
		return;
	}

	switch (TimeDilationAdaptStage)
	{
	case ETimeDilationAdaptStage::Default: break;

	case ETimeDilationAdaptStage::Dilate:
	{
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
		if (LastServerCommandBufferNum >= MaxCommandBufferNum)
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
		// @TODO：暂时不进行收缩，后续需要完善一下

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
		WorldSettings->DemoPlayTimeDilation = TimeDilation;
		WorldSettings->SetTimeDilation(TimeDilation);
		CurTimeDilation = World->GetWorldSettings()->GetEffectiveTimeDilation();

	}
}

int32 FTimeDilationHelper::GetPredictedBufferNumDelta(float LocalPing)
{
	// 预测未来整个RTT的服务器的缓冲区的大小变化，因为从客户端收到服务器最新数据，到服务器接到客户端最新的Input，是一个完整的RTT
	double PredictedDelta = (LocalPing * 0.001) * FixedFrameRate * FMath::Abs(CurTimeDilation - TargetTimeDilation);

	const UEnum* Enum = StaticEnum<ETimeDilationAdaptStage>();
	UE_LOG(LogTimeDilationHelper, Log, TEXT("PredictedBufferNumDelta[%f] State[%s]"), PredictedDelta, *(Enum->GetNameStringByValue((int64)TimeDilationAdaptStage)));

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