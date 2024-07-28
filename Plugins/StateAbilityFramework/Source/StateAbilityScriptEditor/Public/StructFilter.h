#pragma once

#include "CoreMinimal.h"

enum class EStructNodeType : uint8
{
	Node,
	Category,
};

class FStructNodeBase : public TSharedFromThis<FStructNodeBase>
{
public:
	void AddChild(const TSharedRef<FStructNodeBase>& InChild);

	EStructNodeType NodeType;
	FString StructName;
	FString CategoryName;
	TWeakPtr<FStructNodeBase> ParentNode;
	TArray<TSharedPtr<FStructNodeBase>> Children;
};

class FStructNodeData : public FStructNodeBase
{
public:
	FStructNodeData();
	FStructNodeData(const UScriptStruct* InStruct);

	const UScriptStruct* GetStruct() const { return Struct.Get(); }

	/** The localized name of the struct we represent */
	mutable FText StructDisplayName;

	/** The full object path to the struct we represent */
	FSoftObjectPath StructPath;

	/** The full object path to the parent of the struct we represent */
	FSoftObjectPath ParentStructPath;

	/** The struct that we represent (for loaded struct assets, or native structs) */
	mutable TWeakObjectPtr<const UScriptStruct> Struct;
};

class FStructNodeCategory : public FStructNodeBase
{
public:
	FStructNodeCategory(const FString& InCategoryName);
};

struct FSimpleStructFilter
{
	FSimpleStructFilter(UScriptStruct* InBaseStruct);

	TSharedPtr<FStructNodeBase> GatherStructAsTree(UScriptStruct* InFilterStruct);
	void GatherStructAsList(UScriptStruct* InFilterStruct, TArray<TSharedPtr<FStructNodeBase>>& GatheredStructs, bool bIncludeBaseStruct = false);

private:
	TSharedPtr<FStructNodeData> StructRootNode;
};