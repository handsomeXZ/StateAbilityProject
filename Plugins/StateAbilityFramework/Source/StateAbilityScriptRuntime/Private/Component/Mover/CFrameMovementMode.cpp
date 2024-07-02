#include "Component/Mover/CFrameMovementMode.h"

#include "Component/CFrameMoverComponent.h"

UCFrameMovementMode::UCFrameMovementMode()
	: Super()
	, InputComponent(nullptr)
{

}

void UCFrameMovementMode::OnRegistered(FName InModeName)
{
	ModeName = InModeName;
}

void UCFrameMovementMode::SetupInputComponent(UInputComponent* InInputComponent)
{
	InputComponent = InInputComponent;
}

UCFrameMoverComponent* UCFrameMovementMode::GetMoverComp()
{
	return CastChecked<UCFrameMoverComponent>(GetOuter());
}