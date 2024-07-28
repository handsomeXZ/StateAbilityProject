// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "WorkflowOrientedApp/ApplicationMode.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"


/************************************************************************/
/* Node Graph Mode														*/
/************************************************************************/
class FGraphEditorApplicationMode : public FApplicationMode
{
public:
	FGraphEditorApplicationMode(TSharedPtr<class FStateAbilityEditor> InSCTEditor);

	virtual void RegisterTabFactories(TSharedPtr<class FTabManager> InTabManager) override;
	virtual void PostActivateMode() override;

protected:
	TWeakPtr<class FStateAbilityEditor> SEditor;

	// Set of spawnable tabs in State Ability mode
	FWorkflowAllowedTabSet STabFactories;
};


/************************************************************************/
/* State Tree Mode														*/
/************************************************************************/
class FStateTreeEditorApplicationMode : public FApplicationMode
{
public:
	FStateTreeEditorApplicationMode(TSharedPtr<class FStateAbilityEditor> InSCTEditor);

	virtual void RegisterTabFactories(TSharedPtr<class FTabManager> InTabManager) override;
	virtual void PostActivateMode() override;

protected:
	TWeakPtr<class FStateAbilityEditor> SEditor;

	// Set of spawnable tabs in State Ability mode
	FWorkflowAllowedTabSet STabFactories;
};