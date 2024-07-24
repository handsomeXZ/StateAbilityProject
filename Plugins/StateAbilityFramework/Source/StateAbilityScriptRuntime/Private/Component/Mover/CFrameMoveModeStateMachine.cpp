#include "Component/Mover/CFrameMoveModeStateMachine.h"

#include "Components/InputComponent.h"

#include "CommandFrameManager.h"
#include "Component/CFrameMoverComponent.h"
#include "Component/Mover/CFrameLayeredMove.h"
#include "Component/Mover/CFrameProposedMove.h"
#include "Component/Mover/CFrameMovementMixer.h"
#include "Component/Mover/CFrameMoveStateAdapter.h"
#include "Component/Mover/Mode/CFrameWalkingMode.h"
#include "Component/Mover/CFrameMovementTransition.h"

DEFINE_LOG_CATEGORY_STATIC(LogMoveModeStateMachine, Log, All)

UCFrameMoveModeStateMachine::UCFrameMoveModeStateMachine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CurrentMode(nullptr)
	, CurrentModeName(NAME_None)
	, CurrentMixer(nullptr)
{

}

void UCFrameMoveModeStateMachine::Init(FCFrameMovementConfig& Config)
{
	UCFrameMoverComponent* MoverComp = CastChecked<UCFrameMoverComponent>(GetOuter());
	AActor* CompOwner = MoverComp->GetOwner();
	UInputComponent* InputComp = CompOwner->GetComponentByClass<UInputComponent>();

	check(InputComp);

	//////////////////////////////////////////////////////////////////////////

	if (IsValid(Config.MoveStateAdapter))
	{
		Config.MoveStateAdapter->Init(MoverComp);
	}

	//////////////////////////////////////////////////////////////////////////
	// Mixer
	if (IsValid(Config.MovementMixer))
	{
		CurrentMixer = Config.MovementMixer;
	}
	else
	{
		CurrentMixer = NewObject<UCFrameMovementMixer>(this);
	}

	//////////////////////////////////////////////////////////////////////////
	// Mode
	ModeInfos.Empty(Config.MovementModes.Num());
	CurrentMode = nullptr;
	CurrentModeName = NAME_None;
	for (auto& ModePair : Config.MovementModes)
	{
		UCFrameMovementMode* ModeInstance = NewObject<UCFrameMovementMode>(MoverComp, ModePair.Value);
		ModeInstance->OnRegistered(ModePair.Key);
		ModeInstance->SetupInputComponent(InputComp);

		ModeInfos.FindOrAdd(ModePair.Key).ModeInstance = ModeInstance;
		if (!CurrentMode || ModePair.Key == Config.DefaultModeName)
		{
			CurrentMode = ModeInstance;
			CurrentModeName = ModePair.Key;
		}
	}

	// 激活Mode
	CurrentMode->OnActivated();

	//////////////////////////////////////////////////////////////////////////
	// Transitions

	Config.ModeTransitionLinks.Sort([](const FCFrameModeTransitionLink& A, const FCFrameModeTransitionLink& B) {
		return A.Priority > B.Priority;
	});

	for (auto Link : Config.ModeTransitionLinks)
	{
		ModeInfos.FindOrAdd(Link.FromMode).TransitionLinks.Add(Link);
	}

	//////////////////////////////////////////////////////////////////////////
	// LayeredMoves
	LayeredMoves.Empty(Config.LayeredMoves.Num());
	for (auto LayerMove : Config.LayeredMoves)
	{
		LayeredMoves.Add(LayerMove);
		//LayerMove->SetupInputComponent(InputComp);
	}
}

void UCFrameMoveModeStateMachine::FixedTick(float DeltaTime, uint32 RCF, uint32 ICF)
{
	UCFrameMoverComponent* MoverComp = CastChecked<UCFrameMoverComponent>(GetOuter());

	Context.ResetFrameData();
	{
		// Context 初始化
		Context.Init(MoverComp, DeltaTime, RCF, ICF);
		Context.CurrentMode = CurrentMode;
	}

	UCFrameMoveStateAdapter* MoveStateAdapter = Context.MoveStateAdapter;

	if (!Context.IsDataValid())
	{
		return;
	}

	MoveStateAdapter->BeginMoveFrame(DeltaTime, RCF, ICF);

	// Gather any layered move contributions
	FCFrameProposedMove CombinedLayeredMove;
	CombinedLayeredMove.MixMode = ECFrameMoveMixMode::AdditiveVelocity;

	for (TObjectPtr<UCFrameLayeredMove> LayerMove : LayeredMoves)
	{
		FCFrameProposedMove MoveStep;
		LayerMove->GenerateMove(Context, MoveStep);
		CurrentMixer->MixLayeredMove(Context, LayerMove, MoveStep, CombinedLayeredMove);
	}


	CurrentMode->GenerateMove(Context, Context.CombinedMove);

	CurrentMixer->MixProposedMoves(Context, CombinedLayeredMove, Context.CombinedMove);

	CurrentMode->Execute(Context);

	MoveStateAdapter->EndMoveFrame(DeltaTime, RCF, ICF);


	UpdateTransition();
	RecordMovementSnapshot();
}

void UCFrameMoveModeStateMachine::UpdateTransition()
{
	FCFrameMoveModeInfo* ModeInfo = ModeInfos.Find(CurrentModeName);
	if (!ModeInfo)
	{
		return;
	}

	for (FCFrameModeTransitionLink& Transition : ModeInfo->TransitionLinks)
	{
		if (Transition.ModeTransition == nullptr || !Transition.ModeTransition->GetDefaultObject<UCFrameMoveModeTransition>()->OnEvaluate(Context))
		{
			continue;
		}

		if (FCFrameMoveModeInfo* ToModeInfo = ModeInfos.Find(Transition.ToMode))
		{
			CurrentMode->OnDeactivated();
			CurrentMode = ToModeInfo->ModeInstance;
			CurrentModeName = Transition.ToMode;
			CurrentMode->OnActivated();
			break;
		}
		else
		{
			UE_LOG(LogMoveModeStateMachine, Warning, TEXT("ToMode[%s] not found. Is the wrong ModeName configured?"), *(Transition.ToMode.ToString()));
		}
	}
}

void UCFrameMoveModeStateMachine::OnClientRewind()
{
	for (auto& ModeInfoPair : ModeInfos)
	{
		FCFrameMoveModeInfo& ModeInfo = ModeInfoPair.Value;
		if (IsValid(ModeInfo.ModeInstance))
		{
			ModeInfo.ModeInstance->OnClientRewind();
		}
	}
}

void UCFrameMoveModeStateMachine::RecordMovementSnapshot()
{
	UCommandFrameManager* CFrameManager = Context.CFrameManager;
	UCFrameMoveStateAdapter* MoveStateAdapter = Context.MoveStateAdapter;

	// 此时正在回滚并重新模拟，需重新记录快照
	if (CFrameManager->IsInRewinding())
	{
		if (GetWorld()->GetNetMode() == NM_Client)
		{
			UE_LOG(LogMoveModeStateMachine, Log, TEXT("[Client] ICF[%d] Location[%s]"), Context.ICF, *(MoveStateAdapter->GetLocation_WorldSpace().ToCompactString()));
		}
		else
		{
			UE_LOG(LogMoveModeStateMachine, Log, TEXT("[Server] ICF[%d] Location[%s]"), Context.ICF, *(MoveStateAdapter->GetLocation_WorldSpace().ToCompactString()));
		}
		// 历史记录无法被直接覆盖，需要取出后进行修改
		FStructView MovementSnapshotView = CFrameManager->ReadAttributeFromSnapshotBuffer(Context.ICF, FCFrameMovementSnapshot::StaticStruct());
		if (MovementSnapshotView.IsValid())
		{
			FCFrameMovementSnapshot& Snapshot = MovementSnapshotView.Get<FCFrameMovementSnapshot>();

			Snapshot.Location = MoveStateAdapter->GetLocation_WorldSpace();
			Snapshot.Velocity = MoveStateAdapter->GetVelocity_WorldSpace();
			Snapshot.Orientation = MoveStateAdapter->GetOrientation_WorldSpace();

			Snapshot.MovementBase = MoveStateAdapter->GetMovementBase();
			Snapshot.MovementBaseBoneName = MoveStateAdapter->GetMovementBaseBoneName();
			Snapshot.MovementBasePos = MoveStateAdapter->GetMovementBasePos();
			Snapshot.MovementBaseQuat = MoveStateAdapter->GetMovementBaseQuat();
		}
	}
	else
	{
		if (GetWorld()->GetNetMode() == NM_Client)
		{
			UE_LOG(LogMoveModeStateMachine, Log, TEXT("[Client] RCF[%d] Location[%s]"), Context.RCF, *(MoveStateAdapter->GetLocation_WorldSpace().ToCompactString()));
		}
		else
		{
			UE_LOG(LogMoveModeStateMachine, Log, TEXT("[Server] RCF[%d] Location[%s]"), Context.RCF, *(MoveStateAdapter->GetLocation_WorldSpace().ToCompactString()));
		}
		FCFrameMovementSnapshot Snapshot;

		Snapshot.Location = MoveStateAdapter->GetLocation_WorldSpace();
		Snapshot.Velocity = MoveStateAdapter->GetVelocity_WorldSpace();
		Snapshot.Orientation = MoveStateAdapter->GetOrientation_WorldSpace();

		Snapshot.MovementBase = MoveStateAdapter->GetMovementBase();
		Snapshot.MovementBaseBoneName = MoveStateAdapter->GetMovementBaseBoneName();
		Snapshot.MovementBasePos = MoveStateAdapter->GetMovementBasePos();
		Snapshot.MovementBaseQuat = MoveStateAdapter->GetMovementBaseQuat();

		CFrameManager->AttributeSnapshotBuffer.RecordItemData(FCFrameMovementSnapshot::StaticStruct(), (uint8*)&Snapshot, Context.RCF);
	}
}