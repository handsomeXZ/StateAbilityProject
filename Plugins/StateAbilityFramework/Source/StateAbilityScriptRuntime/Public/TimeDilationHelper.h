#pragma once

#include "CoreMinimal.h"

#include "TimeDilationHelper.generated.h"

UENUM()
enum class ETimeDilationAdaptStage : uint8
{
	Default,
	Dilate,
	Shrink,
	PredictMax,
	RealMax,
};

struct FTimeMagAverager
{
	FTimeMagAverager(int32 DataNum);
	void Reset(int DataNum);
	void Push(float value);

	float GetAverage() { return Average; }
	bool IsValid() { return CurNum > 0 && Average >= 0; }

	int32 CurIndex;
	int32 CurNum;
	int32 MaxNum;
	float Average;
	TArray<float> datas;
};

struct FTimeDilationHelper
{
public:
	static const float TargetTimeDilation;
	static const float TimeDilationMag;
	static const float TinyTimeDilationMag;
	static const float StableAccumulationSeconds;
	const int32 MinCommandBufferNum;
	const int32 MaxCommandBufferNum;
	const float FixedFrameRate;

	void Update(UWorld* WorldContext, uint32 ServerCommandBufferNum, bool bFault);
	void FixedTick(UWorld* WorldContext, float DeltaTime);

	FTimeDilationHelper(int32 DataNum, uint32 InMinCommandBufferNum, uint32 InMaxCommandBufferNum, float InFixedFrameRate)
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
	FTimeDilationHelper()
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


private:
	void AdjustClientTimeDilation(UWorld* World, float TimeDilation);
	int32 GetPredictedBufferNumDelta(float LocalPing);
	float GetLocalPing(UWorld* WorldContext);

private:
	FTimeMagAverager TimeMagAverager;

	ETimeDilationAdaptStage TimeDilationAdaptStage;
	float CurTimeDilation;
	float AccumulatedStableSeconds;
	int32 LastServerCommandBufferNum;
};