﻿#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"
#include "EditorUndoClient.h"

class SScriptStateTreeView;
class FScriptStateTreeViewModel;
class UStateTreeBaseNode;

/************************************************************************/
/*																		*/
/*	拓展守则																*/
/*																		*/
/*	1. 对Selection的处理一律使用GetSelectedObjects，而不是GetSelectedNodes，	*/
/*		同时需要处理好非UEdGraphNode类型SelectItem的复制、粘贴、删除等。		*/
/*																		*/
/*																		*/
/************************************************************************/

class FStateAbilityEditor : public FWorkflowCentricApplication, public FEditorUndoClient
{
public:
	FStateAbilityEditor();
	virtual ~FStateAbilityEditor();

	static const FName StateTreeEditorTab;
	static const FName NodeGraphEditorTab;

	static const FName ConfigVarsDetailsTab;
	static const FName AttributeDetailsTab;

	/** Modes in mode switcher */
	static const FName StateTreeModeName;
	static const FName NodeGraphModeName;

	//~ Begin IToolkit Interface
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual FText GetToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual void OnClose() override;
	//~ End IToolkit Interface

	//~ Begin FWorkflowCentricApplication Interface
	virtual void SetCurrentMode(FName NewMode) override;
	//~ End FWorkflowCentricApplication Interface

	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	// End of FEditorUndoClient

	// @todo This is a hack for now until we reconcile the default toolbar with application modes [duplicated from counterpart in Blueprint Editor]
	void RegisterToolbarTab(const TSharedRef<class FTabManager>& TabManager);

	void InitializeAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* InObject);

	// Handle
	void HandleSelectedNodesChanged(const TSet<class UObject*>& NewSelection);
	void HandleSpawnNodeGraph(UEdGraph* EdGraph);
	void HandleReleaseNode(UStateTreeBaseNode* ReleasedNode);

	// EdGraph
	UEdGraph* GetCurrentEdGraph();
	TSharedPtr<SGraphEditor> GetCurrentGraphEditor() { return CurrentGraphEditor; }

	FGraphPanelSelectionSet GetSelectedNodes() const;	// 仅包含UEdGraphNode类型	(少用)
	FGraphPanelSelectionSet GetSelectedObjects() const;	// 包含所有Selection的目标

	virtual void BindCommonCommands();
	// Delegates for graph editor commands
	void SelectAllNodes();
	bool CanSelectAllNodes() const;
	void DeleteSelectedNodes();
	bool CanDeleteNodes() const;
	void DeleteSelectedDuplicatableNodes();
	void CutSelectedNodes();
	bool CanCutNodes() const;
	void CopySelectedNodes();
	bool CanCopyNodes() const;
	void PasteNodes();
	void PasteNodesHere(const FVector2D& Location);
	bool CanPasteNodes() const;
	void DuplicateNodes();
	bool CanDuplicateNodes() const;
	bool CanCreateComment() const;
	void OnCreateComment();
	void OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged);
	void OnCompile();

	// State Tree
	TSharedRef<SWidget> SpawnStateTreeEdTab();
	// Node Graph
	TSharedRef<SWidget> SpawnNodeGraphEdTab();
	// Details Tab
	TSharedRef<SWidget> SpawnConfigVarsDetailsTab();
	TSharedRef<SWidget> SpawnAttributeDetailsTab();

	bool IsPropertyEditable() const;
	void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);

	/** 
	 * Get the localized text to display for the specified mode 
	 * @param	InMode	The mode to display
	 * @return the localized text representation of the mode
	 */
	static FText GetLocalizedMode(FName InMode);

	/** Access the toolbar builder for this editor */
	TSharedPtr<class FStateAbilityEditorToolbar> GetToolbarBuilder() { return ToolbarBuilder; }

	void HandleNewNodeClassPicked(UClass* InClass) const;

	void CreateNewScriptArchetype();
	void CreateNewState() const;
	
	bool CanCreateNewScriptArchetype() const		{ return true; }
	bool CanCreateNewState() const					{ return true; }
	
	bool IsNewStateButtonVisible() const			{ return true; }
	TSharedRef<SWidget> HandleCreateNewStateMenu() const;

	bool CanAccessStateTreeMode() const;
	bool CanAccessNodeGraphMode() const;

	UObject* GetCurrentEditObject() const;
	TSharedPtr<class IDetailsView> GetCurrentDetailsView() const;
	FSlateIcon GetCompileStatusImage() const;
protected:
	/** Called when "Save" is clicked for this asset */
	virtual void SaveAsset_Execute() override;

	void FixupPastedNodes(const TSet<UEdGraphNode*>& NewPastedGraphNodes, const TMap<FGuid/*New*/, FGuid/*Old*/>& NewToOldNodeMapping);
	
private:
	/* The Asset being edited */
	class UStateAbilityScriptArchetype* StateAbilityScriptArchetype;

	//////////////////////////////////////////////////////////////////////////

	/** Property Details View */
	TSharedPtr<class IDetailsView> ConfigVarsDetailsView;
	/** Attribute Details View */
	TSharedPtr<class SAttributeDetailsView> AttributeDetailsView;

	//////////////////////////////////////////////////////////////////////////

	TSet<TWeakObjectPtr<UObject>> LastSelections;	// 并不一定是UEdGraphNode

	//////////////////////////////////////////////////////////////////////////

	/** Tab's Content: SGraphEditor */
	TSharedPtr<SGraphEditor> NodeGraphEditor;
	TSharedPtr<SScriptStateTreeView> StateTreeEditor;
	TSharedPtr<SGraphEditor> CurrentGraphEditor;

	TWeakObjectPtr<UEdGraph> CurrentEdGraph;
	/** The command list for this editor */
	TSharedPtr<FUICommandList> GraphEditorCommands;
	TSharedPtr<class FStateAbilityEditorToolbar> ToolbarBuilder;

};