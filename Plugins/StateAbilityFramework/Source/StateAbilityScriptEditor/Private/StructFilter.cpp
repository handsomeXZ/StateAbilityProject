#include "StructFilter.h"

#define LOCTEXT_NAMESPACE "StructFilter"

void FStructNodeBase::AddChild(const TSharedRef<FStructNodeBase>& InChild)
{
	InChild->ParentNode = AsShared().ToWeakPtr();
	Children.Add(InChild);
}

FStructNodeData::FStructNodeData()
	: StructDisplayName(LOCTEXT("None", "None"))
{
	StructName = TEXT("None");
	CategoryName = TEXT("None");
	NodeType = EStructNodeType::Node;
}

FStructNodeData::FStructNodeData(const UScriptStruct* InStruct)
	: StructDisplayName(InStruct->GetDisplayNameText())
	, StructPath(*InStruct->GetPathName())
	, Struct(InStruct)
{
	NodeType = EStructNodeType::Node;
	StructName = InStruct->GetName();
	CategoryName = InStruct->GetMetaData(TEXT("Category"));

	if (const UScriptStruct* ParentStruct = Cast<UScriptStruct>(InStruct->GetSuperStruct()))
	{
		ParentStructPath = *ParentStruct->GetPathName();
	}
}


FStructNodeCategory::FStructNodeCategory(const FString& InCategoryName)
{
	NodeType = EStructNodeType::Category;
	StructName = TEXT("None");
	CategoryName = InCategoryName;
}

FSimpleStructFilter::FSimpleStructFilter(UScriptStruct* InBaseStruct)
{
	StructRootNode = MakeShared<FStructNodeData>();

	TMap<const UScriptStruct*, TSharedPtr<FStructNodeData>> DataNodes;
	DataNodes.Add(nullptr, StructRootNode);

	TSet<const UScriptStruct*> Visited;
	UPackage* TransientPackage = GetTransientPackage();

	for (TObjectIterator<UScriptStruct> StructIt; StructIt; ++StructIt)
	{
		const UScriptStruct* CurrentStruct = *StructIt;
		if (Visited.Contains(CurrentStruct) || CurrentStruct->GetOutermost() == TransientPackage)
		{
			// Skip transient structs as they are dead leftovers from user struct editing
			continue;
		}

		while (CurrentStruct)
		{
			if (!CurrentStruct->IsChildOf(InBaseStruct))
			{
				break;
			}
			const UScriptStruct* SuperStruct = Cast<UScriptStruct>(CurrentStruct->GetSuperStruct());

			TSharedPtr<FStructNodeData>& ParentEntryRef = DataNodes.FindOrAdd(SuperStruct);
			if (!ParentEntryRef.IsValid())
			{
				check(SuperStruct); // The null entry should have been created above
				ParentEntryRef = MakeShared<FStructNodeData>(SuperStruct);
			}

			// Need to have a pointer in-case the ref moves to avoid an extra look up in the map
			TSharedPtr<FStructNodeData> ParentEntry = ParentEntryRef;

			TSharedPtr<FStructNodeData>& MyEntryRef = DataNodes.FindOrAdd(CurrentStruct);
			if (!MyEntryRef.IsValid())
			{
				MyEntryRef = MakeShared<FStructNodeData>(CurrentStruct);
			}

			// Need to re-acquire the reference in the struct as it may have been invalidated from a re-size
			if (!Visited.Contains(CurrentStruct))
			{
				ParentEntry->AddChild(MyEntryRef.ToSharedRef());
				Visited.Add(CurrentStruct);
			}

			CurrentStruct = SuperStruct;
		}
	}
}

TSharedPtr<FStructNodeBase> FSimpleStructFilter::GatherStructAsTree(UScriptStruct* InFilterStruct)
{
	const FString FilterStructName = InFilterStruct->GetName();
	TArray<TSharedPtr<FStructNodeBase>> WaitingList = {StructRootNode};
	while (!WaitingList.IsEmpty())
	{
		TSharedPtr<FStructNodeBase> Current = WaitingList.Pop();
		if (FilterStructName == Current->StructName)
		{
			return Current;
		}
		WaitingList.Append(Current->Children);
	}

	return nullptr;
}

void FSimpleStructFilter::GatherStructAsList(UScriptStruct* InFilterStruct, TArray<TSharedPtr<FStructNodeBase>>& GatheredStructs, bool bIncludeBaseStruct /* = false */)
{
	TMap<FName, TSharedPtr<FStructNodeBase>> CategoryNodesMap;

	TSharedPtr<FStructNodeBase> BaseStructNode = GatherStructAsTree(InFilterStruct);
	if (!BaseStructNode.IsValid())
	{
		return;
	}

	TArray<TSharedPtr<FStructNodeBase>> WaitingList;
	if (bIncludeBaseStruct)
	{
		WaitingList.Add(BaseStructNode);
	}
	else
	{
		WaitingList.Append(BaseStructNode->Children);
	}

	while (!WaitingList.IsEmpty())
	{
		TSharedPtr<FStructNodeBase> Current = WaitingList.Pop();
		// Category
		FString CategoryName = Current->CategoryName;
		if (CategoryName.IsEmpty())
		{
			CategoryName = LOCTEXT("SConfigVarsTypeSchemaMenu", "Generic").ToString();
		}
		TSharedPtr<FStructNodeBase>& CategoryNodeRef = CategoryNodesMap.FindOrAdd(FName(CategoryName));
		if (!CategoryNodeRef.IsValid())
		{
			CategoryNodeRef = MakeShared<FStructNodeCategory>(CategoryName);
			GatheredStructs.Add(CategoryNodeRef);
		}
		TSharedPtr<FStructNodeBase> CategoryNode = CategoryNodeRef;
		CategoryNode->AddChild(Current.ToSharedRef());

		WaitingList.Append(Current->Children);
	}
}

#undef LOCTEXT_NAMESPACE