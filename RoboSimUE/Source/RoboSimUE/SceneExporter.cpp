// Fill out your copyright notice in the Description page of Project Settings.


#include "SceneExporter.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

FString USceneExporter::GenerateWorldJSON(const UObject* WorldContextObject, FName ActorTag)
{
	// Find all actors with tag
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(WorldContextObject, ActorTag, FoundActors);

	TSharedPtr<FJsonObject> RootJson = MakeShareable(new FJsonObject());
	TArray<TSharedPtr<FJsonValue>> JsonObjectArray;

	for (AActor* Actor : FoundActors)
	{
		if (!Actor) continue;

		TSharedPtr<FJsonObject> ActorJson = MakeShareable(new FJsonObject());

		ActorJson->SetStringField("name", Actor->GetName());

		// Coordinate convention (UE5 to scientific)
		// UE5 left-handed (X forward, Y right, Z up)
		// Scientific right-handed (X forward, Y left, Z up)
		FVector UELoc = Actor->GetActorLocation();

		TArray<TSharedPtr<FJsonValue>> PosArray;
		// Convert cm to m
		PosArray.Add(MakeShareable(new FJsonValueNumber(UELoc.X / 100.0))); 
		PosArray.Add(MakeShareable(new FJsonValueNumber(-UELoc.Y / 100.0))); // Invert Y for right-handed
		PosArray.Add(MakeShareable(new FJsonValueNumber(UELoc.Z / 100.0))); 
		ActorJson->SetArrayField("position", PosArray);

		// Add more fields as needed (rotation as quaternion)
		FQuat UERot = Actor->GetActorQuat();
		FQuat SciRot(-UERot.X, UERot.Y, -UERot.Z, UERot.W); // Invert X and Z for right-handed
		
		TArray<TSharedPtr<FJsonValue>> QuatArray;
		QuatArray.Add(MakeShareable(new FJsonValueNumber(SciRot.W)));
		QuatArray.Add(MakeShareable(new FJsonValueNumber(SciRot.X)));
		QuatArray.Add(MakeShareable(new FJsonValueNumber(SciRot.Y)));
		QuatArray.Add(MakeShareable(new FJsonValueNumber(SciRot.Z)));
		ActorJson->SetArrayField("quat", QuatArray);

		// add sizes
		FVector Origin;
		FVector BoxExtent;
		// Get the absolute physical bounds of the actor (false = don't include purely visual bounds)
		Actor->GetActorBounds(false, Origin, BoxExtent); 

		// Convert the Half-Extent from Centimeters to Meters
		TArray<TSharedPtr<FJsonValue>> SizeArray;
		SizeArray.Add(MakeShareable(new FJsonValueNumber(BoxExtent.X / 100.0)));
		SizeArray.Add(MakeShareable(new FJsonValueNumber(BoxExtent.Y / 100.0))); 
		SizeArray.Add(MakeShareable(new FJsonValueNumber(BoxExtent.Z / 100.0)));
		ActorJson->SetArrayField("size", SizeArray);

		// get shape info from StaticMeshComponent if available
		UStaticMeshComponent* MeshComp = Actor->FindComponentByClass<UStaticMeshComponent>();
		if (MeshComp && MeshComp->GetStaticMesh())
		{
			FString MeshName = MeshComp->GetStaticMesh()->GetName();
			ActorJson->SetStringField("mesh", MeshName);
		}
		
		JsonObjectArray.Add(MakeShareable(new FJsonValueObject(ActorJson)));
	}

	RootJson->SetArrayField("objects", JsonObjectArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootJson.ToSharedRef(), Writer);

	return OutputString;
}