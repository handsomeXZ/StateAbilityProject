// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Subsystems/WorldSubsystem.h"
#include "StructView.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/OnlineReplStructs.h"

#include "Buffer/BufferTypes.h"
#include "Buffer/CircularQueueCore.h"
#include "Net/Packet/CommandFrameInput.h"
#include "Net/CommandFrameNetTypes.h"
#include "TimeDilationHelper.h"

#include "CommandFrameManager.generated.h"

class ACommandFrameNetChannelBase;
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
DECLARE_DELEGATE_OneParam(FOnFrameNetChannelRegistered, ACommandFrameNetChannelBase* Channel);

UCLASS()
class STATEABILITYSCRIPTRUNTIME_API UCommandFrameManager : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	static UCommandFrameManager* Get(UObject* WoldContet);

	static const uint32 MIN_COMMANDFRAME_NUM;			// 命令缓冲区稳定时的期望数量
	static const uint32 MAX_COMMANDFRAME_NUM;			// 命令缓冲区最大容量
	static const uint32 MAX_COMMANDFRAME_REDUNDANT_NUM;	// 命令缓冲区最大冗余数量
	static const uint32 MAX_SNAPSHOTBUFFER_NUM;			// 属性快照缓冲区的最大容量

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
	void RegisterClientChannel(ACommandFrameNetChannelBase* Channel);

	// 注册时，如果存在就会立即调用
	void BindOnFrameNetChannelRegistered(UActorComponent* Key, FOnFrameNetChannelRegistered OnFrameNetChannelRegistered);
	void BindOnFrameNetChannelRegistered(AActor* Key, FOnFrameNetChannelRegistered OnFrameNetChannelRegistered);
	void UnBindOnFrameNetChannelRegistered(UObject* Key);
	void BroadcastOnFrameNetChannelRegistered(ACommandFrameNetChannelBase* Channel);

	// Tick
	uint32 MaxFixedFrameNum;
	float FixedDeltaTime;
	float AccumulateDeltaTime;
	double LastFixedUpdateTime;
	FCommandFrameTickFunction CommandFrameTickFunction;
	FTimerHandle LoadedWorldHandle;

	FOnFixedUpdate OnPreBeginFrame;
	FOnFixedUpdate OnBeginFrame;
	FOnFixedUpdate OnPostBeginFrame;
	FOnFixedUpdate OnPreEndFrame;
	FOnFixedUpdate OnEndFrame;
	FOnFixedUpdate OnPostEndFrame;

	void SetupCommandFrameTickFunction();
	void FlushCommandFrame(float DeltaTime);
	void FlushCommandFrame_Fixed(float DeltaTime);
	void EndPrevFlushCommandFrame(float DeltaTime);
	void AdvancedCommandFrame();
	void BeginNewFlushCommandFrame(float DeltaTime);
	//////////////////////////////////////////////////////////////////////////
	// Rewind
	void ReplayFrames(uint32 RewindedFrame);
	bool IsInRewinding();

	//////////////////////////////////////////////////////////////////////////
	// Input

	void SimulateInput(uint32 CommandFrame);
	// @TODO：轮询开销太大了
	void BuildInputEventMap(APlayerController* PC, TMap<const UInputAction*, TArray<TSharedPtr<FEnhancedInputActionEventBinding>>>& InputEventMap);

	TJOwnerShipCircularQueue<FCommandFrameInputFrame, FUniqueNetIdRepl, TArray<FCommandFrameInputAtom>> CommandBuffer;

	//////////////////////////////////////////////////////////////////////////
	// Client

	APlayerController* GetLocalPlayerController();
	void RecordCommandSnapshot();
	void ClientSendInputNetPacket();
	void ClientReceiveCommandAck(uint32 ServerCommandFrame);
	FStructView ReadAttributeFromSnapshotBuffer(uint32 CommandFrame, const UScriptStruct* Key);

	TJOwnerShipCircularQueue<FCommandFrameAttributeSnapshot, UScriptStruct*, uint8*> AttributeSnapshotBuffer;

	//////////////////////////////////////////////////////////////////////////
	// Server

	void ReceiveInput(ACommandFrameNetChannelBase* Channel, const FUniqueNetIdRepl& NetId, const FCommandFrameInputNetPacket& InputNetPacket);
	void ConsumeInput();

	void ServerSendDeltaNetPacket();
	uint32 GetCurCommandBufferNum(const FUniqueNetIdRepl& Owner);

	//////////////////////////////////////////////////////////////////////////

private:
	FTimeDilationHelper TimeDilationHelper;	// 时间膨胀辅助

	uint32 RealCommandFrame;
	uint32 PrevCommandFrame;
	uint32 InternalCommandFrame;	// InternalCommandFrame 可能随回滚和重新模拟而变化。

	UPROPERTY()
	TObjectPtr<ACommandFrameNetChannelBase> LocalNetChannel;
	UPROPERTY()
	TMap<AController*, ACommandFrameNetChannelBase*> NetChannels;

	TMap<UObject*, FOnFrameNetChannelRegistered> NetChannelDelegateMap;

	TMap<const FUniqueNetIdRepl, TMap<const UInputAction*, TArray<TSharedPtr<FEnhancedInputActionEventBinding>>>> InputProcedureCache;

	//////////////////////////////////////////////////////////////////////////
	// Debug
#if WITH_EDITOR
	TSharedPtr<struct FCFrameSimpleDebugChart> DebugProxy_ClientCmdBufferChart;
	TSharedPtr<struct FCFrameSimpleDebugMultiTimeline> DebugProxy_ClientFrameTimeline;
#endif
};
