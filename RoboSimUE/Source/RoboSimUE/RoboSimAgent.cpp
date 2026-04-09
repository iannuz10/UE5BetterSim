#include "RoboSimAgent.h"
#include "SceneExporter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ZenohSubsystem.h"
#include "Engine/GameInstance.h"

TSet<FString> ARoboSimAgent::GlobalRobotIDs;

ARoboSimAgent::ARoboSimAgent()
{
    PrimaryActorTick.bCanEverTick = true;
    bIsMobile = false;
    RootFrameComponent = nullptr;
}

void ARoboSimAgent::BeginPlay()
{
    Super::BeginPlay();

    // ID Registration Logic
    if (RobotID.IsEmpty())
    {
        RegisteredRobotID = GetName();
    }
    else if (GlobalRobotIDs.Contains(RobotID))
    {
        USceneExporter::WriteSimulationLog(TEXT("WARN"), FString::Printf(TEXT("RobotID '%s' is already in use. Falling back to unique name: '%s'"), *RobotID, *GetName()));
        RegisteredRobotID = GetName();
    }
    else
    {
        RegisteredRobotID = RobotID;
    }

    GlobalRobotIDs.Add(RegisteredRobotID);

    // Zenoh Subscription
    FString StateTopic = FString::Printf(TEXT("sim/agent/%s/state"), *RegisteredRobotID);

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UZenohSubsystem* ZenohSubsystem = GameInstance->GetSubsystem<UZenohSubsystem>())
        {
            UZenohTopicListener* Listener = ZenohSubsystem->SubscribeToTopic(ConnectionName, StateTopic);
            if (Listener)
            {
                ActiveListeners.Add(Listener);
                Listener->OnTopicParsed.AddUObject(this, &ARoboSimAgent::HandleStateUpdate);
                USceneExporter::WriteSimulationLog(TEXT("INFO"), FString::Printf(TEXT("ARoboSimAgent '%s' subscribed to topic: %s"), *RegisteredRobotID, *StateTopic));
            }
        }
    }
}

void ARoboSimAgent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Unregister ID
    if (!RegisteredRobotID.IsEmpty())
    {
        GlobalRobotIDs.Remove(RegisteredRobotID);
    }

    // Cleanly unbind delegates
    for (UZenohTopicListener* Listener : ActiveListeners)
    {
        if (Listener)
        {
            Listener->OnTopicParsed.RemoveAll(this);
        }
    }
    ActiveListeners.Empty();

    Super::EndPlay(EndPlayReason);
}

void ARoboSimAgent::HandleStateUpdate(TSharedPtr<FJsonObject> JsonObject)
{
    if (ApplyState(JsonObject))
    {
        ReceiveAgentStateUpdated();
    }
}

void ARoboSimAgent::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

bool ARoboSimAgent::CacheJointComponents()
{
    JointsCache.Empty();
    InitialTransformsCache.Empty();

    // Automatically assign appropriate tags
    Tags.AddUnique(TEXT("Simulate"));
    Tags.AddUnique(TEXT("Agent"));

    if (bIsMobile)
    {
        Tags.AddUnique(TEXT("Mobile"));
        Tags.Remove(TEXT("Bolted"));
    }
    else
    {
        Tags.AddUnique(TEXT("Bolted"));
        Tags.Remove(TEXT("Mobile"));
    }

    RootFrameComponent = GetRootComponent();

    if (bIsMobile && !RootFrameComponent)
    {
        USceneExporter::WriteSimulationLog(TEXT("ERROR"), TEXT("ARoboSimAgent: Mobile agent missing root component."));
        return false;
    }

    TArray<USceneComponent*> AllComponents;
    GetComponents<USceneComponent>(AllComponents);

    for (USceneComponent* Comp : AllComponents)
    {
        FString CompName = Comp->GetName();
        if (CompName.EndsWith(TEXT("_DefaultSceneRoot")))
        {
            FString JointName = CompName.LeftChop(17);
            
            // Cache the component and its rest transform
            JointsCache.Add(JointName, Comp);
            InitialTransformsCache.Add(JointName, Comp->GetRelativeTransform());
            USceneExporter::WriteSimulationLog(TEXT("DEBUG"), FString::Printf(TEXT("Cached joint: %s -> %s"), *JointName, *CompName));
        }
    }

    if (JointsCache.Num() == 0)
    {
        USceneExporter::WriteSimulationLog(TEXT("WARN"), TEXT("ARoboSimAgent: No joint components found with '_DefaultSceneRoot' suffix."));
        return false;
    }

    return true;
}

bool ARoboSimAgent::ApplyState(TSharedPtr<FJsonObject> JsonObject)
{
    if (!JsonObject.IsValid())
    {
        USceneExporter::WriteSimulationLog(TEXT("ERROR"), TEXT("ARoboSimAgent: Received invalid JSON object."));
        return false;
    }

    // 1. Root Transform (Mobile Base)
    if (bIsMobile && RootFrameComponent && JsonObject->HasField(TEXT("root_transform")))
    {
        const TSharedPtr<FJsonObject> RootObj = JsonObject->GetObjectField(TEXT("root_transform"));
        
        if (RootObj->HasField(TEXT("pos")) && RootObj->HasField(TEXT("quat")))
        {
            const TArray<TSharedPtr<FJsonValue>> PosArr = RootObj->GetArrayField(TEXT("pos"));
            const TArray<TSharedPtr<FJsonValue>> QuatArr = RootObj->GetArrayField(TEXT("quat"));

            if (PosArr.Num() == 3 && QuatArr.Num() == 4)
            {
                FVector GlobalPos(PosArr[0]->AsNumber(), PosArr[1]->AsNumber(), PosArr[2]->AsNumber());
                FQuat GlobalQuat(QuatArr[0]->AsNumber(), QuatArr[1]->AsNumber(), QuatArr[2]->AsNumber(), QuatArr[3]->AsNumber());
                
                RootFrameComponent->SetWorldLocationAndRotation(GlobalPos, GlobalQuat);
            }
        }
    }

    // 2. Joints
    if (JsonObject->HasField(TEXT("joints")))
    {
        const TSharedPtr<FJsonObject> JointsObj = JsonObject->GetObjectField(TEXT("joints"));

        // Hinge joints
        if (JointsObj->HasField(TEXT("hinge")))
        {
            const TSharedPtr<FJsonObject> HingeObj = JointsObj->GetObjectField(TEXT("hinge"));
            for (auto& Pair : HingeObj->Values)
            {
                USceneComponent** CompPtr = JointsCache.Find(Pair.Key);
                if (CompPtr && *CompPtr)
                {
                    float ValueRad = Pair.Value->AsNumber();
                    
                    FTransform* InitialTransformPtr = InitialTransformsCache.Find(Pair.Key);
                    if (InitialTransformPtr)
                    {
                        // Apply rotation around local Z-axis (standard for UE5 imported robots).
                        FQuat AddedRot(FVector(0.0f, 0.0f, 1.0f), ValueRad);
                        (*CompPtr)->SetRelativeRotation(InitialTransformPtr->GetRotation() * AddedRot);
                    }
                }
                else
                {
                    USceneExporter::WriteSimulationLog(TEXT("WARN"), FString::Printf(TEXT("ApplyState: Hinge joint %s not found in cache."), *Pair.Key));
                }
            }
        }

        // Slide joints
        if (JointsObj->HasField(TEXT("slide")))
        {
            const TSharedPtr<FJsonObject> SlideObj = JointsObj->GetObjectField(TEXT("slide"));
            for (auto& Pair : SlideObj->Values)
            {
                USceneComponent** CompPtr = JointsCache.Find(Pair.Key);
                if (CompPtr && *CompPtr)
                {
                    float ValueMeters = Pair.Value->AsNumber();
                    float ValueCm = ValueMeters * 100.0f;
                    
                    FTransform* InitialTransformPtr = InitialTransformsCache.Find(Pair.Key);
                    if (InitialTransformPtr)
                    {
                        // Apply translation along local Z-axis.
                        FVector AddedLoc(0.0f, 0.0f, ValueCm);
                        (*CompPtr)->SetRelativeLocation(InitialTransformPtr->GetLocation() + AddedLoc);
                    }
                }
                else
                {
                    USceneExporter::WriteSimulationLog(TEXT("WARN"), FString::Printf(TEXT("ApplyState: Slide joint %s not found in cache."), *Pair.Key));
                }
            }
        }
    }

    return true;
}
