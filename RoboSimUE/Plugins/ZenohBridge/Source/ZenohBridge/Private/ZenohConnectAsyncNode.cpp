#include "ZenohConnectAsyncNode.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h" // Required for Multithreading

UZenohAsyncConnect* UZenohAsyncConnect::ConnectToZenohAsync(const UObject* WorldContextObject, const FZenohConnectionInfo& ConnectionInfo)
{
	UZenohAsyncConnect* Node = NewObject<UZenohAsyncConnect>();
	Node->WorldContext = WorldContextObject;
	Node->ConnectionData = ConnectionInfo;
	return Node;
}

void UZenohAsyncConnect::Activate()
{
	if (!WorldContext) { OnFailed.Broadcast(); return; }

	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContext);
	if (!GameInstance) { OnFailed.Broadcast(); return; }

	UZenohSubsystem* Subsystem = GameInstance->GetSubsystem<UZenohSubsystem>();
	if (!Subsystem) { OnFailed.Broadcast(); return; }

	// Create a safe copy of the struct to pass into the background thread
	FZenohConnectionInfo SafeData = ConnectionData;

	// JUMP TO A BACKGROUND CPU CORE
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, Subsystem, SafeData]()
	{
		// DNS resolution and TCP handshake happens off the GameThread
		bool bSuccess = Subsystem->Connect(SafeData);

		// JUMP BACK TO THE GAMETHREAD
		// Cannot fire Blueprint events from a background thread
		AsyncTask(ENamedThreads::GameThread, [this, bSuccess]()
		{
			if (bSuccess)
			{
				OnSuccess.Broadcast();
			}
			else
			{
				OnFailed.Broadcast();
			}
		});
	});
}