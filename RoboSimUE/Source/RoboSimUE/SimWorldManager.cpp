#include "SimWorldManager.h"
#include "SceneExporter.h"
#include "ZenohSubsystem.h"
#include "Engine/GameInstance.h"
#include "RoboSimAgent.h"
#include "Kismet/GameplayStatics.h"

ASimWorldManager::ASimWorldManager()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ASimWorldManager::BeginPlay()
{

    Super::BeginPlay();

    // 1. Build the O(1) Actor Cache for the world props
    USceneExporter::BuildActorCache(this, WorldActorTags);

    // 2. Build the O(1) Agent Cache so we can route agent data quickly
    AgentCache.Empty();
    TArray<AActor*> FoundAgents;
    UGameplayStatics::GetAllActorsOfClass(this, ARoboSimAgent::StaticClass(), FoundAgents);
    for (AActor* Actor : FoundAgents)
    {
        if (ARoboSimAgent* Agent = Cast<ARoboSimAgent>(Actor))
        {
            AgentCache.Add(Agent->GetRegisteredID(), Agent);
        }
    }

    // 3. Subscribe to the SINGLE Snapshot topic
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UZenohSubsystem* ZenohSubsystem = GameInstance->GetSubsystem<UZenohSubsystem>())
        {
            
            UZenohTopicListener* Listener = ZenohSubsystem->SubscribeToTopic(WorldConnectionName, WorldTopic);
            if (Listener)
            {
                ActiveListeners.Add(Listener);
                Listener->OnTopicParsed.AddUObject(this, &ASimWorldManager::HandleWorldStateUpdate);
                UE_LOG(LogTemp, Error, TEXT("[SNAPSHOT] Successfully subscribed to %s"), *WorldTopic);
            }
            else
            {
                // IF YOU SEE THIS RED LOG: The Connection Name or Subsystem Routing is broken
                UE_LOG(LogTemp, Fatal, TEXT("[SNAPSHOT] FAILED TO SUBSCRIBE! Is the Connection '%s' actually connected yet?"), *WorldConnectionName.ToString());
            }
        }
    }
}

void ASimWorldManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Cleanly unbind delegates
    for (UZenohTopicListener* Listener : ActiveListeners)
    {
        if (Listener)
        {
            Listener->OnTopicParsed.RemoveAll(this);
        }
    }
    ActiveListeners.Empty();

    // Clear the static cache
    USceneExporter::ClearActorCache();

    Super::EndPlay(EndPlayReason);
}

void ASimWorldManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ASimWorldManager::PublishWorldInit()
{
    FString InitJson = USceneExporter::GenerateWorldJSON(this, WorldActorTags);

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UZenohSubsystem* ZenohSubsystem = GameInstance->GetSubsystem<UZenohSubsystem>())
        {
            if (ZenohSubsystem->Publish(WorldConnectionName, TEXT("sim/world/init"), InitJson))
            {
                USceneExporter::WriteSimulationLog(TEXT("INFO"), TEXT("Published sim/world/init successfully."));
            }
            else
            {
                USceneExporter::WriteSimulationLog(TEXT("ERROR"), TEXT("Failed to publish sim/world/init."));
            }
        }
    }
}

void ASimWorldManager::HandleWorldStateUpdate(TSharedPtr<FJsonObject> JsonObject)
{
    if (!JsonObject.IsValid()) 
    {
        UE_LOG(LogTemp, Error, TEXT("[WorldManager] Received invalid JSON Object"));
        return;
    }

    // TRACER DYE 1: What fields did we actually receive?
    TArray<FString> Keys;
    JsonObject->Values.GetKeys(Keys);
    UE_LOG(LogTemp, Verbose, TEXT("[WorldManager] Received Snapshot with fields: %s"), *FString::Join(Keys, TEXT(", ")));

    // 1. Apply World Data
    if (JsonObject->HasField(TEXT("world")))
    {
        const TSharedPtr<FJsonObject> WorldObj = JsonObject->GetObjectField(TEXT("world"));
        USceneExporter::ApplyWorldStateJSON(this, WorldActorTags, WorldObj);
        ReceiveWorldStateUpdated();
    }

    // 2. Apply Agent Data
    if (JsonObject->HasField(TEXT("agents")))
    {
        const TSharedPtr<FJsonObject> AgentsObj = JsonObject->GetObjectField(TEXT("agents"));
        
        // TRACER DYE 2: How many agents did Python send?
        UE_LOG(LogTemp, Verbose, TEXT("[WorldManager] Processing 'agents' update containing %d agents..."), AgentsObj->Values.Num());

        for (auto& Pair : AgentsObj->Values)
        {
            FString AgentID = Pair.Key;
            TSharedPtr<FJsonObject> AgentData = Pair.Value->AsObject();

            if (AgentData.IsValid())
            {
                if (ARoboSimAgent** FoundAgent = AgentCache.Find(AgentID))
                {
                    if (*FoundAgent)
                    {
                        (*FoundAgent)->ApplyState(AgentData);
                    }
                }
                else
                {
                    // TRACER DYE 3: Cache Miss!
                    UE_LOG(LogTemp, Verbose, TEXT("[WorldManager] CACHE MISS! Python sent data for Agent: '%s', but it is not in the map."), *AgentID);
                    
                    // Print exactly what IS in the cache so we can compare the spelling/casing
                    TArray<FString> CacheKeys;
                    AgentCache.GetKeys(CacheKeys);
                    UE_LOG(LogTemp, Verbose, TEXT("[WorldManager] Available Cache Keys are: %s"), *FString::Join(CacheKeys, TEXT(", ")));
                }
            }
        }
    }
}
