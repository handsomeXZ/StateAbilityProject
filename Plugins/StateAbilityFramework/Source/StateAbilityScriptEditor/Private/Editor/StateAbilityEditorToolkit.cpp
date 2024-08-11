#include "Editor/StateAbilityEditorToolkit.h"

#include "Component/StateAbility/Script/StateAbilityScript.h"
#include "Component/StateAbility/StateAbilityState.h"
#include "StateAbilityScriptEditorData.h"

#include "Graph/StateAbilityScriptGraph.h"
#include "StateAbilityScriptEdGraphSchema.h"
#include "StateAbilityScriptEditor.h"
#include "Node/GraphAbilityNode_Entry.h"
#include "Node/GraphAbilityNode_Action.h"
#include "Editor/StateAbilityEditorToolbar.h"
#include "Editor/StateAbilityEditorCommands.h"
#include "Factory/SAEditorModes.h"
#include "Asset/StateAbilityAssetFactory.h"

#include "Framework/Commands/GenericCommands.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "EdGraphUtilities.h"
#include "Windows/WindowsPlatformApplicationMisc.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"

// StateTree
#include "StateTree/ScriptStateTreeView.h"
#include "StateTree/AttributeDetailsView.h"

#define LOCTEXT_NAMESPACE "StateAbilityEditor"

namespace StateAbilityEditorUtils
{
	template <typename Type>
	class FNewNodeClassFilter : public IClassViewerFilter
	{
	public:
		virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			if (InClass != nullptr)
			{
				return InClass->IsChildOf(Type::StaticClass());
			}
			return false;
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			return InUnloadedClassData->IsChildOf(Type::StaticClass());
		}
	};

	class FChildClassFilter : public IClassViewerFilter
	{
	public:
		virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			TArray<TSharedRef<IClassViewerFilter>> Filters = InInitOptions.ClassFilters;
			check(Filters.Num());
			return false;
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			return true;//InUnloadedClassData->IsChildOf(Type::StaticClass());
		}
	};

	/** Given a selection of nodes, return the instances that should be selected be selected for editing in the property panel */
	UObject* GetSelectionForPropertyEditor(UObject* InSelection);


}

const FName FStateAbilityEditor::StateTreeModeName(TEXT("StateTreeMode"));
const FName FStateAbilityEditor::NodeGraphModeName(TEXT("NodeGraphMode"));

const FName FStateAbilityEditor::StateTreeEditorTab(TEXT("StateTreeEditorTab"));
const FName FStateAbilityEditor::NodeGraphEditorTab(TEXT("NodeGraphEditorTab"));

const FName FStateAbilityEditor::ConfigVarsDetailsTab(TEXT("ConfigVarsDetailsTab"));
const FName FStateAbilityEditor::AttributeDetailsTab(TEXT("AttributeDetailsTab"));

//////////////////////////////////////////////////////////////////////////
UObject* StateAbilityEditorUtils::GetSelectionForPropertyEditor(UObject* InSelection)
{
	if (InSelection == nullptr)
	{
		return nullptr;
	}
	else if (UGraphAbilityNode* GraphAbilityNode = Cast<UGraphAbilityNode>(InSelection))
	{
		return GraphAbilityNode->NodeInstance;
	}
	else if (UStateTreeStateNode* StateTreeStateNode = Cast<UStateTreeStateNode>(InSelection))
	{
		return StateTreeStateNode->StateInstance;
	}
	else if (UStateTreeSharedNode* StateTreeSharedNode = Cast<UStateTreeSharedNode>(InSelection))
	{
		return GetSelectionForPropertyEditor(StateTreeSharedNode->SharedNode.Get());
	}


	return InSelection;
}
//////////////////////////////////////////////////////////////////////////
FStateAbilityEditor::FStateAbilityEditor()
	: FWorkflowCentricApplication()
	, StateAbilityScriptArchetype(nullptr)
{
	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	if (Editor != NULL)
	{
		Editor->RegisterForUndo(this);
	}
}

FStateAbilityEditor::~FStateAbilityEditor()
{
	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	if (Editor)
	{
		Editor->UnregisterForUndo(this);
	}
}

void FStateAbilityEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FWorkflowCentricApplication::RegisterTabSpawners(InTabManager);
}


void FStateAbilityEditor::RegisterToolbarTab(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
}

void FStateAbilityEditor::SetCurrentMode(FName NewMode)
{
	if (NewMode == NodeGraphModeName)
	{
		CurrentGraphEditor = NodeGraphEditor;
	}


	FWorkflowCentricApplication::SetCurrentMode(NewMode);
}

void FStateAbilityEditor::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
		// Clear selection, to avoid holding refs to nodes that go away
		if (NodeGraphEditor.IsValid())
		{
			NodeGraphEditor->ClearSelectionSet();
			NodeGraphEditor->NotifyGraphChanged();
		}
		FSlateApplication::Get().DismissAllMenus();
	}
}

void FStateAbilityEditor::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{
		// Clear selection, to avoid holding refs to nodes that go away
		if (NodeGraphEditor.IsValid())
		{
			NodeGraphEditor->ClearSelectionSet();
			NodeGraphEditor->NotifyGraphChanged();
		}
		FSlateApplication::Get().DismissAllMenus();
	}
}

TSharedRef<SWidget> FStateAbilityEditor::SpawnStateTreeEdTab()
{
	// return SAssignNew(StateTreeEditor, SScriptStateTreeView, StateTreeViewModel, ToolkitCommands);
	return SNew(SHorizontalBox);
}

TSharedRef<SWidget> FStateAbilityEditor::SpawnConfigVarsDetailsTab()
{
	//加载属性编辑器模块
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;
	//DetailsViewArgs.ClassViewerFilters.Add(MakeShareable(new (StateAbilityEditorUtils::FChildClassFilter)).ToSharedRef());

	//创建属性编辑器的Slate
	ConfigVarsDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	ConfigVarsDetailsView->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateSP(this, &FStateAbilityEditor::IsPropertyEditable));
	ConfigVarsDetailsView->OnFinishedChangingProperties().AddSP(this, &FStateAbilityEditor::OnFinishedChangingProperties);
	//将对象传入，这样就是自动生成对象的属性面板

	HandleSelectedNodesChanged({});

	return ConfigVarsDetailsView.ToSharedRef();
}

TSharedRef<SWidget> FStateAbilityEditor::SpawnAttributeDetailsTab()
{
	SAssignNew(AttributeDetailsView, SAttributeDetailsView);

	HandleSelectedNodesChanged({});

	return AttributeDetailsView.ToSharedRef();
}

TSharedRef<SWidget> FStateAbilityEditor::SpawnNodeGraphEdTab()
{
	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FStateAbilityEditor::HandleSelectedNodesChanged);
	InEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FStateAbilityEditor::OnNodeTitleCommitted);

	UEdGraph* EdGraph = GetCurrentEdGraph();

	check(StateAbilityScriptArchetype);
	TSharedRef<SWidget> SEditor = SAssignNew(NodeGraphEditor, SGraphEditor)
		.AdditionalCommands(ToolkitCommands)
		// @TODO：此时除了 StateAbility，其他对象的EdGraph不能正确创建。
		.GraphToEdit(EdGraph)
		.GraphEvents(InEvents);

	CurrentGraphEditor = NodeGraphEditor;

	return SEditor;
}


void FStateAbilityEditor::InitializeAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* InObject)
{
	UStateAbilityScriptArchetype* ArchetypeToEdit = Cast<UStateAbilityScriptArchetype>(InObject);

	if (ArchetypeToEdit)
	{
		StateAbilityScriptArchetype = ArchetypeToEdit;

		UStateAbilityScriptEditorData* EditorData = Cast<UStateAbilityScriptEditorData>(StateAbilityScriptArchetype->EditorData);
		if (!EditorData)
		{
			EditorData = NewObject<UStateAbilityScriptEditorData>(StateAbilityScriptArchetype, FName(TEXT("StateAbilityScriptEditorData")), RF_Transactional);
			StateAbilityScriptArchetype->EditorData = EditorData;
			
			EditorData->Init();
		}

		EditorData->OpenEdit(ToolkitCommands);
		EditorData->GetScriptViewModel()->OnStateTreeNodeSelected.AddSP(this, &FStateAbilityEditor::HandleSelectedNodesChanged);
		//StateTreeViewModel = MakeShareable(new FScriptStateTreeViewModel());
		//StateTreeViewModel->Init(EditorData);

		//StateTreeViewModel->OnSelectionChanged.AddSP(this, &FStateAbilityEditor::HandleSelectedNodesChanged);
		//StateTreeViewModel->OnSpawnNodeGraph.AddSP(this, &FStateAbilityEditor::HandleSpawnNodeGraph);
		//StateTreeViewModel->OnNodeRelease.AddSP(this, &FStateAbilityEditor::HandleReleaseNode);
	}

	TArray<UObject*> ObjectsToEdit;
	if (StateAbilityScriptArchetype != nullptr)
	{
		ObjectsToEdit.Add(StateAbilityScriptArchetype);
	}

	if (!ToolbarBuilder.IsValid())
	{
		ToolbarBuilder = MakeShareable(new FStateAbilityEditorToolbar(SharedThis(this)));
	}

	FStateAbilityEditorCommonCommands::Register();

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	InitAssetEditor(Mode, InitToolkitHost, FStateAbilityScriptEditorModule::StateAbilityEditorAppIdentifier, FTabManager::FLayout::NullLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsToEdit);

	BindCommonCommands();

	AddApplicationMode(StateTreeModeName, MakeShareable(new FStateTreeEditorApplicationMode(SharedThis(this))));
	AddApplicationMode(NodeGraphModeName, MakeShareable(new FGraphEditorApplicationMode(SharedThis(this))));

	if (StateAbilityScriptArchetype != nullptr)
	{
		SetCurrentMode(NodeGraphModeName);
	}


	RegenerateMenusAndToolbars();


	// 这么做的目的仅仅是为了通知FClassHierarchy更新，将新的ScriptClass塞入。
	GEditor->OnClassPackageLoadedOrUnloaded().Broadcast();
}


void FStateAbilityEditor::BindCommonCommands()
{

	// Can't use CreateSP here because derived editor are already implementing TSharedFromThis<FAssetEditorToolkit>
	// however it should be safe, since commands are being used only within this editor
	// if it ever crashes, this function will have to go away and be reimplemented in each derived class

	ToolkitCommands->MapAction(FGenericCommands::Get().SelectAll,
		FExecuteAction::CreateRaw(this, &FStateAbilityEditor::SelectAllNodes),
		FCanExecuteAction::CreateRaw(this, &FStateAbilityEditor::CanSelectAllNodes)
	);

	ToolkitCommands->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateRaw(this, &FStateAbilityEditor::DeleteSelectedNodes),
		FCanExecuteAction::CreateRaw(this, &FStateAbilityEditor::CanDeleteNodes)
	);

	ToolkitCommands->MapAction(FGenericCommands::Get().Copy,
		FExecuteAction::CreateRaw(this, &FStateAbilityEditor::CopySelectedNodes),
		FCanExecuteAction::CreateRaw(this, &FStateAbilityEditor::CanCopyNodes)
	);

	ToolkitCommands->MapAction(FGenericCommands::Get().Cut,
		FExecuteAction::CreateRaw(this, &FStateAbilityEditor::CutSelectedNodes),
		FCanExecuteAction::CreateRaw(this, &FStateAbilityEditor::CanCutNodes)
	);

	ToolkitCommands->MapAction(FGenericCommands::Get().Paste,
		FExecuteAction::CreateRaw(this, &FStateAbilityEditor::PasteNodes),
		FCanExecuteAction::CreateRaw(this, &FStateAbilityEditor::CanPasteNodes)
	);

	ToolkitCommands->MapAction(FGenericCommands::Get().Duplicate,
		FExecuteAction::CreateRaw(this, &FStateAbilityEditor::DuplicateNodes),
		FCanExecuteAction::CreateRaw(this, &FStateAbilityEditor::CanDuplicateNodes)
	);

	ToolkitCommands->MapAction(FStateAbilityEditorCommonCommands::Get().CreateComment,
		FExecuteAction::CreateRaw(this, &FStateAbilityEditor::OnCreateComment),
		FCanExecuteAction::CreateRaw(this, &FStateAbilityEditor::CanCreateComment)
	);

	ToolkitCommands->MapAction(FStateAbilityEditorCommonCommands::Get().NewScriptArchetype,
		FExecuteAction::CreateSP(this, &FStateAbilityEditor::CreateNewScriptArchetype),
		FCanExecuteAction::CreateSP(this, &FStateAbilityEditor::CanCreateNewScriptArchetype)
	);

}

bool FStateAbilityEditor::CanAccessStateTreeMode() const
{
	return StateTreeEditor.IsValid();
}

bool FStateAbilityEditor::CanAccessNodeGraphMode() const
{
	return CurrentEdGraph.IsValid();
}

FText FStateAbilityEditor::GetLocalizedMode(FName InMode)
{
	static TMap< FName, FText > LocModes;

	if (LocModes.Num() == 0)
	{
		LocModes.Add(StateTreeModeName, LOCTEXT("StateTreeModeName", "State Ability"));
		LocModes.Add(NodeGraphModeName, LOCTEXT("NodeGraphModeName", "Node Graph"));
	}

	check(InMode != NAME_None);
	const FText* OutDesc = LocModes.Find(InMode);
	check(OutDesc);
	return *OutDesc;
}

void FStateAbilityEditor::CreateNewScriptArchetype()
{
	FString PathName = StateAbilityScriptArchetype->GetOutermost()->GetPathName();
	PathName = FPaths::GetPath(PathName);
	const FString PathNameWithFilename = PathName / LOCTEXT("NewScriptArchetypeName", "NewScriptArchetype").ToString();

	FString Name;
	FString PackageName;
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(PathNameWithFilename, TEXT(""), PackageName, Name);

	UStateAbilityScriptFactory* Factory = NewObject<UStateAbilityScriptFactory>();
	UStateAbilityScriptArchetype* NewAsset = Cast<UStateAbilityScriptArchetype>(AssetToolsModule.Get().CreateAssetWithDialog(Name, PathName, UStateAbilityScriptArchetype::StaticClass(), Factory));
	
	if (NewAsset != nullptr)
	{
		StateAbilityScriptArchetype = NewAsset;
	}
}

void FStateAbilityEditor::HandleNewNodeClassPicked(UClass* InClass) const
{
	if (GetCurrentEditObject() != nullptr && InClass != nullptr && GetCurrentEditObject()->GetOutermost())
	{
		const FString ClassName = FBlueprintEditorUtils::GetClassNameWithoutSuffix(InClass);

		FString PathName = GetCurrentEditObject()->GetOutermost()->GetPathName();
		PathName = FPaths::GetPath(PathName);

		// Now that we've generated some reasonable default locations/names for the package, allow the user to have the final say
		// before we create the package and initialize the blueprint inside of it.
		FSaveAssetDialogConfig SaveAssetDialogConfig;
		SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
		SaveAssetDialogConfig.DefaultPath = PathName;
		SaveAssetDialogConfig.DefaultAssetName = ClassName + TEXT("_New");
		SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::Disallow;

		const FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		const FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
		if (!SaveObjectPath.IsEmpty())
		{
			const FString SavePackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
			const FString SavePackagePath = FPaths::GetPath(SavePackageName);
			const FString SaveAssetName = FPaths::GetBaseFilename(SavePackageName);

			UPackage* Package = CreatePackage(*SavePackageName);
			if (ensure(Package))
			{
				// Create and init a new Blueprint
				if (UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(InClass, Package, FName(*SaveAssetName), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass()))
				{
					GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(NewBP);

					// Notify the asset registry
					FAssetRegistryModule::AssetCreated(NewBP);

					// Mark the package dirty...
					Package->MarkPackageDirty();
				}
			}
		}


	}

	FSlateApplication::Get().DismissAllMenus();
}

void FStateAbilityEditor::CreateNewState() const
{
	//HandleNewNodeClassPicked(UTransitionAction::StaticClass());
}

TSharedRef<SWidget> FStateAbilityEditor::HandleCreateNewStateMenu() const
{
	FClassViewerInitializationOptions Options;
	Options.bShowUnloadedBlueprints = true;
	Options.ClassFilters.Add(MakeShareable(new StateAbilityEditorUtils::FNewNodeClassFilter<UStateAbilityState>));

	FOnClassPicked OnPicked(FOnClassPicked::CreateSP(this, &FStateAbilityEditor::HandleNewNodeClassPicked));

	return FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, OnPicked);
}

/************************************************************************/
/* Handle																*/
/************************************************************************/
void FStateAbilityEditor::HandleSelectedNodesChanged(const TSet<class UObject*>& NewSelection)
{
	TSet<UObject*> OldSelections;
	for (TWeakObjectPtr<UObject> SelectItem : LastSelections)
	{
		if (!SelectItem.IsValid())
		{
			continue;
		}
		OldSelections.Add(SelectItem.Get());
	}

	// 记录选择项，用于代替GraphEditor->GetSelectedNodes()，因为此方法只能获取UEdGraphNode类型
	LastSelections.Empty(NewSelection.Num());
	for (UObject* SelectItem : NewSelection)
	{
		LastSelections.Add(SelectItem);
		
		if (OldSelections.Contains(SelectItem))
		{
			OldSelections.Remove(SelectItem);
		}
	}

	// 只允许处理一个选项的
	UObject* Selection = nullptr;
	if (NewSelection.Num() == 1)
	{
		Selection = StateAbilityEditorUtils::GetSelectionForPropertyEditor(*NewSelection.begin());
	}

	if (Selection)
	{
		GetCurrentDetailsView()->SetObject(Selection);

		if (AttributeDetailsView.IsValid())
		{
			AttributeDetailsView->SetObject(Selection);
		}

		LastSelections.Add(Selection);
	}
	else
	{
		GetCurrentDetailsView()->SetObject(GetCurrentEditObject());

		if (AttributeDetailsView.IsValid())
		{
			UStateAbilityScriptArchetype* ScriptArchetype = Cast<UStateAbilityScriptArchetype>(GetCurrentEditObject());
			AttributeDetailsView->SetObject(ScriptArchetype->GetDefaultScript());
		}

		// 没什么关注的目标
		LastSelections.Reset();
	}

	for (UObject* OldSelectItem : OldSelections)
	{
		// 剔除之前选中的StateTreeView
		if (UStateTreeBaseNode* StateTreeNode = Cast<UStateTreeBaseNode>(OldSelectItem))
		{
			if (IsValid(StateTreeNode) && StateTreeNode->GetStateTreeViewModel().IsValid())
			{
				StateTreeNode->GetStateTreeViewModel()->ClearSelection(OldSelectItem);
			}
		}
	}
}

void FStateAbilityEditor::HandleSpawnNodeGraph(UEdGraph* EdGraph)
{
	if (EdGraph)
	{
		//CurrentEdGraph = EdGraph;
		//SetCurrentMode(NodeGraphModeName);
	}
}

void FStateAbilityEditor::HandleReleaseNode(UStateTreeBaseNode* ReleasedNode)
{
	//if (ReleasedNode->NodeType == EScriptStateTreeNodeType::Action)
	//{
	//	UStateTreeActionNode* ActionNode = CastChecked<UStateTreeActionNode>(ReleasedNode);

	//	if (CurrentEdGraph.Get() == ActionNode->EdGraph)
	//	{
	//		CurrentEdGraph = nullptr;
	//	}
	//}
}

/************************************************************************/
/* EdGraph																*/
/************************************************************************/
UEdGraph* FStateAbilityEditor::GetCurrentEdGraph()
{
	if (CurrentEdGraph.IsValid())
	{
		return CurrentEdGraph.Get();
	}
	else
	{
		UStateAbilityScriptArchetype* ScriptArchetype = Cast<UStateAbilityScriptArchetype>(GetCurrentEditObject());
		UStateAbilityScriptEditorData* EditorData = Cast<UStateAbilityScriptEditorData>(ScriptArchetype->EditorData);
		CurrentEdGraph = EditorData->GetStateTreeGraph();
		return CurrentEdGraph.Get();
	}
}

FGraphPanelSelectionSet FStateAbilityEditor::GetSelectedNodes() const
{
	FGraphPanelSelectionSet CurrentSelection;
	if (CurrentGraphEditor)
	{
		CurrentSelection = CurrentGraphEditor->GetSelectedNodes();
	}

	return CurrentSelection;
}

FGraphPanelSelectionSet FStateAbilityEditor::GetSelectedObjects() const
{
	FGraphPanelSelectionSet CurrentSelection;

	bool bHasImportantObj = false;
	for (TWeakObjectPtr<UObject> SelectItem : LastSelections)
	{
		if (SelectItem.IsValid())
		{
			UObject* SelectObj = SelectItem.Get();
			if (UStateTreeBaseNode* StateTreeNode = Cast<UStateTreeBaseNode>(SelectObj))
			{
				bHasImportantObj = true;
			}
			CurrentSelection.Add(SelectObj);
		}
	}

	// 如果有需要特别关注的对象被选中，则不再关注GraphNode
	if (!bHasImportantObj)
	{
		CurrentSelection.Append(CurrentGraphEditor->GetSelectedNodes());
	}

	return CurrentSelection;
}

void FStateAbilityEditor::SelectAllNodes()
{
	if (CurrentGraphEditor)
	{
		CurrentGraphEditor->SelectAllNodes();
	}
}

bool FStateAbilityEditor::CanSelectAllNodes() const
{
	return true;
}

void FStateAbilityEditor::DeleteSelectedNodes()
{
	if (GetCurrentMode() == NodeGraphModeName)
	{
		if (!CurrentGraphEditor.IsValid())
		{
			return;
		}

		UStateAbilityScriptEditorData* EditorData = Cast<UStateAbilityScriptEditorData>(StateAbilityScriptArchetype->EditorData);

		const FScopedTransaction Transaction(FGenericCommands::Get().Delete->GetDescription());
		CurrentGraphEditor->GetCurrentGraph()->Modify();

		const FGraphPanelSelectionSet SelectedNodes = GetSelectedObjects();
		CurrentGraphEditor->ClearSelectionSet();

		for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
		{
			if (UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
			{
				if (Node->CanUserDeleteNode())
				{
					Node->Modify();

					if (const UEdGraphSchema* Schema = CurrentGraphEditor->GetCurrentGraph()->GetSchema())
					{
						Schema->BreakNodeLinks(*Node);
					}

					Node->DestroyNode();
				}
			}
			else if (UStateTreeBaseNode* StateTreeNode = Cast<UStateTreeBaseNode>(*SelectedIter))
			{
				if (IsValid(StateTreeNode) && StateTreeNode->GetStateTreeViewModel().IsValid())
				{
					StateTreeNode->GetStateTreeViewModel()->DeleteSelectedNode();
				}
			}
		}
	}
}

bool FStateAbilityEditor::CanDeleteNodes() const
{
	if (GetCurrentMode() == NodeGraphModeName)
	{
		const FGraphPanelSelectionSet SelectedNodes = GetSelectedObjects();
		for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
		{
			if (UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
			{
				return Node->CanUserDeleteNode();
			}
			else if (UStateTreeBaseNode* StateTreeNode = Cast<UStateTreeBaseNode>(*SelectedIter))
			{
				return StateTreeNode->CanReleaseNode();
			}
		}
	}


	return false;
}

void FStateAbilityEditor::DeleteSelectedDuplicatableNodes()
{
	if (!CurrentGraphEditor.IsValid())
	{
		return;
	}

	const FGraphPanelSelectionSet OldSelectedNodes = CurrentGraphEditor->GetSelectedNodes();
	CurrentGraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node && Node->CanDuplicateNode())
		{
			CurrentGraphEditor->SetNodeSelection(Node, true);
		}
	}

	// Delete the duplicatable nodes
	DeleteSelectedNodes();

	CurrentGraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
		{
			CurrentGraphEditor->SetNodeSelection(Node, true);
		}
	}
}

void FStateAbilityEditor::CutSelectedNodes()
{
	CopySelectedNodes();
	DeleteSelectedDuplicatableNodes();
}

bool FStateAbilityEditor::CanCutNodes() const
{
	return CanCopyNodes() && CanDeleteNodes();
}

void FStateAbilityEditor::CopySelectedNodes()
{
	// Export the selected nodes and place the text on the clipboard
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	FString ExportedText;

	int32 CopySubNodeIndex = 0;
	for (FGraphPanelSelectionSet::TIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node == nullptr)
		{
			SelectedIter.RemoveCurrent();
			continue;
		}

		Node->PrepareForCopying();
	}

	FEdGraphUtilities::ExportNodesToText(SelectedNodes, ExportedText);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);

	for (FGraphPanelSelectionSet::TIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UGraphAbilityNode* Node = Cast<UGraphAbilityNode>(*SelectedIter);
		if (Node)
		{
			Node->PostCopyNode();
		}
	}
}

bool FStateAbilityEditor::CanCopyNodes() const
{
	// If any of the nodes can be duplicated then we should allow copying
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node && Node->CanDuplicateNode())
		{
			return true;
		}
	}

	return false;
}

void FStateAbilityEditor::PasteNodes()
{
	if (CurrentGraphEditor)
	{
		PasteNodesHere(CurrentGraphEditor->GetPasteLocation());
	}
}

void FStateAbilityEditor::PasteNodesHere(const FVector2D& Location)
{
	if (!CurrentGraphEditor.IsValid())
	{
		return;
	}

	// Undo/Redo support
	const FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());
	UEdGraph* EdGraph = CurrentGraphEditor->GetCurrentGraph();
	UStateAbilityScriptGraph* SDTGraph = Cast<UStateAbilityScriptGraph>(EdGraph);

	EdGraph->Modify();
	if (SDTGraph)
	{
		SDTGraph->LockUpdates();
	}

	//UStateAbilityScriptGraph* SelectedParent = NULL;
	//bool bHasMultipleNodesSelected = false;

	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	//for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	//{
	//	UGraphAbilityNode* Node = Cast<UGraphAbilityNode>(*SelectedIter);
	//	if (Node && Node->IsSubNode())
	//	{
	//		Node = Node->ParentNode;
	//	}

	//	if (Node)
	//	{
	//		if (SelectedParent == nullptr)
	//		{
	//			SelectedParent = Node;
	//		}
	//		else
	//		{
	//			bHasMultipleNodesSelected = true;
	//			break;
	//		}
	//	}
	//}

	// Clear the selection set (newly pasted stuff will be selected)
	CurrentGraphEditor->ClearSelectionSet();

	// Grab the text to paste from the clipboard.
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	// Import the nodes
	TSet<UEdGraphNode*> PastedNodes;
	FEdGraphUtilities::ImportNodesFromText(EdGraph, TextToImport, /*out*/ PastedNodes);

	//Average position of nodes so we can move them while still maintaining relative distances to each other
	FVector2D AvgNodePosition(0.0f, 0.0f);

	// Number of nodes used to calculate AvgNodePosition
	int32 AvgCount = 0;

	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEdGraphNode* EdNode = *It;
		UGraphAbilityNode* SDTNode = Cast<UGraphAbilityNode>(EdNode);
		if (EdNode)
		{
			AvgNodePosition.X += EdNode->NodePosX;
			AvgNodePosition.Y += EdNode->NodePosY;
			++AvgCount;
		}
	}

	if (AvgCount > 0)
	{
		float InvNumNodes = 1.0f / float(AvgCount);
		AvgNodePosition.X *= InvNumNodes;
		AvgNodePosition.Y *= InvNumNodes;
	}


	TMap<FGuid/*New*/, FGuid/*Old*/> NewToOldNodeMapping;

	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEdGraphNode* PasteNode = *It;
		UGraphAbilityNode* PasteSDTNode = Cast<UGraphAbilityNode>(PasteNode);

		if (PasteNode)
		{
			/*bPastedParentNode = true;*/

			// Select the newly pasted stuff
			CurrentGraphEditor->SetNodeSelection(PasteNode, true);

			PasteNode->NodePosX = (PasteNode->NodePosX - AvgNodePosition.X) + Location.X;
			PasteNode->NodePosY = (PasteNode->NodePosY - AvgNodePosition.Y) + Location.Y;

			PasteNode->SnapToGrid(16);

			const FGuid OldGuid = PasteNode->NodeGuid;

			// Give new node a different Guid from the old one
			PasteNode->CreateNewGuid();

			const FGuid NewGuid = PasteNode->NodeGuid;

			NewToOldNodeMapping.Add(NewGuid, OldGuid);

			//if (PasteSDTNode)
			//{
			//	PasteSDTNode->RemoveAllSubNodes();
			//	ParentMap.Add(PasteSDTNode->CopySubNodeIndex, PasteSDTNode);
			//}
		}
	}

	//for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	//{
	//	UAIGraphNode* PasteNode = Cast<UAIGraphNode>(*It);
	//	if (PasteNode && PasteNode->IsSubNode())
	//	{
	//		PasteNode->NodePosX = 0;
	//		PasteNode->NodePosY = 0;

	//		// remove subnode from graph, it will be referenced from parent node
	//		PasteNode->DestroyNode();

	//		PasteNode->ParentNode = ParentMap.FindRef(PasteNode->CopySubNodeIndex);
	//		if (PasteNode->ParentNode)
	//		{
	//			PasteNode->ParentNode->AddSubNode(PasteNode, EdGraph);
	//		}
	//		else if (!bHasMultipleNodesSelected && !bPastedParentNode && SelectedParent)
	//		{
	//			PasteNode->ParentNode = SelectedParent;
	//			SelectedParent->AddSubNode(PasteNode, EdGraph);
	//		}
	//	}
	//}

	FixupPastedNodes(PastedNodes, NewToOldNodeMapping);

	if (SDTGraph)
	{
		//SDTGraph->UpdateClassData();
		SDTGraph->OnNodesPasted(TextToImport);
		SDTGraph->UnlockUpdates();
	}

	// Update UI
	CurrentGraphEditor->NotifyGraphChanged();

	UObject* GraphOwner = EdGraph->GetOuter();
	if (GraphOwner)
	{
		GraphOwner->PostEditChange();
		GraphOwner->MarkPackageDirty();
	}
}

void FStateAbilityEditor::FixupPastedNodes(const TSet<UEdGraphNode*>& PastedGraphNodes, const TMap<FGuid/*New*/, FGuid/*Old*/>& NewToOldNodeMapping)
{
	
}

bool FStateAbilityEditor::CanPasteNodes() const
{
	if (!CurrentGraphEditor.IsValid())
	{
		return false;
	}

	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	return FEdGraphUtilities::CanImportNodesFromText(CurrentGraphEditor->GetCurrentGraph(), ClipboardContent);
}

void FStateAbilityEditor::DuplicateNodes()
{
	CopySelectedNodes();
	PasteNodes();
}

bool FStateAbilityEditor::CanDuplicateNodes() const
{
	return CanCopyNodes();
}

bool FStateAbilityEditor::CanCreateComment() const
{
	return CurrentGraphEditor.IsValid() ? /*(CurrentGraphEditor->GetNumberOfSelectedNodes() != 0)*/true : false;
}

void FStateAbilityEditor::OnCreateComment()
{
	if (UEdGraph* EdGraph = CurrentGraphEditor.IsValid() ? CurrentGraphEditor->GetCurrentGraph() : nullptr)
	{
		TSharedPtr<FEdGraphSchemaAction> Action = EdGraph->GetSchema()->GetCreateCommentAction();
		if (Action.IsValid())
		{
			Action->PerformAction(EdGraph, nullptr, FVector2D());
		}
	}
}

void FStateAbilityEditor::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged)
{
	if (NodeBeingChanged)
	{
		static const FText TranscationTitle = FText::FromString(FString(TEXT("Rename Node")));
		const FScopedTransaction Transaction(TranscationTitle);
		NodeBeingChanged->Modify();
		NodeBeingChanged->OnRenameNode(NewText.ToString());
	}
}

FName FStateAbilityEditor::GetToolkitFName() const
{
	return FName("StateAbilityScript Editor");
}

FText FStateAbilityEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "StateAbilityScriptEditor");
}

FString FStateAbilityEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "StateAbilityScriptEditor").ToString();
}

FText FStateAbilityEditor::GetToolkitName() const
{
	if (GetEditingObjects().Num() > 0)
	{
		return FAssetEditorToolkit::GetLabelForObject(GetEditingObjects()[0]);
	}

	return FText();
}

FText FStateAbilityEditor::GetToolkitToolTipText() const
{
	const UObject* EditingObject = (UObject*)StateAbilityScriptArchetype;
	if (EditingObject != nullptr)
	{
		return FAssetEditorToolkit::GetToolTipTextForObject(EditingObject);
	}

	return FText();
}

void FStateAbilityEditor::OnClose()
{
	if (IsValid(StateAbilityScriptArchetype))
	{
		StateAbilityScriptArchetype->EditorData;
	}

	FWorkflowCentricApplication::OnClose();
}

FLinearColor FStateAbilityEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.0f, 1.0f, 0.2f, 0.5f);
}

bool FStateAbilityEditor::IsPropertyEditable() const
{
	return true;
}

void FStateAbilityEditor::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	TSharedPtr<SGraphEditor> MyGraphEditor;
	if (GetCurrentMode() == NodeGraphModeName && NodeGraphEditor.IsValid())
	{
		MyGraphEditor = NodeGraphEditor;
	}

	UEdGraph* EdGraph = MyGraphEditor->GetCurrentGraph();
	UStateAbilityScriptGraph* SDTGraph = Cast<UStateAbilityScriptGraph>(EdGraph);
	if (SDTGraph)
	{
		SDTGraph->UpdateAsset();
	}
	
	// Hope to Rebuild Widget(A mistake?)
	if (MyGraphEditor.IsValid() && MyGraphEditor->GetCurrentGraph())
	{
		MyGraphEditor->GetCurrentGraph()->GetSchema()->ForceVisualizationCacheClear();
	}
}

void FStateAbilityEditor::SaveAsset_Execute()
{
	UStateAbilityScriptEditorData* EditorData = Cast<UStateAbilityScriptEditorData>(StateAbilityScriptArchetype->EditorData);
	check(EditorData);

	EditorData->Save();

	// save asset
	FAssetEditorToolkit::SaveAsset_Execute();

	// 这么做的目的仅仅是为了通知FClassHierarchy更新，将新的ScriptClass塞入。
	GEditor->OnClassPackageLoadedOrUnloaded().Broadcast();
}

void FStateAbilityEditor::OnCompile()
{
	if (GetCurrentMode() == NodeGraphModeName)
	{
		if (UStateAbilityGraph* SAGraph = Cast<UStateAbilityGraph>(CurrentEdGraph))
		{
			SAGraph->OnCompile();
		}
	}
}

FSlateIcon FStateAbilityEditor::GetCompileStatusImage() const
{
	static const FName CompileStatusBackground("Blueprint.CompileStatus.Background");
	static const FName CompileStatusUnknown("Blueprint.CompileStatus.Overlay.Unknown");
	static const FName CompileStatusError("Blueprint.CompileStatus.Overlay.Error");
	static const FName CompileStatusGood("Blueprint.CompileStatus.Overlay.Good");
	static const FName CompileStatusWarning("Blueprint.CompileStatus.Overlay.Warning");

	//if (CurrentEdGraph == nullptr)
	//{
	//	return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusUnknown);
	//}

	//const bool bCompiledDataResetDuringLoad = StateTree->LastCompiledEditorDataHash == EditorDataHash && !StateTree->IsReadyToRun();

	//if (!bLastCompileSucceeded || bCompiledDataResetDuringLoad)
	//{
	//	return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusError);
	//}

	//if (StateTree->LastCompiledEditorDataHash != EditorDataHash)
	//{
	//	return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusUnknown);
	//}

	return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusGood);
}

UObject* FStateAbilityEditor::GetCurrentEditObject() const
{
	if (GetCurrentMode() == StateTreeModeName || GetCurrentMode() == NodeGraphModeName)
	{
		return StateAbilityScriptArchetype;
	}

	return nullptr;
}

TSharedPtr<class IDetailsView> FStateAbilityEditor::GetCurrentDetailsView() const
{
	if (GetCurrentMode() == StateTreeModeName || GetCurrentMode() == NodeGraphModeName)
	{
		return ConfigVarsDetailsView;
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE