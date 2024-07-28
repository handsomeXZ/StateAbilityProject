#include "Component/StateAbility/Script/StateAbilityScriptArchetype.h"

#include "Component/StateAbility/Script/StateAbilityScript.h"

#if WITH_EDITOR
#include "CookerSettings.h"
#include "Editor.h"
#else
#include "UObject/LinkerLoad.h"
#endif //WITH_EDITOR

//////////////////////////////////////////////////////////////////////////
// UStateAbilityScriptArchetype
UStateAbilityScriptArchetype::UStateAbilityScriptArchetype(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, RootNodeID(0)
	, GeneratedScriptClass(nullptr)
	, EditorData(nullptr)
{

}

UStateAbilityScript* UStateAbilityScriptArchetype::GetDefaultScript()
{
	return Cast<UStateAbilityScript>(GeneratedScriptClass->GetDefaultObject(false));
}

#if WITH_EDITOR

bool UStateAbilityScriptArchetype::RenameGeneratedClasses(const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags)
{
	const bool bRenameGeneratedClasses = !(Flags & REN_SkipGeneratedClasses);

	if (bRenameGeneratedClasses)
	{
		const auto TryFreeCDOName = [](UClass* ForClass, UObject* ToOuter, ERenameFlags InFlags)
			{
				if (ForClass->ClassDefaultObject)
				{
					FName CDOName = ForClass->GetDefaultObjectName();

					if (UObject* Obj = StaticFindObjectFast(UObject::StaticClass(), ToOuter, CDOName))
					{
						FName NewName = MakeUniqueObjectName(ToOuter, Obj->GetClass(), CDOName);
						Obj->Rename(*(NewName.ToString()), ToOuter, InFlags | REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
					}
				}
			};

		const auto CheckRedirectors = [](FName ClassName, UClass* ForClass, UObject* NewOuter)
			{
				if (UObjectRedirector* Redirector = FindObjectFast<UObjectRedirector>(NewOuter, ClassName))
				{
					// If we found a redirector, check that the object it points to is of the same class.
					if (Redirector->DestinationObject
						&& Redirector->DestinationObject->GetClass() == ForClass->GetClass())
					{
						Redirector->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
					}
				}
			};

		FName GenClassName;
		GetScriptClassNames(GenClassName, FName(InName));

		UPackage* NewTopLevelObjectOuter = NewOuter ? NewOuter->GetOutermost() : NULL;
		if (GeneratedScriptClass != NULL)
		{
			// check for collision of CDO name, move aside if necessary:
			TryFreeCDOName(GeneratedScriptClass, NewTopLevelObjectOuter, Flags);
			CheckRedirectors(GenClassName, GeneratedScriptClass, NewTopLevelObjectOuter);
			bool bMovedOK = GeneratedScriptClass->Rename(*GenClassName.ToString(), NewTopLevelObjectOuter, Flags);
			if (!bMovedOK)
			{
				return false;
			}
		}

	}
	return true;
}

bool UStateAbilityScriptArchetype::Rename(const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags)
{
	const FName OldName = GetFName();

	// Move generated class/CDO to the new package, to create redirectors
	if (!RenameGeneratedClasses(InName, NewOuter, Flags))
	{
		return false;
	}

	if (Super::Rename(InName, NewOuter, Flags))
	{
		return true;
	}

	return false;
}

void UStateAbilityScriptArchetype::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	UObject* ScriptCDO = nullptr;
	if (GeneratedScriptClass)
	{
		ScriptCDO = GeneratedScriptClass->GetDefaultObject(/*bCreateIfNeeded*/false);
		if (ScriptCDO)
		{
			ScriptCDO->GetAssetRegistryTags(OutTags);
		}
	}

	Super::GetAssetRegistryTags(OutTags);
}

void UStateAbilityScriptArchetype::PostLoadAssetRegistryTags(const FAssetData& InAssetData, TArray<FAssetRegistryTag>& OutTagsAndValuesToUpdate) const
{
	Super::PostLoadAssetRegistryTags(InAssetData, OutTagsAndValuesToUpdate);

	auto FixTagValueShortClassName = [&InAssetData, &OutTagsAndValuesToUpdate](FName TagName, FAssetRegistryTag::ETagType TagType)
		{
			FString TagValue = InAssetData.GetTagValueRef<FString>(TagName);
			if (!TagValue.IsEmpty() && TagValue != TEXT("None"))
			{
				if (UClass::TryFixShortClassNameExportPath(TagValue, ELogVerbosity::Warning,
					TEXT("UStateAbilityScriptArchetype::PostLoadAssetRegistryTags"), true /* bClearOnError */))
				{
					OutTagsAndValuesToUpdate.Add(FAssetRegistryTag(TagName, TagValue, TagType));
				}
			}
		};

	static const FName GeneratedClassPath(TEXT("GeneratedScriptClass"));
	FixTagValueShortClassName(GeneratedClassPath, FAssetRegistryTag::TT_Hidden);
}

FPrimaryAssetId UStateAbilityScriptArchetype::GetPrimaryAssetId() const
{
	// Forward to our Class, which will forward to CDO if needed
	// We use Generated instead of Skeleton because the CDO data is more accurate on Generated
	if (GeneratedScriptClass && GeneratedScriptClass->ClassDefaultObject)
	{
		return GeneratedScriptClass->GetPrimaryAssetId();
	}

	return FPrimaryAssetId();
}

bool UStateAbilityScriptArchetype::NeedsLoadForClient() const
{
	return false;
}

bool UStateAbilityScriptArchetype::NeedsLoadForServer() const
{
	return false;
}

bool UStateAbilityScriptArchetype::NeedsLoadForEditorGame() const
{
	return true;
}

void UStateAbilityScriptArchetype::PostInitProperties()
{
	Super::PostInitProperties();
	UE_LOG(LogTemp, Warning, TEXT("Archetype Loaded"));
}

#endif

//////////////////////////////////////////////////////////////////////////
// UStateAbilityScriptClass
UStateAbilityScriptClass::UStateAbilityScriptClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

FPrimaryAssetId UStateAbilityScriptClass::GetPrimaryAssetId() const
{
	FPrimaryAssetId AssetId;
	if (!ClassDefaultObject)
	{
		// All UStateAbilityScriptClass objects should have a pointer to their generated ClassDefaultObject, except for the
		// CDO itself of the UStateAbilityScriptClass class.
		verify(HasAnyFlags(RF_ClassDefaultObject));
		return AssetId;
	}

	AssetId = ClassDefaultObject->GetPrimaryAssetId();

	return AssetId;
}

bool UStateAbilityScriptClass::NeedsLoadForServer() const
{
	// This logic can't be used for targets that use editor content because UBlueprint::NeedsLoadForEditorGame
	// returns true and forces all UBlueprints to be loaded for -game or -server runs. The ideal fix would be
	// to remove UBlueprint::NeedsLoadForEditorGame, after that it would be nice if we could just implement
	// UBlueprint::NeedsLoadForEditorGame here, but we can't because then our CDO doesn't get loaded. We *could*
	// fix that behavior, but instead I'm just abusing IsRunningCommandlet() so that this logic only runs during cook:
	if (IsRunningCommandlet() && !HasAnyFlags(RF_ClassDefaultObject))
	{
		if (ensure(GetSuperClass()) && !GetSuperClass()->NeedsLoadForServer())
		{
			return false;
		}
		if (ensure(ClassDefaultObject) && !ClassDefaultObject->NeedsLoadForServer())
		{
			return false;
		}
	}
	return Super::NeedsLoadForServer();
}

bool UStateAbilityScriptClass::NeedsLoadForClient() const
{
	// This logic can't be used for targets that use editor content because UBlueprint::NeedsLoadForEditorGame
	// returns true and forces all UBlueprints to be loaded for -game or -server runs. The ideal fix would be
	// to remove UBlueprint::NeedsLoadForEditorGame, after that it would be nice if we could just implement
	// UBlueprint::NeedsLoadForEditorGame here, but we can't because then our CDO doesn't get loaded. We *could*
	// fix that behavior, but instead I'm just abusing IsRunningCommandlet() so that this logic only runs during cook:
	if (IsRunningCommandlet() && !HasAnyFlags(RF_ClassDefaultObject))
	{
		if (ensure(GetSuperClass()) && !GetSuperClass()->NeedsLoadForClient())
		{
			return false;
		}
		if (ensure(ClassDefaultObject) && !ClassDefaultObject->NeedsLoadForClient())
		{
			return false;
		}
	}
	return Super::NeedsLoadForClient();
}

bool UStateAbilityScriptClass::NeedsLoadForEditorGame() const
{
	return true;
}

void UStateAbilityScriptClass::PostInitProperties()
{
	Super::PostInitProperties();
	UE_LOG(LogTemp, Warning, TEXT("ScriptClass Loaded"));
}


void UStateAbilityScriptClass::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
#if WITH_EDITOR
	// Make UStateAbilityScriptClass from a cooked package standalone so it doesn't get GCed
	if (GEditor && bCooked)
	{
		SetFlags(RF_Standalone);
	}
#endif //if WITH_EDITOR
#endif // WITH_EDITORONLY_DATA

	// ReferenceToken
	AssembleReferenceTokenStream(true);
}

void UStateAbilityScriptClass::Link(FArchive& Ar, bool bRelinkExistingProperties)
{
	Super::Link(Ar, bRelinkExistingProperties);

	// ReferenceToken
	AssembleReferenceTokenStream(true);
}

void UStateAbilityScriptClass::InitPropertiesFromCustomList(uint8* DataPtr, const uint8* DefaultDataPtr)
{
	// 全部浅拷贝
	/*FMemory::Memcpy(DataPtr, DefaultDataPtr, GetPropertiesSize());*/	// 会拷贝一些没必要的数据，例如Name，Outer，Flags，ObjIndex等

	// 依然全部浅拷贝
	for (FProperty* P = this->PropertyLink; P; P = P->PropertyLinkNext)
	{
		bool bIsTransient = P->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient);
		if (!bIsTransient || !P->ContainsInstancedObjectProperty())
		{
			P->CopyCompleteValue_InContainer(DataPtr, DefaultDataPtr);
		}
	}
}

//UObject* UStateAbilityScriptClass::GetArchetypeForCDO() const
//{
//	if (OverridenArchetypeForCDO)
//	{
//		ensure(OverridenArchetypeForCDO->IsA(GetSuperClass()));
//		return OverridenArchetypeForCDO;
//	}
//
//	return Super::GetArchetypeForCDO();
//}
