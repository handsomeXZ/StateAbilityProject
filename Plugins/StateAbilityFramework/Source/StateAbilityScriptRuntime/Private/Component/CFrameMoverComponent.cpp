#include "Component/CFrameMoverComponent.h"

#include "GameFramework/PhysicsVolume.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/GameNetworkManager.h"

#include "CommandFrameManager.h"
#include "Net/Packet/CommandFramePacket.h"
#include "Component/Mover/CFrameMovementMode.h"
#include "Component/Mover/CFrameMoveModeStateMachine.h"
#include "Component/Mover/CFrameMoveStateAdapter.h"

DEFINE_LOG_CATEGORY_STATIC(LogCFrameMmoverComp, Log, All)

const FVector UCFrameMoverComponent::DefaultGravityAccel = FVector(0.0, 0.0, -980.0);
const FVector UCFrameMoverComponent::DefaultUpDir = FVector(0.0, 0.0, 1.0);

UCFrameMoverComponent::UCFrameMoverComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bInInitializeComponent(false)
	, bInOnRegister(false)
	, UpdatedComponent(nullptr)
	, UpdatedCompAsPrimitive(nullptr)
	, PrimaryVisualComponent(nullptr)
	, CFrameManager(nullptr)
	, ModeFSM(nullptr)
{

	bWantsInitializeComponent = true;
	bAutoActivate = true;

}

void UCFrameMoverComponent::InitializeComponent()
{
	TGuardValue<bool> InInitializeComponentGuard(bInInitializeComponent, true);

	// RootComponent is null in OnRegister for blueprint (non-native) root components.
	if (!UpdatedComponent)
	{
		// Auto-register owner's root component if found.
		if (AActor* MyActor = GetOwner())
		{
			if (USceneComponent* NewUpdatedComponent = MyActor->GetRootComponent())
			{
				SetUpdatedComponent(NewUpdatedComponent);
			}
			else
			{
				ensureMsgf(false, TEXT("No root component found on %s. Simulation initialization will most likely fail."), *GetPathNameSafe(MyActor));
			}
		}
	}

	Super::InitializeComponent();
}


void UCFrameMoverComponent::UninitializeComponent()
{
	Super::UninitializeComponent();
}


void UCFrameMoverComponent::OnRegister()
{
	TGuardValue<bool> InOnRegisterGuard(bInOnRegister, true);

	UpdatedCompAsPrimitive = Cast<UPrimitiveComponent>(UpdatedComponent);
	Super::OnRegister();

	const UWorld* MyWorld = GetWorld();

	if (MyWorld && MyWorld->IsGameWorld())
	{
		const AActor* MyActor = GetOwner();

		USceneComponent* NewUpdatedComponent = UpdatedComponent;
		if (!UpdatedComponent)
		{
			// Auto-register owner's root component if found.
			if (MyActor)
			{
				NewUpdatedComponent = MyActor->GetRootComponent();
			}
		}

		SetUpdatedComponent(NewUpdatedComponent);

		// If no primary visual component is already set, fall back to searching for any kind of mesh
		if (!PrimaryVisualComponent && MyActor)
		{
			PrimaryVisualComponent = MyActor->FindComponentByClass<UMeshComponent>();
		}
	}
}

void UCFrameMoverComponent::BeginPlay()
{
	Super::BeginPlay();

	check(GetCommandFrameManager());

	if (UpdatedComponent)
	{
		// remove from tick prerequisite
		UpdatedComponent->PrimaryComponentTick.RemovePrerequisite(this, CFrameManager->GetTickFuntion());

		// force ticks after movement component updates
		UpdatedComponent->PrimaryComponentTick.AddPrerequisite(this, CFrameManager->GetTickFuntion());
	}

	if (!ModeFSM)
	{
		ModeFSM = NewObject<UCFrameMoveModeStateMachine>(this);
		ModeFSM->Init(MovementConfig);
	}

	UpdateTickRegistration();

	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UCFrameMoverComponent::PostBeginPlay);
}

void UCFrameMoverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CFrameManager->UnBindOnFrameNetChannelRegistered(this);
	CFrameManager->OnEndFrame.RemoveAll(this);
}

void UCFrameMoverComponent::PostBeginPlay()
{
	// 必须得在下一帧尝试绑定，因为在Actor的BeginPlay执行时，可能还未绑定PC。（例如单机、DS模式）
	CFrameManager->BindOnFrameNetChannelRegistered(this, FOnFrameNetChannelRegistered::CreateUObject(this, &UCFrameMoverComponent::OnFrameNetChannelRegistered));

	CFrameManager->OnEndFrame.AddUObject(this, &UCFrameMoverComponent::FixedTick);
}

void UCFrameMoverComponent::OnFrameNetChannelRegistered(ACommandFrameNetChannelBase* Channel)
{
	if (Channel->GetNetConnection() == GetOwner()->GetNetConnection())
	{
		Channel->RegisterNetPacketProcedure(EDeltaNetPacketType::Movement, this);
	}
}

void UCFrameMoverComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	// Remove delegates from old component
	if (UpdatedComponent)
	{
		UpdatedComponent->SetShouldUpdatePhysicsVolume(false);
		UpdatedComponent->SetPhysicsVolume(NULL, true);
		UpdatedComponent->PhysicsVolumeChangedDelegate.RemoveDynamic(this, &UCFrameMoverComponent::OnPhysicsVolumeChanged);
	}

	if (UpdatedCompAsPrimitive)
	{
		UpdatedCompAsPrimitive->OnComponentBeginOverlap.RemoveDynamic(this, &UCFrameMoverComponent::OnBeginOverlap);
	}

	// Don't assign pending kill components, but allow those to null out previous UpdatedComponent.
	UpdatedComponent = GetValid(NewUpdatedComponent);
	UpdatedCompAsPrimitive = Cast<UPrimitiveComponent>(UpdatedComponent);

	// Assign delegates
	if (IsValid(UpdatedComponent))
	{
		UpdatedComponent->SetShouldUpdatePhysicsVolume(true);
		UpdatedComponent->PhysicsVolumeChangedDelegate.AddUniqueDynamic(this, &UCFrameMoverComponent::OnPhysicsVolumeChanged);

		if (!bInOnRegister && !bInInitializeComponent)
		{
			// UpdateOverlaps() in component registration will take care of this.
			UpdatedComponent->UpdatePhysicsVolume(true);
		}
	}

	if (IsValid(UpdatedCompAsPrimitive))
	{
		UpdatedCompAsPrimitive->OnComponentBeginOverlap.AddDynamic(this, &UCFrameMoverComponent::OnBeginOverlap);
	}

}

void UCFrameMoverComponent::UpdateTickRegistration()
{
	check(GetCommandFrameManager());

}

UCommandFrameManager* UCFrameMoverComponent::GetCommandFrameManager()
{
	if (!IsValid(CFrameManager))
	{
		CFrameManager = GetWorld()->GetSubsystem<UCommandFrameManager>();
	}

	return CFrameManager;
}

void UCFrameMoverComponent::FixedTick(float DeltaTime, uint32 RCF, uint32 ICF)
{
	ModeFSM->FixedTick(DeltaTime, RCF, ICF);
}

void UCFrameMoverComponent::HandleImpact(const FHitResult& Hit, const FName ModeName, const FVector& MoveDelta)
{
	if (ModeName == NAME_None)
	{
		OnHandleImpact(Hit, ModeFSM->GetCurrentModeName(), MoveDelta);
	}
	else
	{
		OnHandleImpact(Hit, ModeName, MoveDelta);
	}
}

	

void UCFrameMoverComponent::OnHandleImpact(const FHitResult& Hit, const FName ModeName, const FVector& MoveDelta)
{
	// TODO: Handle physics impacts here - ie when player runs into box, impart force onto box
	// 可以参考 UCharacterMovementComponent::HandleImpact
}

void UCFrameMoverComponent::OnServerNetSync(FNetProcedureSyncParam& SyncParam)
{
	UCFrameMoveStateAdapter* Adapter = MovementConfig.MoveStateAdapter;
	FArchive& Ar = SyncParam.Ar;
	UPackageMap* Map = SyncParam.Map;
	bool& bOutSuccess = SyncParam.bOutSuccess;
	
	FVector Location = Adapter->GetLocation_WorldSpace();
	FVector Velocity = Adapter->GetVelocity_WorldSpace();
	SerializePackedVector<100, 30>(Location, Ar);
	SerializePackedVector<10, 16>(Velocity, Ar);
	Adapter->GetOrientation_WorldSpace().SerializeCompressedShort(Ar);


	UPrimitiveComponent* MovementBase = Adapter->GetMovementBase();
	FName MovementBaseBoneName = Adapter->GetMovementBaseBoneName();
	//FVector MovementBasePos = Adapter->GetMovementBasePos();
	//FQuat MovementBaseQuat = Adapter->GetMovementBaseQuat();

	// Optional movement base
	bool bIsUsingMovementBase = MovementBase != nullptr;
	Ar.SerializeBits(&bIsUsingMovementBase, 1);

	if (bIsUsingMovementBase)
	{
		Ar << MovementBase;
		Ar << MovementBaseBoneName;	// @TODO：传递FName等同于传递FString，挺浪费的

		//SerializePackedVector<100, 30>(MovementBasePos, Ar);
		//MovementBaseQuat.NetSerialize(Ar, Map, bOutSuccess);
	}

	bOutSuccess = true;
}

void UCFrameMoverComponent::OnClientNetSync(FNetProcedureSyncParam& SyncParam, bool& bNeedRewind)
{
	UCFrameMoveStateAdapter* Adapter = MovementConfig.MoveStateAdapter;
	FArchive& Ar = SyncParam.Ar;
	UPackageMap* Map = SyncParam.Map;
	bool& bOutSuccess = SyncParam.bOutSuccess;


	FVector Location;
	FVector Velocity;
	FRotator Orientation;
	SerializePackedVector<100, 30>(Location, Ar);
	SerializePackedVector<10, 16>(Velocity, Ar);
	Orientation.SerializeCompressedShort(Ar);

	// Optional movement base
	bool bIsUsingMovementBase;
	Ar.SerializeBits(&bIsUsingMovementBase, 1);

	UPrimitiveComponent* MovementBase = nullptr;
	FName MovementBaseBoneName = NAME_None;
	//FVector MovementBasePos;
	//FQuat MovementBaseQuat;

	if (bIsUsingMovementBase)
	{
		Ar << MovementBase;
		Ar << MovementBaseBoneName;

		//SerializePackedVector<100, 30>(MovementBasePos, Ar);
		//MovementBaseQuat.NetSerialize(Ar, Map, bOutSuccess);
	}

	if (CheckClientExceedsAllowablePositionError(SyncParam, Location))
	{
		if (SyncParam.NetPacket.bLocal)
		{
			AdjustClientPosition(SyncParam, Location, Velocity, Orientation, MovementBase, MovementBaseBoneName);

			UE_LOG(LogCFrameMmoverComp, Log, TEXT("[Client Rewind] ICF[%d] Location[%s]"), SyncParam.NetPacket.ServerCommandFrame, *(Adapter->GetLocation_WorldSpace().ToCompactString()));
			UE_LOG(LogCFrameMmoverComp, Log, TEXT("[Client Rewind] ICF[%d] ServerVel[%s] LocalVel[%s]"), SyncParam.NetPacket.ServerCommandFrame, *(Velocity.ToCompactString()), *(Adapter->GetVelocity_WorldSpace().ToCompactString()));

			bNeedRewind = true;
		}
	}

	bOutSuccess = true;
}

void UCFrameMoverComponent::OnClientRewind()
{
	ModeFSM->OnClientRewind();
}

bool UCFrameMoverComponent::CheckClientExceedsAllowablePositionError(FNetProcedureSyncParam& SyncParam, const FVector& ServerWorldLocation)
{
	// 参照 UCharacterMovementComponent::ServerExceedsAllowablePositionError

	if (!GetCommandFrameManager())
	{
		return false;
	}
	FStructView MovementSnapshotView = CFrameManager->ReadAttributeFromSnapshotBuffer(SyncParam.NetPacket.ServerCommandFrame, FCFrameMovementSnapshot::StaticStruct());

	if (!MovementSnapshotView.IsValid())
	{
		return false;
	}

	// Check for disagreement in movement mode
	//const uint8 CurrentPackedMovementMode = PackNetworkMovementMode();
	//if (CurrentPackedMovementMode != ClientMovementMode)
	//{
	//	// Consider this a major correction, see SendClientAdjustment()
	//	bNetworkLargeClientCorrection = true;
	//	return true;
	//}

	FCFrameMovementSnapshot& MovementSnapshot = MovementSnapshotView.Get<FCFrameMovementSnapshot>();

	const FVector LocDiff = MovementSnapshot.Location - ServerWorldLocation;
	// 啥玩意？为了读取通用配置？
	const AGameNetworkManager* GameNetworkManager = (const AGameNetworkManager*)(AGameNetworkManager::StaticClass()->GetDefaultObject());
	if (GameNetworkManager->ExceedsAllowablePositionError(LocDiff))
	{
		return true;
	}

	return false;

}

void UCFrameMoverComponent::AdjustClientPosition(FNetProcedureSyncParam& SyncParam, FVector WorldLocation, FVector WorldVelocity, FRotator WorldOrientation, UPrimitiveComponent* MovementBase, FName MovementBaseBoneName)
{
	UpdatedComponent->SetWorldLocationAndRotation(WorldLocation, WorldOrientation);

	// 速度覆盖这两个值就够了？
	// ComponentVelocity仅外部可能会用到，移动组件所用的都是Adapter，且ComponentVelocity的速度更新也由Adapter提供数据。
	{
		UpdatedComponent->ComponentVelocity = WorldVelocity;

		// 更新记录的速度
		MovementConfig.MoveStateAdapter->SetLastFrameVelocity(WorldVelocity, ModeFSM->GetMovementContext().DeltaTime);
		MovementConfig.MoveStateAdapter->UpdateMoveFrame();
	}

	MovementConfig.MoveStateAdapter->SetMovementBase(MovementBase, MovementBaseBoneName);
}

FVector UCFrameMoverComponent::GetGravityAcceleration() const
{
	if (bHasGravityOverride)
	{
		return GravityAccelOverride;
	}

	if (UpdatedComponent)
	{
		APhysicsVolume* CurPhysVolume = UpdatedComponent->GetPhysicsVolume();
		if (CurPhysVolume)
		{
			return CurPhysVolume->GetGravityZ() * FVector::UpVector;
		}
	}

	return UCFrameMoverComponent::DefaultGravityAccel;
}

FVector UCFrameMoverComponent::GetUpDirection() const
{
	const FVector DeducedUpDir = -GetGravityAcceleration().GetSafeNormal();

	if (DeducedUpDir.IsZero())
	{
		return UCFrameMoverComponent::DefaultUpDir;
	}

	return DeducedUpDir;
}