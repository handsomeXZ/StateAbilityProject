#include "Net/CommandFrameNetChannel.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "CommandFrameManager.h"

#if WITH_EDITOR
#include "Debug/DebugUtils.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogCommandFrameNetChannel, Log, Verbose)

PRIVATE_DEFINE_NAMESPACE(ADefaultCommandFrameNetChannel, FBitReader, Pos)

namespace DeltaNetPacketUtils
{
	template<>
	void NetSync<EDeltaNetPacketType::Movement>(const FCommandFrameDeltaNetPacket& NetPacket, FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		if (!NetPacket.NetChannel)
		{
			return;
		}
		if (ICommandFrameNetProcedure* Procedure = NetPacket.NetChannel->GetNetPacketProcedure(EDeltaNetPacketType::Movement))
		{
			FNetProcedureSyncParam SyncParam(NetPacket, Ar, Map, bOutSuccess);
			if (Ar.IsSaving())
			{
				Procedure->OnServerNetSync(SyncParam);
			}
			else if (Ar.IsLoading())
			{
				bool bNeedRewind = false;
				Procedure->OnClientNetSync(SyncParam, bNeedRewind);
				if (bNeedRewind)
				{
					NetPacket.NetChannel->RewindFrame();
				}
			}
		}
		else
		{
			bOutSuccess = false;
		}
	}

	template<>
	void NetSync<EDeltaNetPacketType::StateAbilityScript>(const FCommandFrameDeltaNetPacket& NetPacket, FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		if (!NetPacket.NetChannel)
		{
			return;
		}
		if (ICommandFrameNetProcedure* Procedure = NetPacket.NetChannel->GetNetPacketProcedure(EDeltaNetPacketType::StateAbilityScript))
		{
			FNetProcedureSyncParam SyncParam(NetPacket, Ar, Map, bOutSuccess);
			if (Ar.IsSaving())
			{
				Procedure->OnServerNetSync(SyncParam);
			}
			else if (Ar.IsLoading())
			{
				bool bNeedRewind = false;
				Procedure->OnClientNetSync(SyncParam, bNeedRewind);
				if (bNeedRewind)
				{
					NetPacket.NetChannel->RewindFrame();
				}
			}
		}
		else
		{
			bOutSuccess = false;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

ADefaultCommandFrameNetChannel::ADefaultCommandFrameNetChannel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LastServerCommandFrame(0)
	, LastClientCommandFrame(0)
	, NetChannelState(ECommandFrameNetChannelState::Unkown)
	, CFrameManager(nullptr)
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	NetUpdateFrequency = 1;

	bNetLoadOnClient = false;
}

void ADefaultCommandFrameNetChannel::BeginPlay()
{
	if (GetWorld()->GetNetMode() == NM_Client)
	{
		CFrameManager = GetCommandFrameManager();
		check(CFrameManager);
		CFrameManager->RegisterClientChannel(this);

		CFrameManager->OnPreEndFrame.AddUObject(this, &ADefaultCommandFrameNetChannel::FixedTick);
	}
}

void ADefaultCommandFrameNetChannel::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CFrameManager->OnPreEndFrame.RemoveAll(this);
}

void ADefaultCommandFrameNetChannel::FixedTick(float DeltaTime, uint32 RCF, uint32 ICF)
{
	// 等待回滚结束才能继续处理有序的DeltaPackets数据
	if (!CFrameManager->IsInRewinding())
	{
		VerifyUnorderedPackets();

		ShrinkUnorderedPackets();
	}
}

void ADefaultCommandFrameNetChannel::ClientSend_CommandFrameInputNetPacket(FCommandFrameInputNetPacket& InputNetPacket)
{
	ServerReceive_CommandFrameInputNetPacket(InputNetPacket);
}

void ADefaultCommandFrameNetChannel::ServerSend_CommandFrameDeltaNetPacket(FCommandFrameDeltaNetPacket& DeltaNetPacket)
{
	DeltaNetPacket.PrevServerCommandFrame = LastServerCommandFrame;
	DeltaNetPacket.NetChannel = this;

	LastServerCommandFrame = DeltaNetPacket.ServerCommandFrame;

	ClientReceive_CommandFrameDeltaNetPacket(DeltaNetPacket);
}

ICommandFrameNetProcedure* ADefaultCommandFrameNetChannel::GetNetPacketProcedure(EDeltaNetPacketType NetPacketType)
{
	TWeakObjectPtr<UObject> WeakProcedure = NetPacketProcedures.FindOrAdd(NetPacketType);
	if (WeakProcedure.IsValid())
	{
		// GetClass()->ImplementsInterface(UCommandFrameNetProcedure::StaticClass())
		return (ICommandFrameNetProcedure*)(WeakProcedure->GetNativeInterfaceAddress(UCommandFrameNetProcedure::StaticClass()));
	}

	return nullptr;
}

void ADefaultCommandFrameNetChannel::RegisterNetPacketProcedure(EDeltaNetPacketType NetPacketType, UObject* Procedure)
{
	if (IsValid(Procedure))
	{
		NetPacketProcedures.FindOrAdd(NetPacketType) = Procedure;
	}
}
//////////////////////////////////////////////////////////////////////////

void ADefaultCommandFrameNetChannel::ServerReceive_CommandFrameInputNetPacket_Implementation(const FCommandFrameInputNetPacket& InputNetPacket)
{
	if (IsValid(GetCommandFrameManager()))
	{
		UE_LOG(LogCommandFrameNetChannel, Verbose, TEXT("CommandFrameNetChannel[%s] has Received Input"), *GetName());

		APlayerController* PC = GetOwner<APlayerController>();
		APlayerState* PS = PC->GetPlayerState<APlayerState>();

		GetCommandFrameManager()->ReceiveInput(this, PS->GetUniqueId(), InputNetPacket);

		// 异常情况处理
		// 1. 客户端CF过期，且BufferNum == 0。此时需要同步所有状态。
		// 2. 客户端CF异常领先？暂时不处理，不好判断是否异常。
		uint32 RCF = GetCommandFrameManager()->GetRCF();
		uint32 CommandBufferNum = GetCommandFrameManager()->GetCurCommandBufferNum(PS->GetUniqueId());
		if (NetChannelState == ECommandFrameNetChannelState::Normal && InputNetPacket.ClientCommandFrame < RCF && CommandBufferNum == 0)
		{
			UE_LOG(LogCommandFrameNetChannel, Warning, TEXT("Server Wait Client Catch!!"));

			NetChannelState = ECommandFrameNetChannelState::WaitCatch;

			FCommandFrameDeltaNetPacket Packet;
			Packet.ServerCommandFrame = RCF;

			DeltaNetPacketUtils::BuildPacket_Fault_FrameExpiry(Packet);
			// @TODO：全量同步

			ServerSend_CommandFrameDeltaNetPacket(Packet);
		}
		else if (NetChannelState == ECommandFrameNetChannelState::WaitCatch && InputNetPacket.ClientCommandFrame > RCF && CommandBufferNum)
		{
			NetChannelState = ECommandFrameNetChannelState::Normal;
		}
	}
	else
	{
		UE_LOG(LogCommandFrameNetChannel, Warning, TEXT("CommandFrameNetChannel[%s] has discard input for invalid CFrameManager"), *GetName());
	}
}

void ADefaultCommandFrameNetChannel::ClientReceive_CommandFrameDeltaNetPacket_Implementation(const FCommandFrameDeltaNetPacket& DeltaNetPacket)
{
	if (GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UE_LOG(LogCommandFrameNetChannel, Verbose, TEXT("ClientReceive SCF[%d] Prev[%d] LocalLast[%d]"), DeltaNetPacket.ServerCommandFrame, DeltaNetPacket.PrevServerCommandFrame, LastServerCommandFrame);
	
	FNetBitReader NetBitReader;
	NetBitReader.PackageMap = DeltaNetPacket.PackageMap;
	NetBitReader.SetData((uint8*)DeltaNetPacket.RawData.GetData(), DeltaNetPacket.RawData.Num());

	ProcessDeltaPrefix(DeltaNetPacket, NetBitReader);

	// 本地第0帧为无效帧，无条件接受
	if (LastServerCommandFrame < DeltaNetPacket.PrevServerCommandFrame && LastServerCommandFrame != 0)
	{
		// 乱序了，提前到达。加入待处理队列。
		UnorderedPackets.Add(DeltaNetPacket.PrevServerCommandFrame, DeltaNetPacket);
	}
	else if(LastServerCommandFrame == DeltaNetPacket.PrevServerCommandFrame || LastServerCommandFrame == 0)
	{
		// ProcessDeltaPackaged(DeltaNetPacket, NetBitReader);

		// 即使是没乱序的包，也需要入队列，这样可以避免在接收RPC的处理时间过长。可以参考Iris
		UnorderedPackets.Add(DeltaNetPacket.PrevServerCommandFrame, DeltaNetPacket);
	}
	else
	{
		// 重复的包
	}
}

void ADefaultCommandFrameNetChannel::ProcessDeltaPrefix(const FCommandFrameDeltaNetPacket& DeltaNetPacket, FNetBitReader& NetBitReader)
{
	if (DeltaNetPacket.bLocal && IsValid(GetCommandFrameManager()))
	{
		bool bOutSuccess = true;
		auto Func = [&DeltaNetPacket, this](int32 PrefixDataSize, uint32 ServerCommandBufferNum, bool bFault) {
			uint32 RealCommandFrame = GetCommandFrameManager()->GetRCF();
			if (RealCommandFrame <= DeltaNetPacket.ServerCommandFrame)
			{
				// 严重落后，直接修改帧号
				ResetCommandFrame(DeltaNetPacket.ServerCommandFrame, DeltaNetPacket.PrevServerCommandFrame);

				return;
			}

			GetCommandFrameManager()->UpdateTimeDilationHelper(ServerCommandBufferNum, bFault);
		};

		DeltaNetPacketUtils::SerializeDeltaPrefix(DeltaNetPacket, NetBitReader, NetBitReader.PackageMap, bOutSuccess, Func);
	}

	PRIVATE_GET_NAMESPACE(ADefaultCommandFrameNetChannel, &NetBitReader, Pos) = 0;
}

void ADefaultCommandFrameNetChannel::ProcessDeltaPackaged(FCommandFrameDeltaNetPacket& DeltaNetPacket, FNetBitReader& NetBitReader)
{
	UE_LOG(LogCommandFrameNetChannel, Verbose, TEXT("Change LocalLast From[%d] To[%d]"), LastServerCommandFrame, DeltaNetPacket.ServerCommandFrame);
	LastServerCommandFrame = DeltaNetPacket.ServerCommandFrame;
	
	// 处理数据
	if (DeltaNetPacket.bLocal && IsValid(GetCommandFrameManager()))
	{
		UE_LOG(LogCommandFrameNetChannel, Verbose, TEXT("ClientReceiveCommandAck[%d]"), DeltaNetPacket.ServerCommandFrame);
		GetCommandFrameManager()->ClientReceiveCommandAck(DeltaNetPacket.ServerCommandFrame);
	}

	bool bOutSuccess = true;
	int32 OutPrefixDataSize = 0;
	DeltaNetPacketUtils::SerializeDeltaPrefix(DeltaNetPacket, NetBitReader, NetBitReader.PackageMap, bOutSuccess, [&OutPrefixDataSize](int32 PrefixDataSize, uint32 ServerCommandBufferNum, bool bFault) {
		OutPrefixDataSize = PrefixDataSize;
	});
	//PRIVATE_GET_NAMESPACE(ADefaultCommandFrameNetChannel, &NetBitReader, Pos) = OutPrefixDataSize;

	DeltaNetPacketUtils::SerializeDeltaPackaged(DeltaNetPacket, NetBitReader, NetBitReader.PackageMap, bOutSuccess);
	if (NetChannelState == ECommandFrameNetChannelState::WaitRewind)
	{
		// Rewinding...
		for (auto& ProcedurePair : NetPacketProcedures)
		{
			if (ProcedurePair.Value.IsValid())
			{
				ICommandFrameNetProcedure* NetProcedure = (ICommandFrameNetProcedure*)(ProcedurePair.Value->GetNativeInterfaceAddress(UCommandFrameNetProcedure::StaticClass()));
				NetProcedure->OnClientRewind();
			}
		}

		CFrameManager->ReplayFrames(DeltaNetPacket.ServerCommandFrame);
	}

	NetChannelState = ECommandFrameNetChannelState::Normal;

	// 处理乱序包
	RemoveUnorderedPacket(DeltaNetPacket.PrevServerCommandFrame);
	VerifyUnorderedPackets();
}

void ADefaultCommandFrameNetChannel::VerifyUnorderedPackets()
{
	while (FCommandFrameDeltaNetPacket* PacketPtr = UnorderedPackets.Find(LastServerCommandFrame))
	{
		FNetBitReader NetBitReader;
		NetBitReader.SetData((uint8*)PacketPtr->RawData.GetData(), PacketPtr->RawData.Num());
		ProcessDeltaPackaged(*PacketPtr, NetBitReader);
	}
}

void ADefaultCommandFrameNetChannel::ShrinkUnorderedPackets()
{
	for (auto It = UnorderedPackets.CreateIterator(); It; ++It)
	{
		if (It->Key < LastServerCommandFrame)
		{
			It.RemoveCurrent();
		}
	}
}

void ADefaultCommandFrameNetChannel::RemoveUnorderedPacket(uint32 CommandFrame)
{
	UnorderedPackets.Remove(CommandFrame);
}

void ADefaultCommandFrameNetChannel::ResetCommandFrame(uint32 ServerCommandFrame, uint32 PrevServerCommandFrame)
{
	CFrameManager->ResetCommandFrame(ServerCommandFrame);
	LastServerCommandFrame = PrevServerCommandFrame;

	for (auto It = UnorderedPackets.CreateIterator(); It; ++It)
	{
		if (It->Key <= ServerCommandFrame)
		{
			It.RemoveCurrent();
		}
	}

	// 需要尝试赶上服务器的进度
	CFrameManager->UpdateTimeDilationHelper(0, true);
}

UCommandFrameManager* ADefaultCommandFrameNetChannel::GetCommandFrameManager()
{
	if (!IsValid(CFrameManager))
	{
		NetChannelState = ECommandFrameNetChannelState::Normal;
		CFrameManager = GetWorld()->GetSubsystem<UCommandFrameManager>();
	}

	return CFrameManager;
}