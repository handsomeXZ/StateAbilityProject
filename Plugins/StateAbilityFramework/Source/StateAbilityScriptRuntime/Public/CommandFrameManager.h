// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/OnlineReplStructs.h"

#include "Buffer/BufferTypes.h"
#include "Buffer/CircularQueueCore.h"
#include "Net/Packet/CommandFrameInput.h"
#include "TimeDilationHelper.h"

#include "CommandFrameManager.generated.h"

class ACommandFrameNetChannel;
struct FCommandFrameInputNetPacket;

/**
* Tick function that the CommandFrame tick
**/
USTRUCT()
struct FCommandFrameTickFunction : public FTickFunction
{
	GENERATED_USTRUCT_BODY()

	/** World this tick function belongs to **/
	class UCommandFrameManager* Target;

	/**
		* Abstract function actually execute the tick.
		* @param DeltaTime - frame time to advance, in seconds
		* @param TickType - kind of tick for this frame
		* @param CurrentThread - thread we are executing on, useful to pass along as new tasks are created
		* @param MyCompletionGraphEvent - completion event for this task. Useful for holding the completetion of this task until certain child tasks are complete.
	**/
	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	/** Abstract function to describe this tick. Used to print messages about illegal cycles in the dependency graph **/
	virtual FString DiagnosticMessage() override;
	/** Function used to describe this tick for active tick reporting. **/
	virtual FName DiagnosticContext(bool bDetailed) override;
};

template<>
struct TStructOpsTypeTraits<FCommandFrameTickFunction> : public TStructOpsTypeTraitsBase2<FCommandFrameTickFunction>
{
	enum
	{
		WithCopy = false
	};
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnFixedUpdate, float /* DeltaTime */, uint32 /* RCF */, uint32 /* ICF */);

UCLASS()
class STATEABILITYSCRIPTRUNTIME_API UCommandFrameManager : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	static UCommandFrameManager* Get(UObject* WoldContet);

	static const uint32 MIN_COMMANDFRAME_NUM;
	static const uint32 MAX_COMMANDFRAME_NUM;
	static const uint32 MAX_SNAPSHOTBUFFER_NUM;

	static const float FixedFrameRate;

	UCommandFrameManager();
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection);
	virtual void Deinitialize();
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	void UpdateTimeDilationHelper(uint32 ServerCommandBufferNum, bool bFault);
	void ResetCommandFrame(uint32 CommandFrame);
	bool HasManagerPrepared();
	FTickFunction& GetTickFuntion() { return CommandFrameTickFunction; }
	uint32 GetRCF() { return RealCommandFrame; }
	uint32 GetICF() { return InternalCommandFrame; }

	// Player
	void PostLogin(AGameModeBase* GameMode, APlayerController* PC);
	void LoginOut(AGameModeBase* GameMode, AController* PC);
	void RegisterClientChannel(ACommandFrameNetChannel* Channel);


	// Tick
	uint32 MaxFixedFrameNum;
	float FixedDeltaTime;
	float AccumulateDeltaTime;
	double LastFixedUpdateTime;
	FCommandFrameTickFunction CommandFrameTickFunction;
	FTimerHandle LoadedWorldHandle;

	FOnFixedUpdate OnBeginFrame;
	FOnFixedUpdate OnEndFrame;

	void SetupCommandFrameTickFunction();
	void FlushCommandFrame(float DeltaTime);
	void FlushCommandFrame_Fixed(float DeltaTime);

	//////////////////////////////////////////////////////////////////////////

	TJOwnerShipCircularQueue<FCommandFrameInputFrame, FUniqueNetIdRepl, TArray<FCommandFrameInputAtom>> CommandBuffer;

	//////////////////////////////////////////////////////////////////////////
	// Client

	APlayerController* GetLocalPlayerController();
	void UpdateLocalHistoricalData();
	void ClientSendInputNetPacket();
	void ClientReceiveCommandAck(uint32 ServerCommandFrame);

	TJOwnerShipCircularQueue<FCommandFrameAttributeSnapshot, UScriptStruct*, uint8*> AttributeSnapshotBuffer;

	//////////////////////////////////////////////////////////////////////////
	// Server

	void ReceiveInput(const FUniqueNetIdRepl& NetId, const FCommandFrameInputNetPacket& InputNetPacket);
	void ConsumeInput();
	void SimulateInput(uint32 CommandFrame);

	// @TODO：轮询开销太大了
	void BuildInputEventMap(APlayerController* PC, TMap<const UInputAction*, TArray<TUniquePtr<FEnhancedInputActionEventBinding>>>& InputEventMap);

	void ServerSendDeltaNetPacket();
	uint32 GetCurCommandBufferNum(const FUniqueNetIdRepl& Owner);

	//////////////////////////////////////////////////////////////////////////

private:
	FTimeDilationHelper TimeDilationHelper;	// 时间膨胀辅助

	uint32 RealCommandFrame;
	uint32 PrevCommandFrame;
	uint32 InternalCommandFrame;	// InternalCommandFrame 可能随回滚和重新模拟而变化。

	UPROPERTY()
	TObjectPtr<ACommandFrameNetChannel> LocalNetChannel;
	UPROPERTY()
	TMap<AController*, ACommandFrameNetChannel*> NetChannels;
};
