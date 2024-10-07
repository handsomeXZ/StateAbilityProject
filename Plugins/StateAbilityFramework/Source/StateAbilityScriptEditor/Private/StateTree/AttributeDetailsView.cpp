#include "StateTree/AttributeDetailsView.h"

#include "StructUtils.h"
#include "StructUtilsMetadata.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "SPinTypeSelector.h"
#include "DetailLayoutBuilder.h"
#include "EdGraphSchema_K2.h"

#include "Attribute/AttributeBag/AttributeBagUtils.h"

#define LOCTEXT_NAMESPACE "AttributeDetails"

namespace Attribute
{
	// UFunction calling helpers. 
	// Use our "own" hardcoded reflection system for types used in UFunctions calls in this file.
	template<typename T> struct TypeName { static const TCHAR* Get() = delete; };

#define DEFINE_TYPENAME(InType) template<> struct TypeName<InType> { static const TCHAR *Get() { return TEXT(#InType); }};
	DEFINE_TYPENAME(bool)
		DEFINE_TYPENAME(FGuid)
		DEFINE_TYPENAME(FName)
		DEFINE_TYPENAME(FEdGraphPinType)
#undef DEFINE_TYPENAME

	// Wrapper around a param that store an address (const_cast for const ptr, be careful of that)
	// a string identifiying the underlying cpp type and if the input value is const, mark it const.
	struct FFuncParam
	{
		void* Value = nullptr;
		const TCHAR* CppType;
		bool bIsConst = false;

		template<typename T>
		static FFuncParam Make(T& Value)
		{
			FFuncParam Result;
			Result.Value = &Value;
			Result.CppType = TypeName<T>::Get();
			return Result;
		}

		template<typename T>
		static FFuncParam Make(const T& Value)
		{
			FFuncParam Result;
			Result.Value = &const_cast<T&>(Value);
			Result.CppType = TypeName<T>::Get();
			Result.bIsConst = true;
			return Result;
		}
	};

	template<typename T>
	bool IsScriptStruct(const FProperty* Property)
	{
		if (!Property)
		{
			return false;
		}

		const FStructProperty* StructProperty = CastField<const FStructProperty>(Property);
		return StructProperty && StructProperty->Struct->IsA(TBaseStructure<T>::Get()->GetClass());
	}

	bool FindUserFunction(const FProperty* Property, FName InFuncMetadataName, UFunction*& OutFunc, UObject*& OutTarget)
	{
		OutFunc = nullptr;
		OutTarget = nullptr;

		if (!Property || !Property->HasMetaData(InFuncMetadataName))
		{
			return false;
		}

		FString FunctionName = Property->GetMetaData(InFuncMetadataName);
		if (FunctionName.IsEmpty())
		{
			return false;
		}

		UObject* OwnerObject;
		OwnerObject = Property->GetOwnerUObject();

		// Check for external function references, taken from GetOptions
		if (FunctionName.Contains(TEXT(".")))
		{
			OutFunc = FindObject<UFunction>(nullptr, *FunctionName, true);

			if (ensureMsgf(OutFunc && OutFunc->HasAnyFunctionFlags(EFunctionFlags::FUNC_Static), TEXT("[%s] Didn't find function %s or expected it to be static"), *InFuncMetadataName.ToString(), *FunctionName))
			{
				UObject* GetOptionsCDO = OutFunc->GetOuterUClass()->GetDefaultObject();
				OutTarget = GetOptionsCDO;
			}
		}
		else if (OwnerObject)
		{
			OutTarget = OwnerObject;
			OutFunc = OutTarget->GetClass() ? OutTarget->GetClass()->FindFunctionByName(*FunctionName) : nullptr;
		}

		// Only support native functions
		if (!ensureMsgf(OutFunc && OutFunc->IsNative(), TEXT("[%s] Didn't find function %s or expected it to be native"), *InFuncMetadataName.ToString(), *FunctionName))
		{
			OutFunc = nullptr;
			OutTarget = nullptr;
		}

		return OutTarget != nullptr && OutFunc != nullptr;
	}

	template <typename ReturnType, typename... ArgsTypes>
	TValueOrError<ReturnType, void> CallFunc(UObject* InTargetObject, UFunction* InFunc, ArgsTypes&& ...InArgs)
	{
		if (!InTargetObject || !InFunc)
		{
			return MakeError();
		}

		constexpr int32 NumParms = std::is_same_v <ReturnType, void> ? sizeof...(ArgsTypes) : (sizeof...(ArgsTypes) + 1);

		if (NumParms != InFunc->NumParms)
		{
			return MakeError();
		}

		FFuncParam InParams[NumParms] = { FFuncParam::Make(std::forward<ArgsTypes>(InArgs)) ... };

		auto Invoke = [InTargetObject, InFunc, &InParams, NumParms](ReturnType* OutResult) -> bool
			{
				// Validate that the function has a return property if the return type is not void.
				if (std::is_same_v<ReturnType, void> != (InFunc->GetReturnProperty() == nullptr))
				{
					return false;
				}

				// Allocating our "stack" for the function call on the stack (will be freed when this function is exited)
				uint8* StackMemory = (uint8*)FMemory_Alloca(InFunc->ParmsSize);
				FMemory::Memzero(StackMemory, InFunc->ParmsSize);

				if constexpr (!std::is_same_v<ReturnType, void>)
				{
					check(OutResult != nullptr);
					InParams[NumParms - 1] = FFuncParam::Make(*OutResult);
				}

				bool bValid = true;
				int32 ParamIndex = 0;

				// Initializing our "stack" with our parameters. Use the property system to make sure more complex types
				// are constructed before being set.
				for (TFieldIterator<FProperty> It(InFunc); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
				{
					FProperty* LocalProp = *It;
					if (!LocalProp->HasAnyPropertyFlags(CPF_ZeroConstructor))
					{
						LocalProp->InitializeValue_InContainer(StackMemory);
					}

					if (bValid)
					{
						if (ParamIndex >= NumParms)
						{
							bValid = false;
							continue;
						}

						FFuncParam& Param = InParams[ParamIndex++];

						if (LocalProp->GetCPPType() != Param.CppType)
						{
							bValid = false;
							continue;
						}

						LocalProp->SetValue_InContainer(StackMemory, Param.Value);
					}
				}

				if (bValid)
				{
					FFrame Stack(InTargetObject, InFunc, StackMemory, nullptr, InFunc->ChildProperties);
					InFunc->Invoke(InTargetObject, Stack, OutResult);
				}

				ParamIndex = 0;
				// Copy back all non-const out params (that is not the return param, this one is already set by the invoke call)
				// from the stack, also making sure that the constructed types are destroyed accordingly.
				for (TFieldIterator<FProperty> It(InFunc); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
				{
					FProperty* LocalProp = *It;
					FFuncParam& Param = InParams[ParamIndex++];

					if (LocalProp->HasAnyPropertyFlags(CPF_OutParm) && !LocalProp->HasAnyPropertyFlags(CPF_ReturnParm) && !Param.bIsConst)
					{
						LocalProp->GetValue_InContainer(StackMemory, Param.Value);
					}

					LocalProp->DestroyValue_InContainer(StackMemory);
				}

				return bValid;
			};

		if constexpr (std::is_same_v<ReturnType, void>)
		{
			if (Invoke(nullptr))
			{
				return MakeValue();
			}
			else
			{
				return MakeError();
			}
		}
		else
		{
			ReturnType OutResult{};
			if (Invoke(&OutResult))
			{
				return MakeValue(std::move(OutResult));
			}
			else
			{
				return MakeError();
			}
		}
	}


	// Validate that the function pass as parameter has signature ReturnType(ArgsTypes...)
	template <typename ReturnType, typename... ArgsTypes>
	bool ValidateFunctionSignature(UFunction* InFunc)
	{
		if (!InFunc)
		{
			return false;
		}

		constexpr int32 NumParms = std::is_same_v <ReturnType, void> ? sizeof...(ArgsTypes) : (sizeof...(ArgsTypes) + 1);

		if (NumParms != InFunc->NumParms)
		{
			return false;
		}

		const TCHAR* ArgsCppTypes[NumParms] = { TypeName<ArgsTypes>::Get() ... };

		// If we have a return type, put it at the end. UFunction will have the return type after InArgs in the field iterator.
		if constexpr (!std::is_same_v<ReturnType, void>)
		{
			ArgsCppTypes[NumParms - 1] = TypeName<ReturnType>::Get();
		}
		else
		{
			// Otherwise, check that the function doesn't have a return param
			if (InFunc->GetReturnProperty() != nullptr)
			{
				return false;
			}
		}

		int32 ArgCppTypesIndex = 0;
		for (TFieldIterator<FProperty> It(InFunc); It && It->HasAnyPropertyFlags(EPropertyFlags::CPF_Parm); ++It)
		{
			const FString PropertyCppType = It->GetCPPType();
			if (PropertyCppType != ArgsCppTypes[ArgCppTypesIndex])
			{
				return false;
			}

			// Also making sure that the last param is a return param, if we have a return value
			if constexpr (!std::is_same_v<ReturnType, void>)
			{
				if (ArgCppTypesIndex == NumParms - 1 && !It->HasAnyPropertyFlags(EPropertyFlags::CPF_ReturnParm))
				{
					return false;
				}
			}

			ArgCppTypesIndex++;
		}

		return true;
	}

	bool CanHaveMemberVariableOfType(const FEdGraphPinType& PinType)
	{
		if (PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
			|| PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard
			|| PinType.PinCategory == UEdGraphSchema_K2::PC_MCDelegate
			|| PinType.PinCategory == UEdGraphSchema_K2::PC_Delegate
			|| PinType.PinCategory == UEdGraphSchema_K2::PC_Interface)
		{
			return false;
		}

		return true;
	}


	/** @return sanitized property name based on the input string. */
	FName GetValidPropertyName(const FString& Name)
	{
		FName Result;
		if (!Name.IsEmpty())
		{
			if (!FName::IsValidXName(Name, INVALID_OBJECTNAME_CHARACTERS))
			{
				Result = MakeObjectNameFromDisplayLabel(Name, NAME_None);
			}
			else
			{
				Result = FName(Name);
			}
		}
		else
		{
			Result = FName(TEXT("Property"));
		}

		return Result;
	}


	FAttributeBag* GetAttributeBagFromBagProperty(const FProperty* BagProperty, UObject* BagOwner)
	{
		if (const FStructProperty* StructProperty = CastField<const FStructProperty>(BagProperty))
		{
			if (StructProperty->Struct->IsChildOf(FAttributeBag::StaticStruct()))
			{
				void* RawData = StructProperty->ContainerPtrToValuePtr<void>(BagOwner);
				return UE::StructUtils::GetStructPtr<FAttributeBag>(StructProperty->Struct, RawData);
			}
		}
		return nullptr;
	}

	/** @return Attribute bag struct common to all edited properties. */
	const UScriptStruct* GetCommonBagStruct(const FProperty* StructProperty, UObject* BagOwner)
	{
		const UScriptStruct* CommonBagStruct = nullptr;

		if (FAttributeBag* AttributeBag = GetAttributeBagFromBagProperty(StructProperty, BagOwner))
		{
			CommonBagStruct = AttributeBag->GetScriptStruct();
		}
		return CommonBagStruct;
	}

	namespace DynamicBag
	{
		FAttributeDynamicBag* GetAttributeDynamicBagFromBagProperty(const FProperty* DynamicBagProperty, UObject* DynamicBagOwner)
		{
			if (const FStructProperty* StructProperty = CastField<const FStructProperty>(DynamicBagProperty))
			{
				if (StructProperty->Struct->IsChildOf(FAttributeDynamicBag::StaticStruct()))
				{
					void* RawData = StructProperty->ContainerPtrToValuePtr<void>(DynamicBagOwner);
					return UE::StructUtils::GetStructPtr<FAttributeDynamicBag>(StructProperty->Struct, RawData);
				}
			}
			return nullptr;
		}

		/** @return Attribute DynamicBag struct to all edited properties. */
		const UAttributeBagStruct* GetDynamicBagStruct(const FProperty* StructProperty, UObject* BagOwner)
		{
			const UAttributeBagStruct* CommonBagStruct = nullptr;

			if (FAttributeDynamicBag* DynamicBag = GetAttributeDynamicBagFromBagProperty(StructProperty, BagOwner))
			{
				CommonBagStruct = DynamicBag->GetAttributeBagStruct();
			}
			return CommonBagStruct;
		}

		/** @return property descriptors of the Attribute DynamicBag struct to all edited properties. */
		TArray<FAttributeBagPropertyDesc> GetDynamicPropertyDescs(const FProperty* DynamicBagProperty, UObject* DynamicBagOwner)
		{
			TArray<FAttributeBagPropertyDesc> PropertyDescs;

			if (const UAttributeBagStruct* BagStruct = GetDynamicBagStruct(DynamicBagProperty, DynamicBagOwner))
			{
				PropertyDescs = BagStruct->GetPropertyDescs();
			}

			return PropertyDescs;
		}

		/** @return Blueprint pin type from property descriptor. */
		FEdGraphPinType GetPropertyDescAsPin(const FAttributeBagPropertyDesc& Desc)
		{
			UEnum* PropertyTypeEnum = StaticEnum<EAttributeBagPropertyType>();
			check(PropertyTypeEnum);

			FEdGraphPinType PinType;
			PinType.PinSubCategory = NAME_None;

			// Container type
			//@todo: Handle nested containers in property selection.
			const EAttributeBagContainerType ContainerType = Desc.ContainerTypes.GetFirstContainerType();
			switch (ContainerType)
			{
			case EAttributeBagContainerType::Array:
				PinType.ContainerType = EPinContainerType::Array;
				break;
			default:
				PinType.ContainerType = EPinContainerType::None;
			}

			// Value type
			switch (Desc.ValueType)
			{
			case EAttributeBagPropertyType::Bool:
				PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
				break;
			case EAttributeBagPropertyType::Byte:
				PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
				break;
			case EAttributeBagPropertyType::Int32:
				PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
				break;
			case EAttributeBagPropertyType::Int64:
				PinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
				break;
			case EAttributeBagPropertyType::Float:
				PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
				PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
				break;
			case EAttributeBagPropertyType::Double:
				PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
				PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
				break;
			case EAttributeBagPropertyType::Name:
				PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
				break;
			case EAttributeBagPropertyType::String:
				PinType.PinCategory = UEdGraphSchema_K2::PC_String;
				break;
			case EAttributeBagPropertyType::Text:
				PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
				break;
			case EAttributeBagPropertyType::Enum:
				// @todo: some pin coloring is not correct due to this (byte-as-enum vs enum). 
				PinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
				PinType.PinSubCategoryObject = const_cast<UObject*>(Desc.ValueTypeObject.Get());
				break;
			case EAttributeBagPropertyType::Struct:
				PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
				PinType.PinSubCategoryObject = const_cast<UObject*>(Desc.ValueTypeObject.Get());
				break;
			case EAttributeBagPropertyType::Object:
				PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
				PinType.PinSubCategoryObject = const_cast<UObject*>(Desc.ValueTypeObject.Get());
				break;
			case EAttributeBagPropertyType::SoftObject:
				PinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
				PinType.PinSubCategoryObject = const_cast<UObject*>(Desc.ValueTypeObject.Get());
				break;
			case EAttributeBagPropertyType::Class:
				PinType.PinCategory = UEdGraphSchema_K2::PC_Class;
				PinType.PinSubCategoryObject = const_cast<UObject*>(Desc.ValueTypeObject.Get());
				break;
			case EAttributeBagPropertyType::SoftClass:
				PinType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
				PinType.PinSubCategoryObject = const_cast<UObject*>(Desc.ValueTypeObject.Get());
				break;
			default:
				ensureMsgf(false, TEXT("Unhandled value type %s"), *UEnum::GetValueAsString(Desc.ValueType));
				break;
			}

			return PinType;
		}

		/** Sets property descriptor based on a Blueprint pin type. */
		void SetPropertyDescFromPin(FAttributeBagPropertyDesc& Desc, const FEdGraphPinType& PinType)
		{
			const UAttributeBagSchema* Schema = GetDefault<UAttributeBagSchema>();
			check(Schema);

			// remove any existing containers
			Desc.ContainerTypes.Reset();

			// Fill Container types, if any
			switch (PinType.ContainerType)
			{
			case EPinContainerType::Array:
				Desc.ContainerTypes.Add(EAttributeBagContainerType::Array);
				break;
			case EPinContainerType::Set:
				ensureMsgf(false, TEXT("Unsuported container type [Set] "));
				break;
			case EPinContainerType::Map:
				ensureMsgf(false, TEXT("Unsuported container type [Map] "));
				break;
			default:
				break;
			}

			// Value type
			if (PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
			{
				Desc.ValueType = EAttributeBagPropertyType::Bool;
				Desc.ValueTypeObject = nullptr;
			}
			else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
			{
				if (UEnum* Enum = Cast<UEnum>(PinType.PinSubCategoryObject))
				{
					Desc.ValueType = EAttributeBagPropertyType::Enum;
					Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
				}
				else
				{
					Desc.ValueType = EAttributeBagPropertyType::Byte;
					Desc.ValueTypeObject = nullptr;
				}
			}
			else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
			{
				Desc.ValueType = EAttributeBagPropertyType::Int32;
				Desc.ValueTypeObject = nullptr;
			}
			else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int64)
			{
				Desc.ValueType = EAttributeBagPropertyType::Int64;
				Desc.ValueTypeObject = nullptr;
			}
			else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Real)
			{
				if (PinType.PinSubCategory == UEdGraphSchema_K2::PC_Float)
				{
					Desc.ValueType = EAttributeBagPropertyType::Float;
					Desc.ValueTypeObject = nullptr;
				}
				else if (PinType.PinSubCategory == UEdGraphSchema_K2::PC_Double)
				{
					Desc.ValueType = EAttributeBagPropertyType::Double;
					Desc.ValueTypeObject = nullptr;
				}
			}
			else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
			{
				Desc.ValueType = EAttributeBagPropertyType::Name;
				Desc.ValueTypeObject = nullptr;
			}
			else if (PinType.PinCategory == UEdGraphSchema_K2::PC_String)
			{
				Desc.ValueType = EAttributeBagPropertyType::String;
				Desc.ValueTypeObject = nullptr;
			}
			else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
			{
				Desc.ValueType = EAttributeBagPropertyType::Text;
				Desc.ValueTypeObject = nullptr;
			}
			else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Enum)
			{
				Desc.ValueType = EAttributeBagPropertyType::Enum;
				Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
			}
			else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
			{
				Desc.ValueType = EAttributeBagPropertyType::Struct;
				Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
			}
			else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
			{
				Desc.ValueType = EAttributeBagPropertyType::Object;
				Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
			}
			else if (PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject)
			{
				Desc.ValueType = EAttributeBagPropertyType::SoftObject;
				Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
			}
			else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
			{
				Desc.ValueType = EAttributeBagPropertyType::Class;
				Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
			}
			else if (PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass)
			{
				Desc.ValueType = EAttributeBagPropertyType::SoftClass;
				Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
			}
			else
			{
				ensureMsgf(false, TEXT("Unhandled pin category %s"), *PinType.PinCategory.ToString());
			}
		}

		/** Creates new property bag struct and sets all properties to use it, migrating over old values. */
		void SetPropertyDescs(const FProperty* DynamicBagProperty, UObject* DynamicBagOwner, const TConstArrayView<FAttributeBagPropertyDesc> PropertyDescs)
		{
			if (ensure(IsScriptStruct<FAttributeDynamicBag>(DynamicBagProperty)))
			{
				// Create new bag struct
				const UAttributeBagStruct* NewBagStruct = UAttributeBagStruct::GetOrCreateFromDescs(PropertyDescs);

				// Migrate structs to the new type, copying values over.
				if (FAttributeDynamicBag* DynamicBag = GetAttributeDynamicBagFromBagProperty(DynamicBagProperty, DynamicBagOwner))
				{
					DynamicBag->ResetDataStruct(NewBagStruct);
				}
			}
		}

		template<typename TFunc>
		void ApplyChangesToPropertyDescs(const FText& SessionName, const FProperty* DynamicBagProperty, UObject* DynamicBagOwner, TFunc&& Function)
		{
			if (!IsValid(DynamicBagOwner) || !IsScriptStruct<FAttributeDynamicBag>(DynamicBagProperty))
			{
				return;
			}

			FScopedTransaction Transaction(SessionName);
			TArray<FAttributeBagPropertyDesc> PropertyDescs = GetDynamicPropertyDescs(DynamicBagProperty, DynamicBagOwner);

			Function(PropertyDescs);

			SetPropertyDescs(DynamicBagProperty, DynamicBagOwner, PropertyDescs);
		}

	}


	/** @return true of the property name is not used yet by the property bag structure common to all edited properties. */
	bool IsUniqueName(const FName NewName, const FName OldName, const FProperty* StructProperty, UObject* BagOwner)
	{
		if (NewName == OldName)
		{
			return false;
		}

		if (const UScriptStruct* BagStruct = GetCommonBagStruct(StructProperty, BagOwner))
		{
			for (TFieldIterator<FProperty> PropIt(BagStruct); PropIt; ++PropIt)
			{
				FProperty* Property = *PropIt;
				if (NewName == Property->GetFName())
				{
					return false;
				}
			}
		}

		return true;
	}

	/** @return Blueprint pin type from property descriptor. */
	FEdGraphPinType GetPropertyCommonAsPin(const FProperty* Property)
	{
		// @TODO: 开销有点大？
		FAttributeBagPropertyDesc Desc(Property->GetFName(), Property);

		return DynamicBag::GetPropertyDescAsPin(Desc);
	}

}

//////////////////////////////////////////////////////////////////////////
// FAttributeDetailsNode
FAttributeDetailsNode::FAttributeDetailsNode(FAttributeBag& AttributeBag, int32 InPropIndex, const FText& InDisplayName, UObject* InPropertyOuter, FProperty* InProperty, TSharedRef<FAttributeDetailsViewModel> InViewModel)
	: UID(AttributeBag.GetUID()), PropIndex(InPropIndex), DisplayName(InDisplayName), PropertyOuter(InPropertyOuter), Property(InProperty), ViewModel(InViewModel.ToWeakPtr())
{
	
}

void FAttributeDetailsNode::GenerateAttributeChildren(FAttributeBag& AttributeBag)
{
	int32 NodeIndex = 0;
	for (TFieldIterator<FProperty> PropIt(AttributeBag.GetScriptStruct()); PropIt; ++PropIt)
	{
		FProperty* ChlidProp = *PropIt;

		// 不允许嵌套
		if (FStructProperty* StructProperty = CastField<FStructProperty>(ChlidProp))
		{
			ensureAlwaysMsgf(StructProperty->Struct->IsChildOf(FAttributeBag::StaticStruct()), TEXT("AttributeBag nesting is forbidden!!!"));
			return;
		}

		FText ChildNodeName = ChlidProp->GetDisplayNameText();

		TSharedPtr<FAttributeDetailsNode> ChildNode = MakeShared<FAttributeDetailsNode>(AttributeBag, NodeIndex++, ChildNodeName, PropertyOuter, ChlidProp, ViewModel.Pin().ToSharedRef());

		ChildNode->Parent = SharedThis(this);

		Children.Add(ChildNode);
	}

	FStructProperty* StructProperty = CastField<FStructProperty>(Property);

	if (StructProperty && StructProperty->Struct->IsChildOf(FAttributeDynamicBag::StaticStruct()) && !StructProperty->FindMetaData(TEXT("NotDynamicAttributeBag")))
	{
		// 如果是DynamicBag，允许创建 AddWidget
		SAssignNew(ValueOverride, SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(LOCTEXT("AddAttribute_Tooltip", "Add new attribute"))
			.OnClicked_Lambda([StructProperty, DynamicBagOuter = PropertyOuter, this]()
			{
				if (!IsDataValid())
				{
					return FReply::Handled();
				}
				constexpr int32 MaxIterations = 100;
				FName NewName(TEXT("NewAttribute"));
				int32 Number = 1;
				while (!Attribute::IsUniqueName(NewName, FName(), StructProperty, DynamicBagOuter) && Number < MaxIterations)
				{
					Number++;
					NewName.SetNumber(Number);
				}
				if (Number == MaxIterations)
				{
					return FReply::Handled();
				}

				Attribute::DynamicBag::ApplyChangesToPropertyDescs(
					LOCTEXT("OnPropertyAdded", "Add Property"), StructProperty, DynamicBagOuter,
					[&NewName](TArray<FAttributeBagPropertyDesc>& PropertyDescs)
					{
						PropertyDescs.Emplace(NewName, EAttributeBagPropertyType::Bool, nullptr, PropertyDescs.Num());
					});

				// 重新构建TreeView，否则会有异常
				ViewModel.Pin()->OnUpdateDetails.Broadcast();
				return FReply::Handled();
			})
			.Content()
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.PlusCircle"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];
	}
}

bool FAttributeDetailsNode::IsDataValid() const
{
	return PropIndex != INDEX_NONE
		&& IsValid(PropertyOuter) 
		&& Property
		&& ViewModel.IsValid();
}

TSharedPtr<FAttributeDetailsNode> FAttributeDetailsNode::GetParentNode()
{
	return Parent;
}

TSharedPtr<FAttributeDetailsNode> FAttributeDetailsNode::GetOutermostNode()
{
	if (!Parent.IsValid())
	{
		return SharedThis(this);
	}
	else
	{
		return Parent->GetOutermostNode();
	}
}

//////////////////////////////////////////////////////////////////////////
// SAttributeDetailsView
void SAttributeDetailsView::Construct(const FArguments& InArgs)
{
	ViewModel = MakeShared<FAttributeDetailsViewModel>();
	ViewModel->OnUpdateDetails.AddSP(this, &SAttributeDetailsView::UpdateDetails);
	ViewModel->OnPinInfoChanged.AddSP(this, &SAttributeDetailsView::UpdateDetails);

	//TSharedRef<SScrollBar> HorizontalScrollBar = SNew(SScrollBar)
	//	.Orientation(Orient_Horizontal)
	//	.Thickness(FVector2D(12.0f, 12.0f));

	TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.Thickness(FVector2D(12.0f, 12.0f));

	TreeViewWidget = SNew(STreeView<TSharedPtr<FAttributeDetailsNode>>)
		.OnGenerateRow(this, &SAttributeDetailsView::OnGenerateRow)
		.OnGetChildren(this, &SAttributeDetailsView::OnGetChildren)
		.TreeItemsSource(&TreeRoots)
		.ItemHeight(32)
		//.OnSelectionChanged(this, &SAttributeDetailsView::OnTreeSelectionChanged)
		//.OnExpansionChanged(this, &SAttributeDetailsView::OnTreeExpansionChanged)
		//.OnContextMenuOpening(this, &SAttributeDetailsView::OnContextMenuOpening)
		.AllowOverscroll(EAllowOverscroll::No)
		.ExternalScrollbar(VerticalScrollBar);

	ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f)
				[
					TreeViewWidget.ToSharedRef()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					VerticalScrollBar
				]

			]
		];
}

void SAttributeDetailsView::SetObject(UObject* InSelection)
{
	AttributeBagOwner = InSelection;
	TreeRoots.Empty();
	
	if (IsValid(InSelection))
	{
		int32 RootNodeIndex = 0;
		for (TFieldIterator<FProperty> PropIt(InSelection->GetClass()); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			FStructProperty* StructProperty = CastField<FStructProperty>(Property);

			if (!StructProperty || !StructProperty->Struct->IsChildOf(FAttributeBag::StaticStruct()))
			{
				continue;
			}

			FText RootName = Property->GetDisplayNameText();
			
			FAttributeBag* AttributeBag = StructProperty->ContainerPtrToValuePtr<FAttributeBag>(InSelection);

			TSharedPtr<FAttributeDetailsNode> RootNode = MakeShared<FAttributeDetailsNode>(*AttributeBag, RootNodeIndex++, RootName, InSelection, Property, ViewModel.ToSharedRef());
			RootNode->GenerateAttributeChildren(*AttributeBag);

			TreeRoots.Add(RootNode);
		}
	}

	TreeViewWidget->SetTreeItemsSource(&TreeRoots);

	for (auto& Node : TreeRoots)
	{
		TreeViewWidget->SetItemExpansion(Node, true);
	}

	TreeViewWidget->ClearSelection();
	TreeViewWidget->RebuildList();
}

void SAttributeDetailsView::UpdateDetails()
{
	if (TreeViewWidget.IsValid() && AttributeBagOwner.IsValid())
	{
		SetObject(AttributeBagOwner.Get());
	}
}

TSharedRef<ITableRow> SAttributeDetailsView::OnGenerateRow(TSharedPtr<FAttributeDetailsNode> InNode, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	return SNew(SAttributeDetailsViewRow, InOwnerTableView, InNode.ToSharedRef(), ViewModel);
}

void SAttributeDetailsView::OnGetChildren(TSharedPtr<FAttributeDetailsNode> InParent, TArray<TSharedPtr<FAttributeDetailsNode>>& OutChildren)
{
	OutChildren = InParent->Children;
}


//////////////////////////////////////////////////////////////////////////
// SAttributeDetailsViewRow
void SAttributeDetailsViewRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedRef<FAttributeDetailsNode> InNode, TWeakPtr<FAttributeDetailsViewModel> InViewModel)
{
	ViewModel = InViewModel;
	Node = InNode;

	TSharedPtr<FAttributeDetailsNode> OutermostNode = Node->GetOutermostNode();
	FStructProperty* StructProperty = CastField<FStructProperty>(Node->Property);

	bool bReadOnly = !(Attribute::IsScriptStruct<FAttributeDynamicBag>(OutermostNode->Property)) || OutermostNode->Property->FindMetaData(TEXT("NotDynamicAttributeBag"));
	bool bIsAttributeBag = StructProperty && StructProperty->Struct->IsChildOf(FAttributeBag::StaticStruct());

	TWeakPtr<SAttributeDetailsViewRow> WeakViewRow = SharedThis(this).ToWeakPtr();

	ConstructInternal(STableRow::FArguments()
		.Padding(5.0f)
		//.OnDragDetected(this, &SAttributeDetailsViewRow::HandleDragDetected)
		//.OnCanAcceptDrop(this, &SAttributeDetailsViewRow::HandleCanAcceptDrop)
		//.OnAcceptDrop(this, &SAttributeDetailsViewRow::HandleAcceptDrop)
		//.Style(&FStateAbilityEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("StateTree.Selection"))
		, InOwnerTableView);

	SAssignNew(ValueWidget, SHorizontalBox);

	if(!bIsAttributeBag)
	{
		ValueWidget->AddSlot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Left)
		.AutoWidth()
		[
			SNew(SPinTypeSelector, FGetPinTypeTree::CreateSP(this, &SAttributeDetailsViewRow::GetFilteredVariableTypeTree))
			.TargetPinType_Lambda([WeakViewRow]() {
				if (WeakViewRow.IsValid())
				{
					return WeakViewRow.Pin()->GetPinInfo();
				}
				return FEdGraphPinType();
			})
			.OnPinTypeChanged_Lambda([WeakViewRow](const FEdGraphPinType& PinType) {
				if (WeakViewRow.IsValid())
				{
					return WeakViewRow.Pin()->PinInfoChanged(PinType);
				}
			})
			.Schema(GetDefault<UAttributeBagSchema>())
			.bAllowArrays(true)
			.TypeTreeFilter(ETypeTreeFilter::None)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ReadOnly(bReadOnly)
		];
	}

	if (Node->ValueOverride.IsValid())
	{
		ValueWidget->AddSlot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Left)
		[
			Node->ValueOverride.ToSharedRef()
		];
	}


	this->ChildSlot
	.HAlign(HAlign_Fill)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Left)
		.AutoWidth()
		[
			SNew(SExpanderArrow, SharedThis(this))
			.ShouldDrawWires(false)
			.IndentAmount(16)
			.BaseIndentLevel(0)
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Left)
		.Padding(FMargin(4.0f, 4.0f))
		.FillWidth(0.5f)
		[
			SAssignNew(NameTextBlock, SInlineEditableTextBlock)
			.Text(this, &SAttributeDetailsViewRow::GetAttributeName)
			.MultiLine(false)
			.Clipping(EWidgetClipping::ClipToBounds)
			.IsReadOnly(bReadOnly || bIsAttributeBag)
			.IsSelected_Lambda([bReadOnly]() {
				return !bReadOnly;
			})// 在TreeView内TextBlock会失去焦点，所以只能用这种方式
			.OnVerifyTextChanged_Lambda([Node = Node](const FText& InText, FText& OutErrorMessage)
				{
					if (!Node->IsDataValid())
					{
						return false;
					}

					const FName NewName = Attribute::GetValidPropertyName(InText.ToString());
					const FName OldName = Node->Property->GetFName();
					TSharedPtr<FAttributeDetailsNode> OutermostNode = Node->GetOutermostNode();

					bool bResult = Attribute::IsUniqueName(NewName, OldName, OutermostNode->Property, Node->PropertyOuter);
					if (!bResult)
					{
						OutErrorMessage = LOCTEXT("MustBeUniqueName", "Attribute must have unique name");
					}
					return bResult;
				})
			.OnTextCommitted_Lambda([Node = Node, ViewModel = ViewModel](const FText& InNewText, ETextCommit::Type InCommitType)
				{
					if (!Node->IsDataValid())
					{
						return;
					}

					if (InCommitType == ETextCommit::OnCleared)
					{
						return;
					}

					const FName NewName = Attribute::GetValidPropertyName(InNewText.ToString());
					const FName OldName = Node->Property->GetFName();
					TSharedPtr<FAttributeDetailsNode> OutermostNode = Node->GetOutermostNode();

					if (!Attribute::IsUniqueName(NewName, OldName, OutermostNode->Property, OutermostNode->PropertyOuter))
					{
						return;
					}

					Attribute::DynamicBag::ApplyChangesToPropertyDescs(
						LOCTEXT("OnAttributeNameChanged", "Change Attribute Name"), OutermostNode->Property, Node->PropertyOuter,
						[&NewName, &OldName](TArray<FAttributeBagPropertyDesc>& PropertyDescs)
						{
							if (FAttributeBagPropertyDesc* Desc = PropertyDescs.FindByPredicate([OldName = OldName](const FAttributeBagPropertyDesc& Desc) { return Desc.Name == OldName; }))
							{
								Desc->Name = NewName;
							}
						});

					// 重新构建TreeView，否则会有异常
					ViewModel.Pin()->OnPinInfoChanged.Broadcast();
				})
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Left)
		.FillWidth(0.5f)
		[
			ValueWidget.ToSharedRef()
		]
	];
}

FText SAttributeDetailsViewRow::GetAttributeName() const
{
	return Node->DisplayName;
}

void SAttributeDetailsViewRow::GetFilteredVariableTypeTree(TArray<TSharedPtr<class UEdGraphSchema_K2::FPinTypeTreeInfo>>& TypeTree, ETypeTreeFilter TypeTreeFilter)
{
	// The SPinTypeSelector popup might outlive this details view, so bag struct property can be invalid here.
	if (!Node->IsDataValid())
	{
		return;
	}

	TSharedPtr<FAttributeDetailsNode> OutermostNode = Node->GetOutermostNode();

	UFunction* IsPinTypeAcceptedFunc = nullptr;
	UObject* IsPinTypeAcceptedTarget = nullptr;
	if (Attribute::FindUserFunction(OutermostNode->Property, UE::StructUtils::Metadata::IsPinTypeAcceptedName, IsPinTypeAcceptedFunc, IsPinTypeAcceptedTarget))
	{
		check(IsPinTypeAcceptedFunc && IsPinTypeAcceptedTarget);

		// We need to make sure the signature matches perfectly: bool(FEdGraphPinType)
		bool bFuncIsValid = Attribute::ValidateFunctionSignature<bool, FEdGraphPinType>(IsPinTypeAcceptedFunc);
		if (!ensureMsgf(bFuncIsValid, TEXT("[%s] Function %s does not have the right signature."), *UE::StructUtils::Metadata::IsPinTypeAcceptedName.ToString(), *IsPinTypeAcceptedFunc->GetName()))
		{
			return;
		}
	}

	auto IsPinTypeAccepted = [IsPinTypeAcceptedFunc, IsPinTypeAcceptedTarget](const FEdGraphPinType& InPinType) -> bool
		{
			if (IsPinTypeAcceptedFunc && IsPinTypeAcceptedTarget)
			{
				const TValueOrError<bool, void> bIsValid = Attribute::CallFunc<bool>(IsPinTypeAcceptedTarget, IsPinTypeAcceptedFunc, InPinType);
				return bIsValid.HasValue() && bIsValid.GetValue();
			}
			else
			{
				return true;
			}
		};

	check(GetDefault<UEdGraphSchema_K2>());
	TArray<TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>> TempTypeTree;
	GetDefault<UAttributeBagSchema>()->GetVariableTypeTree(TempTypeTree, TypeTreeFilter);

	// Filter
	for (TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>& PinType : TempTypeTree)
	{
		if (!PinType.IsValid() || !IsPinTypeAccepted(PinType->GetPinType(/*bForceLoadSubCategoryObject*/false)))
		{
			continue;
		}

		for (int32 ChildIndex = 0; ChildIndex < PinType->Children.Num(); )
		{
			TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo> Child = PinType->Children[ChildIndex];
			if (Child.IsValid())
			{
				const FEdGraphPinType& ChildPinType = Child->GetPinType(/*bForceLoadSubCategoryObject*/false);

				if (!Attribute::CanHaveMemberVariableOfType(ChildPinType) || !IsPinTypeAccepted(ChildPinType))
				{
					PinType->Children.RemoveAt(ChildIndex);
					continue;
				}
			}
			++ChildIndex;
		}

		TypeTree.Add(PinType);
	}
}

FEdGraphPinType SAttributeDetailsViewRow::GetPinInfo()
{
	// The SPinTypeSelector popup might outlive this details view, so bag struct property can be invalid here.
	if (!Node->IsDataValid())
	{
		return FEdGraphPinType();
	}

	return Attribute::GetPropertyCommonAsPin(Node->Property);
}

void SAttributeDetailsViewRow::PinInfoChanged(const FEdGraphPinType& PinType)
{
	// The SPinTypeSelector popup might outlive this details view, so bag struct property can be invalid here.
	if (!Node->IsDataValid())
	{
		return;
	}

	TSharedPtr<FAttributeDetailsNode> OutermostNode = Node->GetOutermostNode();

	Attribute::DynamicBag::ApplyChangesToPropertyDescs(
		LOCTEXT("OnPropertyTypeChanged", "Change Property Type"), OutermostNode->Property, OutermostNode->PropertyOuter,
		[&PinType, Node = Node, OutermostNode](TArray<FAttributeBagPropertyDesc>& PropertyDescs)
		{
			if (!OutermostNode->IsDataValid())
			{
				return;
			}

			// Find and change struct type
			if (FAttributeBagPropertyDesc* Desc = PropertyDescs.FindByPredicate([Property = Node->Property](const FAttributeBagPropertyDesc& Desc) { return Desc.CachedProperty == Property; }))
			{
				Attribute::DynamicBag::SetPropertyDescFromPin(*Desc, PinType);
			}
		});

	ViewModel.Pin()->OnPinInfoChanged.Broadcast();
}

#undef LOCTEXT_NAMESPACE