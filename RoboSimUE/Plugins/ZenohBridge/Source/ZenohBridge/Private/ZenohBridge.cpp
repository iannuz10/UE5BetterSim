#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"

class FZenohBridgeModule : public IModuleInterface
{
public:
    void* ZenohDllHandle = nullptr;

    virtual void StartupModule() override
    {
        FString LibraryName;
        FString Subfolder;
        
#if PLATFORM_WINDOWS
        LibraryName = TEXT("zenohc.dll");
        Subfolder = TEXT("Win64");
#elif PLATFORM_MAC
        LibraryName = TEXT("libzenohc.dylib");
        Subfolder = TEXT("Mac");
#else
        UE_LOG(LogTemp, Error, TEXT("[ZenohBridge] Unsupported Platform!"));
        return;
#endif

        FString DllPath;

        // Try to find the plugin
        TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("ZenohBridge");

        if (Plugin.IsValid())
        {
            // Plugin manager found it
            DllPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Source/ThirdParty/bin/zenohc.dll"));
        }
        else
        {
            // Fallback path: Manually assume where the folder is
            // This prevents the crash if PluginManager is not ready yet
            UE_LOG(LogTemp, Warning, TEXT("[ZenohBridge] Warning: FindPlugin failed. Using manual fallback path."));
            DllPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Plugins/ZenohBridge/Source/ThirdParty/bin/zenohc.dll"));
        }

        // Log for debugging
        UE_LOG(LogTemp, Log, TEXT("[ZenohBridge] Attempting to load DLL from: %s"), *DllPath);

        // Load the DLL
        if (FPaths::FileExists(DllPath))
        {
            ZenohDllHandle = FPlatformProcess::GetDllHandle(*DllPath);
        }

        // Final Fallback: Check the Project Binaries folder (Build.cs copies it here)
        if (ZenohDllHandle == nullptr)
        {
            FString BinPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries/Win64/zenohc.dll"));
            if (FPaths::FileExists(BinPath))
            {
                UE_LOG(LogTemp, Warning, TEXT("[ZenohBridge] Source DLL failed. Trying Project Binaries: %s"), *BinPath);
                ZenohDllHandle = FPlatformProcess::GetDllHandle(*BinPath);
            }
        }

        if (ZenohDllHandle)
        {
            UE_LOG(LogTemp, Log, TEXT("[ZenohBridge] DLL Loaded Successfully!"));
        }
        else
        {
            // Don't crash, just be mad in the logs
            UE_LOG(LogTemp, Error, TEXT("[ZenohBridge] CRITICAL ERROR: Could not load zenohc.dll. Check paths!"));
        }
    }

    virtual void ShutdownModule() override
    {
        if (ZenohDllHandle)
        {
            FPlatformProcess::FreeDllHandle(ZenohDllHandle);
            ZenohDllHandle = nullptr;
        }
    }
};

IMPLEMENT_MODULE(FZenohBridgeModule, ZenohBridge)