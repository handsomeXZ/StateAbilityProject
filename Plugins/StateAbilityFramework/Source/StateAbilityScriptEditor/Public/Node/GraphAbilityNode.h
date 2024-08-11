// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "GraphAbilityNode.generated.h"

/**
 *
 */
UCLASS()
class UGraphAbilityNode : public UEdGraphNode
{
	GENERATED_BODY()
public:
	UGraphAbilityNode()
		: NodeClass(nullptr)
		, NodeInstance(nullptr)
	{}

	/** instance class */
	UPROPERTY()
	UClass* NodeClass;

	UPROPERTY()
	TObjectPtr<UObject> NodeInstance;

	//~ Begin UEdGraphNode Interface
	virtual void PostPlacedNewNode() override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	virtual void NodeConnectionListChanged() override;
	virtual void PrepareForCopying() override;
	virtual bool CanDuplicateNode() const override;
	virtual bool CanUserDeleteNode() const override;
	virtual void DestroyNode() override;
	virtual void PostEditUndo() override;
	//~ End UEdGraphNode Interface

	// @return the input pin for this Node
	virtual UEdGraphPin* GetInputPin() const { return nullptr; }

	// @return the output pin for this Node
	virtual UEdGraphPin* GetOutputPin() const { return nullptr; }

	// @return the name of this Node
	virtual FString GetNodeName() const { return TEXT("GraphAbilityNode"); }

	virtual void PostCopyNode();

	/** initialize instance object  */
	virtual void InitializeNode(UEdGraph* InGraph);

	virtual void ResetNodeOwner();

};
