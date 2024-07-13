#include "Component/Mover/CFrameMoveModeStateMachine.h"

#include "Components/InputComponent.h"

#include "CommandFrameManager.h"
#include "Component/CFrameMoverComponent.h"
#include "Component/Mover/CFrameLayeredMove.h"
#include "Component/Mover/CFrameProposedMove.h"
#include "Component/Mover/CFrameMovementMixer.h"
#include "Component/Mover/CFrameMoveStateAdapter.h"
#include "Component/Mover/Mode/CFrameWalkingMode.h"

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

	//////////////////////////////////////////////////////////////////////////
	// Transitions
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
	Context.Init(MoverComp, DeltaTime, RCF, ICF);

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

	RecordMovementSnapshot();
}

void UCFrameMoveModeStateMachine::RecordMovementSnapshot()
{
	UCommandFrameManager* CFrameManager = Context.CFrameManager;
	UCFrameMoveStateAdapter* MoveStateAdapter = Context.MoveStateAdapter;

	// 此时正在回滚并重新模拟，无需重新记录快照
	if (CFrameManager->IsInRewinding())
	{
		return;
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