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
	static FString GenerateWorldJSON(const UObject* WorldContextObject, const TArray<FName>& ActorTags);

	UFUNCTION(BlueprintCallable, Category = "Simulation|Import", meta = (WorldContext = "WorldContextObject"))
	static void ApplyWorldStateJSON(const UObject* WorldContextObject, const TArray<FName>& ActorTags, const FString& JsonString);

	// Splits the data into two Maps so Blueprints know which math to apply
	UFUNCTION(BlueprintCallable, Category = "Simulation|Import")
	static bool ParseJointData(const FString& JsonString, TMap<FString, float>& OutHingeJoints, TMap<FString, float>& OutSlideJoints);
};
