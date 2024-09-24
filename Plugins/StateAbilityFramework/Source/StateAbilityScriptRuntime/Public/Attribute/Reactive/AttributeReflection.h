#pragma once

class FReactivePropertyBase;

// contains operations that can be performed with property
struct FPropertyOperationsBase
{
	virtual ~FPropertyOperationsBase() {}

	// Reads value from InOwner and writes to memory pointed by OutValue. (OutHasValue denotes whether TOptional property has value, Non-TOptional always returns true)
	virtual void GetValue(void* InOwner, void* OutValue, bool& OutHasValue) const = 0;

	// Writes value to InOwner from memory pointer by InValue. (InHasValue denotes whether TOptional property has value)
	virtual void SetValue(void* InOwner, void* InValue, bool InHasValue = true) const = 0;

	// Returns whether this property might contain Object Reference for GC
	virtual bool ContainsObjectReference(bool bIncludeNoFieldProperties) const = 0;

	virtual void AddFieldClassProperty(UField* TargetFieldClass) const = 0;

	// Returns UField of owning ReactiveModel
	virtual UField* GetReactiveModelFieldClass() const = 0;
};

struct FReactivePropertyOperations : public FPropertyOperationsBase
{
	virtual ~FReactivePropertyOperations() {}

	// Pointer to a FViewModelPropertyBase
	const FReactivePropertyBase* Property = nullptr;
};


template<typename OpsType>
struct FReactivePropertyReflection
{
	static_assert(std::is_base_of<FPropertyOperationsBase, OpsType>::value, "Operations Type must derive from FPropertyOperationsBase");

	struct FFlags
	{
		bool IsOptional : 1;
		bool HasPublicGetter : 1;
		bool HasPublicSetter : 1;
	};

	const OpsType* GetOperations() const
	{
		return static_cast<const OpsType*>(Operations.GetTypedPtr());
	}

	OpsType* GetOperations()
	{
		return static_cast<OpsType*>(Operations.GetTypedPtr());
	}

	const FReactivePropertyBase* GetProperty() const
	{
		return Operations.GetTypedPtr()->Property;
	}

	FFlags Flags;
	TTypeCompatibleBytes<OpsType> Operations;
};