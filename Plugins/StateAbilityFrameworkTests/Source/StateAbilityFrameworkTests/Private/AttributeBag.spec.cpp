#include "AttributeBagTest.h"

#define TEST_TRUE(expression) \
	TEST_BOOLEAN_(TEXT(#expression), expression, true)

#define TEST_FALSE(expression) \
	TEST_BOOLEAN_(TEXT(#expression), expression, false)

#define TEST_EQUAL(expression, expected) \
	TEST_BOOLEAN_(TEXT(#expression), expression, expected)

#define TEST_BOOLEAN_(text, expression, expected) \
	TestEqual(text, expression, expected);

BEGIN_DEFINE_SPEC(FAttributeBagSpec, "StateAbilityFramework.Attribute.AttributeBag", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
UAttributeBagTestObject* BagTestObject;
END_DEFINE_SPEC(FAttributeBagSpec)

namespace
{
	UWorld* GetSimpleEngineAutomationTestGameWorld(const int32 TestFlags)
	{
		// Accessing the game world is only valid for game-only 
		//check((TestFlags & EAutomationTestFlags::ApplicationContextMask) == EAutomationTestFlags::ClientContext);
		const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
		
		ensureMsgf(WorldContexts.Last().WorldType == EWorldType::Game || WorldContexts.Last().WorldType == EWorldType::PIE, TEXT("Please run the game first."));

		if (WorldContexts.Last().WorldType == EWorldType::Game || WorldContexts.Last().WorldType == EWorldType::PIE)
		{
			return WorldContexts.Last().World();
		}
		else
		{
			return nullptr;
		}
	}
}

void FAttributeBagSpec::Define()
{
	BeforeEach([this]() {
		BagTestObject = NewObject<UAttributeBagTestObject>();

		if (UWorld* World = GetSimpleEngineAutomationTestGameWorld(GetTestFlags()))
		{
			BagTestObject->Initialize(World);
		}
	});

	Describe("AttributeBag", [this]()
	{
		It("Should initialize bag successfully", EAsyncExecution::ThreadPool, [this]()
		{
			const FAttributeBagTestData* TestData = (const FAttributeBagTestData*)BagTestObject->Bag.GetMemory();

			TEST_BOOLEAN_("Bag Data is valid.", TestData != nullptr, true);
			if (TestData)
			{
				TEST_EQUAL(TestData->Int32Value, 666);
			}

			const FAttributeReactiveBagTestData* ReactiveTestData = (const FAttributeReactiveBagTestData*)BagTestObject->ReactiveBag.GetMemory();

			TEST_BOOLEAN_("ReactiveBag Data is valid.", ReactiveTestData != nullptr, true);
			if (ReactiveTestData)
			{
				TEST_EQUAL(ReactiveTestData->GetInt32Value(), 666);
			}
		});

		It("Should change int32 value and CallBack by reactive", EAsyncExecution::ThreadPool, [this]()
		{
			FAttributeReactiveBagTestData* ReactiveTestData = (FAttributeReactiveBagTestData*)BagTestObject->ReactiveBag.GetMutableMemory();

			if (ReactiveTestData)
			{
				int32 ExecCount = 0;

				Bind(ReactiveTestData, UAttributeModelObjectBase::Int32ValueProperty(), [&ExecCount] {
					++ExecCount;
					});

				ReactiveTestData->SetInt32Value(123);
				ReactiveTestData->ClearAllBindEntry();
				TEST_BOOLEAN_("Reactive model data binding has executed.", ExecCount, 1);
			}
		});
	});


	AfterEach([this]() {
		BagTestObject->MarkAsGarbage();
		BagTestObject = nullptr;
	});
}