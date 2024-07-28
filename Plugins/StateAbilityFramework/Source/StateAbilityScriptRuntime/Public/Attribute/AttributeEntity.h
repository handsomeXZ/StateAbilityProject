#pragma once
#include "CoreMinimal.h"

#include "StructView.h"

#include "MassEntityTypes.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"

struct FAttributeEntityBuildParam
{
	UWorld* World = nullptr;
	TArray<const UScriptStruct*> ArchetypeFragment;
	FMassArchetypeSharedFragmentValues SharedFragmentValue;
};

// FMassSharedFragment

/**
 * 1. 网络传输？用不上，因为每个同步对象都是单独序列化的。
 * 2. 状态快照？用不上，因为仅Autonomous需要记录快照。
 * 3. 数据分离？可以，将数据交给World下的单例管理，方便读取。
 * 4. 仅运行时才有数据
 */
struct FAttributeEntity
{
	FAttributeEntity();
	virtual ~FAttributeEntity();

	// 拷贝不会分配新的数据
	FAttributeEntity(const FAttributeEntity& InOther);
	FAttributeEntity& operator=(const FAttributeEntity& InOther);
	
	// 分配数据
	virtual void Initialize(FAttributeEntityBuildParam& BuildParam);
	bool IsDataValid() const;

	FStructView Get(const UScriptStruct* FragmentType) const;
	
	template<typename FragmentType>
	FragmentType& Get() const;
	template<typename FragmentType>
	FragmentType* GetPtr() const;

	template<typename SharedFragmentType>
	SharedFragmentType& GetShared() const;
	
private:
	FMassEntityHandle EntityHandle;
	TWeakObjectPtr<UMassEntitySubsystem> EntitySubsystem;
};

template<typename FragmentType>
FragmentType& FAttributeEntity::Get() const
{
	ensureMsgf(EntitySubsystem.IsValid() && EntityHandle.IsValid(), TEXT("EntitySubsystem is invalid or EntityHandle is invalid."));

	return EntitySubsystem->GetEntityManager().GetFragmentData<FragmentType>(EntityHandle);
}

template<typename FragmentType>
FragmentType* FAttributeEntity::GetPtr() const
{
	if (EntitySubsystem.IsValid() && EntityHandle.IsValid())
	{
		return EntitySubsystem->GetEntityManager().GetFragmentDataPtr<FragmentType>(EntityHandle);
	}

	return nullptr;
}

template<typename SharedFragmentType>
SharedFragmentType& FAttributeEntity::GetShared() const
{
	ensureMsgf(EntitySubsystem.IsValid() && EntityHandle.IsValid(), TEXT("EntitySubsystem is invalid or EntityHandle is invalid."));

	return EntitySubsystem->GetEntityManager().GetSharedFragmentDataChecked<SharedFragmentType>(EntityHandle);
}