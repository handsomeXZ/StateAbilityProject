
#pragma once

#include "CoreMinimal.h"
#include "Node/GraphAbilityNode.h"
#include "GraphAbilityNode_Action.generated.h"

class UObject;

UCLASS(MinimalAPI)
class UGraphAbilityNode_Action : public UGraphAbilityNode
{
	GENERATED_BODY()
public:
	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual bool CanDuplicateNode() const override { return true; }
	//~ End UEdGraphNode Interface

	//~ Begin USCTGraphNode Interface
	virtual UEdGraphPin* GetInputPin() const override;
	virtual UEdGraphPin* GetOutputPin() const override;
	virtual FString GetNodeName() const override;
	virtual void InitializeNode(UEdGraph* InGraph) override;
	//~ End USCTGraphNode Interface

	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface

	DECLARE_MULTICAST_DELEGATE(FOnUpdateGraphNode);
	FOnUpdateGraphNode OnUpdateGraphNode;

	UClass* GetNodeClass() const;

};
