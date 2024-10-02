#pragma once
#include "CoreMinimal.h"

#include "Attribute/Reactive/AttributeReactive.h"
#include "Attribute/Reactive/AttributeMacros.h"
#include "AttributeModelTest.generated.h"

USTRUCT()
struct STATEABILITYFRAMEWORKTESTS_API FAttributeModelTestBase
{
	GENERATED_BODY()
public:
	virtual ~FAttributeModelTestBase() {}
};

USTRUCT()
struct STATEABILITYFRAMEWORKTESTS_API FAttributeModelTest_NoAttribute : public FAttributeModelTestBase, public TReactiveModel<FAttributeModelTest_NoAttribute>
{
	GENERATED_BODY()

public:
	REACTIVE_BODY(FAttributeModelTest_NoAttribute);

};

USTRUCT()
struct STATEABILITYFRAMEWORKTESTS_API FAttributeModelTest_OneAttribute : public FAttributeModelTestBase, public TReactiveModel<FAttributeModelTest_OneAttribute>
{
	GENERATED_BODY()

public:
	FAttributeModelTest_OneAttribute()
		: Int32ValueField(666)
	{
	}

	REACTIVE_BODY(FAttributeModelTest_OneAttribute);
	REACTIVE_ATTRIBUTE(int32, Int32Value);
};

USTRUCT()
struct STATEABILITYFRAMEWORKTESTS_API FAttributeModelTest_NoNewAttribute : public FAttributeModelTest_OneAttribute
{
	GENERATED_BODY()

public:
	REACTIVE_BODY(FAttributeModelTest_NoNewAttribute);

};

USTRUCT()
struct STATEABILITYFRAMEWORKTESTS_API FAttributeModelTest_TwoAttribute : public FAttributeModelTest_NoNewAttribute
{
	GENERATED_BODY()

public:
	FAttributeModelTest_TwoAttribute()
		: ObjectValueField(nullptr)
	{
	}
	REACTIVE_BODY(FAttributeModelTest_TwoAttribute);
	REACTIVE_ATTRIBUTE(UObject*, ObjectValue);
};

//////////////////////////////////////////////////////////////////////////

UCLASS()
class STATEABILITYFRAMEWORKTESTS_API UAttributeModelObjectBase : public UObject, public TReactiveModel<UAttributeModelObjectBase>
{
	GENERATED_BODY()
public:
	UAttributeModelObjectBase()
		: Int32ValueField(666)
	{
	}

	REACTIVE_BODY(UAttributeModelObjectBase);
	REACTIVE_ATTRIBUTE(int32, Int32Value);
	REACTIVE_ATTRIBUTE(TArray<int32>, ArrayValue);
};

UCLASS()
class STATEABILITYFRAMEWORKTESTS_API UAttributeModelObject_NoNewAttribute : public UAttributeModelObjectBase
{
	GENERATED_BODY()
public:
	REACTIVE_BODY(UAttributeModelObject_NoNewAttribute);
};