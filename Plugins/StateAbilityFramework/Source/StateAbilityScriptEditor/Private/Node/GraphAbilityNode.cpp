#include "Node/GraphAbilityNode.h"

#include "Kismet2/KismetEditorUtilities.h"

#include "StateAbilityScriptEditorData.h"
#include "Component/StateAbility/Script/StateAbilityScriptArchetype.h"

#define LOCTEXT_NAMESPACE "GraphAbilityNode"

void UGraphAbilityNode::InitializeNode(UEdGraph* InGraph)
{
	// empty in base class
}

void UGraphAbilityNode::PostPlacedNewNode()
{
	// NodeInstance can be already spawned by paste operation, don't override it

	if (NodeClass && (NodeInstance == nullptr))
	{
		UEdGraph* MyGraph = GetGraph();
		//UObject* GraphOwner = MyGraph ? MyGraph->GetOuter() : nullptr;
		if (MyGraph)
		{
			// 我们不再默认实例化，将是否实例化交由子类自己决定

			InitializeNode(MyGraph);

			if (NodeInstance)
			{
				// Redo / Undo
				NodeInstance->SetFlags(RF_Transactional);
				NodeInstance->ClearFlags(RF_Transient);
			}
		}
	}
}

bool UGraphAbilityNode::CanDuplicateNode() const
{
	return true;
}

bool UGraphAbilityNode::CanUserDeleteNode() const
{
	return true;
}

void UGraphAbilityNode::DestroyNode()
{
	UEdGraphNode::DestroyNode();

	UStateAbilityScriptArchetype* ScriptArchetype = GetGraph()->GetTypedOuter<UStateAbilityScriptArchetype>();
	TSharedPtr<FAbilityScriptViewModel> ScriptViewModel = Cast<UStateAbilityScriptEditorData>(ScriptArchetype->EditorData)->GetScriptViewModel();
	
	for (UEdGraphPin* Pin : Pins)
	{
		ScriptViewModel->ClearRecordedEventslot(Pin);
	}
}

void UGraphAbilityNode::PostEditUndo()
{
	Super::PostEditUndo();

	ResetNodeOwner();
}

void UGraphAbilityNode::PostCopyNode()
{
	ResetNodeOwner();
}


void UGraphAbilityNode::PrepareForCopying()
{
	if (NodeInstance)
	{
		// Temporarily take ownership of the node instance, so that it is not deleted when cutting
		// Because until now, node has always been owned by GraphOwner (Asset)
		NodeInstance->Rename(nullptr, this, REN_DontCreateRedirectors | REN_DoNotDirty);
	}
}

void UGraphAbilityNode::ResetNodeOwner()
{
	if (IsValid(NodeInstance))
	{
		UStateAbilityScriptArchetype* ScriptArchetype = GetGraph()->GetTypedOuter<UStateAbilityScriptArchetype>();
		NodeInstance->Rename(NULL, ScriptArchetype, REN_DontCreateRedirectors | REN_DoNotDirty);
		NodeInstance->ClearFlags(RF_Transient);
	}
}

void UGraphAbilityNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	Super::AutowireNewNode(FromPin);

	if (FromPin != nullptr)
	{
		UEdGraphPin* OutputPin = GetOutputPin();

		if (GetSchema()->TryCreateConnection(FromPin, GetInputPin()))
		{
			FromPin->GetOwningNode()->NodeConnectionListChanged();
		}
		else if (OutputPin != nullptr && GetSchema()->TryCreateConnection(OutputPin, FromPin))
		{
			NodeConnectionListChanged();
		}
	}
}

void UGraphAbilityNode::NodeConnectionListChanged()
{
	Super::NodeConnectionListChanged();

	/*Cast<USDTGraph>(GetGraph())->UpdateAsset();*/
}


#undef LOCTEXT_NAMESPACE