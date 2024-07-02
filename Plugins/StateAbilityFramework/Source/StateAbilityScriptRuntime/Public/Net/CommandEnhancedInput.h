// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "EnhancedInputSubsystemInterface.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "EnhancedInputComponent.h"

#include "PrivateAccessor.h"
#include "Net/Packet/CommandFrameInput.h"

#include "CommandEnhancedInput.generated.h"

struct FInputActionInstance;

#define INPUTFILTER_PRIORITY_COMMON 1
#define INPUTFILTER_PRIORITY_AIM 10
#define INPUTFILTER_PRIORITY_MOVE 100

PRIVATE_DEFINE(FInputBindingHandle, uint32, Handle)
PRIVATE_DEFINE(UEnhancedInputComponent, TArray<TUniquePtr<FEnhancedInputActionEventBinding>>, EnhancedActionEventBindings);

UENUM(BlueprintType)
enum class ECommandInputActionGroup : uint8
{
	Default		UMETA(DisplayName = "默认（非同步）"),
	Common		UMETA(DisplayName = "通用（帧同步）"),
	Aim			UMETA(DisplayName = "瞄准（帧同步）"),
	Move		UMETA(DisplayName = "移动（帧同步）"),
};

/**
 * All players input subsystem
 * 
 * 必须遵守的约定：	配置的InputAction仅被用于帧同步。
 */
UCLASS(BlueprintType)
class STATEABILITYSCRIPTRUNTIME_API UCommandEnhancedInputSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	//~ Begin UWorldSubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem interface

	//~FTickableObjectBase interface
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	//~End of FTickableObjectBase interface

	ECommandInputActionGroup GetGroupType(const UInputAction* InputAction) const;

public:
	uint32 GetInputNum() const;

	void ClearInput();
	void RecordInput(const FInputActionInstance& ActionData);
	void ReadInputs(TArray<FCommandFrameInputAtom>& OutInputs);
	void PopInputs(TArray<FCommandFrameInputAtom>& OutInputs);
	void PopInputsWithRawData(uint8* OutRawData);	// 特殊方式，内部不会进行任何检查。不推荐使用。

	void RegisterInputFilter(ECommandInputActionGroup GroupType, TSharedRef<struct FCommandEnhancedInputFilter> InputFilter);

	// 绑定帧同步的输入映射上下文
	UFUNCTION(BlueprintCallable, Category = "Input", meta = (AutoCreateRefTerm = "Options"))
	virtual void AddMappingContext(const UInputMappingContext* MappingContext, ECommandInputActionGroup InputActionGroup, int32 Priority, const FModifyContextOptions& Options = FModifyContextOptions());


protected:
	void RebuildInputFilterMap();

	UPROPERTY()
	TMap<const UInputAction*, ECommandInputActionGroup> GroupMap;

	TMap<ECommandInputActionGroup, TSharedPtr<struct FCommandEnhancedInputFilter>> InputFilterMap;
};

struct FCommandEnhancedInputFilter : public TSharedFromThis<FCommandEnhancedInputFilter>
{
	FCommandEnhancedInputFilter(uint32 InPriority = INPUTFILTER_PRIORITY_COMMON)
		: Priority(InPriority)
	{}
	virtual ~FCommandEnhancedInputFilter() {}

	virtual void Tick(float DeltaTime);
	virtual void ClearInput();
	virtual void RecordInput(const FInputActionInstance& ActionData);
	virtual const TArray<FCommandFrameInputAtom>& GetInputData() const;

	uint32 GetPriority() { return Priority; }
protected:
	const uint32 Priority;
	TArray<FCommandFrameInputAtom> InputData;
};

struct FCommandEnhancedInputFilter_Aim : public FCommandEnhancedInputFilter
{
	FCommandEnhancedInputFilter_Aim() : FCommandEnhancedInputFilter(INPUTFILTER_PRIORITY_AIM)
	{}

	virtual ~FCommandEnhancedInputFilter_Aim() {}
	//virtual void Tick(float DeltaTime) override;
	//virtual void RecordInput(const FInputActionInstance& ActionData) override;
};

struct FCommandEnhancedInputFilter_Move : public FCommandEnhancedInputFilter
{
	FCommandEnhancedInputFilter_Move() : FCommandEnhancedInputFilter(INPUTFILTER_PRIORITY_AIM)
	{}
	virtual ~FCommandEnhancedInputFilter_Move() {}

	//virtual void Tick(float DeltaTime) override;
	//virtual void RecordInput(const FInputActionInstance& ActionData) override;
};

template<typename TSignature>
struct FCommandInputEventDelegateBinding : public FEnhancedInputActionEventDelegateBinding<TSignature>
{
private:
	FCommandInputEventDelegateBinding(const FCommandInputEventDelegateBinding<TSignature>& CloneFrom, EInputBindingClone Clone, UWorld* World)
		: FEnhancedInputActionEventDelegateBinding<TSignature>(CloneFrom.GetAction(), CloneFrom.GetTriggerEvent())
		, WeakCommandInputSubsystem(World->GetSubsystem<UCommandEnhancedInputSubsystem>())
	{
		PRIVATE_GET(this, Handle) = CloneFrom.GetHandle();
		this->Delegate = TEnhancedInputUnifiedDelegate<TSignature>(CloneFrom.Delegate);
		this->Delegate.SetShouldFireWithEditorScriptGuard(CloneFrom.Delegate.ShouldFireWithEditorScriptGuard());
	}
	FCommandInputEventDelegateBinding(const FCommandInputEventDelegateBinding<TSignature>& CloneFrom, EInputBindingClone Clone, TWeakObjectPtr<UCommandEnhancedInputSubsystem> CommandInputSubsystem)
		: FEnhancedInputActionEventDelegateBinding<TSignature>(CloneFrom.GetAction(), CloneFrom.GetTriggerEvent())
		, WeakCommandInputSubsystem(CommandInputSubsystem)
	{
		PRIVATE_GET(this, Handle) = CloneFrom.GetHandle();
		this->Delegate = TEnhancedInputUnifiedDelegate<TSignature>(CloneFrom.Delegate);
		this->Delegate.SetShouldFireWithEditorScriptGuard(CloneFrom.Delegate.ShouldFireWithEditorScriptGuard());
	}
public:
	FCommandInputEventDelegateBinding(const UInputAction* Action, ETriggerEvent InTriggerEvent, UWorld* World)
		: FEnhancedInputActionEventDelegateBinding<TSignature>(Action, InTriggerEvent)
		, WeakCommandInputSubsystem(World->GetSubsystem<UCommandEnhancedInputSubsystem>())
	{}
	virtual TUniquePtr<FEnhancedInputActionEventBinding> Clone() const override
	{
		return TUniquePtr<FEnhancedInputActionEventBinding>(new FCommandInputEventDelegateBinding<TSignature>(*this, EInputBindingClone::ForceClone, WeakCommandInputSubsystem));
	}

	// Implemented below.
	virtual void Execute(const FInputActionInstance& ActionData) const override;

	TWeakObjectPtr<UCommandEnhancedInputSubsystem> WeakCommandInputSubsystem;
};
// Action event delegate execution by signature.


UCLASS()
class STATEABILITYSCRIPTRUNTIME_API UCommandEnhancedInputComponent
	: public UEnhancedInputComponent
{
	GENERATED_BODY()
public:
	/** Removes all action bindings. */
	virtual void ClearActionBindings() override;

	/** Clears any input callback delegates from the given UObject */
	virtual void ClearBindingsForObject(UObject* InOwner) override;

	void RemoveCommandInputByHandle(const uint32 Handle);
	void RemoveCommandInput(const FInputBindingHandle& BindingToRemove);

	const TMap<uint32, TUniquePtr<FEnhancedInputActionEventBinding>>& GetCommandInputBindings() const { return CommandInputEventBindingMap; }

	/** The collection of action bindings. */
	TMap<uint32, TUniquePtr<FEnhancedInputActionEventBinding>> CommandInputEventBindingMap;

	/**
	* Binds a delegate function matching any of the handler signatures to a UInputAction assigned via UInputMappingContext to the owner of this component.
	*/
#define DEFINE_BIND_COMMANDINPUT(HANDLER_SIG)																																			\
	template<class UserClass, typename... VarTypes>																																					\
	FEnhancedInputActionEventBinding& BindCommandInput(const UInputAction* Action, ETriggerEvent TriggerEvent, UserClass* Object, typename HANDLER_SIG::template TMethodPtr< UserClass, VarTypes... > Func, VarTypes... Vars) \
	{																																													\
		FEnhancedInputActionEventDelegateBinding<HANDLER_SIG>* BindingPtr = new FCommandInputEventDelegateBinding<HANDLER_SIG>(Action, TriggerEvent, GetWorld());											\
		TUniquePtr<FEnhancedInputActionEventDelegateBinding<HANDLER_SIG>> AB = TUniquePtr<FEnhancedInputActionEventDelegateBinding<HANDLER_SIG>>(BindingPtr);														\
		AB->Delegate.BindDelegate<UserClass>(Object, Func, Vars...);																													\
		AB->Delegate.SetShouldFireWithEditorScriptGuard(ShouldFireDelegatesInEditor());																									\
		TArray<TUniquePtr<FEnhancedInputActionEventBinding>>& Bindings = PRIVATE_GET(this, EnhancedActionEventBindings);																\
		FEnhancedInputActionEventBinding& Handle = *(Bindings.Add_GetRef(MoveTemp(AB)));																								\
		CommandInputEventBindingMap.Add(Handle.GetHandle(), Handle.Clone());																											\
		return Handle;																																									\
	}

	DEFINE_BIND_COMMANDINPUT(FEnhancedInputActionHandlerSignature);
	DEFINE_BIND_COMMANDINPUT(FEnhancedInputActionHandlerValueSignature);
	DEFINE_BIND_COMMANDINPUT(FEnhancedInputActionHandlerInstanceSignature);

	/**
	 * Binds to an object UFUNCTION
	 */
	FEnhancedInputActionEventBinding& BindCommandInput(const UInputAction* Action, ETriggerEvent TriggerEvent, UObject* Object, FName FunctionName)
	{		
		FEnhancedInputActionEventDelegateBinding<FEnhancedInputActionHandlerDynamicSignature>* BindingPtr = new FCommandInputEventDelegateBinding<FEnhancedInputActionHandlerDynamicSignature>(Action, TriggerEvent, GetWorld());
		TUniquePtr<FEnhancedInputActionEventDelegateBinding<FEnhancedInputActionHandlerDynamicSignature>> AB = TUniquePtr<FEnhancedInputActionEventDelegateBinding<FEnhancedInputActionHandlerDynamicSignature>>(BindingPtr);

		AB->Delegate.BindDelegate(Object, FunctionName);
		AB->Delegate.SetShouldFireWithEditorScriptGuard(ShouldFireDelegatesInEditor());

		TArray<TUniquePtr<FEnhancedInputActionEventBinding>>& Bindings = PRIVATE_GET(this, EnhancedActionEventBindings);
		FEnhancedInputActionEventBinding& Handle = *(Bindings.Add_GetRef(MoveTemp(AB)));

		CommandInputEventBindingMap.Add(Handle.GetHandle(), Handle.Clone());

		return Handle;
	}
};

template<>
inline void FCommandInputEventDelegateBinding<FEnhancedInputActionHandlerSignature>::Execute(const FInputActionInstance& ActionData) const
{
	Delegate.Execute();
	if (Delegate.IsBound() && WeakCommandInputSubsystem.IsValid())
	{
		UCommandEnhancedInputSubsystem* Subsystem = WeakCommandInputSubsystem.Get();
		Subsystem->RecordInput(ActionData);
	}
}

template<>
inline void FCommandInputEventDelegateBinding<FEnhancedInputActionHandlerValueSignature>::Execute(const FInputActionInstance& ActionData) const
{
	Delegate.Execute(ActionData.GetValue());
	if (Delegate.IsBound() && WeakCommandInputSubsystem.IsValid())
	{
		UCommandEnhancedInputSubsystem* Subsystem = WeakCommandInputSubsystem.Get();
		Subsystem->RecordInput(ActionData);
	}
}

template<>
inline void FCommandInputEventDelegateBinding<FEnhancedInputActionHandlerInstanceSignature>::Execute(const FInputActionInstance& ActionData) const
{
	Delegate.Execute(ActionData);
	if (Delegate.IsBound() && WeakCommandInputSubsystem.IsValid())
	{
		UCommandEnhancedInputSubsystem* Subsystem = WeakCommandInputSubsystem.Get();
		Subsystem->RecordInput(ActionData);
	}
}

template<>
inline void FCommandInputEventDelegateBinding<FEnhancedInputActionHandlerDynamicSignature>::Execute(const FInputActionInstance& ActionData) const
{
	Delegate.Execute(ActionData.GetValue(), ActionData.GetElapsedTime(), ActionData.GetTriggeredTime(), ActionData.GetSourceAction());
	if (Delegate.IsBound() && WeakCommandInputSubsystem.IsValid())
	{
		UCommandEnhancedInputSubsystem* Subsystem = WeakCommandInputSubsystem.Get();
		Subsystem->RecordInput(ActionData);
	}
}