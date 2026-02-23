// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SceneExporter.generated.h"

/**
 * 
 */
UCLASS()
class ROBOSIMUE_API USceneExporter : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, Category = "Simulation|Export", meta = (WorldContext = "WorldContextObject"))	
	static FString GenerateWorldJSON(const UObject* WorldContextObject, FName ActorTag);

	UFUNCTION(BlueprintCallable, Category = "Simulation|Import", meta = (WorldContext = "WorldContextObject"))
	static void ApplyWorldStateJSON(const UObject* WorldContextObject, FName ActorTag, const FString& JsonString);
};
