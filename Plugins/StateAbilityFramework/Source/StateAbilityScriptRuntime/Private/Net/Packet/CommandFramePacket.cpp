#include "Net/Packet/CommandFramePacket.h"

#include "Engine/PackageMapClient.h"
#include "Engine/NetConnection.h"
#include "GameFramework/PlayerState.h"

#include "Buffer/BufferTypes.h"
#include "CommandFrameManager.h"
#include "Net/Packet/CommandFrameInput.h"
#include "Net/CommandFrameNetTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogCommandFrameNetPacket, Log, Verbose)

PRIVATE_DEFINE_NAMESPACE(FCommandFrameInputNetPacket, FBitReader, Pos)

struct FBitArchiveSizeScope
{
	FBitArchiveSizeScope(FArchive& Ar, int32& InSerialSize)
		: Archive(Ar)
		, SerialSize(InSerialSize)
	{
		if (Ar.IsSaving())
		{
			FBitWriter& BitWriter = static_cast<FBitWriter&>(Ar);
			HeadOffset = BitWriter.GetNumBits();
		}
		else if (Ar.IsLoading())
		{
			FBitReader& BitReader = static_cast<FBitReader&>(Ar);
			HeadOffset = BitReader.GetNumBits();
		}

		Ar << SerialSize;

		if (Ar.IsSaving())
		{
			FBitWriter& BitWriter = static_cast<FBitWriter&>(Ar);

			InitialOffset = BitWriter.GetNumBits();
		}
	}
	~FBitArchiveSizeScope()
	{
		if (Archive.IsSaving())
		{
			FBitWriter& BitWriter = static_cast<FBitWriter&>(Archive);

			const int64 FinalOffset = BitWriter.GetNumBits();

			int32* Int32Data = (int32*)(BitWriter.GetData() + ((HeadOffset + 7) >> 3));	// 位偏移 -> 字节偏移，同时覆写占位数据
			SerialSize = (int32)(FinalOffset - InitialOffset);
			*Int32Data = SerialSize;
		}
	}

private:
	int64 HeadOffset;
	int64 InitialOffset;
	FArchive& Archive;
	int32& SerialSize;
};

namespace DeltaNetPacketUtils
{
	void BuildPacket_Fault_FrameExpiry(FCommandFrameDeltaNetPacket& Packet)
	{
		Packet.PacketType = (EDeltaNetPacketType)(Packet.PacketType | EDeltaNetPacketType::Fault_FrameExpiry);
	}

	void SerializeDeltaPrefix(const FCommandFrameDeltaNetPacket& NetPacket, FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess, FPrefixCallBackFunc CallBack)
	{
		int32 PrefixDataSize = 0;
		uint32 ServerCommandBufferNum = 0;
		bool bFault = false;

		// 非Local暂时没有Prefix
		if (!NetPacket.bLocal)
		{
			return;
		}

		if (Ar.IsSaving())
		{
			bFault = NetPacket.PacketType & 0xF0000000;

			// PrefixDataSize并不包含自身
			FBitArchiveSizeScope Scope(Ar, PrefixDataSize);
			
			UCommandFrameManager* CFManager = NetPacket.NetChannel->GetWorld()->GetSubsystem<UCommandFrameManager>();
			APlayerController* PC = Cast<APlayerController>(NetPacket.NetChannel->GetNetConnection()->OwningActor);
			APlayerState* PS = PC->GetPlayerState<APlayerState>();

			ServerCommandBufferNum = CFManager->GetCurCommandBufferNum(PS->GetUniqueId());

			Ar << ServerCommandBufferNum;
			Ar << bFault;

			if (CallBack)
			{
				CallBack(PrefixDataSize, ServerCommandBufferNum, bFault);
			}
		}
		else if (Ar.IsLoading())
		{
			Ar << PrefixDataSize;
			Ar << ServerCommandBufferNum;
			Ar << bFault;

			if (CallBack)
			{
				CallBack(PrefixDataSize, ServerCommandBufferNum, bFault);
			}
		}
	}

	void SerializeDeltaPackaged(FCommandFrameDeltaNetPacket& NetPacket, FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		bOutSuccess = true;

		if (Ar.IsSaving())
		{
			bool bNetSyncSuccess = false;
			DeltaNetPacketUtils::NetSync<EDeltaNetPacketType::Movement>(NetPacket, Ar, Map, bNetSyncSuccess);
			if (bNetSyncSuccess)
			{
				NetPacket.PacketType = (EDeltaNetPacketType)(NetPacket.PacketType | EDeltaNetPacketType::Movement);
			}

			bNetSyncSuccess = false;
			DeltaNetPacketUtils::NetSync<EDeltaNetPacketType::StateAbilityScript>(NetPacket, Ar, Map, bNetSyncSuccess);
			if (bNetSyncSuccess)
			{
				NetPacket.PacketType = (EDeltaNetPacketType)(NetPacket.PacketType | EDeltaNetPacketType::StateAbilityScript);
			}

			bNetSyncSuccess = false;
			DeltaNetPacketUtils::NetSync<EDeltaNetPacketType::Fault_FrameExpiry>(NetPacket, Ar, Map, bNetSyncSuccess);
			if (bNetSyncSuccess)
			{
				NetPacket.PacketType = (EDeltaNetPacketType)(NetPacket.PacketType | EDeltaNetPacketType::Fault_FrameExpiry);
			}
		}
		else if (Ar.IsLoading())
		{
			if (NetPacket.PacketType & EDeltaNetPacketType::Movement)
			{
				DeltaNetPacketUtils::NetSync<EDeltaNetPacketType::Movement>(NetPacket, Ar, Map, bOutSuccess);
				if (!bOutSuccess)
				{
					return;
				}
			}

			if (NetPacket.PacketType & EDeltaNetPacketType::StateAbilityScript)
			{
				DeltaNetPacketUtils::NetSync<EDeltaNetPacketType::StateAbilityScript>(NetPacket, Ar, Map, bOutSuccess);
				if (!bOutSuccess)
				{
					return;
				}
			}

			if (NetPacket.PacketType & EDeltaNetPacketType::Fault_FrameExpiry)
			{
				DeltaNetPacketUtils::NetSync<EDeltaNetPacketType::Fault_FrameExpiry>(NetPacket, Ar, Map, bOutSuccess);
				if (!bOutSuccess)
				{
					return;
				}
			}
		}


	}

	template<>
	void NetSync<EDeltaNetPacketType::Fault_FrameExpiry>(const FCommandFrameDeltaNetPacket& NetPacket, FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		if (NetPacket.bLocal)
		{
			
		}
	}


}

//////////////////////////////////////////////////////////////////////////
// FCommandFrameDeltaNetPacket
FCommandFrameDeltaNetPacket::FCommandFrameDeltaNetPacket()
	: ServerCommandFrame(0)
	, PrevServerCommandFrame(0)
	, NetChannel(nullptr)
	, PacketType(EDeltaNetPacketType::None)
{

}

FCommandFrameDeltaNetPacket::FCommandFrameDeltaNetPacket(uint32 InServerCommandFrame, uint32 InPrevServerCommandFrame, EDeltaNetPacketType InPacketType, ACommandFrameNetChannelBase* InNetChannel)
	: ServerCommandFrame(InServerCommandFrame)
	, PrevServerCommandFrame(InPrevServerCommandFrame)
	, NetChannel(InNetChannel)
	, PacketType(InPacketType)
{

}

bool FCommandFrameDeltaNetPacket::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	if (Ar.IsSaving())
	{
		PackageMap = Map;

		UPackageMapClient* PackageMapClient = Cast<UPackageMapClient>(Map);
		UNetConnection* NetConnection = PackageMapClient->GetConnection();

		UNetConnection* OwnerConnection = NetChannel->GetNetConnection();
		bLocal = NetConnection == OwnerConnection;
	}
	else if (Ar.IsLoading())
	{
		PackageMap = Map;
	}

	Ar << ServerCommandFrame;
	Ar << PrevServerCommandFrame;
	Ar << NetChannel;

	Ar << bLocal;

	if (Ar.IsSaving())
	{
		FNetBitWriter NetBitWriter;
		NetBitWriter.PackageMap = PackageMap;
		NetBitWriter.SetAllowResize(true);

		DeltaNetPacketUtils::SerializeDeltaPrefix(*this, NetBitWriter, Map, bOutSuccess);

		if (RawData.IsEmpty())
		{
			bOutSuccess = true;

			DeltaNetPacketUtils::SerializeDeltaPackaged(*this, NetBitWriter, Map, bOutSuccess);
		}

		if (!bOutSuccess)
		{
			return false;
		}

		RawData.SetNumUninitialized(NetBitWriter.GetNumBits());

		check(RawData.Num() >= NetBitWriter.GetNumBits());
		FMemory::Memcpy(RawData.GetData(), NetBitWriter.GetData(), NetBitWriter.GetNumBytes());
	}

	uint32 IntPacketType = uint32(PacketType);
	Ar << IntPacketType;
	if (Ar.IsLoading())
	{
		PacketType = EDeltaNetPacketType(IntPacketType);
	}

	// Array size in bits, using minimal number of bytes to write it out.
	uint32 NumBits = RawData.Num();
	Ar.SerializeIntPacked(NumBits);
	if (Ar.IsLoading())
	{
		RawData.Init(0, NumBits);
	}
	// Array data
	if (NumBits)
	{
		Ar.SerializeBits(RawData.GetData(), NumBits);
	}

	if (Ar.IsSaving() && bLocal)
	{
		// 避免Local目标的序列化影响后续的Remote序列化
		RawData.Empty(0);
	}

	bOutSuccess = true;
	return !Ar.IsError();
}



//////////////////////////////////////////////////////////////////////////
// FCommandFrameInputNetPacket
FCommandFrameInputNetPacket::FCommandFrameInputNetPacket(UNetConnection* Connection, TCircularQueueView<FCommandFrameInputFrame>& InputFrames)
	: ClientCommandFrame(0)
{
	WriteRedundantData(Connection, InputFrames);
}

bool FCommandFrameInputNetPacket::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	Ar << ClientCommandFrame;

	// Array size in bits, using minimal number of bytes to write it out.
	uint32 NumBits = RawData.Num();
	Ar.SerializeIntPacked(NumBits);

	if (Ar.IsLoading())
	{
		RawData.Init(0, NumBits);
	}

	// Array data
	Ar.SerializeBits(RawData.GetData(), NumBits);

	bOutSuccess = true;
	return !Ar.IsError();
}

void FCommandFrameInputNetPacket::WriteRedundantData(UNetConnection* Connection, TCircularQueueView<FCommandFrameInputFrame>& InputFrames)
{
	if (!InputFrames.IsEmpty())
	{
		ClientCommandFrame = InputFrames[InputFrames.Num() - 1].CommandFrame;
	}

	// FNetBitWriter
	RawData.Empty();

	UE_LOG(LogCommandFrameNetPacket, Verbose, TEXT("WriteRedundantData Begin"));

	UE_LOG(LogCommandFrameNetPacket, Log, TEXT("WriteRedundantData FramesCount[%d]"), InputFrames.Num());

	int64 TotalNumBits = 0;
	for (int32 Index = InputFrames.Num() - 1; Index >= 0; --Index)
	{
		FCommandFrameInputFrame& InputFrame = InputFrames[Index];

		FNetBitWriter& NetBitWriter = InputFrame.SharedSerialization;
		NetBitWriter.PackageMap = Connection->PackageMap;

		if (!NetBitWriter.GetNumBits())
		{
			NetBitWriter << InputFrame.CommandFrame;

			int32 DataCount = InputFrame.InputQueue.Num();

			NetBitWriter << DataCount;

			int32 DataSize = 0;
			{
				FBitArchiveSizeScope Scope(NetBitWriter, DataSize);

				// GetTypeSize() 返回字节数，实际需要bit数，所以*8
				// 但FCommandFrameInputAtom目前内部包含UObject，不能直接拷贝，只有启用了FNetworkGUID，才能直接拷贝
				//NetBitWriter.SerializeBits(InputFrame.InputQueue.GetData(), InputFrame.InputQueue.GetTypeSize() * DataCount * 8);
				
				bool bOutSuccess = false;
				UScriptStruct* InputAtomStruct = FCommandFrameInputAtom::StaticStruct();
				for (FCommandFrameInputAtom& InputAtom : InputFrame.InputQueue)
				{
					InputAtomStruct->GetCppStructOps()->NetSerialize(NetBitWriter, NetBitWriter.PackageMap, bOutSuccess, &InputAtom);
				}
			}

			UE_LOG(LogCommandFrameNetPacket, Log, TEXT("WriteRedundantData Frame[%d] DataCount[%d] DataSize[%d]"), InputFrame.CommandFrame, DataCount, DataSize);
		}

		UE_LOG(LogCommandFrameNetPacket, Log, TEXT("WriteRedundantData Frame[%d] SharedSerialization NumBits[%d]"), InputFrame.CommandFrame, NetBitWriter.GetNumBits());

		TotalNumBits += NetBitWriter.GetNumBits();
	}

	UE_LOG(LogCommandFrameNetPacket, Log, TEXT("WriteRedundantData Allocate RawData[%d]"), TotalNumBits);
	RawData.SetNumUninitialized(TotalNumBits);

	check(RawData.Num() >= TotalNumBits);

	uint8* DataStream = (uint8*)RawData.GetData();
	for (int32 Index = InputFrames.Num() - 1; Index >= 0; --Index)
	{
		FCommandFrameInputFrame& InputFrame = InputFrames[Index];

		FBitWriter& BitWriter = InputFrame.SharedSerialization;
		int64 NumBytes = BitWriter.GetNumBytes();
		FMemory::Memcpy(DataStream, BitWriter.GetData(), NumBytes);

		DataStream += NumBytes;
	}

	UE_LOG(LogCommandFrameNetPacket, Verbose, TEXT("WriteRedundantData End"));
}

void FCommandFrameInputNetPacket::ReadRedundantData(UNetConnection* Connection, TFunction<uint8*(uint32 CommandFrame, int32 DataCount)> AllocateData) const
{
	FNetBitReader Ar;
	Ar.PackageMap = Connection->PackageMap;
	Ar.SetData((uint8*)RawData.GetData(), RawData.Num());

	bool bPaused = false;

	UE_LOG(LogCommandFrameNetPacket, Verbose, TEXT("ReadRedundantData Begin"));
	UE_LOG(LogCommandFrameNetPacket, Verbose, TEXT("ReadRedundantData DataTotalSize[%d]"), RawData.Num());

	while (!Ar.AtEnd() && !bPaused)
	{
		uint32 CommandFrame;
		int32 DataCount;
		int32 DataSize;

		Ar << CommandFrame;
		Ar << DataCount;

		FBitArchiveSizeScope Scope(Ar, DataSize);

		UE_LOG(LogCommandFrameNetPacket, Log, TEXT("ReadRedundantData CF[%d] DataCount[%d] DataSize[%d]"), CommandFrame, DataCount, DataSize);

		FCommandFrameInputAtom* InputAtomData = (FCommandFrameInputAtom*)AllocateData(CommandFrame, DataCount);
		if (InputAtomData)
		{
			bool bOutSuccess = false;
			UScriptStruct* InputAtomStruct = FCommandFrameInputAtom::StaticStruct();
			for (int32 index = 0; index < DataCount; ++index)
			{
				InputAtomStruct->GetCppStructOps()->NetSerialize(Ar, Ar.PackageMap, bOutSuccess, InputAtomData + index);
			}
		}
		else
		{
			PRIVATE_GET_NAMESPACE(FCommandFrameInputNetPacket, &Ar, Pos) = Ar.GetPosBits() + DataSize;
		}
		
	}

	UE_LOG(LogCommandFrameNetPacket, Verbose, TEXT("ReadRedundantData End"));
}