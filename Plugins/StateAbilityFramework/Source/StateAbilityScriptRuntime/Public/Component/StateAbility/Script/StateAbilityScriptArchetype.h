#pragma once

#include "CoreMinimal.h"

#include "Attribute/AttributeBag.h"
#include "Component/StateAbility/StateAbilityAction.h"
#include "Component/StateAbility/StateAbilityState.h"
#include "Component/StateAbility/Script/StateAbilityScriptNetProto.h"

#include "StateAbilityScriptArchetype.generated.h"

class UStateAbilityScriptClass;
class UStateAbilityScript;

UCLASS()
class STATEABILITYSCRIPTRUNTIME_API UStateAbilityScriptArchetype : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY()
	uint32 RootNodeID;
	UPROPERTY()
	TArray<UStateAbilityState*> StateTemplates;
	UPROPERTY()
	TMap<uint32, UStateAbilityNodeBase*> ActionSequenceMap;
	UPROPERTY()
	TMap<uint32, UStateAbilityNodeBase*> ActionMap;
	UPROPERTY()
	TMap<FGuid, uint32> EventSlotMap;

	UPROPERTY()
	FGuid PakUID;
	UPROPERTY(nontransactional)
	UStateAbilityScriptClass* GeneratedScriptClass;

	UPROPERTY()
	FStateAbilityScriptNetProto NetDeltasProtocal;

	UStateAbilityScript* GetDefaultScript();

#if WITH_EDITOR
	void Reset()
	{
		RootNodeID = 0;
		StateTemplates.Empty();
		ActionSequenceMap.Empty();
		ActionMap.Empty();
		EventSlotMap.Empty();
	}
#endif

#if WITH_EDITOR
	void GetScriptClassNames(FName& GeneratedClassName, FName NameOverride = NAME_None) const
	{
		FName NameToUse = (NameOverride != NAME_None) ? NameOverride : GetFName();

		const FString GeneratedClassNameString = FString::Printf(TEXT("%s_C"), *NameToUse.ToString());
		GeneratedClassName = FName(*GeneratedClassNameString);
	}

	/** Renames only the generated classes. Should only be used internally or when testing for rename. */
	virtual bool RenameGeneratedClasses(const TCHAR* NewName = nullptr, UObject* NewOuter = nullptr, ERenameFlags Flags = REN_None);

	//~ Begin UObject Interface (WITH_EDITOR)
	//virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
	virtual bool Rename(const TCHAR* NewName = nullptr, UObject* NewOuter = nullptr, ERenameFlags Flags = REN_None) override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual void PostLoadAssetRegistryTags(const FAssetData& InAssetData, TArray<FAssetRegistryTag>& OutTagsAndValuesToUpdate) const;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual bool NeedsLoadForServer() const override;
	virtual bool NeedsLoadForClient() const override;
	virtual bool NeedsLoadForEditorGame() const override;
	virtual void PostInitProperties() override;
	//~ End UObject interface
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UObject> EditorData;
#endif
};


UCLASS()
class STATEABILITYSCRIPTRUNTIME_API UStateAbilityScriptClass : public UClass
{
	GENERATED_UCLASS_BODY()
	public:
	//~ UObject interface
	//virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual bool NeedsLoadForServer() const override;
	virtual bool NeedsLoadForClient() const override;
	virtual bool NeedsLoadForEditorGame() const override;
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	//~ End UObject interface

	//~ UClass interface
	//virtual UObject* GetArchetypeForCDO() const override;
	virtual void InitPropertiesFromCustomList(uint8* DataPtr, const uint8* DefaultDataPtr);
	virtual void Link(FArchive& Ar, bool bRelinkExistingProperties) override;
	//~ End UClass interface

	//UPROPERTY()
	//TObjectPtr<UObject> OverridenArchetypeForCDO;
};
