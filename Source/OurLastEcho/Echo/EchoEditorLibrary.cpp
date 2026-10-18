// Our Last Echo

#include "EchoEditorLibrary.h"
#include "OurLastEcho.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "Editor.h"
#include "Landscape.h"
#include "LandscapeDataAccess.h"
#include "LandscapeInfo.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "StateTree.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditingSubsystem.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorModule.h"
#include "StateTreeEditorSchema.h"
#include "StateTreeState.h"
#include "UObject/Package.h"
#include "EchoEnemyStateTree.h"
#endif

UObject* UEchoEditorLibrary::BuildEnemyStateTree(const FString& AssetPath)
{
#if WITH_EDITOR
	const FString Name = FPackageName::GetLongPackageAssetName(AssetPath);
	UPackage* Package = CreatePackage(*AssetPath);
	Package->FullyLoad();
	UStateTree* Tree = FindObject<UStateTree>(Package, *Name);
	const bool bNew = Tree == nullptr;
	if (!Tree)
	{
		Tree = NewObject<UStateTree>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
	}

	// Fresh editor data for the AI component schema (what UStateTreeFactory does for a new asset)
	const TSubclassOf<UStateTreeSchema> SchemaClass = UStateTreeAIComponentSchema::StaticClass();
	FStateTreeEditorModule& EditorModule = FStateTreeEditorModule::GetModule();
	UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(Tree, EditorModule.GetEditorDataClass(SchemaClass.Get()), NAME_None, RF_Transactional);
	Tree->EditorData = EditorData;
	EditorData->Schema = NewObject<UStateTreeSchema>(EditorData, SchemaClass, NAME_None, RF_Transactional);
	EditorData->EditorSchema = NewObject<UStateTreeEditorSchema>(EditorData, EditorModule.GetEditorSchemaClass(SchemaClass.Get()), NAME_None, RF_Transactional);

	// Root picks the first child whose condition holds; every child goes back to Root when its task finishes
	UStateTreeState& Root = EditorData->AddRootState();
	auto AddState = [&Root](const TCHAR* StateName, const TOptional<EEchoEnemyCheck> Check, EEchoEnemyTaskKind Kind)
	{
		UStateTreeState& State = Root.AddChildState(FName(StateName));
		if (Check.IsSet())
		{
			State.AddEnterCondition<FEchoEnemyCondition>(Check.GetValue());
		}
		State.AddTask<FEchoEnemyTask>(Kind);
		State.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Root);
	};
	AddState(TEXT("Stunned"), EEchoEnemyCheck::IsStunned, EEchoEnemyTaskKind::Stunned);
	AddState(TEXT("Attack"), EEchoEnemyCheck::CanAttack, EEchoEnemyTaskKind::Attack);
	AddState(TEXT("Chase"), EEchoEnemyCheck::HasTarget, EEchoEnemyTaskKind::Chase);
	AddState(TEXT("Idle"), TOptional<EEchoEnemyCheck>(), EEchoEnemyTaskKind::Idle);

	FStateTreeCompilerLog Log;
	if (!UStateTreeEditingSubsystem::CompileStateTree(Tree, Log))
	{
		Log.DumpToLog(LogOurLastEcho);
		UE_LOG(LogOurLastEcho, Error, TEXT("BuildEnemyStateTree: %s didn't compile"), *AssetPath);
		return nullptr;
	}
	if (bNew)
	{
		FAssetRegistryModule::AssetCreated(Tree);
	}
	Tree->MarkPackageDirty();
	UE_LOG(LogOurLastEcho, Log, TEXT("BuildEnemyStateTree: built %s"), *AssetPath);
	return Tree;
#else
	return nullptr;
#endif
}

ALandscape* UEchoEditorLibrary::CreateLandscapeFromHeights(FVector Location, FVector Scale, int32 ComponentCountX, int32 ComponentCountY,
	int32 QuadsPerComponent, const TArray<float>& Heights, UMaterialInterface* Material, const FString& Label, FName Tag)
{
#if WITH_EDITOR
	static const TSet<int32> ValidQuads = { 7, 15, 31, 63, 127, 255 };
	if (!ValidQuads.Contains(QuadsPerComponent) || ComponentCountX < 1 || ComponentCountY < 1 || FMath::IsNearlyZero(Scale.Z))
	{
		UE_LOG(LogOurLastEcho, Error, TEXT("CreateLandscapeFromHeights: invalid size (quads %d, components %dx%d, scale z %f)"), QuadsPerComponent, ComponentCountX, ComponentCountY, Scale.Z);
		return nullptr;
	}

	const int32 VertsX = ComponentCountX * QuadsPerComponent + 1;
	const int32 VertsY = ComponentCountY * QuadsPerComponent + 1;
	if (Heights.Num() != VertsX * VertsY)
	{
		UE_LOG(LogOurLastEcho, Error, TEXT("CreateLandscapeFromHeights: expected %d heights (%d x %d), got %d"), VertsX * VertsY, VertsX, VertsY, Heights.Num());
		return nullptr;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	// Heights are stored as uint16 around a mid value, in units of the actor's Z scale
	TArray<uint16> HeightData;
	HeightData.SetNumUninitialized(Heights.Num());
	for (int32 Index = 0; Index < Heights.Num(); ++Index)
	{
		HeightData[Index] = LandscapeDataAccess::GetTexHeight(Heights[Index] / Scale.Z);
	}

	TMap<FGuid, TArray<uint16>> HeightDataPerLayer;
	HeightDataPerLayer.Add(FGuid(), MoveTemp(HeightData));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayer;
	MaterialLayerDataPerLayer.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());

	ALandscape* Landscape = World->SpawnActor<ALandscape>(Location, FRotator::ZeroRotator);
	Landscape->LandscapeMaterial = Material;
	Landscape->SetActorRelativeScale3D(Scale);
	Landscape->Import(FGuid::NewGuid(), 0, 0, VertsX - 1, VertsY - 1, 1, QuadsPerComponent, HeightDataPerLayer, TEXT(""),
		MaterialLayerDataPerLayer, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());

	if (ULandscapeInfo* Info = Landscape->GetLandscapeInfo())
	{
		Info->UpdateLayerInfoMap(Landscape);
	}

	Landscape->SetActorLabel(Label);
	if (!Tag.IsNone())
	{
		Landscape->Tags.AddUnique(Tag);
	}

	UE_LOG(LogOurLastEcho, Log, TEXT("CreateLandscapeFromHeights: created %s (%d x %d vertices)"), *Label, VertsX, VertsY);
	return Landscape;
#else
	return nullptr;
#endif
}

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemFactoryNew.h"
#endif

UObject* UEchoEditorLibrary::CreateNiagaraSystemFromEmitter(const FString& AssetPath, const FString& EmitterPath, UMaterialInterface* SpriteMaterial)
{
#if WITH_EDITOR
	UNiagaraEmitter* Template = LoadObject<UNiagaraEmitter>(nullptr, *EmitterPath);
	if (!Template)
	{
		UE_LOG(LogOurLastEcho, Error, TEXT("CreateNiagaraSystemFromEmitter: emitter %s not found"), *EmitterPath);
		return nullptr;
	}

	const FString AssetName = FPackageName::GetShortName(AssetPath);
	UPackage* Package = CreatePackage(*AssetPath);
	if (UObject* Existing = FindObject<UObject>(Package, *AssetName))
	{
		// Rename the old one out of the way so the new asset can take its name
		Existing->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
	}

	UNiagaraSystem* System = NewObject<UNiagaraSystem>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	UNiagaraSystemFactoryNew::InitializeSystem(System, true);
	FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Template, Template->GetExposedVersion().VersionGuid, true);

	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		if (FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData())
		{
			for (UNiagaraRendererProperties* Renderer : Data->GetRenderers())
			{
				if (UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer); Sprite && SpriteMaterial)
				{
					Sprite->Material = SpriteMaterial;
				}
			}
		}
	}

	System->RequestCompile(false);
	System->WaitForCompilationComplete();
	FAssetRegistryModule::AssetCreated(System);
	Package->MarkPackageDirty();
	UE_LOG(LogOurLastEcho, Log, TEXT("CreateNiagaraSystemFromEmitter: made %s from %s"), *AssetPath, *EmitterPath);
	return System;
#else
	return nullptr;
#endif
}
