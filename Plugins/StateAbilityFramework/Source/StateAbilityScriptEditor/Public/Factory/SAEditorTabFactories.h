// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "EdGraph/EdGraph.h"
#include "Internationalization/Text.h"
#include "Misc/Attribute.h"
#include "Templates/SharedPointer.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class SDockTab;
class SGraphEditor;
class SWidget;
struct FSlateBrush;

/************************************************************************/
/* State Tree					                                        */
/************************************************************************/
struct FStateTreeEditorSummoner : public FWorkflowTabFactory
{
public:
	FStateTreeEditorSummoner(TSharedPtr<class FStateAbilityEditor> InEditorPtr);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<class FStateAbilityEditor> SEditorPtr;
};

struct FStateTreeDetailsSummoner : public FWorkflowTabFactory
{
public:
	FStateTreeDetailsSummoner(TSharedPtr<class FStateAbilityEditor> InEditorPtr);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<class FStateAbilityEditor> SEditorPtr;
};

struct FStateTreeAttributeDetailsSummoner : public FWorkflowTabFactory
{
public:
	FStateTreeAttributeDetailsSummoner(TSharedPtr<class FStateAbilityEditor> InEditorPtr);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<class FStateAbilityEditor> SEditorPtr;
};


/************************************************************************/
/* Node Graph					                                        */
/************************************************************************/
struct FNodeGraphEditorSummoner : public FWorkflowTabFactory
{
public:
	FNodeGraphEditorSummoner(TSharedPtr<class FStateAbilityEditor> InEditorPtr);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<class FStateAbilityEditor> SEditorPtr;
};

struct FNodeGraphDetailsSummoner : public FWorkflowTabFactory
{
public:
	FNodeGraphDetailsSummoner(TSharedPtr<class FStateAbilityEditor> InEditorPtr);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<class FStateAbilityEditor> SEditorPtr;
};