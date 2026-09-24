// Our Last Echo

#include "EchoEditorLibrary.h"
#include "OurLastEcho.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Landscape.h"
#include "LandscapeDataAccess.h"
#include "LandscapeInfo.h"
#include "Materials/MaterialInterface.h"
#endif

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
