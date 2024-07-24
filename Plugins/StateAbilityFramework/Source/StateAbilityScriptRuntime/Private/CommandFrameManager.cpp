#include "CommandFrameManager.h"

#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include "CommandFrameSetting.h"
#include "Net/CommandEnhancedInput.h"

// @TODO: Manager本身不应该直接使用这个
#include "Net/CommandFrameNetChannel.h"

#if WITH_EDITOR
#include "Debug/DebugUtils.h"
#endif

#define LOCTEXT_NAMESPACE "CommandFrameManager"

PRIVATE_DEFINE(APlayerController, TArray<TWeakObjectPtr<UInputComponent>>, CurrentInputStack)

DEFINE_LOG_CATEGORY_STATIC(LogCommandFrameManager, Log, Verbose)

const uint32 UCommandFrameManager::MIN_COMMANDFRAME_NUM = 4;
const uint32 UCommandFrameManager::MAX_COMMANDFRAME_NUM = 32;
const uint32 UCommandFrameManager::MAX_COMMANDFRAME_REDUNDANT_NUM = 16;
const uint32 UCommandFrameManager::MAX_SNAPSHOTBUFFER_NUM = 32;

const float UCommandFrameManager::FixedFrameRate = 30.0f;

// 统一以秒（s）为时间单位

namespace CFrameUtils
{
	AController* GetOwnerIsController(AActor* Actor)
	{
		if (AController* Controller = Cast<AController>(Actor))
		{
			return Controller;
		}

		AActor* Owner = Actor->GetOwner();
		if (!IsValid(Owner))
		{
			return nullptr;
		}

		return GetOwnerIsController(Owner);
	}
}

//////////////////////////////////////////////////////////////////////////
// FCommandFrameTickFunction

void FCommandFrameTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	UE_LOG(LogCommandFrameManager, Verbose, TEXT("TickFunction ExecuteTick"));

	check(Target);
	if (IsValid(Target))
	{
		UE_LOG(LogCommandFrameManager, Verbose, TEXT("TickFunction FlushCommandFrame"));
		Target->FlushCommandFrame(DeltaTime);
	}
}

FString FCommandFrameTickFunction::DiagnosticMessage()
{
	return TEXT("FCommandFrameTickFunction");
}

FName FCommandFrameTickFunction::DiagnosticContext(bool bDetailed)
{
	return FName(TEXT("CommandFrameTick"));
}

//////////////////////////////////////////////////////////////////////////
// UCommandFrameManager

UCommandFrameManager::UCommandFrameManager()
	: Super()
	, FixedDeltaTime(0.0f)
	, AccumulateDeltaTime(0.0f)
	, LastFixedUpdateTime(0.0)
	, CommandBuffer(MAX_COMMANDFRAME_NUM)
	, AttributeSnapshotBuffer(MAX_SNAPSHOTBUFFER_NUM)
	, TimeDilationHelper(10, MIN_COMMANDFRAME_NUM, MAX_COMMANDFRAME_NUM, FixedFrameRate)
	, RealCommandFrame(0)
	, PrevCommandFrame(0)
	, InternalCommandFrame(0)
	, LocalNetChannel(nullptr)
{

}

UCommandFrameManager* UCommandFrameManager::Get(UObject* WoldContet)
{
	if (IsValid(WoldContet) && WoldContet->GetWorld())
	{
		return WoldContet->GetWorld()->GetSubsystem<UCommandFrameManager>();
	}

	return nullptr;
}

bool UCommandFrameManager::ShouldCreateSubsystem(UObject* Outer) const
{
	// Getting setting on whether to turn off subsystem or not
	const bool bShouldCreate = GetDefault<UCommandFrameSettings>()->bEnableCommandFrameNetReplication;

	return bShouldCreate && Super::ShouldCreateSubsystem(Outer);
}

void UCommandFrameManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FGameModeEvents::GameModePostLoginEvent.AddUObject(this, &UCommandFrameManager::PostLogin);
	FGameModeEvents::GameModeLogoutEvent.AddUObject(this, &UCommandFrameManager::LoginOut);

	if (IsValid(GetWorld()))
	{
		// 在当前帧，UWorld还未完成初始化。
		FTimerManager& TimerManager = GetWorld()->GetTimerManager();
		LoadedWorldHandle = TimerManager.SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UCommandFrameManager::SetupCommandFrameTickFunction));
	}

	FCFDebugHelper::Get().SetWorld(GetWorld());
}

void UCommandFrameManager::Deinitialize()
{
	UE_LOG(LogCommandFrameManager, Verbose, TEXT("CommandFrameManager Deinitialize"));

	if (CommandFrameTickFunction.IsTickFunctionRegistered())
	{
		CommandFrameTickFunction.UnRegisterTickFunction();
	}

	FGameModeEvents::GameModePostLoginEvent.RemoveAll(this);
	FGameModeEvents::GameModeLogoutEvent.RemoveAll(this);

	if (IsValid(GetWorld()))
	{
		GetWorld()->GetTimerManager().ClearTimer(LoadedWorldHandle);
	}

	for (auto& ChannelPair : NetChannels)
	{
		if (IsValid(ChannelPair.Value))
		{
			ChannelPair.Value->Destroy();
		}
	}
	NetChannels.Empty();

	if (IsValid(LocalNetChannel))
	{
		LocalNetChannel->Destroy();
	}
	LocalNetChannel = nullptr;

	Super::Deinitialize();
}

bool UCommandFrameManager::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCommandFrameManager::SetupCommandFrameTickFunction()
{
	UWorld* LoadedWorld = GetWorld();
	UGameInstance* GameInstance = LoadedWorld->GetGameInstance();

	UE_LOG(LogCommandFrameManager, Verbose, TEXT("CommandFrameManager SetupCommandFrameTickFunction"));

	if (!CommandFrameTickFunction.IsTickFunctionRegistered())
	{
		CommandFrameTickFunction.bCanEverTick = true;
		CommandFrameTickFunction.Target = this;

		CommandFrameTickFunction.TickGroup = TG_PrePhysics;
		CommandFrameTickFunction.EndTickGroup = TG_PrePhysics;
		CommandFrameTickFunction.RegisterTickFunction(LoadedWorld->PersistentLevel);

		CommandFrameTickFunction.bHighPriority = true;
		CommandFrameTickFunction.bStartWithTickEnabled = true;
		CommandFrameTickFunction.bAllowTickOnDedicatedServer = true;

		CommandFrameTickFunction.TickInterval = 0;

		MaxFixedFrameNum = 4;
		FixedDeltaTime = 1.0 / FixedFrameRate;

		LastFixedUpdateTime = FPlatformTime::Seconds();
	}
}

void UCommandFrameManager::FlushCommandFrame(float DeltaTime)
{
	float RealDeltaTime = (float)(FPlatformTime::Seconds() - LastFixedUpdateTime);

	AccumulateDeltaTime += RealDeltaTime * TimeDilationHelper.GetCurTimeDilation();
	LastFixedUpdateTime = FPlatformTime::Seconds();

	UE_LOG(LogCommandFrameManager, Log, TEXT("FlushCommandFrame DeltaTime[%f] AccumulateDeltaTime[%f] CurTime[%f]"), DeltaTime, AccumulateDeltaTime, LastFixedUpdateTime);

	uint32 Num = 1;
	while (AccumulateDeltaTime > FixedDeltaTime && Num++ <= MaxFixedFrameNum)
	{
		AccumulateDeltaTime -= FixedDeltaTime;
		UE_LOG(LogCommandFrameManager, Log, TEXT("FlushCommandFrame_Fixed"));
		FlushCommandFrame_Fixed(FixedDeltaTime);
	}

	if (Num > MaxFixedFrameNum)
	{
		// 避免雪崩，重置时间。
		AccumulateDeltaTime = 0.0f;
		UE_LOG(LogCommandFrameManager, Warning, TEXT("GameThread is taking too long. In order to prevent a cascade effect, it has been forcefully halted. Please pay attention to optimization!"));
	}
}

void UCommandFrameManager::FlushCommandFrame_Fixed(float DeltaTime)
{
	if (!HasManagerPrepared())
	{
		return;
	}

	// Tick: TG_PrePhysics
	// DeltaTime = FixedDeltaTime;

	EndPrevFlushCommandFrame(DeltaTime);

	AdvancedCommandFrame();

	BeginNewFlushCommandFrame(DeltaTime);
}

void UCommandFrameManager::EndPrevFlushCommandFrame(float DeltaTime)
{
	PrevCommandFrame = RealCommandFrame;

	//////////////////////////////////////////////////////////////////////////
	// 处理前一帧
	// 第0帧空，不做处理
	if (RealCommandFrame)
	{
		// 因为玩家操作不会因为回滚等情况而被修改，所以可以先行记录。
		if (GetWorld()->GetNetMode() == ENetMode::NM_Client)
		{
			// 记录上一帧的命令快照
			RecordCommandSnapshot();
			
			// 发送InputPacket
			ClientSendInputNetPacket();
		}
		
		// 检查并处理有序的Delta包等，因此这里有可能会触发回滚
		OnPreEndFrame.Broadcast(DeltaTime, RealCommandFrame, InternalCommandFrame);

		// 移动的Tick 和 状态快照基本会在这里执行
		OnEndFrame.Broadcast(DeltaTime, RealCommandFrame, InternalCommandFrame);
		
		if (GetWorld()->GetNetMode() == ENetMode::NM_DedicatedServer)
		{
			ServerSendDeltaNetPacket();
		}

		OnPostEndFrame.Broadcast(DeltaTime, RealCommandFrame, InternalCommandFrame);

#if WITH_EDITOR
		if (GetWorld()->GetNetMode() == ENetMode::NM_Client)
		{
			if (!DebugProxy_ClientFrameTimeline.IsValid())
			{
				DebugProxy_ClientFrameTimeline = FCFrameSimpleDebugMultiTimeline::CreateDebugProxy(FName(TEXT("CommandFrameManager")), LOCTEXT("CommandFrameManager", "[Client] FrameTimeline"), 200, 4, 20, FVector2f(350.0f, 50.0f));
			}

			if (IsInRewinding())
			{
				DebugProxy_ClientFrameTimeline->Push(FCFrameSimpleDebugMultiTimeline::FTimePoint(InternalCommandFrame, FColor(230, 126, 34)));
			}
			else
			{
				DebugProxy_ClientFrameTimeline->Push(FCFrameSimpleDebugMultiTimeline::FTimePoint(RealCommandFrame, FColor(39, 174, 96)));
			}
		}
#endif
	}
}

void UCommandFrameManager::AdvancedCommandFrame()
{
	// 命令帧进入新的一帧
	if (!IsInRewinding())
	{
		++RealCommandFrame;
	}
	++InternalCommandFrame;
}

void UCommandFrameManager::BeginNewFlushCommandFrame(float DeltaTime)
{
	//////////////////////////////////////////////////////////////////////////
	// 预处理

	OnPreBeginFrame.Broadcast(DeltaTime, RealCommandFrame, InternalCommandFrame);
	//////////////////////////////////////////////////////////////////////////
	// 处理当前帧

	OnBeginFrame.Broadcast(DeltaTime, RealCommandFrame, InternalCommandFrame);

	if (GetWorld()->GetNetMode() == ENetMode::NM_DedicatedServer)
	{
		// 因为是用Client的Input来模拟当前最新帧，所以得用RCF

		// 服务端在完成Tick后，需要消耗记录的Input来执行
		ConsumeInput();
	}
	else if (GetWorld()->GetNetMode() == ENetMode::NM_Client && !IsInRewinding())
	{
		// 仅客户端需要更新时间膨胀
		TimeDilationHelper.FixedTick(GetWorld(), DeltaTime);
	}

	OnPostBeginFrame.Broadcast(DeltaTime, RealCommandFrame, InternalCommandFrame);
}

void UCommandFrameManager::ReplayFrames(uint32 RewindedFrame)
{
#if WITH_EDITOR
	if (!DebugProxy_ClientFrameTimeline.IsValid())
	{
		DebugProxy_ClientFrameTimeline = FCFrameSimpleDebugMultiTimeline::CreateDebugProxy(FName(TEXT("CommandFrameManager")), LOCTEXT("CommandFrameManager", "[Client] FrameTimeline"), 200, 4, 20, FVector2f(350.0f, 50.0f));
	}

	DebugProxy_ClientFrameTimeline->Push(FCFrameSimpleDebugMultiTimeline::FTimePoint(RewindedFrame, FColor(192, 57, 43)));
#endif

	TimeDilationHelper.Update(GetWorld(), true);

	// 已将状态重置到RewindedFrame了，需要从ICF开始重新模拟到RCF。
	InternalCommandFrame = RewindedFrame + 1;

	for (; InternalCommandFrame < RealCommandFrame;)
	{
		BeginNewFlushCommandFrame(FixedDeltaTime);

		SimulateInput(InternalCommandFrame);

		EndPrevFlushCommandFrame(FixedDeltaTime);

		AdvancedCommandFrame();
	}

	InternalCommandFrame = RealCommandFrame;

	// 到这里后，已经是进入了原本的RCF的帧处理期间。
	// 此时需要最后模拟一次输入
	BeginNewFlushCommandFrame(FixedDeltaTime);
	SimulateInput(RealCommandFrame);
}

void UCommandFrameManager::UpdateTimeDilationHelper(uint32 ServerCommandBufferNum, bool bFault)
{
	TimeDilationHelper.Update(GetWorld(), ServerCommandBufferNum, bFault);
}

bool UCommandFrameManager::HasManagerPrepared()
{
	if (GetWorld()->GetNetMode() == NM_Client)
	{
		APlayerController* PC = GetLocalPlayerController();
		if (!PC)
		{
			return false;
		}
		APlayerState* PS = PC->GetPlayerState<APlayerState>();
		if (!PS)
		{
			return false;
		}
	}

	return true;
}

bool UCommandFrameManager::IsInRewinding()
{
	// 这里由ReplayFrames保证RCF处于最新未处理的帧上。
	return InternalCommandFrame < RealCommandFrame;
}

APlayerController* UCommandFrameManager::GetLocalPlayerController()
{
	UGameInstance* GameInstance = GetWorld()->GetGameInstance();

	if (GameInstance->GetNumLocalPlayers())
	{
		// 加速查询
		return GameInstance->GetFirstLocalPlayerController();
	}
	else
	{
		// Only return a local PlayerController from the given World.
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PC = Iterator->Get();
			if (PC && PC->IsLocalController())
			{
				return PC;
			}
		}
	}

	return nullptr;
}

void UCommandFrameManager::RecordCommandSnapshot()
{
	if (IsInRewinding())
	{
		return;
	}

	// 处理前一帧
	APlayerController* PC = GetLocalPlayerController();
	APlayerState* PS = PC->GetPlayerState<APlayerState>();

	UCommandEnhancedInputSubsystem* InputSystem = GetWorld()->GetSubsystem<UCommandEnhancedInputSubsystem>();

	// 命令快照
	if (uint8* RawData = CommandBuffer.AllocateItemData(PS->GetUniqueId(), InputSystem->GetInputNum(), RealCommandFrame))
	{
		// 申请未初始化的空间
		// 如果预分配的RawData不为空，则开始填充
		InputSystem->PopInputsWithRawData(RawData);
	}


#if WITH_EDITOR
	FCommandFrameInputFrame* InputFrame = CommandBuffer.ReadData(RealCommandFrame);
	if (!InputFrame)
	{
		return;
	}

	if (PC->GetPawn())
	{
		APawn* Pawn = PC->GetPawn();

		uint8 InputCount = 0;
		if (uint8* Counter = InputFrame->OrderCounter.Find(PS->GetUniqueId()))
		{
			InputCount = *Counter;
		}

		FCFDebugHelper::Get().DrawDebugString_3D(Pawn->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f), FString::Printf(TEXT("[Client] CmdCount: %d"), InputCount), nullptr, FColor::Green, -1, false, 1.0f);
	}

	if (!DebugProxy_ClientCmdBufferChart.IsValid())
	{
		DebugProxy_ClientCmdBufferChart = FCFrameSimpleDebugChart::CreateDebugProxy(FName(TEXT("ClientCommandBuffer")), LOCTEXT("ClientCommandBuffer", "[Client] CommandBuffer"), 16, FVector2D(-1, MAX_COMMANDFRAME_NUM), 20.0f, FVector2f(200.0f, 150.0f));
	}

	DebugProxy_ClientCmdBufferChart->Push(FCFDebugChartPoint(FVector2D(RealCommandFrame, CommandBuffer.Count())));
#endif
}

void UCommandFrameManager::ClientSendInputNetPacket()
{
	if (IsInRewinding())
	{
		return;
	}

	APlayerController* PC = GetLocalPlayerController();

	if (!IsValid(LocalNetChannel))
	{
		return;
	}

	UE_LOG(LogCommandFrameManager, Verbose, TEXT("Waiting to send InputNetPacket Frame[%d]"), RealCommandFrame);

	
	uint32 RedundantNum = FMath::Min(CommandBuffer.Count(), MAX_COMMANDFRAME_REDUNDANT_NUM);
	TCircularQueueView<FCommandFrameInputFrame> RangeView = CommandBuffer.ReadRangeData_Shrink(RealCommandFrame, RedundantNum);
	FCommandFrameInputNetPacket InputNetPacket(PC->GetNetConnection(), RangeView);
	LocalNetChannel->ClientSend_CommandFrameInputNetPacket(InputNetPacket);
}

void UCommandFrameManager::ClientReceiveCommandAck(uint32 ServerCommandFrame)
{
	// 仅对输入缓冲区进行标记Ack
	CommandBuffer.AckData(ServerCommandFrame);
}

FStructView UCommandFrameManager::ReadAttributeFromSnapshotBuffer(uint32 CommandFrame, const UScriptStruct* Key)
{
	if (FCommandFrameAttributeSnapshot* AttributeSnapshot = AttributeSnapshotBuffer.ReadData(CommandFrame))
	{
		return FStructView(Key, AttributeSnapshot->ReadItemData(Key));
	}
	return nullptr;
}

void UCommandFrameManager::ResetCommandFrame(uint32 CommandFrame)
{
	RealCommandFrame = CommandFrame;
	PrevCommandFrame = CommandFrame;
	InternalCommandFrame = CommandFrame;
	CommandBuffer.AckData(CommandFrame);
	AttributeSnapshotBuffer.Empty();
}

void UCommandFrameManager::ReceiveInput(ACommandFrameNetChannelBase* Channel, const FUniqueNetIdRepl& NetId, const FCommandFrameInputNetPacket& InputNetPacket)
{
	auto AllocateData = [this, NetId](uint32 CommandFrame, int32 DataCount) -> uint8* {
		return CommandBuffer.AllocateItemData(NetId, DataCount, CommandFrame);
	};

	UE_LOG(LogCommandFrameManager, Log, TEXT("ReadRedundantData [%s] RCF[%u] ICF[%u] BufferNum[%u]"), *(NetId.ToString()), RealCommandFrame, InternalCommandFrame, CommandBuffer.Count(NetId));

	APlayerController* PC = Cast<APlayerController>(Channel->GetOwner());

	InputNetPacket.ReadRedundantData(PC->GetNetConnection(), AllocateData);
}

void UCommandFrameManager::ConsumeInput()
{
	if (IsInRewinding())
	{
		return;
	}

	// 处理当前帧
	SimulateInput(RealCommandFrame);



	// 最后Ack
	CommandBuffer.AckNextData();
}

void UCommandFrameManager::SimulateInput(uint32 CommandFrame)
{
	InputProcedureCache.Reset();

	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PC = Iterator->Get();
		if (PC)
		{
			APlayerState* PS = PC->GetPlayerState<APlayerState>();
			TMap<const UInputAction*, TArray<TSharedPtr<FEnhancedInputActionEventBinding>>>& InputEventMapRef = InputProcedureCache.FindOrAdd(PS->GetUniqueId());
			BuildInputEventMap(PC, InputEventMapRef);
		}
	}

	FCommandFrameInputFrame* InputFrame = CommandBuffer.ReadData(CommandFrame);
	if (!InputFrame)
	{
		return;
	}

#if WITH_EDITOR
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PC = Iterator->Get();
		if (PC && PC->GetPawn() && PC->GetPlayerState<APlayerState>())
		{
			APawn* Pawn = PC->GetPawn();
			APlayerState* PS = PC->GetPlayerState<APlayerState>();

			uint8 InputCount = 0;
			if (uint8* Counter = InputFrame->OrderCounter.Find(PS->GetUniqueId()))
			{
				InputCount = *Counter;
			}

			if (GetWorld()->GetNetMode() == NM_DedicatedServer)
			{
				FCFDebugHelper::Get().DrawDebugString_3D(Pawn->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f), FString::Printf(TEXT("[Server]CmdCount: %d"), InputCount), nullptr, FColor::White, -1, false, 1.0f);
			}
			else
			{
				FCFDebugHelper::Get().DrawDebugString_3D(Pawn->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f), FString::Printf(TEXT("[Client Rewind]CmdCount: %d"), InputCount), nullptr, FColor::Red, -1, false, 1.0f);
			}
		}
	}
#endif

	uint32 InputIndex = 0;

	for (auto& Counter : InputFrame->OrderCounter)
	{
		const FUniqueNetIdRepl& NetId = Counter.Key;
		TMap<const UInputAction*, TArray<TSharedPtr<FEnhancedInputActionEventBinding>>>& InputEventMap = InputProcedureCache.FindOrAdd(NetId);
		uint8 Count = Counter.Value;

		if (!Count || InputEventMap.IsEmpty())
		{
			InputIndex += Count;
			continue;
		}

		

		while (Count--)
		{
			FCommandFrameInputAtom& InputAtom = InputFrame->InputQueue[InputIndex++];
			TArray<TSharedPtr<FEnhancedInputActionEventBinding>>* InputBindings = InputEventMap.Find(InputAtom.InputAction);
			if (!InputBindings)
			{
				continue;
			}

			static TArray<TSharedPtr<FEnhancedInputActionEventBinding>> TriggeredDelegates;
			TriggeredDelegates.Reset();
			
			for (const TSharedPtr<FEnhancedInputActionEventBinding>& Binding : *InputBindings)
			{
				const ETriggerEvent BoundTriggerEvent = Binding->GetTriggerEvent();
				// Raise appropriate delegate to report on event state
				if (InputAtom.TriggerEvent == BoundTriggerEvent)
				{
					// Record intent to trigger started as well as triggered
					// EmplaceAt 0 for the "Started" event it is always guaranteed to fire before Triggered

					FCommandFrameInputActionInstance ActionData(InputAtom);

					if (BoundTriggerEvent == ETriggerEvent::Started)
					{
						Binding->Execute(ActionData);
					}
					else
					{
						Binding->Execute(ActionData);
					}

					// Keep track of the triggered actions this tick so that we can quickly look them up later when determining chorded action state
					if (BoundTriggerEvent == ETriggerEvent::Triggered)
					{
						// @TODO
						// TriggeredActionsThisTick.Add(ActionData->GetSourceAction());
					}
				}
			}
			
		}


	}


	// 在模拟Input完成后，需要把存入的InputSysytem记录的数据全部移除
	if (UCommandEnhancedInputSubsystem* InputSystem = GetWorld()->GetSubsystem<UCommandEnhancedInputSubsystem>())
	{
		InputSystem->ClearInput();
	}
}

void UCommandFrameManager::BuildInputEventMap(APlayerController* PC, TMap<const UInputAction*, TArray<TSharedPtr<FEnhancedInputActionEventBinding>>>& InputEventMap)
{
	static TArray<UInputComponent*> InputStack;
	InputStack.Reset();

	// Controlled pawn gets last dibs on the input stack
	APawn* ControlledPawn = PC->GetPawnOrSpectator();
	if (ControlledPawn)
	{
		if (ControlledPawn->InputEnabled())
		{
			// Get the explicit input component that is created upon Pawn possession. This one gets last dibs.
			if (ControlledPawn->InputComponent)
			{
				InputStack.Push(ControlledPawn->InputComponent);
			}

			// See if there is another InputComponent that was added to the Pawn's components array (possibly by script).
			for (UActorComponent* ActorComponent : ControlledPawn->GetComponents())
			{
				UInputComponent* PawnInputComponent = Cast<UInputComponent>(ActorComponent);
				if (PawnInputComponent && PawnInputComponent != ControlledPawn->InputComponent)
				{
					InputStack.Push(PawnInputComponent);
				}
			}
		}
	}


	if (PC->InputEnabled())
	{
		InputStack.Push(PC->InputComponent);
	}

	// Components pushed on to the stack get priority
	TArray<TWeakObjectPtr<UInputComponent>>& PCCurrentInputStack = PRIVATE_GET(PC, CurrentInputStack);
	for (int32 Idx = 0; Idx < PCCurrentInputStack.Num(); ++Idx)
	{
		UInputComponent* IC = PCCurrentInputStack[Idx].Get();
		if (IC)
		{
			InputStack.Push(IC);
		}
		else
		{
			PCCurrentInputStack.RemoveAt(Idx--);
		}
	}


	for (UInputComponent* IC : InputStack)
	{
		if (UCommandEnhancedInputComponent* CEInputComp = Cast<UCommandEnhancedInputComponent>(IC))
		{
			for (auto& BindingPair : CEInputComp->GetCommandInputBindings())
			{
				const TSharedPtr<FEnhancedInputActionEventBinding>& Binding = BindingPair.Value;

				InputEventMap.FindOrAdd(Binding->GetAction()).Add(Binding);
			}
		}
	}
}

void UCommandFrameManager::ServerSendDeltaNetPacket()
{
	if (IsInRewinding())
	{
		return;
	}

	for (auto& ChannelPair : NetChannels)
	{
		FCommandFrameDeltaNetPacket Packet;
		Packet.ServerCommandFrame = RealCommandFrame;

		ChannelPair.Value->ServerSend_CommandFrameDeltaNetPacket(Packet);
	}
}

uint32 UCommandFrameManager::GetCurCommandBufferNum(const FUniqueNetIdRepl& Owner)
{
	return CommandBuffer.Count(Owner);
}

void UCommandFrameManager::PostLogin(AGameModeBase* GameMode, APlayerController* PC)
{
	if (GetWorld()->GetAuthGameMode() == GameMode)
	{
		if (ACommandFrameNetChannelBase** ChannelPtr = NetChannels.Find(PC))
		{
			// 重新指向新的Owner
			(*ChannelPtr)->SetOwner(PC);

			BroadcastOnFrameNetChannelRegistered(*ChannelPtr);
		}
		else
		{
			FActorSpawnParameters Param;
			Param.Owner = PC;

			// @TODO：默认直接创建通用的ADefaultCommandFrameNetChannel，后续可以改为可配置...
			ACommandFrameNetChannelBase* Channel = GetWorld()->SpawnActor<ADefaultCommandFrameNetChannel>(Param);
			Channel->RegisterCFrameManager(this);
			Channel->SetOwner(PC);

			NetChannels.Add(PC, Channel);

			BroadcastOnFrameNetChannelRegistered(Channel);
		}
	}
}

void UCommandFrameManager::LoginOut(AGameModeBase* GameMode, AController* PC)
{
	if (GetWorld()->GetAuthGameMode() == GameMode)
	{
		if (auto ChannelPtr = NetChannels.Find(PC))
		{
			(*ChannelPtr)->Destroy();
		}

		NetChannels.Remove(PC);
	}
}

void UCommandFrameManager::RegisterClientChannel(ACommandFrameNetChannelBase* Channel)
{
	APlayerController* PC = Cast<APlayerController>(Channel->GetOwner());
	if (PC)
	{
		LocalNetChannel = Channel;
		NetChannels.Add(PC, Channel);

		BroadcastOnFrameNetChannelRegistered(Channel);
	}
}

void UCommandFrameManager::BindOnFrameNetChannelRegistered(UActorComponent* Key, FOnFrameNetChannelRegistered OnFrameNetChannelRegistered)
{
	NetChannelDelegateMap.Add(Key, OnFrameNetChannelRegistered);

	AController* Controller = CFrameUtils::GetOwnerIsController(Key->GetOwner());

	if (IsValid(Controller))
	{
		if (ACommandFrameNetChannelBase** ChannelPtr = NetChannels.Find(Controller))
		{
			BroadcastOnFrameNetChannelRegistered(*ChannelPtr);
		}
	}
}

void UCommandFrameManager::BindOnFrameNetChannelRegistered(AActor* Key, FOnFrameNetChannelRegistered OnFrameNetChannelRegistered)
{
	NetChannelDelegateMap.Add(Key, OnFrameNetChannelRegistered);

	AController* Controller = CFrameUtils::GetOwnerIsController(Key);

	if (IsValid(Controller))
	{
		if (ACommandFrameNetChannelBase** ChannelPtr = NetChannels.Find(Controller))
		{
			BroadcastOnFrameNetChannelRegistered(*ChannelPtr);
		}
	}
}

void UCommandFrameManager::UnBindOnFrameNetChannelRegistered(UObject* Key)
{
	NetChannelDelegateMap.Remove(Key);
}

void UCommandFrameManager::BroadcastOnFrameNetChannelRegistered(ACommandFrameNetChannelBase* Channel)
{
	for (auto& DelegatePair : NetChannelDelegateMap)
	{
		DelegatePair.Value.ExecuteIfBound(Channel);
	}
}

#undef LOCTEXT_NAMESPACE