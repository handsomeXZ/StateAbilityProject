#include "Component/CFrameMoverComponent.h"

#include "GameFramework/PhysicsVolume.h"

#include "CommandFrameManager.h"
#include "Component/Mover/CFrameMovementMode.h"
#include "Component/Mover/CFrameMoveModeStateMachine.h"

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

	CFrameManager->OnEndFrame.AddUObject(this, &UCFrameMoverComponent::FixedTick);
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

void UCFrameMoverComponent::OnNetSync(FNetProcedureSyncParam& SyncParam)
{
	if (SyncParam.Ar.IsSaving())
	{
		//SyncParam.Ar << 
	}
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