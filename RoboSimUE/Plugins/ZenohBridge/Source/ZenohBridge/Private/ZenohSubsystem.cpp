#include "ZenohSubsystem.h"

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
    Super::Deinitialize();
    UE_LOG(LogTemp, Log, TEXT("[ZenohSubsystem] Deinitialized. All connections closed."));
}

// ==========================================
// FTICKABLE GAMEOBJECT INTERFACE
// ==========================================
void UZenohSubsystem::Tick(float DeltaTime)
{
    // Lock the map. If the background thread is currently adding a connection, 
    // the GameThread will politely wait for a microsecond here.
    FScopeLock Lock(&ConnectionMapLock);

    for (auto& Pair : ActiveConnections)
    {
        FName ConnName = Pair.Key;
        FZenohBackend* Backend = Pair.Value;

        if (Backend)
        {
            FZenohMessage IncomingMessage;
            while (Backend->MessageQueue.Dequeue(IncomingMessage))
            {
                HandleZenohMessage(ConnName, IncomingMessage.Topic, IncomingMessage.Payload);
            }
        }
    }
}

TStatId UZenohSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UZenohSubsystem, STATGROUP_Tickables);
}

// ==========================================
// LIFECYCLE CONTROL
// ==========================================
bool UZenohSubsystem::Connect(const FZenohConnectionInfo& ConnectionInfo)
{
    // 1. Clean up old connections safely (Disconnect already has its own lock inside it!)
    Disconnect(ConnectionInfo.ConnectionName);

    // 2. Prepare the connection strings
    FString ProtocolStr = (ConnectionInfo.Protocol == EZenohProtocol::TCP) ? TEXT("tcp") : TEXT("udp");
    FString Endpoint = FString::Printf(TEXT("%s/%s:%d"), *ProtocolStr, *ConnectionInfo.IPAddress, ConnectionInfo.Port);
    FString ModeStr = (ConnectionInfo.ConnectionMode == EZenohMode::Client) ? TEXT("client") : TEXT("peer");

    UE_LOG(LogTemp, Log, TEXT("[ZenohSubsystem] [%s] Connecting as %s to %s..."), *ConnectionInfo.ConnectionName.ToString(), *ModeStr, *Endpoint);

    // 3. DO THE HEAVY LIFTING WITHOUT A LOCK!
    // The background thread does the DNS and TCP handshake freely here.
    // Because no lock is held, the GameThread's Tick() sails right past without freezing!
    FZenohBackend* NewBackend = new FZenohBackend();
    
    if (NewBackend->Initialize(ModeStr, Endpoint))
    {
        // 4. SUCCESS! Now we lock the map for 1 microsecond just to add the connection.
        FScopeLock Lock(&ConnectionMapLock);
        ActiveConnections.Add(ConnectionInfo.ConnectionName, NewBackend);
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
    // Lock the map! Protect it while the background thread modifies it.
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
    // Lock the map!
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
    return ActiveConnections.Contains(ConnectionName);
}

// ==========================================
// MESSAGING
// ==========================================
void UZenohSubsystem::Subscribe(FName ConnectionName, FString Topic)
{
    if (FZenohBackend** BackendPtr = ActiveConnections.Find(ConnectionName))
    {
        if (*BackendPtr)
        {
            (*BackendPtr)->Subscribe(Topic);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[ZenohSubsystem] Ignored Subscribe: Connection '%s' not found!"), *ConnectionName.ToString());
    }
}

bool UZenohSubsystem::Publish(FName ConnectionName, FString Topic, FString Message)
{
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

void UZenohSubsystem::HandleZenohMessage(const FName& ConnectionName, const FString& Topic, const FString& Payload)
{
    if (OnMessageReceived.IsBound())
    {
        OnMessageReceived.Broadcast(ConnectionName, Topic, Payload);
    }
}