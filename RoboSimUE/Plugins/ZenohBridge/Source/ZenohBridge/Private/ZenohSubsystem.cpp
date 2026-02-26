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
    // 1. Loop through every active connection in our dictionary
    for (auto& Pair : ActiveConnections)
    {
        FName ConnName = Pair.Key;
        FZenohBackend* Backend = Pair.Value;

        if (Backend)
        {
            FZenohMessage IncomingMessage;
            
            // 2. Drain the lock-free queue for this specific connection
            while (Backend->MessageQueue.Dequeue(IncomingMessage))
            {
                // 3. Hand the data over to the Blueprint system, tagging it with the Connection Name!
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
bool UZenohSubsystem::Connect(FName ConnectionName, EZenohMode ConnectionMode, EZenohProtocol Protocol, FString IPAddress, int32 Port)
{
    // If the user tries to connect with a name that already exists, clean up the old one first
    if (ActiveConnections.Contains(ConnectionName))
    {
        UE_LOG(LogTemp, Warning, TEXT("[ZenohSubsystem] Connection '%s' already exists! Disconnecting old session..."), *ConnectionName.ToString());
        Disconnect(ConnectionName);
    }

    FZenohBackend* NewBackend = new FZenohBackend();

    FString ProtocolStr = (Protocol == EZenohProtocol::TCP) ? TEXT("tcp") : TEXT("udp");
    FString Endpoint = FString::Printf(TEXT("%s/%s:%d"), *ProtocolStr, *IPAddress, Port);
    FString ModeStr = (ConnectionMode == EZenohMode::Client) ? TEXT("client") : TEXT("peer");

    UE_LOG(LogTemp, Log, TEXT("[ZenohSubsystem] [%s] Connecting as %s to %s..."), *ConnectionName.ToString(), *ModeStr, *Endpoint);

    if (NewBackend->Initialize(ModeStr, Endpoint))
    {
        // Save the successful connection in our dictionary
        ActiveConnections.Add(ConnectionName, NewBackend);
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
    if (FZenohBackend** BackendPtr = ActiveConnections.Find(ConnectionName))
    {
        if (*BackendPtr)
        {
            // Delete the backend object, which triggers the C++ destructor and closes the Zenoh session
            delete *BackendPtr; 
        }
        
        ActiveConnections.Remove(ConnectionName);
        UE_LOG(LogTemp, Log, TEXT("[ZenohSubsystem] Disconnected '%s'."), *ConnectionName.ToString());
    }
}

void UZenohSubsystem::DisconnectAll()
{
    for (auto& Pair : ActiveConnections)
    {
        if (Pair.Value)
        {
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