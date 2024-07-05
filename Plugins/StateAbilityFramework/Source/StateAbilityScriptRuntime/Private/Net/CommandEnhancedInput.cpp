#include "Net/CommandEnhancedInput.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/InputDelegateBinding.h"

#include "CommandFrameSetting.h"

PRIVATE_DEFINE_FUNC_NAMESPACE(UCommandEnhancedInputComponent, APawn, void, SetupPlayerInputComponent, UInputComponent*);

bool UCommandEnhancedInputSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

#if !UE_SERVER
	// Getting setting on whether to turn off subsystem or not
	const bool bShouldCreate = GetDefault<UCommandFrameSettings>()->bEnableCommandFrameNetReplication;
#else
	const bool bShouldCreate = false;
#endif

	return bShouldCreate && Super::ShouldCreateSubsystem(Outer);
}

void UCommandEnhancedInputSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	InputFilterMap.Add(ECommandInputActionGroup::Common, MakeShared<FCommandEnhancedInputFilter>());
	InputFilterMap.Add(ECommandInputActionGroup::Aim, MakeShared<FCommandEnhancedInputFilter_Aim>());
	InputFilterMap.Add(ECommandInputActionGroup::Move, MakeShared<FCommandEnhancedInputFilter_Move>());

	RebuildInputFilterMap();
}

void UCommandEnhancedInputSubsystem::Deinitialize()
{
	Super::Deinitialize();


}

bool UCommandEnhancedInputSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// The world subsystem shouldn't be used in the editor
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCommandEnhancedInputSubsystem::Tick(float DeltaTime)
{
	for (auto& InputFilterPair : InputFilterMap)
	{
		InputFilterPair.Value->Tick(DeltaTime);
	}
}

ETickableTickType UCommandEnhancedInputSubsystem::GetTickableTickType() const
{
	return ETickableTickType::Conditional;
}

bool UCommandEnhancedInputSubsystem::IsTickable() const
{
	return !HasAnyFlags(RF_ClassDefaultObject);
}

TStatId UCommandEnhancedInputSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCommandEnhancedInputSubsystem, STATGROUP_Tickables);
}

UWorld* UCommandEnhancedInputSubsystem::GetTickableGameObjectWorld() const
{
	return GetWorld();
}

ECommandInputActionGroup UCommandEnhancedInputSubsystem::GetGroupType(const UInputAction* InputAction) const
{
	if (const ECommandInputActionGroup* GroupTypePtr = GroupMap.Find(InputAction))
	{
		return *GroupTypePtr;
	}
	return ECommandInputActionGroup::Default;
}

void UCommandEnhancedInputSubsystem::ClearInput()
{
	for (auto& InputFilterPair : InputFilterMap)
	{
		InputFilterPair.Value->ClearInput();
	}
}

void UCommandEnhancedInputSubsystem::RecordInput(const FInputActionInstance& ActionData)
{
	if (auto InputFilterPtr = InputFilterMap.Find(GetGroupType(ActionData.GetSourceAction())))
	{
		(*InputFilterPtr)->RecordInput(ActionData);
	}
}

void UCommandEnhancedInputSubsystem::ReadInputs(TArray<FCommandFrameInputAtom>& OutInputs)
{
	for (auto& InputFilterPair : InputFilterMap)
	{
		OutInputs.Append(InputFilterPair.Value->GetInputData());
	}
}

void UCommandEnhancedInputSubsystem::PopInputs(TArray<FCommandFrameInputAtom>& OutInputs)
{
	for (auto& InputFilterPair : InputFilterMap)
	{
		OutInputs.Append(InputFilterPair.Value->GetInputData());
		InputFilterPair.Value->ClearInput();
	}
}

void UCommandEnhancedInputSubsystem::PopInputsWithRawData(uint8* OutRawData)
{
	for (auto& InputFilterPair : InputFilterMap)
	{
		const TArray<FCommandFrameInputAtom>& InputDataRef = InputFilterPair.Value->GetInputData();

		FMemory::Memcpy(OutRawData, InputDataRef.GetData(), sizeof(FCommandFrameInputAtom) * InputDataRef.Num());

		OutRawData += InputDataRef.Num();

		InputFilterPair.Value->ClearInput();
	}
}

uint32 UCommandEnhancedInputSubsystem::GetInputNum() const
{
	uint32 Num = 0;
	for (auto& InputFilterPair : InputFilterMap)
	{
		Num += InputFilterPair.Value->GetInputData().Num();
	}
	return Num;
}

void UCommandEnhancedInputSubsystem::RegisterInputFilter(ECommandInputActionGroup GroupType, TSharedRef<FCommandEnhancedInputFilter> InputFilter)
{
	InputFilterMap.Add(GroupType, InputFilter);

	RebuildInputFilterMap();
}

void UCommandEnhancedInputSubsystem::AddMappingContext(const UInputMappingContext* MappingContext, ECommandInputActionGroup InputActionGroup, int32 Priority, const FModifyContextOptions& Options)
{
	if (!IsValid(MappingContext))
	{
		return;
	}

	if (APlayerController* PC = GetWorld()->GetGameInstance()->GetFirstLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* EISystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			EISystem->AddMappingContext(MappingContext, Priority, Options);
		}
	}
	
	for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
	{
		GroupMap.Add(Mapping.Action, InputActionGroup);
	}
}

void UCommandEnhancedInputSubsystem::RebuildInputFilterMap()
{
	InputFilterMap.ValueSort([](const TSharedPtr<FCommandEnhancedInputFilter>& A, const TSharedPtr<FCommandEnhancedInputFilter>& B) {
		return A->GetPriority() > B->GetPriority();
	});
}


//////////////////////////////////////////////////////////////////////////

void FCommandEnhancedInputFilter::Tick(float DeltaTime)
{

}

void FCommandEnhancedInputFilter::ClearInput()
{
	InputData.Empty();
}

void FCommandEnhancedInputFilter::RecordInput(const FInputActionInstance& ActionData)
{
	InputData.Emplace(ActionData);
}

const TArray<FCommandFrameInputAtom>& FCommandEnhancedInputFilter::GetInputData() const
{
	return InputData;
}

//////////////////////////////////////////////////////////////////////////
// UCommandEnhancedInputComponent
void UCommandEnhancedInputComponent::BeginPlay()
{
	Super::BeginPlay();

	APawn* Owner = GetOwner<APawn>();
	if (!IsValid(Owner))
	{
		return;
	}

	PRIVATE_GET_FUNC_NAMESPACE(UCommandEnhancedInputComponent, Owner, SetupPlayerInputComponent)(this);

	RegisterComponent();

	if (UInputDelegateBinding::SupportsInputDelegate(Owner->GetClass()))
	{
		bBlockInput = Owner->bBlockInput;
		UInputDelegateBinding::BindInputDelegatesWithSubojects(Owner, this);
	}
}

void UCommandEnhancedInputComponent::ClearActionBindings()
{
	Super::ClearActionBindings();
}

void UCommandEnhancedInputComponent::ClearBindingsForObject(UObject* InOwner)
{
	Super::ClearBindingsForObject(InOwner);

	for (auto It = CommandInputEventBindingMap.CreateIterator(); It; ++It)
	{
		if (It->Value->IsBoundToObject(InOwner))
		{
			It.RemoveCurrent();
		}
	}
}

void UCommandEnhancedInputComponent::RemoveCommandInputByHandle(const uint32 Handle)
{
	Super::RemoveBindingByHandle(Handle);

	CommandInputEventBindingMap.Remove(Handle);
}

void UCommandEnhancedInputComponent::RemoveCommandInput(const FInputBindingHandle& BindingToRemove)
{
	RemoveCommandInputByHandle(BindingToRemove.GetHandle());
}