#include "Net/Packet/CommandFramePacket.h"

#include "Engine/PackageMapClient.h"
#include "Engine/NetConnection.h"

#include "Buffer/BufferTypes.h"
#include "Net/Packet/CommandFrameInput.h"
#include "CommandFrameManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCommandFrameNetPacket, Log, All)

PRIVATE_DEFINE_NAMESPACE(FBitReader, Pos, FCommandFrameInputNetPacket)

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
		Packet.PacketType = (EDeltaNetPacketType)(Packet.PacketType & EDeltaNetPacketType::Fault_FrameExpiry);
	}

	template<>
	bool NetSerialize<EDeltaNetPacketType::Fault_FrameExpiry>(FCommandFrameDeltaNetPacket& Packet, bool bLocal)
	{
		if (bLocal)
		{
			
		}

		return true;
	}

}

//////////////////////////////////////////////////////////////////////////
// FCommandFrameDeltaNetPacket
FCommandFrameDeltaNetPacket::FCommandFrameDeltaNetPacket(uint32 InServerCommandFrame, uint32 InPrevServerCommandFrame, EDeltaNetPacketType InPacketType, ACommandFrameNetChannel* InChannel)
	: ServerCommandFrame(InServerCommandFrame)
	, PrevServerCommandFrame(InPrevServerCommandFrame)
	, Channel(InChannel)
	, PacketType(InPacketType)
{

}

bool FCommandFrameDeltaNetPacket::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	if (Ar.IsSaving())
	{
		UPackageMapClient* PackageMapClient = Cast<UPackageMapClient>(Map);
		UNetConnection* NetConnection = PackageMapClient->GetConnection();

		UNetConnection* OwnerConnection = Channel->GetNetConnection();
		bLocal = NetConnection == OwnerConnection;
	}

	Ar << ServerCommandFrame;
	Ar << PrevServerCommandFrame;
	Ar << Channel;

	uint32 IntPacketType = uint32(PacketType);
	Ar << IntPacketType;

	if (Ar.IsLoading())
	{
		PacketType = EDeltaNetPacketType(IntPacketType);
	}

	Ar << bLocal;

	if (bLocal)
	{
		if (Ar.IsSaving())
		{
			bool bFault = PacketType & 0xF0000000;
			uint32 ServerCommandBufferNum = 0;

			FBitWriter BitWriter;
			BitWriter.SetAllowResize(true);

			UCommandFrameManager* CFManager = Channel->GetWorld()->GetSubsystem<UCommandFrameManager>();
			APlayerController* PC = Cast<APlayerController>(Channel->GetNetConnection()->OwningActor);
			APlayerState* PS = PC->GetPlayerState<APlayerState>();

			ServerCommandBufferNum = CFManager->GetCurCommandBufferNum(PS->GetUniqueId());

			BitWriter << bFault;
			BitWriter << ServerCommandBufferNum;

			RawData.SetNumUninitialized(BitWriter.GetNumBits());

			check(RawData.Num() >= BitWriter.GetNumBits());
			FMemory::Memcpy(RawData.GetData(), BitWriter.GetData(), BitWriter.GetNumBytes());
		}
	}
	else
	{

	}

	if (RawData.IsEmpty())
	{
		if (PacketType & EDeltaNetPacketType::Movement)
		{

		}

		if (PacketType & EDeltaNetPacketType::StateAbilityScript)
		{

		}

		if (PacketType & EDeltaNetPacketType::Fault_FrameExpiry)
		{
			if (!DeltaNetPacketUtils::NetSerialize<EDeltaNetPacketType::Fault_FrameExpiry>(*this, bLocal))
			{
				bOutSuccess = false;
				return false;
			}
		}
	}

	Ar << ObjectPool;


	// Array size in bits, using minimal number of bytes to write it out.
	uint32 NumBits = RawData.Num();
	Ar.SerializeIntPacked(NumBits);
	if (Ar.IsLoading())
	{
		RawData.Init(0, NumBits);
	}
	// Array data
	Ar.SerializeBits(RawData.GetData(), NumBits);

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
FCommandFrameInputNetPacket::FCommandFrameInputNetPacket(TCircularQueueView<FCommandFrameInputFrame>& InputFrames)
	: ClientCommandFrame(0)
{
	WriteRedundantData(InputFrames);
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

void FCommandFrameInputNetPacket::WriteRedundantData(TCircularQueueView<FCommandFrameInputFrame>& InputFrames)
{
	if (!InputFrames.IsEmpty())
	{
		ClientCommandFrame = InputFrames[InputFrames.Num() - 1].CommandFrame;
	}

	// FNetBitWriter
	RawData.Empty();

	UE_LOG(LogCommandFrameNetPacket, Log, TEXT("WriteRedundantData Begin"));

	UE_LOG(LogCommandFrameNetPacket, Log, TEXT("WriteRedundantData FramesCount[%d]"), InputFrames.Num());

	int64 TotalNumBits = 0;
	for (int32 Index = InputFrames.Num() - 1; Index >= 0; --Index)
	{
		FCommandFrameInputFrame& InputFrame = InputFrames[Index];

		FBitWriter& BitWriter = InputFrame.SharedSerialization;

		if (!BitWriter.GetNumBits())
		{
			BitWriter << InputFrame.CommandFrame;

			int32 DataCount = InputFrame.InputQueue.Num();

			BitWriter << DataCount;

			int32 DataSize = 0;
			{
				FBitArchiveSizeScope Scope(BitWriter, DataSize);

				BitWriter.SerializeBits(InputFrame.InputQueue.GetData(), InputFrame.InputQueue.GetTypeSize() * DataCount);
			}

			UE_LOG(LogCommandFrameNetPacket, Log, TEXT("WriteRedundantData Frame[%d] DataCount[%d] DataSize[%d]"), InputFrame.CommandFrame, DataCount, DataSize);
		}
		TotalNumBits += BitWriter.GetNumBits();
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

	UE_LOG(LogCommandFrameNetPacket, Log, TEXT("WriteRedundantData End"));
}

void FCommandFrameInputNetPacket::ReadRedundantData(TFunction<FCommandFrameInputAtom * (uint32 CommandFrame, int32 DataCount)> AllocateData) const
{
	FBitReader Ar;
	Ar.SetData((uint8*)RawData.GetData(), RawData.Num());

	bool bPaused = false;

	UE_LOG(LogCommandFrameNetPacket, Log, TEXT("ReadRedundantData Begin"));
	UE_LOG(LogCommandFrameNetPacket, Log, TEXT("ReadRedundantData DataTotalSize[%d]"), RawData.Num());

	while (!Ar.AtEnd() && !bPaused)
	{
		uint32 CommandFrame;
		int32 DataCount;
		int32 DataSize;

		Ar << CommandFrame;
		Ar << DataCount;

		FBitArchiveSizeScope Scope(Ar, DataSize);

		UE_LOG(LogCommandFrameNetPacket, Log, TEXT("ReadRedundantData CF[%d] DataCount[%d] DataSize[%d]"), CommandFrame, DataCount, DataSize);

		FCommandFrameInputAtom* InputAtomData = AllocateData(CommandFrame, DataCount);
		if (InputAtomData)
		{
			UScriptStruct* InputAtomStruct = FCommandFrameInputAtom::StaticStruct();
			for (int32 index = 0; index < DataCount; ++index)
			{
				InputAtomStruct->GetCppStructOps()->Serialize(Ar, InputAtomData + index);
			}
		}
		else
		{
			PRIVATE_GET_NAMESPACE(&Ar, Pos, FCommandFrameInputNetPacket) = Ar.GetPosBits() + DataSize;
		}
		
	}

	UE_LOG(LogCommandFrameNetPacket, Log, TEXT("ReadRedundantData End"));
}