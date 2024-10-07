#pragma once
#include "CoreMinimal.h"

#include "Attribute/AttributeBag/AttributeBagUtils.h"
#include "Attribute/Reactive/AttributeReactive.h"

#include "AttributeBagTest.generated.h"

USTRUCT()
struct FAttributeBagTestData : public FMassFragment
{
	GENERATED_BODY()

	int32 Int32Value = 666;
};

USTRUCT()
struct FAttributeReactiveBagTestDataBase : public FAttributeReactiveBagDataBase
{
	GENERATED_BODY()

	FAttributeReactiveBagTestDataBase()
		: Int32ValueField(666)
	{
	}

	REACTIVE_BODY(FAttributeReactiveBagTestDataBase);
	REACTIVE_ATTRIBUTE(int32, Int32Value);
};

USTRUCT()
struct FAttributeReactiveBagTestData : public FAttributeReactiveBagTestDataBase
{
	GENERATED_BODY()

	REACTIVE_BODY(FAttributeReactiveBagTestData);
	REACTIVE_ATTRIBUTE(TArray<int32>, ArrayValue);
};


UCLASS()
class UAttributeBagTestObject : public UObject
{
	GENERATED_BODY()
public:
	UAttributeBagTestObject()
		: Bag(FAttributeBagTestData::StaticStruct())
	{
		ReactiveBag.InitializeReactive<FAttributeReactiveBagTestData>();
	}

	void Initialize(UWorld* World)
	{
		FAttributeEntityBuildParam BuildParam;
		BuildParam.World = World;
		Bag.Initialize(BuildParam);

		FAttributeEntityBuildParam BuildParam2;
		BuildParam2.World = World;
		ReactiveBag.Initialize(BuildParam2);
	}

	FAttributeEntityBag Bag;
	FAttributeReactiveBag ReactiveBag;
};