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

	void Update(UWorld* WorldContext, bool bFault);
	void Update(UWorld* WorldContext, uint32 ServerCommandBufferNum, bool bFault);
	void FixedTick(UWorld* WorldContext, float DeltaTime);
	ETimeDilationAdaptStage GetState() { return TimeDilationAdaptStage; }

	FTimeDilationHelper(int32 DataNum, uint32 InMinCommandBufferNum, uint32 InMaxCommandBufferNum, float InFixedFrameRate);
	FTimeDilationHelper();

	float GetCurTimeDilation() { return CurTimeDilation; }


private:
	void AdjustClientTimeDilation(UWorld* World, float TimeDilation);
	int32 GetPredictedBufferNumDelta(float LocalPing);
	float GetLocalPing(UWorld* WorldContext);

private:
	FTimeMagAverager TimeMagAverager;

	ETimeDilationAdaptStage TimeDilationAdaptStage;
	float CurTimeDilation;
	float AccumulatedStableSeconds;
	float LastServerCommandBufferNum;

#if WITH_EDITOR
	TSharedPtr<struct FCFrameSimpleDebugText> DebugProxy_StateText;
#endif
};