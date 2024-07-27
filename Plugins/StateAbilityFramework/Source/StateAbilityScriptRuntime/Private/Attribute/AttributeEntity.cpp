#include "Attribute/AttributeEntity.h"


FAttributeEntity::FAttributeEntity()
{

}

FAttributeEntity::~FAttributeEntity()
{
	if (EntityHandle.IsValid() && EntitySubsystem.IsValid())
	{
		EntitySubsystem->GetMutableEntityManager().DestroyEntity(EntityHandle);
	}
}

FAttributeEntity::FAttributeEntity(const FAttributeEntity& InOther)
	: EntityHandle(InOther.EntityHandle)
	, EntitySubsystem(InOther.EntitySubsystem)
{

}

FAttributeEntity& FAttributeEntity::operator=(const FAttributeEntity& InOther)
{
	EntityHandle = InOther.EntityHandle;
	EntitySubsystem = InOther.EntitySubsystem;

	return *this;
}

void FAttributeEntity::Initialize(FAttributeEntityBuildParam& BuildParam)
{
	check(IsValid(BuildParam.World));

	EntitySubsystem = BuildParam.World->GetSubsystem<UMassEntitySubsystem>();

	if (EntitySubsystem.IsValid())
	{
		FMassArchetypeHandle ArchetypeHandle = EntitySubsystem->GetMutableEntityManager().CreateArchetype(BuildParam.ArchetypeFragment);
		EntityHandle = EntitySubsystem->GetMutableEntityManager().CreateEntity(ArchetypeHandle, BuildParam.SharedFragmentValue);
	}
}

bool FAttributeEntity::IsDataValid() const
{
	return EntityHandle.IsValid() && EntitySubsystem.IsValid();
}

FStructView FAttributeEntity::Get(const UScriptStruct* FragmentType) const
{
	if (EntitySubsystem.IsValid() && EntityHandle.IsValid())
	{
		return EntitySubsystem->GetEntityManager().GetFragmentDataStruct(EntityHandle, FragmentType);
	}

	return FStructView();
}