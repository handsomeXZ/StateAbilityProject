
#include "Graph/StateAbilityGraph.h"

#include "SGraphNode.h"
#include "SGraphPanel.h"

#define LOCTEXT_NAMESPACE "StateAbilityGraph"

/////////////////////////////////////////////////////
// UStateAbilityGraph

UStateAbilityGraph::UStateAbilityGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAllowDeletion = false;
	bAllowRenaming = true;
	bLockUpdates = false;
}

void UStateAbilityGraph::UpdateAsset_Internal(int32 UpdateFlags)
{
	if (bLockUpdates)
	{
		return;
	}

	UpdateAsset(UpdateFlags);
}

void UStateAbilityGraph::Init()
{
	// Initialize the graph
	const UEdGraphSchema* MySchema = GetSchema();
	MySchema->CreateDefaultNodesForGraph(*this);
}

void UStateAbilityGraph::OnNodesPasted(const FString& ImportStr)
{
	// empty in base class
}

bool UStateAbilityGraph::IsLocked() const
{
	return bLockUpdates;
}

void UStateAbilityGraph::LockUpdates()
{
	bLockUpdates = true;
}

void UStateAbilityGraph::UnlockUpdates()
{
	bLockUpdates = false;
	UpdateAsset_Internal();
}

void UStateAbilityGraph::OnSave()
{
	UpdateAsset_Internal();
}

#undef LOCTEXT_NAMESPACE
