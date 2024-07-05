#include "Net/CommandFrameNetChannel.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "CommandFrameManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCommandFrameNetChannel, Log, Verbose)

PRIVATE_DEFINE_NAMESPACE(ACommandFrameNetChannel, FBitReader, Pos)

ACommandFrameNetChannel::ACommandFrameNetChannel(const FObjectInitializer& ObjectInitializer)
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

void ACommandFrameNetChannel::BeginPlay()
{
	if (GetWorld()->GetNetMode() == NM_Client)
	{
		CFrameManager = GetCommandFrameManager();
		check(CFrameManager);
		CFrameManager->RegisterClientChannel(this);

		CFrameManager->OnEndFrame.AddUObject(this, &ACommandFrameNetChannel::FixedTick);
	}
}

void ACommandFrameNetChannel::FixedTick(float DeltaTime, uint32 RCF, uint32 ICF)
{
	VerifyUnorderedPackets();

	ShrinkUnorderedPackets();
}

void ACommandFrameNetChannel::ClientSend_CommandFrameInputNetPacket(FCommandFrameInputNetPacket& InputNetPacket)
{
	ServerReceive_CommandFrameInputNetPacket(InputNetPacket);
}

void ACommandFrameNetChannel::ServerSend_CommandFrameDeltaNetPacket(FCommandFrameDeltaNetPacket& DeltaNetPacket)
{
	DeltaNetPacket.PrevServerCommandFrame = LastServerCommandFrame;
	DeltaNetPacket.Channel = this;

	LastServerCommandFrame = DeltaNetPacket.ServerCommandFrame;

	ClientReceive_CommandFrameDeltaNetPacket(DeltaNetPacket);
}

void ACommandFrameNetChannel::ServerReceive_CommandFrameInputNetPacket_Implementation(const FCommandFrameInputNetPacket& InputNetPacket)
{
	if (IsValid(GetCommandFrameManager()))
	{
		UE_LOG(LogCommandFrameNetChannel, Verbose, TEXT("CommandFrameNetChannel[%s] has Received Input"), *GetName());

		APlayerController* PC = GetOwner<APlayerController>();
		APlayerState* PS = PC->GetPlayerState<APlayerState>();

		GetCommandFrameManager()->ReceiveInput(PS->GetUniqueId(), InputNetPacket);

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

void ACommandFrameNetChannel::ClientReceive_CommandFrameDeltaNetPacket_Implementation(const FCommandFrameDeltaNetPacket& DeltaNetPacket)
{
	if (GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UE_LOG(LogCommandFrameNetChannel, Verbose, TEXT("ClientReceive SCF[%d] Prev[%d] LocalLast[%d]"), DeltaNetPacket.ServerCommandFrame, DeltaNetPacket.PrevServerCommandFrame, LastServerCommandFrame);
	
	FBitReader BitReader;
	BitReader.SetData((uint8*)DeltaNetPacket.RawData.GetData(), DeltaNetPacket.RawData.Num());

	ProcessDeltaPrefix(DeltaNetPacket, BitReader);

	// 本地第0帧为无效帧，无条件接受
	if (LastServerCommandFrame < DeltaNetPacket.PrevServerCommandFrame && LastServerCommandFrame != 0)
	{
		// 乱序了，提前到达。加入待处理队列。
		UnorderedPackets.Add(DeltaNetPacket.PrevServerCommandFrame, DeltaNetPacket);
	}
	else if(LastServerCommandFrame == DeltaNetPacket.PrevServerCommandFrame || LastServerCommandFrame == 0)
	{
		ProcessDeltaPackaged(DeltaNetPacket, BitReader);

	}
	else
	{
		// 重复的包
	}
}

void ACommandFrameNetChannel::ProcessDeltaPrefix(const FCommandFrameDeltaNetPacket& DeltaNetPacket, FBitReader& BitReader)
{
	if (DeltaNetPacket.bLocal && IsValid(GetCommandFrameManager()))
	{
		bool bFault = false;
		uint32 ServerCommandBufferNum = 0;

		BitReader << bFault;
		BitReader << ServerCommandBufferNum;

		uint32 RealCommandFrame = GetCommandFrameManager()->GetRCF();
		if (RealCommandFrame <= DeltaNetPacket.ServerCommandFrame)
		{
			// 严重落后，直接修改帧号
			ResetCommandFrame(DeltaNetPacket.ServerCommandFrame, DeltaNetPacket.PrevServerCommandFrame);

			return;
		}

		GetCommandFrameManager()->UpdateTimeDilationHelper(ServerCommandBufferNum, bFault);
	}

	PRIVATE_GET_NAMESPACE(ACommandFrameNetChannel, &BitReader, Pos) = 0;
}

void ACommandFrameNetChannel::ProcessDeltaPackaged(const FCommandFrameDeltaNetPacket& DeltaNetPacket, FBitReader& BitReader)
{
	UE_LOG(LogCommandFrameNetChannel, Verbose, TEXT("Change LocalLast From[%d] To[%d]"), LastServerCommandFrame, DeltaNetPacket.ServerCommandFrame);
	LastServerCommandFrame = DeltaNetPacket.ServerCommandFrame;
	
	// 处理数据
	if (DeltaNetPacket.bLocal && IsValid(GetCommandFrameManager()))
	{
		UE_LOG(LogCommandFrameNetChannel, Verbose, TEXT("ClientReceiveCommandAck[%d]"), DeltaNetPacket.ServerCommandFrame);
		GetCommandFrameManager()->ClientReceiveCommandAck(DeltaNetPacket.ServerCommandFrame);
	}


	// 处理乱序包
	RemoveUnorderedPacket(DeltaNetPacket.PrevServerCommandFrame);
	VerifyUnorderedPackets();
}

void ACommandFrameNetChannel::VerifyUnorderedPackets()
{
	while (FCommandFrameDeltaNetPacket* PacketPtr = UnorderedPackets.Find(LastServerCommandFrame))
	{
		FBitReader BitReader;
		BitReader.SetData((uint8*)PacketPtr->RawData.GetData(), PacketPtr->RawData.Num());
		ProcessDeltaPackaged(*PacketPtr, BitReader);
	}
}

void ACommandFrameNetChannel::ShrinkUnorderedPackets()
{
	for (auto It = UnorderedPackets.CreateIterator(); It; ++It)
	{
		if (It->Key < LastServerCommandFrame)
		{
			It.RemoveCurrent();
		}
	}
}

void ACommandFrameNetChannel::RemoveUnorderedPacket(uint32 CommandFrame)
{
	UnorderedPackets.Remove(CommandFrame);
}

void ACommandFrameNetChannel::ResetCommandFrame(uint32 ServerCommandFrame, uint32 PrevServerCommandFrame)
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

UCommandFrameManager* ACommandFrameNetChannel::GetCommandFrameManager()
{
	if (!IsValid(CFrameManager))
	{
		NetChannelState = ECommandFrameNetChannelState::Normal;
		CFrameManager = GetWorld()->GetSubsystem<UCommandFrameManager>();
	}

	return CFrameManager;
}