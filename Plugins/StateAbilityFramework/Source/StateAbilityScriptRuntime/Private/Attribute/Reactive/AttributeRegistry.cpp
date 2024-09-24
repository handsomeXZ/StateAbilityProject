#include "Attribute/Reactive/AttributeRegistry.h"

#include "Algo/TopologicalSort.h"

#include "Attribute/Reactive/AttributeReflection.h"

namespace Attribute::Reactive
{
	TMap<UClass*, TArray<FReactiveRegistry::FAttributeReflection>> FReactiveRegistry::ClassProperties;
	TMap<UScriptStruct*, TArray<FReactiveRegistry::FAttributeReflection>> FReactiveRegistry::StructProperties;

	TArray<FReactiveRegistry::FUnprocessedPropertyEntry>& FReactiveRegistry::GetUnprocessedClassProperties()
	{
		static TArray<FUnprocessedPropertyEntry> Result;
		return Result;
	}

	TArray<FReactiveRegistry::FUnprocessedPropertyEntry>& FReactiveRegistry::GetUnprocessedStructProperties()
	{
		static TArray<FUnprocessedPropertyEntry> Result;
		return Result;
	}

	TSet<FReactiveRegistry::FFieldClassGetterPtr>& FReactiveRegistry::GetUnprocessedClass()
	{
		static TSet<FFieldClassGetterPtr> Result;
		return Result;
	}

	TSet<FReactiveRegistry::FFieldClassGetterPtr>& FReactiveRegistry::GetUnprocessedStruct()
	{
		static TSet<FFieldClassGetterPtr> Result;
		return Result;
	}

	void FReactiveRegistry::RegisterClassProperties(TArray<UClass*>& LocalRegisteredClass, TSet<UClass*>& ProcessedClassSet)
	{
		for (auto& Property : GetUnprocessedClassProperties())
		{
			UClass* NewFieldClass = (UClass*)Property.GetFieldClass();

			TArray<FAttributeReflection>* NewArray = ClassProperties.Find(NewFieldClass);
			if (!NewArray)
			{
				NewArray = &ClassProperties.Emplace(NewFieldClass);
				LocalRegisteredClass.Add((UClass*)NewFieldClass);
				ProcessedClassSet.Add(NewFieldClass);
			}

			// We need to check that we don't have same property already registered
			// It is possible if same ReactiveModel is referenced from different modules (.dll) leading to multiple instantiations of registrator template
			const bool bPropertyAlreadyRegistered = NewArray->ContainsByPredicate([&](const FAttributeReflection& ExistingReflection)
				{
					return ExistingReflection.GetProperty() == Property.Reflection.GetProperty();
				});

			if (!bPropertyAlreadyRegistered)
			{
				NewArray->Add(Property.Reflection);
			}
		}

		GetUnprocessedClassProperties().Empty();
	}

	void FReactiveRegistry::RegisterStructProperties(TArray<UScriptStruct*>& LocalRegisteredStruct, TSet<UScriptStruct*>& ProcessedStructSet)
	{
		for (auto& Property : GetUnprocessedStructProperties())
		{
			UScriptStruct* NewFieldClass = (UScriptStruct*)Property.GetFieldClass();

			TArray<FAttributeReflection>* NewArray = StructProperties.Find(NewFieldClass);
			if (!NewArray)
			{
				NewArray = &StructProperties.Emplace(NewFieldClass);
				LocalRegisteredStruct.Add((UScriptStruct*)NewFieldClass);
				ProcessedStructSet.Add(NewFieldClass);
			}

			const bool bPropertyAlreadyRegistered = NewArray->ContainsByPredicate([&](const FAttributeReflection& ExistingReflection)
				{
					return ExistingReflection.GetProperty() == Property.Reflection.GetProperty();
				});

			if (!bPropertyAlreadyRegistered)
			{
				NewArray->Add(Property.Reflection);
			}
		}

		GetUnprocessedStructProperties().Empty();
	}

	void FReactiveRegistry::ProcessPendingRegistrations()
	{
		if (GIsInitialLoad)
		{
			// wait until UObject subsystem is loaded
			return;
		}

		// Find all class/struct that define the attribute or registered manually
		TArray<UClass*> LocalRegisteredClass;
		TArray<UScriptStruct*> LocalRegisteredStruct;

		TSet<UClass*> ProcessedClassSet;
		TSet<UScriptStruct*> ProcessedStructSet;

		RegisterClassProperties(LocalRegisteredClass, ProcessedClassSet);
		RegisterStructProperties(LocalRegisteredStruct, ProcessedStructSet);

		if (!LocalRegisteredClass.IsEmpty())
		{
			// Add all derived classes of NewlyAddedReactiveModels to the list
			// Some derived classes may have no properties, but their schemas must still be adjusted to account for schemas of base classes
			// We need to add them manually because we won't know about them otherwise
			for (FFieldClassGetterPtr GetterPtr : GetUnprocessedClass())
			{
				UClass* FieldClass = (UClass*)GetterPtr();
				if (!ProcessedClassSet.Contains(FieldClass))
				{
					LocalRegisteredClass.Add(FieldClass);
					ProcessedClassSet.Add(FieldClass);
				}
			}

			for (UClass* CurClass : ProcessedClassSet)
			{
				TArray<UClass*> TempAdded;
				CurClass = CurClass->GetSuperClass();
				while (CurClass != nullptr)
				{
					if (!ProcessedClassSet.Contains(CurClass))
					{
						TempAdded.Add(CurClass);
					}
					else
					{
						LocalRegisteredClass.Append(TempAdded);
					}

					CurClass = CurClass->GetSuperClass();
				}
			}

			// Sort ReactiveModels so base classes are located before derived ones
			// This way we guarantee that base classes' schema will be generated first
			// If they are in different modules, Unreal will handle module load ordering
			// In monolithic mode all ReactiveModels are processed at once, like in a single module
			// TopologicalSort may return incorrect results in case we have class hierarachy A -> B -> C, but only A and C present in Classes
			// but we always call EnrichWithDerivedClasses before sorting, so this situation never happens actually
			Algo::TopologicalSort(LocalRegisteredClass, [](UClass* C) { return TArray<UClass*, TFixedAllocator<1>>{ C->GetSuperClass() }; });

			// Patch all new ReactiveModel FieldClasses, so their properties will be processed by GC
			for (UClass* Class : LocalRegisteredClass)
			{
				AssembleReferenceSchema(Class);
			}
		}

		if (!LocalRegisteredStruct.IsEmpty())
		{
			// Add all derived classes of NewlyAddedReactiveModels to the list
			// Some derived classes may have no properties, but their schemas must still be adjusted to account for schemas of base classes
			// We need to add them manually because we won't know about them otherwise
			for (FFieldClassGetterPtr GetterPtr : GetUnprocessedStruct())
			{
				UScriptStruct* FieldClass = (UScriptStruct*)GetterPtr();
				if (!ProcessedStructSet.Contains(FieldClass))
				{
					LocalRegisteredStruct.Add(FieldClass);
					ProcessedStructSet.Add(FieldClass);
				}
			}

			for (UScriptStruct* CurStruct : ProcessedStructSet)
			{
				TArray<UScriptStruct*> TempAdded;
				CurStruct = (UScriptStruct*)CurStruct->GetSuperStruct();
				while (CurStruct != nullptr)
				{
					if (!ProcessedStructSet.Contains(CurStruct))
					{
						TempAdded.Add(CurStruct);
					}
					else
					{
						LocalRegisteredStruct.Append(TempAdded);
					}
					CurStruct = (UScriptStruct*)CurStruct->GetSuperStruct();
				}
			}

			// Sort ReactiveModels so base classes are located before derived ones
			// This way we guarantee that base classes' schema will be generated first
			// If they are in different modules, Unreal will handle module load ordering
			// In monolithic mode all ReactiveModels are processed at once, like in a single module
			// TopologicalSort may return incorrect results in case we have class hierarachy A -> B -> C, but only A and C present in Classes
			// but we always call EnrichWithDerivedClasses before sorting, so this situation never happens actually
			Algo::TopologicalSort(LocalRegisteredStruct, [](UScriptStruct* S) { return TArray<UScriptStruct*, TFixedAllocator<1>>{ (UScriptStruct*)S->GetSuperStruct() }; });

			// Patch all new ReactiveModel FieldClasses, so their properties will be processed by GC
			for (UScriptStruct* Struct : LocalRegisteredStruct)
			{
				AssembleReferenceSchema(Struct);
			}
		}


		GetUnprocessedClass().Empty();
		GetUnprocessedStruct().Empty();
	}

	const FReactivePropertyOperations* FReactiveRegistry::FindPropertyOperations(UClass* Class, const FName& InPropertyName)
	{
		// find properties of requested class
		TArray<FAttributeReflection>* ArrayPtr = ClassProperties.Find(Class);

		if (ArrayPtr)
		{
			for (const FAttributeReflection& Item : *ArrayPtr)
			{
				if (Item.GetProperty()->GetName() == InPropertyName)
				{
					return Item.GetOperations();
				}
			}
		}

		// if not found - look in a super class
		UClass* SuperClass = Class->GetSuperClass();
		if (SuperClass)
		{
			return FindPropertyOperations(SuperClass, InPropertyName);
		}

		return nullptr;
	}

	const FReactivePropertyOperations* FReactiveRegistry::FindPropertyOperations(UScriptStruct* Struct, const FName& InPropertyName)
	{
		// find properties of requested struct
		TArray<FAttributeReflection>* ArrayPtr = StructProperties.Find(Struct);

		if (ArrayPtr)
		{
			for (const FAttributeReflection& Item : *ArrayPtr)
			{
				if (Item.GetProperty()->GetName() == InPropertyName)
				{
					return Item.GetOperations();
				}
			}
		}

		// if not found - look in a super Struct
		UScriptStruct* SuperStruct = (UScriptStruct*)Struct->GetSuperStruct();
		if (SuperStruct)
		{
			return FindPropertyOperations(SuperStruct, InPropertyName);
		}

		return nullptr;
	}

	void FReactiveRegistry::AssembleReferenceSchema(UClass* Class)
	{
		// We take only properties of current class, because AssembleTokenStream merges schemas with Super class
		const TArray<FAttributeReflection>* Properties = ClassProperties.Find(Class);

		if (Properties == nullptr)
		{
			// this Class does not have its own properties, just update schema to include schemas from parent class
			Class->AssembleReferenceTokenStream(true);
			return;
		}

		// Assemble FProperties and add them to Class
		// Save original pointer to first property, so we can restore it later
		FField* FirstOriginalField = Class->ChildProperties;

		// Reverse the order of properties, otherwise it may lead to cache misses.
		// Create FProperty objects and add them to class
		for (auto PropIt = Properties->rbegin(); PropIt != Properties->rend(); ++PropIt)
		{
			const FAttributeReflection& RefProperty = *PropIt;
			RefProperty.GetOperations()->AddFieldClassProperty(Class);
		}

		// Link class to populate cached data inside newly created FProperties
		Class->StaticLink();

		// Create schema that includes our new FProperties
		Class->AssembleReferenceTokenStream(true);

		// delete all unrelevant properties and restore original list pointer
		FField* CurField = Class->ChildProperties;
		while (CurField && CurField != FirstOriginalField)
		{
			delete CurField;
			CurField = CurField->Next;
		}
		Class->ChildProperties = FirstOriginalField;
		Class->StaticLink();
	}

	void FReactiveRegistry::AssembleReferenceSchema(UScriptStruct* Struct)
	{
		// We take only properties of current struct, because AssembleTokenStream merges schemas with Super struct
		const TArray<FAttributeReflection>* Properties = StructProperties.Find(Struct);

		if (Properties == nullptr)
		{
			return;
		}

		// Reverse the order of properties, otherwise it may lead to cache misses.
		// Create FProperty objects and add them to struct
		for (auto PropIt = Properties->rbegin(); PropIt != Properties->rend(); ++PropIt)
		{
			const FAttributeReflection& RefProperty = *PropIt;
			RefProperty.GetOperations()->AddFieldClassProperty(Struct);
		}

		// Link struct to populate cached data inside newly created FProperties
		Struct->StaticLink();

		// struct can't delete properties(for GC)
	}
}