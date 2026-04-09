#include "ZenohSubsystem.h"
#include "ZenohBackend.h"
#include "ZenohMessageRouter.h"
#include "HAL/RunnableThread.h"

// ==========================================
// SUBSYSTEM LIFECYCLE 
// ==========================================
void UZenohSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    UE_LOG(LogTemp, Log, TEXT("[ZenohSubsystem] Initialized. Ready for connections."));
}

void UZenohSubsystem::Deinitialize()
{
    // Cleanly shut down all background network threads when the game closes
    DisconnectAll();
    
    TopicListeners.Empty();
    Super::Deinitialize();
    UE_LOG(LogTemp, Log, TEXT("[ZenohSubsystem] Deinitialized. All connections closed."));
}

// ==========================================
// LIFECYCLE CONTROL
// ==========================================
bool UZenohSubsystem::Connect(const FZenohConnectionInfo& ConnectionInfo)
{
    // Clean up old connections safely
    Disconnect(ConnectionInfo.ConnectionName);

    // Prepare the connection strings
    FString ProtocolStr = (ConnectionInfo.Protocol == EZenohProtocol::TCP) ? TEXT("tcp") : TEXT("udp");
    FString Endpoint = FString::Printf(TEXT("%s/%s:%d"), *ProtocolStr, *ConnectionInfo.IPAddress, ConnectionInfo.Port);
    FString ModeStr = (ConnectionInfo.ConnectionMode == EZenohMode::Client) ? TEXT("client") : TEXT("peer");    

    UE_LOG(LogTemp, Log, TEXT("[ZenohSubsystem] [%s] Connecting as %s to %s..."), *ConnectionInfo.ConnectionName.ToString(), *ModeStr, *Endpoint);

    FZenohBackend* NewBackend = new FZenohBackend();
    NewBackend->ConnectionName = ConnectionInfo.ConnectionName;

    if (NewBackend->Initialize(ModeStr, Endpoint, this))
    {
        // Apply the current performance setting to the newly created worker
        if (NewBackend->MessageRouter)
        {
            NewBackend->MessageRouter->SetAsyncParsing(bEnableAsyncParsing);
        }
        
        FScopeLock Lock(&ConnectionMapLock);
        ActiveConnections.Add(ConnectionInfo.ConnectionName, NewBackend);

        // The connection is now online. Flush any deferred subscriptions.
        if (TArray<FString>* PendingTopics = PendingSubscriptions.Find(ConnectionInfo.ConnectionName))
        {
            for (const FString& PendingTopic : *PendingTopics)
            {
                ActiveConnections[ConnectionInfo.ConnectionName]->Subscribe(PendingTopic);
            }
            PendingSubscriptions.Remove(ConnectionInfo.ConnectionName);
        }

        return true;
    }
    else
    {
        delete NewBackend;
        return false;
    }
}

void UZenohSubsystem::Disconnect(FName ConnectionName)
{
    FScopeLock Lock(&ConnectionMapLock);

    if (FZenohBackend** BackendPtr = ActiveConnections.Find(ConnectionName))
    {
        if (*BackendPtr)
        {
            (*BackendPtr)->Shutdown();
            delete *BackendPtr;
        }
        ActiveConnections.Remove(ConnectionName);
        UE_LOG(LogTemp, Log, TEXT("[ZenohSubsystem] Disconnected '%s'."), *ConnectionName.ToString());
    }
}

void UZenohSubsystem::DisconnectAll()
{
    FScopeLock Lock(&ConnectionMapLock);

    for (auto& Pair : ActiveConnections)
    {
        if (Pair.Value)
        {
            Pair.Value->Shutdown();
            delete Pair.Value;
        }
    }
    ActiveConnections.Empty();
}

bool UZenohSubsystem::IsConnected(FName ConnectionName) const
{
    FScopeLock Lock(&ConnectionMapLock);
    return ActiveConnections.Contains(ConnectionName);
}

// ==========================================
// MESSAGING
// ==========================================

UZenohTopicListener* UZenohSubsystem::SubscribeToTopic(FName ConnectionName, FString Topic)
{
    FString RoutingKey = ConnectionName.ToString() + TEXT("::") + Topic;

    UZenohTopicListener* Listener = TopicListeners.FindRef(RoutingKey);
    if (!Listener)
    {
        Listener = NewObject<UZenohTopicListener>(this);
        TopicListeners.Add(RoutingKey, Listener);
    }

    FScopeLock Lock(&ConnectionMapLock);
    if (FZenohBackend** BackendPtr = ActiveConnections.Find(ConnectionName))
    {
        if (*BackendPtr)
        {
            (*BackendPtr)->Subscribe(Topic);
        }
    }
    else
    {
        PendingSubscriptions.FindOrAdd(ConnectionName).AddUnique(Topic);
        UE_LOG(LogTemp, Log, TEXT("[Zenoh] Connection '%s' not ready. Deferring subscription to '%s'."), *ConnectionName.ToString(), *Topic);
    }

    return Listener;
}

bool UZenohSubsystem::Publish(FName ConnectionName, FString Topic, FString Message)
{
    FScopeLock Lock(&ConnectionMapLock);
    if (FZenohBackend** BackendPtr = ActiveConnections.Find(ConnectionName))
    {
        if (*BackendPtr)
        {
            return (*BackendPtr)->Publish(Topic, Message);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[ZenohSubsystem] Ignored Publish: Connection '%s' not found!"), *ConnectionName.ToString());
    return false;
}

void UZenohSubsystem::SetUseAsyncParsing(bool bEnableAsync)
{
    // Save it so future connections inherit this setting
    bEnableAsyncParsing = bEnableAsync;

    // Safely update all currently active connections
    FScopeLock Lock(&ConnectionMapLock);
    for (auto& Pair : ActiveConnections)
    {
        if (FZenohBackend* Backend = Pair.Value)
        {
            if (Backend->MessageRouter)
            {
                Backend->MessageRouter->SetAsyncParsing(bEnableAsync);
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[Zenoh] ThreadPool Parsing is now %s"), bEnableAsyncParsing ? TEXT("ENABLED (Heavy Load Mode)") : TEXT("DISABLED (Low Latency Mode)"));
}

void UZenohSubsystem::ProcessParsedPayload(const FName& ConnectionName, const FString& Topic, int64 MsgId, TSharedPtr<FJsonObject> ParsedJson)
{
    // 1. RECONSTRUCT THE FULL ROUTING KEY
    // This MUST match the format in SubscribeToTopic: "SimBrain::sim/state/snapshot"
    FString RoutingKey = ConnectionName.ToString() + TEXT("::") + Topic;

    // 2. AGGRESSIVE LOGGING (Force these to show up)
    UE_LOG(LogTemp, Error, TEXT("[ZENOH] Network In: %s (MsgId: %lld)"), *RoutingKey, MsgId);

    if (!ParsedJson.IsValid()) return;

    // 3. FIND AND BROADCAST
    if (UZenohTopicListener** ListenerPtr = TopicListeners.Find(RoutingKey))
    {
        if (*ListenerPtr)
        {
            UE_LOG(LogTemp, Warning, TEXT("[ZENOH] Dispatching to Manager/Agent..."));
            (*ListenerPtr)->OnTopicParsed.Broadcast(ParsedJson);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[ZENOH] No one is listening for Key: %s"), *RoutingKey);
        
        // DEBUG: Let's see what keys ARE available
        TArray<FString> ActiveKeys;
        TopicListeners.GetKeys(ActiveKeys);
        UE_LOG(LogTemp, Log, TEXT("[ZENOH] Active Subscriptions: %s"), *FString::Join(ActiveKeys, TEXT(", ")));
    }
}