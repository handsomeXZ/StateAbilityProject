#include "StateTree/AttributeDetailsView.h"

#include "StructUtils.h"
#include "StructUtilsMetadata.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "SPinTypeSelector.h"
#include "DetailLayoutBuilder.h"
#include "EdGraphSchema_K2.h"

#include "Attribute/AttributeBag.h"

#define LOCTEXT_NAMESPACE "AttributeDetails"

namespace State::StructUtils
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

	FStateAbilityAttributeBag* GetAttributeBagFromBagProperty(const FProperty* BagProperty, UObject* BagOwner)
	{
		if (const FStructProperty* StructProperty = CastField<const FStructProperty>(BagProperty))
		{
			if (StructProperty->Struct->IsChildOf(FStateAbilityAttributeBag::StaticStruct()))
			{
				void* RawData = StructProperty->ContainerPtrToValuePtr<void>(BagOwner);
				return UE::StructUtils::GetStructPtr<FStateAbilityAttributeBag>(StructProperty->Struct, RawData);
			}
		}
		return nullptr;
	}

	/** @return Attribute bag struct common to all edited properties. */
	const UStateAttributeBag* GetCommonBagStruct(const FProperty* StructProperty, UObject* BagOwner)
	{
		const UStateAttributeBag* CommonBagStruct = nullptr;

		if (FStateAbilityAttributeBag* AttributeBag = GetAttributeBagFromBagProperty(StructProperty, BagOwner))
		{
			CommonBagStruct = AttributeBag->GetAttributeBagStruct();
		}
		return CommonBagStruct;
	}

	/** @return property descriptors of the Attribute bag struct common to all edited properties. */
	TArray<FAttributeBagPropertyDesc> GetCommonPropertyDescs(const FProperty* StructProperty, UObject* BagOwner)
	{
		TArray<FAttributeBagPropertyDesc> PropertyDescs;

		if (const UStateAttributeBag* BagStruct = GetCommonBagStruct(StructProperty, BagOwner))
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
	void SetPropertyDescs(const FProperty* StructProperty, UObject* BagOwner, const TConstArrayView<FAttributeBagPropertyDesc> PropertyDescs)
	{
		if (ensure(IsScriptStruct<FStateAbilityAttributeBag>(StructProperty)))
		{
			// Create new bag struct
			const UStateAttributeBag* NewBagStruct = UStateAttributeBag::GetOrCreateFromDescs(PropertyDescs);

			// Migrate structs to the new type, copying values over.
			if (FStateAbilityAttributeBag* Bag = GetAttributeBagFromBagProperty(StructProperty, BagOwner))
			{
				Bag->MigrateToNewBagStruct(NewBagStruct);
			}
		}
	}

	template<typename TFunc>
	void ApplyChangesToPropertyDescs(const FText& SessionName, const FProperty* StructProperty, UObject* BagOwner, TFunc&& Function)
	{
		if (!StructProperty || !IsValid(BagOwner))
		{
			return;
		}

		FScopedTransaction Transaction(SessionName);
		TArray<FAttributeBagPropertyDesc> PropertyDescs = GetCommonPropertyDescs(StructProperty, BagOwner);

		Function(PropertyDescs);

		SetPropertyDescs(StructProperty, BagOwner, PropertyDescs);
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

	/** @return true of the property name is not used yet by the property bag structure common to all edited properties. */
	bool IsUniqueName(const FName NewName, const FName OldName, const FProperty* StructProperty, UObject* BagOwner)
	{
		if (NewName == OldName)
		{
			return false;
		}

		if (const UStateAttributeBag* BagStruct = GetCommonBagStruct(StructProperty, BagOwner))
		{
			return BagStruct->FindPropertyDescByName(NewName) == nullptr;
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
}

//////////////////////////////////////////////////////////////////////////
// FAttributeDetailsNode
FAttributeDetailsNode::FAttributeDetailsNode(FText& InName)
	: NodeType(EAttributeDetailsNodeType::None)
	, Index(-1)
	, Owner(nullptr)
	, Name(InName)
	, AttributeDesc(nullptr)
{

}

FAttributeDetailsNode::FAttributeDetailsNode(const FAttributeBagPropertyDesc* InAttributeDesc)
	: NodeType(EAttributeDetailsNodeType::AttributeNode)
	, Index(-1)
	, Owner(nullptr)
	, AttributeDesc(InAttributeDesc)
{

}

TSharedPtr<FAttributeDetailsNode> FAttributeDetailsNode::CreateCategoryNode(FText& InCategory)
{
	TSharedPtr<FAttributeDetailsNode> CategoryNode = MakeShared<FAttributeDetailsNode>(InCategory);
	CategoryNode->NodeType = EAttributeDetailsNodeType::CategoryNode;
	return CategoryNode;
}

TSharedPtr<FAttributeDetailsNode> FAttributeDetailsNode::CreateAttributeNode(const FAttributeBagPropertyDesc* InAttributeDesc)
{
	TSharedPtr<FAttributeDetailsNode> AttributeNode = MakeShared<FAttributeDetailsNode>(InAttributeDesc);
	AttributeNode->Name = FText::FromName(InAttributeDesc->Name);
	return AttributeNode;
}

//////////////////////////////////////////////////////////////////////////
// SAttributeDetailsView
void SAttributeDetailsView::Construct(const FArguments& InArgs)
{
	ViewModel = MakeShared<FAttributeDetailsViewModel>();
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

	AddAttributeWidget = SNew(SHorizontalBox);

	ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f)
			[
				AddAttributeWidget.ToSharedRef()
			]
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
	AttributeBag = nullptr;
	TreeRoots.Empty();
	
	if (IsValid(InSelection))
	{
		BagStructProperty = InSelection->GetClass()->FindPropertyByName(TEXT("AttributeBag"));
		AttributeBag = State::StructUtils::GetAttributeBagFromBagProperty(BagStructProperty, InSelection);
		if (AttributeBag && AttributeBag->IsValid())
		{
			const UStateAttributeBag* AttributeBagStruct = AttributeBag->GetAttributeBagStruct();
			TConstArrayView<FAttributeBagPropertyDesc> ArrayView = AttributeBagStruct->GetPropertyDescs();
			for (const FAttributeBagPropertyDesc* It = ArrayView.begin(); It < ArrayView.end(); ++It)
			{
				TreeRoots.Add(FAttributeDetailsNode::CreateAttributeNode(It));
				TreeRoots.Last()->Index = ArrayView.end() - It;
				TreeRoots.Last()->Owner = InSelection;
			}
		}
	}

	TreeViewWidget->RequestTreeRefresh();
	GenerateAddAttributeWidget();
}

void SAttributeDetailsView::UpdateDetails()
{
	if (TreeViewWidget.IsValid() && AttributeBagOwner.IsValid())
	{
		SetObject(AttributeBagOwner.Get());
	}
}

void SAttributeDetailsView::GenerateAddAttributeWidget()
{
	AddAttributeWidget->ClearChildren();

	if (!BagStructProperty || !AttributeBagOwner.IsValid())
	{
		return;
	}

	AddAttributeWidget->AddSlot()
	.VAlign(VAlign_Center)
	.AutoWidth()
	[
		SNew(SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(LOCTEXT("AddAttribute_Tooltip", "Add new attribute"))
			.OnClicked_Lambda([StructProperty = BagStructProperty, BagOwner = AttributeBagOwner.Get(), this]()
				{
					constexpr int32 MaxIterations = 100;
					FName NewName(TEXT("NewAttribute"));
					int32 Number = 1;
					while (!State::StructUtils::IsUniqueName(NewName, FName(), StructProperty, BagOwner) && Number < MaxIterations)
					{
						Number++;
						NewName.SetNumber(Number);
					}
					if (Number == MaxIterations)
					{
						return FReply::Handled();
					}

					State::StructUtils::ApplyChangesToPropertyDescs(
						LOCTEXT("OnPropertyAdded", "Add Property"), StructProperty, BagOwner,
						[&NewName](TArray<FAttributeBagPropertyDesc>& PropertyDescs)
						{
							PropertyDescs.Emplace(NewName, EAttributeBagPropertyType::Bool, nullptr, PropertyDescs.Num());
						});

					// 重新构建TreeView，否则会有异常
					UpdateDetails();
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

TSharedRef<ITableRow> SAttributeDetailsView::OnGenerateRow(TSharedPtr<FAttributeDetailsNode> InNode, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	return SNew(SAttributeDetailsViewRow, InOwnerTableView, InNode, BagStructProperty, ViewModel);
}

void SAttributeDetailsView::OnGetChildren(TSharedPtr<FAttributeDetailsNode> InParent, TArray<TSharedPtr<FAttributeDetailsNode>>& OutChildren)
{
	if (InParent->NodeType == EAttributeDetailsNodeType::CategoryNode)
	{
		OutChildren = { InParent };
	}
}


//////////////////////////////////////////////////////////////////////////
// SAttributeDetailsViewRow
void SAttributeDetailsViewRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedPtr<FAttributeDetailsNode> InNode, const FProperty* BagStructProperty, TSharedPtr<FAttributeDetailsViewModel> InViewModel)
{
	ViewModel = InViewModel;
	Node = InNode;

	const FProperty* AttibuteProperty = InNode->AttributeDesc->CachedProperty;
	UObject* Owner = InNode->Owner;

	auto GetFilteredVariableTypeTree = [BagStructProperty = BagStructProperty, Owner](TArray<TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>>& TypeTree, ETypeTreeFilter TypeTreeFilter)
		{
			// The SPinTypeSelector popup might outlive this details view, so bag struct property can be invalid here.
			if (!BagStructProperty || !IsValid(Owner))
			{
				return;
			}

			UFunction* IsPinTypeAcceptedFunc = nullptr;
			UObject* IsPinTypeAcceptedTarget = nullptr;
			if (State::StructUtils::FindUserFunction(BagStructProperty, UE::StructUtils::Metadata::IsPinTypeAcceptedName, IsPinTypeAcceptedFunc, IsPinTypeAcceptedTarget))
			{
				check(IsPinTypeAcceptedFunc && IsPinTypeAcceptedTarget);

				// We need to make sure the signature matches perfectly: bool(FEdGraphPinType)
				bool bFuncIsValid = State::StructUtils::ValidateFunctionSignature<bool, FEdGraphPinType>(IsPinTypeAcceptedFunc);
				if (!ensureMsgf(bFuncIsValid, TEXT("[%s] Function %s does not have the right signature."), *UE::StructUtils::Metadata::IsPinTypeAcceptedName.ToString(), *IsPinTypeAcceptedFunc->GetName()))
				{
					return;
				}
			}

			auto IsPinTypeAccepted = [IsPinTypeAcceptedFunc, IsPinTypeAcceptedTarget](const FEdGraphPinType& InPinType) -> bool
				{
					if (IsPinTypeAcceptedFunc && IsPinTypeAcceptedTarget)
					{
						const TValueOrError<bool, void> bIsValid = State::StructUtils::CallFunc<bool>(IsPinTypeAcceptedTarget, IsPinTypeAcceptedFunc, InPinType);
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

						if (!State::StructUtils::CanHaveMemberVariableOfType(ChildPinType) || !IsPinTypeAccepted(ChildPinType))
						{
							PinType->Children.RemoveAt(ChildIndex);
							continue;
						}
					}
					++ChildIndex;
				}

				TypeTree.Add(PinType);
			}
		};

	auto GetPinInfo = [BagStructProperty = BagStructProperty, Owner, AttibuteProperty]()
		{
			// The SPinTypeSelector popup might outlive this details view, so bag struct property can be invalid here.
			if (!BagStructProperty || !AttibuteProperty || !IsValid(Owner))
			{
				return FEdGraphPinType();
			}

			TArray<FAttributeBagPropertyDesc> AttributeDescs = State::StructUtils::GetCommonPropertyDescs(BagStructProperty, Owner);

			if (FAttributeBagPropertyDesc* Desc = AttributeDescs.FindByPredicate([AttibuteProperty](const FAttributeBagPropertyDesc& Desc) { return Desc.CachedProperty == AttibuteProperty; }))
			{
				return State::StructUtils::GetPropertyDescAsPin(*Desc);
			}

			return FEdGraphPinType();
		};

	auto PinInfoChanged = [ViewModel = ViewModel, BagStructProperty = BagStructProperty, Owner, AttibuteProperty](const FEdGraphPinType& PinType)
		{
			// The SPinTypeSelector popup might outlive this details view, so bag struct property can be invalid here.
			if (!BagStructProperty || !AttibuteProperty || !IsValid(Owner))
			{
				return;
			}

			State::StructUtils::ApplyChangesToPropertyDescs(
				LOCTEXT("OnPropertyTypeChanged", "Change Property Type"), BagStructProperty, Owner,
				[&PinType, &AttibuteProperty](TArray<FAttributeBagPropertyDesc>& PropertyDescs)
				{
					// Find and change struct type
					if (FAttributeBagPropertyDesc* Desc = PropertyDescs.FindByPredicate([Property = AttibuteProperty](const FAttributeBagPropertyDesc& Desc) { return Desc.CachedProperty == Property; }))
					{
						State::StructUtils::SetPropertyDescFromPin(*Desc, PinType);
					}
				});

			ViewModel->OnPinInfoChanged.Broadcast();
		};

	ConstructInternal(STableRow::FArguments()
		.Padding(5.0f)
		//.OnDragDetected(this, &SAttributeDetailsViewRow::HandleDragDetected)
		//.OnCanAcceptDrop(this, &SAttributeDetailsViewRow::HandleCanAcceptDrop)
		//.OnAcceptDrop(this, &SAttributeDetailsViewRow::HandleAcceptDrop)
		//.Style(&FStateAbilityEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("StateTree.Selection"))
		, InOwnerTableView);

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
			.IsReadOnly(Node->AttributeDesc->HasAnyFlags(EAttributeFlags::AF_ReadOnly))
			.IsSelected_Lambda([Node = Node](){
				if (!Node->AttributeDesc->HasAnyFlags(EAttributeFlags::AF_ReadOnly))
				{
					// 在TreeView内TextBlock会失去焦点，所以只能用这种方式
					return true;
				}
				return false;
			})
			.OnVerifyTextChanged_Lambda([BagStructProperty = BagStructProperty, Owner, AttibuteProperty](const FText& InText, FText& OutErrorMessage)
				{
					const FName NewName = State::StructUtils::GetValidPropertyName(InText.ToString());
					const FName OldName = AttibuteProperty->GetFName();
					bool bResult = State::StructUtils::IsUniqueName(NewName, OldName, BagStructProperty, Owner);
					if (!bResult)
					{
						OutErrorMessage = LOCTEXT("MustBeUniqueName", "Attribute must have unique name");
					}
					return bResult;
				})
			.OnTextCommitted_Lambda([BagStructProperty = BagStructProperty, Owner, AttibuteProperty, ViewModel = ViewModel](const FText& InNewText, ETextCommit::Type InCommitType)
				{
					if (InCommitType == ETextCommit::OnCleared)
					{
						return;
					}

					const FName NewName = State::StructUtils::GetValidPropertyName(InNewText.ToString());
					const FName OldName = AttibuteProperty->GetFName();
					if (!State::StructUtils::IsUniqueName(NewName, OldName, BagStructProperty, Owner))
					{
						return;
					}

					State::StructUtils::ApplyChangesToPropertyDescs(
						LOCTEXT("OnAttributeNameChanged", "Change Attribute Name"), BagStructProperty, Owner,
						[&NewName, &OldName](TArray<FAttributeBagPropertyDesc>& PropertyDescs)
						{
							if (FAttributeBagPropertyDesc* Desc = PropertyDescs.FindByPredicate([OldName = OldName](const FAttributeBagPropertyDesc& Desc) { return Desc.Name == OldName; }))
							{
								Desc->Name = NewName;
							}
						});

					// 重新构建TreeView，否则会有异常
					ViewModel->OnPinInfoChanged.Broadcast();
				})
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Left)
		.FillWidth(0.5f)
		[
			SAssignNew(PinTypeSelector, SPinTypeSelector, FGetPinTypeTree::CreateLambda(GetFilteredVariableTypeTree))
			.TargetPinType_Lambda(GetPinInfo)
			.OnPinTypeChanged_Lambda(PinInfoChanged)
			.Schema(GetDefault<UAttributeBagSchema>())
			.bAllowArrays(true)
			.TypeTreeFilter(ETypeTreeFilter::None)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ReadOnly(Node->AttributeDesc->HasAnyFlags(EAttributeFlags::AF_ReadOnly))
		]
	];
}

FText SAttributeDetailsViewRow::GetAttributeName() const
{
	return Node->Name;
}

#undef LOCTEXT_NAMESPACE