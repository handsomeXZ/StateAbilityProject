#include "AttributeModelTest.h"

#include "Attribute/Reactive/AttributeReflection.h"

#define TEST_TRUE(expression) \
	TEST_BOOLEAN_(TEXT(#expression), expression, true)

#define TEST_FALSE(expression) \
	TEST_BOOLEAN_(TEXT(#expression), expression, false)

#define TEST_EQUAL(expression, expected) \
	TEST_BOOLEAN_(TEXT(#expression), expression, expected)

#define TEST_BOOLEAN_(text, expression, expected) \
	TestEqual(text, expression, expected);

BEGIN_DEFINE_SPEC(FAttributeModelSpec, "StateAbilityFramework.Attribute.Reactive", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
UAttributeModelObjectBase* ModelObject;
UAttributeModelObject_NoNewAttribute* ModelObject_NoNewAttribute;

FAttributeModelTest_OneAttribute ModelData_OneAttribute;
FAttributeModelTest_NoNewAttribute ModelData_NoNewAttribute;
FAttributeModelTest_TwoAttribute ModelData_TwoAttribute;
END_DEFINE_SPEC(FAttributeModelSpec)

void FAttributeModelSpec::Define()
{
	BeforeEach([this]() {
		ModelObject = NewObject<UAttributeModelObjectBase>();
		ModelObject_NoNewAttribute = NewObject<UAttributeModelObject_NoNewAttribute>();

		ModelData_OneAttribute = FAttributeModelTest_OneAttribute();
	});

	Describe("Properties", [this]()
	{
		It("Should find property in Struct and UObject Derived from TReactiveModel", EAsyncExecution::ThreadPool, [this]()
		{
			const FReactivePropertyOperations* Struct_PropertyOps = Attribute::Reactive::FReactiveRegistryHelper_Field<FAttributeModelTest_OneAttribute>::FindPropertyOperations(TEXT("Int32Value"));
			const FReactivePropertyOperations* Object_PropertyOps = Attribute::Reactive::FReactiveRegistryHelper_Field<UAttributeModelObjectBase>::FindPropertyOperations(TEXT("Int32Value"));

			TEST_TRUE(Struct_PropertyOps != nullptr);
			if (Struct_PropertyOps)
			{
				TEST_TRUE(Struct_PropertyOps->Property != nullptr);
				if (Struct_PropertyOps->Property)
				{
					TEST_TRUE(Struct_PropertyOps->Property == FAttributeModelTest_OneAttribute::Int32ValueProperty());
				}
			}
			TEST_TRUE(Object_PropertyOps != nullptr);
			if (Object_PropertyOps)
			{
				TEST_TRUE(Object_PropertyOps->Property != nullptr);
				if (Object_PropertyOps->Property)
				{
					TEST_TRUE(Object_PropertyOps->Property == UAttributeModelObjectBase::Int32ValueProperty());
				}
			}
		});

		It("Should find property in DerivedStruct and DerivedUObject(not define new Attribute)", EAsyncExecution::ThreadPool, [this]()
		{
			const FReactivePropertyOperations* Struct_PropertyOps = Attribute::Reactive::FReactiveRegistryHelper_Field<FAttributeModelTest_NoNewAttribute>::FindPropertyOperations(TEXT("Int32Value"));
			const FReactivePropertyOperations* Object_PropertyOps = Attribute::Reactive::FReactiveRegistryHelper_Field<UAttributeModelObject_NoNewAttribute>::FindPropertyOperations(TEXT("Int32Value"));

			TEST_TRUE(Struct_PropertyOps != nullptr);
			if (Struct_PropertyOps)
			{
				TEST_TRUE(Struct_PropertyOps->Property != nullptr);
				if (Struct_PropertyOps->Property)
				{
					TEST_TRUE(Struct_PropertyOps->Property == FAttributeModelTest_NoNewAttribute::Int32ValueProperty());
				}
			}

			TEST_TRUE(Object_PropertyOps != nullptr);
			if (Object_PropertyOps)
			{
				TEST_TRUE(Object_PropertyOps->Property != nullptr);
				if (Object_PropertyOps->Property)
				{
					TEST_TRUE(Object_PropertyOps->Property == UAttributeModelObject_NoNewAttribute::Int32ValueProperty());
				}
			}
		});
	});

	Describe("GetValue", [this]()
	{
		It("Should get valid int32 value by Operation", EAsyncExecution::ThreadPool, [this]()
		{
			const FReactivePropertyOperations* Struct_PropertyOps = Attribute::Reactive::FReactiveRegistryHelper_Field<FAttributeModelTest_OneAttribute>::FindPropertyOperations(TEXT("Int32Value"));
			const FReactivePropertyOperations* Object_PropertyOps = Attribute::Reactive::FReactiveRegistryHelper_Field<UAttributeModelObjectBase>::FindPropertyOperations(TEXT("Int32Value"));

			if (Struct_PropertyOps)
			{
				int32 StructOutValue = 0;
				Struct_PropertyOps->GetValueCopy(&ModelData_OneAttribute, &StructOutValue);
				TEST_EQUAL(StructOutValue == 666, true);

				int32& ValueRef = Struct_PropertyOps->GetValueRef<int32>(&ModelData_OneAttribute);
				ValueRef = 0;

				Struct_PropertyOps->GetValueCopy(&ModelData_OneAttribute, &StructOutValue);
				TEST_BOOLEAN_(TEXT("GetValueRef int32 value Successfully"), StructOutValue, 0);

				int32& ValueRef2 = Struct_PropertyOps->GetValueRef<FAttributeModelTest_OneAttribute, int32>(&ModelData_OneAttribute);
				ValueRef2 = 666;

				Struct_PropertyOps->GetValueCopy(&ModelData_OneAttribute, &StructOutValue);
				TEST_BOOLEAN_(TEXT("GetValueRef_Check int32 value Successfully"), StructOutValue, 666);
			}

			if (Object_PropertyOps)
			{
				int32 ObjectOutValue = 0;
				Object_PropertyOps->GetValueCopy(ModelObject, &ObjectOutValue);
				TEST_EQUAL(ObjectOutValue == 666, true);

				int32& ValueRef = Object_PropertyOps->GetValueRef<int32>(ModelObject);
				ValueRef = 0;

				Object_PropertyOps->GetValueCopy(ModelObject, &ObjectOutValue);
				TEST_BOOLEAN_(TEXT("GetValueRef_UnCheck int32 value Successfully"), ObjectOutValue, 0);

				int32& ValueRef2 = Object_PropertyOps->GetValueRef<UAttributeModelObjectBase, int32>(ModelObject);
				ValueRef2 = 666;

				Object_PropertyOps->GetValueCopy(ModelObject, &ObjectOutValue);
				TEST_BOOLEAN_(TEXT("GetValueRef_Check int32 value Successfully"), ObjectOutValue, 666);
			}
		});

		It("Should get valid int32 value by Getter", EAsyncExecution::ThreadPool, [this]()
		{
			int32 StructOutValue = ModelData_OneAttribute.GetInt32Value();
			TEST_EQUAL(StructOutValue == 666, true);
				
			int32 ObjectOutValue = ModelObject->GetInt32Value();
			TEST_EQUAL(ObjectOutValue == 666, true);
		});

		It("Should get valid TArray value by Operation", EAsyncExecution::ThreadPool, [this]()
		{
			const FReactivePropertyOperations* Object_PropertyOps = Attribute::Reactive::FReactiveRegistryHelper_Field<UAttributeModelObjectBase>::FindPropertyOperations(TEXT("ArrayValue"));
		
			if (Object_PropertyOps)
			{
				TArray<int32>& ArrayValueRef = Object_PropertyOps->GetValueRef<TArray<int32>>(ModelObject);
				ArrayValueRef.Add(666);
				
				TArray<int32> ArrayOutValue;
				Object_PropertyOps->GetValueCopy(ModelObject, &ArrayOutValue);
				TEST_BOOLEAN_(TEXT("GetValueRef_Check TArray value Successfully"), ArrayOutValue.Num(), 1);
			}
		});
	});

	Describe("SetValue", [this]()
	{
		It("Should set int32 value successfully by Operation", EAsyncExecution::ThreadPool, [this]()
		{
			const FReactivePropertyOperations* Struct_PropertyOps = Attribute::Reactive::FReactiveRegistryHelper_Field<FAttributeModelTest_OneAttribute>::FindPropertyOperations(TEXT("Int32Value"));
			const FReactivePropertyOperations* Object_PropertyOps = Attribute::Reactive::FReactiveRegistryHelper_Field<UAttributeModelObjectBase>::FindPropertyOperations(TEXT("Int32Value"));

			if (Struct_PropertyOps)
			{
				int32 NewValue = 888;
				Struct_PropertyOps->SetValue(&ModelData_OneAttribute, &NewValue);

				int32 StructOutValue = 0;
				Struct_PropertyOps->GetValueCopy(&ModelData_OneAttribute, &StructOutValue);
				TEST_BOOLEAN_("Operation changed the value of 666.", StructOutValue != 666, true);
				TEST_BOOLEAN_("Operation sets the value to 888.", StructOutValue, 888);
			}
				
			if (Object_PropertyOps)
			{
				int32 NewValue = 888;
				Object_PropertyOps->SetValue(ModelObject, &NewValue);

				int32 ObjectOutValue = 0;
				Object_PropertyOps->GetValueCopy(ModelObject, &ObjectOutValue);
				TEST_BOOLEAN_("Operation changed the value of 666.", ObjectOutValue != 666, true);
				TEST_BOOLEAN_("Operation sets the value to 888.", ObjectOutValue, 888);
			}
		});

		It("Should set int32 value successfully by Setter", EAsyncExecution::ThreadPool, [this]()
		{
			ModelData_OneAttribute.SetInt32Value(888);
			int32 StructOutValue = ModelData_OneAttribute.GetInt32Value();
			TEST_BOOLEAN_("Operation changed the value of 666.", StructOutValue != 666, true);
			TEST_BOOLEAN_("Operation sets the value to 888.", StructOutValue, 888);

			ModelObject->SetInt32Value(888);
			int32 ObjectOutValue = ModelObject->GetInt32Value();
			TEST_BOOLEAN_("Operation changed the value of 666.", ObjectOutValue != 666, true);
			TEST_BOOLEAN_("Operation sets the value to 888.", ObjectOutValue, 888);
		});
	});

	Describe("SetValue Comparison", [this]()
	{

		// @TODO：需要等到响应式测试完成才方便测试这部分
		/*It("Should set int32 value successfully by Operation", EAsyncExecution::ThreadPool, [this]()
		{
			const FReactivePropertyOperations* Struct_PropertyOps = Attribute::Reactive::FReactiveRegistryHelper_Field<FAttributeModelTest_OneAttribute>::FindPropertyOperations(TEXT("Int32Value"));
			const FReactivePropertyOperations* Object_PropertyOps = Attribute::Reactive::FReactiveRegistryHelper_Field<UAttributeModelObjectBase>::FindPropertyOperations(TEXT("Int32Value"));

			
		});*/


	});

	Describe("Reactive", [this]()
	{
		It("Should be properly bind and unbind", EAsyncExecution::ThreadPool, [this]()
		{
			TEST_BOOLEAN_("BindEntry Num is correct", ModelObject->GetBindEntriesNum(), 3);

			int32 EntryItemNum = 0;
			auto Func = [](){};

			FBindEntryHandle Handle_1 = Bind(ModelObject, UAttributeModelObjectBase::Int32ValueProperty(), Func);
			FBindEntryHandle Handle_2 = Bind(ModelObject, UAttributeModelObjectBase::Int32ValueProperty(), Func);
			FBindEntryHandle Handle_3 = Bind(ModelObject, UAttributeModelObjectBase::ArrayValueProperty(), Func);

			EntryItemNum = ModelObject->GetBindEntry(UAttributeModelObjectBase::Int32ValueProperty()).GetEntryItemsNum();
			TEST_BOOLEAN_("Bind int32 value successfully", EntryItemNum, 2);

			EntryItemNum = ModelObject->GetBindEntry(UAttributeModelObjectBase::ArrayValueProperty()).GetEntryItemsNum();
			TEST_BOOLEAN_("Bind TArray value successfully", EntryItemNum, 1);

			UnBind(ModelObject, Handle_1);
			EntryItemNum = ModelObject->GetBindEntry(UAttributeModelObjectBase::Int32ValueProperty()).GetEntryItemsNum();
			TEST_BOOLEAN_("UnBind int32 value successfully", EntryItemNum, 1);

			ModelObject->ClearBindEntry(UAttributeModelObjectBase::Int32ValueProperty());
			EntryItemNum = ModelObject->GetBindEntry(UAttributeModelObjectBase::Int32ValueProperty()).GetEntryItemsNum();
			TEST_BOOLEAN_("Remove Int32Value's BindEntry successfully", EntryItemNum, 0);


			ModelObject->ClearBindEntry(UAttributeModelObjectBase::ArrayValueProperty());
			EntryItemNum = ModelObject->GetBindEntry(UAttributeModelObjectBase::ArrayValueProperty()).GetEntryItemsNum();
			TEST_BOOLEAN_("Remove ArrayValue's BindEntry successfully", EntryItemNum, 0);
		});

		It("Should change int32 value and CallBack by reactive", EAsyncExecution::ThreadPool, [this]()
		{
			int32 ExecCount = 0;

			Bind(ModelObject, UAttributeModelObjectBase::Int32ValueProperty(), [&ExecCount] {
				++ExecCount;
			});

			ModelObject->SetInt32Value(123);
			ModelObject->ClearAllBindEntry();
			TEST_BOOLEAN_("Reactive model data binding has executed.", ExecCount, 1);
		});

		It("Should change TArray value and CallBack by reactive", EAsyncExecution::ThreadPool, [this]()
		{
			int32 ExecCount = 0;

			Bind(ModelObject, UAttributeModelObjectBase::ArrayValueProperty(), [&ExecCount] {
				++ExecCount;
			});

			TArray<int32>& ArrayValueRef = ModelObject->GetArrayValue();
			ArrayValueRef.Add(1);
			MARK_MODEL_ATTRIBUTE_DIRTY(ModelObject, UAttributeModelObject_NoNewAttribute, ArrayValue);

			ModelObject->ClearAllBindEntry();
			TEST_BOOLEAN_("Reactive model data binding has executed.", ExecCount, 1);
		});
	});

	Describe("Reactive Effect", [this]()
	{
		It("Should change int32 value and RunEffect", EAsyncExecution::ThreadPool, [this]()
		{
			int32 ExecCount = 0;

			FAttributeBindEffect Effect = MakeEffect(ModelObject, [ModelObject = ModelObject, &ExecCount] {
				if (IsValid(ModelObject))
				{
					if (ExecCount < 1)
					{
						int32 Value = ModelObject->GetInt32Value_Effect();
					}
					ExecCount += 1;
				}
			});

			Effect.Run();						// +1

			ModelObject->SetInt32Value(123);	// +1 and remove Int32Value's dependency (for ExecCount == 3)
			Effect.Run();						// +1
			ModelObject->SetInt32Value(321);	// +0
			TEST_BOOLEAN_("Reactive model data binding has executed and Dependency removed successfully.", ExecCount, 3);

			ExecCount -= 3;						// ExecCount == 0

			Effect.Run();						// +1
			ModelObject->SetInt32Value(123);	// +1
			TEST_BOOLEAN_("Reactive model data binding has executed and Dependency re-added successfully.", ExecCount, 2);
		});

		It("Should change int32 value and RunSharedEffect", EAsyncExecution::ThreadPool, [this]()
		{
			int32 ExecCount = 0;

			FAttributeBindSharedEffect Effect = MakeSharedEffect(ModelObject, [ModelObject = ModelObject, &ExecCount] {
				if (IsValid(ModelObject))
				{
					if (ExecCount < 1)
					{
						int32 Value = ModelObject->GetInt32Value_Effect();
					}
					ExecCount += 1;
				}
			});

			Effect.Run();						// +1

			ModelObject->SetInt32Value(123);	// +1 and remove Int32Value's dependency (for ExecCount == 3)
			Effect.Run();						// +1
			ModelObject->SetInt32Value(321);	// +0
			TEST_BOOLEAN_("Reactive model data binding has executed and Dependency removed successfully.", ExecCount, 3);

			ExecCount -= 3;						// ExecCount == 0

			Effect.Run();						// +1
			ModelObject->SetInt32Value(123);	// +1
			TEST_BOOLEAN_("Reactive model data binding has executed and Dependency re-added successfully.", ExecCount, 2);
		});
	});

	AfterEach([this]() {
		ModelObject->MarkAsGarbage();
		ModelObject = nullptr;
	});
}