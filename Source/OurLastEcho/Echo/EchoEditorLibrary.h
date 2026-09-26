// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EchoEditorLibrary.generated.h"

class ALandscape;
class UMaterialInterface;

/**
 *  Editor-only helpers for the Python level builders in Scripts/.
 *  Exposed to Python as unreal.EchoEditorLibrary. They do nothing in a packaged game.
 */
UCLASS()
class UEchoEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 *  Creates a Landscape in the editor world from a height grid, the same way the Landscape
	 *  mode's "New Landscape" button does (Python has no API for this).
	 *
	 *  @param Location          World position of the landscape's first vertex (min X, min Y corner)
	 *  @param Scale             Actor scale. X/Y = cm between vertices; Z = 100 gives a +/-256 m height range
	 *  @param ComponentCountX   Components along X
	 *  @param ComponentCountY   Components along Y
	 *  @param QuadsPerComponent Quads per component side: 7, 15, 31, 63, 127 or 255 (one section per component)
	 *  @param Heights           World-space heights in cm relative to Location.Z, row by row (index = Y * VertsX + X),
	 *                           where VertsX = ComponentCountX * QuadsPerComponent + 1
	 *  @param Material          Landscape material (optional)
	 *  @param Label             Outliner label
	 *  @param Tag               Actor tag, so builder scripts can find and replace it
	 *  @return The new landscape, or null if the inputs were invalid
	 */
	UFUNCTION(BlueprintCallable, Category="Echo|Editor", meta=(DevelopmentOnly))
	static ALandscape* CreateLandscapeFromHeights(FVector Location, FVector Scale, int32 ComponentCountX, int32 ComponentCountY,
		int32 QuadsPerComponent, const TArray<float>& Heights, UMaterialInterface* Material, const FString& Label, FName Tag);

	/**
	 *  Creates (or replaces) a Niagara system asset containing a copy of one emitter, e.g. one of Niagara's
	 *  templates, with every sprite renderer using SpriteMaterial. Python can't build Niagara systems otherwise.
	 *  The caller saves the asset.
	 *
	 *  @param AssetPath       e.g. /Game/Echo/FX/NS_TitleLeaves
	 *  @param EmitterPath     e.g. /Niagara/DefaultAssets/Templates/Emitters/BlowingParticles
	 *  @param SpriteMaterial  must be usable with Niagara sprites (bUsedWithNiagaraSprites)
	 */
	UFUNCTION(BlueprintCallable, Category="Echo|Editor", meta=(DevelopmentOnly))
	static UObject* CreateNiagaraSystemFromEmitter(const FString& AssetPath, const FString& EmitterPath, UMaterialInterface* SpriteMaterial);
};
