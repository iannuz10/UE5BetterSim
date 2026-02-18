#include "ZenohBackend.h"
#include "Containers/StringConv.h"
#include "Async/Async.h" // <--- FIX 1: Added this include!

// --- ISOLATED ZENOH INCLUDES ---
#include "Windows/AllowWindowsPlatformTypes.h"
#pragma warning(push)
#pragma warning(disable: 4191)
#pragma warning(disable: 4005) // Macro redefinition

#include "zenoh.h"

#undef ALIGN
#undef ZENOHC_API

#pragma warning(pop)
#include "Windows/HideWindowsPlatformTypes.h"
// ------------------------------

// This struct lives ONLY inside this .cpp file
struct FZenohState
{
    struct z_owned_session_t Session;
    struct z_owned_subscriber_t Subscriber;
    bool bInitialized = false;
};

// --- CALLBACK WRAPPER ---
// FIX 2: Correct signature matching Zenoh 1.0.0+ (struct z_loaned_sample_t*)
void zenoh_pimpl_callback(struct z_loaned_sample_t* sample, void* context)
{
    // 1. Get Payload
    const struct z_loaned_bytes_t* payload = z_sample_payload(sample);
    size_t len = z_bytes_len(payload);
    
    TArray<uint8> Buffer;
    Buffer.SetNumUninitialized(len + 1);
    
    struct z_bytes_reader_t reader = z_bytes_get_reader(payload);
    z_bytes_reader_read(&reader, Buffer.GetData(), len);
    Buffer[len] = 0;

    FString Msg = UTF8_TO_TCHAR((const char*)Buffer.GetData());

    // 2. Execute the user's callback
    auto* CallbackPtr = static_cast<FZenohBackend::FOnMessageCallback*>(context);
    if (CallbackPtr)
    {
        // Execute on Game Thread
        AsyncTask(ENamedThreads::GameThread, [CallbackPtr, Msg]()
        {
            (*CallbackPtr)(Msg);
        });
    }
}

FZenohBackend::FZenohBackend()
{
    State = new FZenohState();
}

FZenohBackend::~FZenohBackend()
{
    Shutdown();
    delete State;
}

bool FZenohBackend::Initialize()
{
    // 1. Create Config
    struct z_owned_config_t config;
    z_config_default(&config);

    // --- FIX: FORCE CONNECTION TO DOCKER ---
    // We explicitly tell Zenoh to connect to localhost:7447 (where Docker is listening)
    // We borrow the config mutably to modify it
    if (zc_config_insert_json5(z_config_loan_mut(&config), "connect/endpoints", "['tcp/127.0.0.1:7447']") < 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Zenoh] Failed to set connect endpoint config!"));
    }
    // ---------------------------------------

    struct z_open_options_t options;
    z_open_options_default(&options);

    // 2. Open Session
    if (z_open(&State->Session, (struct z_moved_config_t*)&config, &options) < 0)
    {
        return false;
    }
    State->bInitialized = true;
    return true;
}

void FZenohBackend::Shutdown()
{
    if (State && State->bInitialized)
    {
        // Close Session
        z_close(z_session_loan_mut(&State->Session), NULL);
        z_session_drop((struct z_moved_session_t*)&State->Session);
        State->bInitialized = false;
    }
}

bool FZenohBackend::Publish(const FString& Topic, const FString& Message)
{
    if (!State || !State->bInitialized) return false;

    struct z_put_options_t options;
    z_put_options_default(&options);
    
    FTCHARToUTF8 TopicUTF(*Topic);
    struct z_view_keyexpr_t key;
    z_view_keyexpr_from_str(&key, TopicUTF.Get());

    FTCHARToUTF8 MsgUTF(*Message);
    struct z_owned_bytes_t payload;
    z_bytes_copy_from_str(&payload, MsgUTF.Get());

    if (z_put(z_session_loan(&State->Session), z_view_keyexpr_loan(&key), (struct z_moved_bytes_t*)&payload, &options) < 0)
    {
        return false;
    }
    return true;
}

void FZenohBackend::Subscribe(const FString& Topic, FOnMessageCallback Callback)
{
    if (!State || !State->bInitialized) return;

    // Store callback on heap so Zenoh can hold onto it
    auto* HeapCallback = new FOnMessageCallback(Callback);

    struct z_subscriber_options_t sub_opts;
    z_subscriber_options_default(&sub_opts);

    struct z_view_keyexpr_t key;
    FTCHARToUTF8 TopicUTF(*Topic);
    z_view_keyexpr_from_str(&key, TopicUTF.Get());

    struct z_owned_closure_sample_t closure;
    // Now this matches because zenoh_pimpl_callback has the right type
    z_closure_sample(&closure, zenoh_pimpl_callback, NULL, HeapCallback);

    z_declare_subscriber(z_session_loan(&State->Session), &State->Subscriber, z_view_keyexpr_loan(&key), (struct z_moved_closure_sample_t*)&closure, &sub_opts);
}