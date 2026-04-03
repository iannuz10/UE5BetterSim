// Fill out your copyright notice in the Description page of Project Settings.

#include "RoboSimAgent.h"
#include "SceneExporter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

ARoboSimAgent::ARoboSimAgent()
{
	PrimaryActorTick.bCanEverTick = false;
}

// ==========================================
// COMPONENT CACHE
// ==========================================

void ARoboSimAgent::CacheJointComponents()
{
	JointComponentCache.Empty();
	JointRestRotations.Empty();
	WarnedMissingJoints.Empty();
	bCacheBuilt = false;
	bParseErrorLogged = false;
	bFirstCallLogged = false;

	TArray<USceneComponent*> AllComponents;
	GetComponents<USceneComponent>(AllComponents);

	// Log every component name (raw → cleaned) for cross-referencing against MuJoCo
	// joint names. This dump is the primary diagnostic for naming mismatches.
	FString Dump = FString::Printf(
		TEXT("ARoboSimAgent [%s]: CacheJointComponents — %d components found:"),
		*GetName(), AllComponents.Num()
	);

	for (USceneComponent* Comp : AllComponents)
	{
		if (!Comp) continue;

		FString RawName   = Comp->GetName();
		FString CleanName = CleanComponentName(RawName);

		if (CleanName.IsEmpty())
		{
			Dump += FString::Printf(TEXT("\n  raw='%s'  [SKIPPED — empty after cleaning]"), *RawName);
			continue;
		}

		if (JointComponentCache.Contains(CleanName))
		{
			Dump += FString::Printf(TEXT("\n  raw='%s'  clean='%s'  [SKIPPED — name collision]"), *RawName, *CleanName);
			continue;
		}

		// Capture the imported rest-pose rotation BEFORE the simulation modifies anything.
		// Joint angles arriving from MuJoCo will be applied as a Yaw delta on top of this.
		FRotator RestRot = Comp->GetRelativeRotation();
		JointComponentCache.Add(CleanName, Comp);
		JointRestRotations.Add(CleanName, RestRot);

		Dump += FString::Printf(
			TEXT("\n  raw='%s'  clean='%s'  restRot=(P=%.2f Y=%.2f R=%.2f)  [CACHED]"),
			*RawName, *CleanName, RestRot.Pitch, RestRot.Yaw, RestRot.Roll
		);
	}

	bCacheBuilt = true;

	USceneExporter::WriteSimulationLog(TEXT("DEBUG"), Dump);
	USceneExporter::WriteSimulationLog(
		TEXT("INFO"),
		FString::Printf(
			TEXT("ARoboSimAgent [%s]: cache ready — %d unique components indexed."),
			*GetName(), JointComponentCache.Num()
		)
	);
}

FString ARoboSimAgent::CleanComponentName(const FString& RawName)
{
	FString Cleaned = RawName;

	// Strip well-known UE5 GLB/Blueprint import artifact suffix.
	// Add more suffixes here as the asset pipeline evolves.
	Cleaned.RemoveFromEnd(TEXT("_DefaultSceneRoot"), ESearchCase::CaseSensitive);

	return Cleaned;
}

// ==========================================
// PER-FRAME JOINT APPLICATION
// ==========================================

void ARoboSimAgent::ApplyJointStates(const FString& JsonPayload)
{
	if (!bCacheBuilt)
	{
		USceneExporter::WriteSimulationLog(
			TEXT("WARN"),
			FString::Printf(
				TEXT("ARoboSimAgent [%s]: ApplyJointStates called before CacheJointComponents. Call CacheJointComponents in BeginPlay."),
				*GetName()
			)
		);
		return;
	}

	// ─── Parse JSON ───────────────────────────────────────────────────────
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonPayload);

	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		if (!bParseErrorLogged)
		{
			USceneExporter::WriteSimulationLog(
				TEXT("ERROR"),
				FString::Printf(
					TEXT("ARoboSimAgent [%s]: Failed to parse joint state JSON. First 200 chars: '%s'"),
					*GetName(), *JsonPayload.Left(200)
				)
			);
			bParseErrorLogged = true;
		}
		return;
	}

	const TSharedPtr<FJsonObject>* JointsObj;
	if (!Root->TryGetObjectField(TEXT("joints"), JointsObj))
	{
		if (!bParseErrorLogged)
		{
			USceneExporter::WriteSimulationLog(
				TEXT("WARN"),
				FString::Printf(
					TEXT("ARoboSimAgent [%s]: JSON missing 'joints' field. Payload: '%s'"),
					*GetName(), *JsonPayload.Left(200)
				)
			);
			bParseErrorLogged = true;
		}
		return;
	}

	// MuJoCo prefixes all joint names with "{ActorName}_". Strip it before cache lookup
	// because UE5 component names come from the raw URDF without any agent prefix.
	const FString AgentPrefix = GetName() + TEXT("_");

	// ─── First-call diagnostic dump ───────────────────────────────────────
	// Logs received joint names, lookup keys, cache hit/miss, and rest rotations ONCE.
	// Triggers again if CacheJointComponents is re-called (e.g. on scene rebuild).
	if (!bFirstCallLogged)
	{
		FString Diag = FString::Printf(
			TEXT("ARoboSimAgent [%s]: FIRST ApplyJointStates. AgentPrefix='%s'. Cache=%d entries."),
			*GetName(), *AgentPrefix, JointComponentCache.Num()
		);

		const TSharedPtr<FJsonObject>* HingeCheck;
		if ((*JointsObj)->TryGetObjectField(TEXT("hinge"), HingeCheck))
		{
			Diag += TEXT("\n  HINGE joints:");
			for (const auto& Pair : (*HingeCheck)->Values)
			{
				FString LookupKey = Pair.Key;
				if (LookupKey.StartsWith(AgentPrefix)) LookupKey.MidInline(AgentPrefix.Len());

				const FRotator* RestRot = JointRestRotations.Find(LookupKey);
				bool bFound = JointComponentCache.Contains(LookupKey);

				Diag += FString::Printf(
					TEXT("\n    raw='%s'  lookup='%s'  found=%s  val=%.4f rad  restRot=%s"),
					*Pair.Key, *LookupKey,
					bFound ? TEXT("YES") : TEXT("NO"),
					Pair.Value->AsNumber(),
					RestRot ? *RestRot->ToString() : TEXT("N/A")
				);
			}
		}

		const TSharedPtr<FJsonObject>* SlideCheck;
		if ((*JointsObj)->TryGetObjectField(TEXT("slide"), SlideCheck))
		{
			Diag += TEXT("\n  SLIDE joints:");
			for (const auto& Pair : (*SlideCheck)->Values)
			{
				FString LookupKey = Pair.Key;
				if (LookupKey.StartsWith(AgentPrefix)) LookupKey.MidInline(AgentPrefix.Len());

				bool bFound = JointComponentCache.Contains(LookupKey);
				Diag += FString::Printf(
					TEXT("\n    raw='%s'  lookup='%s'  found=%s  val=%.4f m"),
					*Pair.Key, *LookupKey, bFound ? TEXT("YES") : TEXT("NO"), Pair.Value->AsNumber()
				);
			}
		}

		USceneExporter::WriteSimulationLog(TEXT("DEBUG"), Diag);
		bFirstCallLogged = true;
	}

	// ─── Hinge (revolute) joints ──────────────────────────────────────────
	// Value: radians, Y-flip already applied by MuJoCo StatePublisher.
	//
	// Applied as a quaternion composition: Q_final = Q_rest * Q_joint(angle, localZ).
	// This correctly rotates the joint by 'angle' around the component's OWN local Z axis
	// regardless of how Q_rest is oriented, and is immune to gimbal lock.
	// (Euler addition would break for joints 2–7 which have R=90° in their rest rotation.)
	//
	// Assumption: LinkForge imports URDF joints with their joint axis aligned to local Z.
	const TSharedPtr<FJsonObject>* HingeObj;
	if ((*JointsObj)->TryGetObjectField(TEXT("hinge"), HingeObj))
	{
		for (const auto& Pair : (*HingeObj)->Values)
		{
			FString LookupKey = Pair.Key;
			if (LookupKey.StartsWith(AgentPrefix)) LookupKey.MidInline(AgentPrefix.Len());

			USceneComponent* const* CompPtr   = JointComponentCache.Find(LookupKey);
			const FRotator*         RestRotPtr = JointRestRotations.Find(LookupKey);

			if (CompPtr && *CompPtr && RestRotPtr)
			{
				// Compose: rest orientation * joint rotation around local Z.
				FQuat RestQuat  = RestRotPtr->Quaternion();
				FQuat JointQuat = FQuat(FVector::UpVector, (float)Pair.Value->AsNumber()); // angle in radians around Z
				(*CompPtr)->SetRelativeRotation(RestQuat * JointQuat);
			}
			else if (!WarnedMissingJoints.Contains(LookupKey))
			{
				USceneExporter::WriteSimulationLog(
					TEXT("WARN"),
					FString::Printf(
						TEXT("ARoboSimAgent [%s]: Hinge joint '%s' (lookup='%s') not in cache. Verify GLB component names against MJCF joint names."),
						*GetName(), *Pair.Key, *LookupKey
					)
				);
				WarnedMissingJoints.Add(LookupKey);
			}
		}
	}

	// ─── Slide (prismatic) joints ─────────────────────────────────────────
	// Value: meters → centimeters, applied as relative Z offset from rest location.
	// Assumption: joint translates along local Z axis.
	const TSharedPtr<FJsonObject>* SlideObj;
	if ((*JointsObj)->TryGetObjectField(TEXT("slide"), SlideObj))
	{
		for (const auto& Pair : (*SlideObj)->Values)
		{
			FString LookupKey = Pair.Key;
			if (LookupKey.StartsWith(AgentPrefix)) LookupKey.MidInline(AgentPrefix.Len());

			USceneComponent* const* CompPtr = JointComponentCache.Find(LookupKey);
			if (CompPtr && *CompPtr)
			{
				float Centimeters = (float)Pair.Value->AsNumber() * 100.0f;
				(*CompPtr)->SetRelativeLocation(FVector(0.0f, 0.0f, Centimeters));
			}
			else if (!WarnedMissingJoints.Contains(LookupKey))
			{
				USceneExporter::WriteSimulationLog(
					TEXT("WARN"),
					FString::Printf(
						TEXT("ARoboSimAgent [%s]: Slide joint '%s' (lookup='%s') not in cache. Verify GLB component names against MJCF joint names."),
						*GetName(), *Pair.Key, *LookupKey
					)
				);
				WarnedMissingJoints.Add(LookupKey);
			}
		}
	}
}
