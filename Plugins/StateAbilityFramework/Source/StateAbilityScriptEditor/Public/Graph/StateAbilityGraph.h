
#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "StateAbilityGraph.generated.h"

class UObject;

enum EUpdateFlags
{
	RebuildGraph = 0,
};

UCLASS(MinimalAPI)
class UStateAbilityGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()

	virtual void Init();
	virtual void OnNodesPasted(const FString& ImportStr);
	virtual void UpdateAsset(EUpdateFlags UpdateFlags = EUpdateFlags::RebuildGraph) {}
	virtual void OnCompile() {}

	void UpdateAsset_Internal(EUpdateFlags UpdateFlags = EUpdateFlags::RebuildGraph);

	bool IsLocked() const;
	void LockUpdates();
	void UnlockUpdates();
	void OnSave();

protected:

	virtual void PostEditUndo() override;

private:

	/** if set, graph modifications won't cause updates in internal tree structure
	 *  flag allows freezing update during heavy changes like pasting new nodes 
	 */
	uint32 bLockUpdates : 1;

};

