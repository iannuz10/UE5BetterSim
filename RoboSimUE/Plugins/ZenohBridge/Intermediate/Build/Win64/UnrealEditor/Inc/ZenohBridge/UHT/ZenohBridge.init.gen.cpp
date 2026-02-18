// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeZenohBridge_init() {}
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");	ZENOHBRIDGE_API UFunction* Z_Construct_UDelegateFunction_ZenohBridge_OnZenohMessage__DelegateSignature();
	static FPackageRegistrationInfo Z_Registration_Info_UPackage__Script_ZenohBridge;
	FORCENOINLINE UPackage* Z_Construct_UPackage__Script_ZenohBridge()
	{
		if (!Z_Registration_Info_UPackage__Script_ZenohBridge.OuterSingleton)
		{
		static UObject* (*const SingletonFuncArray[])() = {
			(UObject* (*)())Z_Construct_UDelegateFunction_ZenohBridge_OnZenohMessage__DelegateSignature,
		};
		static const UECodeGen_Private::FPackageParams PackageParams = {
			"/Script/ZenohBridge",
			SingletonFuncArray,
			UE_ARRAY_COUNT(SingletonFuncArray),
			PKG_CompiledIn | 0x00000000,
			0xE2E5C2E8,
			0x9BB4868A,
			METADATA_PARAMS(0, nullptr)
		};
		UECodeGen_Private::ConstructUPackage(Z_Registration_Info_UPackage__Script_ZenohBridge.OuterSingleton, PackageParams);
	}
	return Z_Registration_Info_UPackage__Script_ZenohBridge.OuterSingleton;
}
static FRegisterCompiledInInfo Z_CompiledInDeferPackage_UPackage__Script_ZenohBridge(Z_Construct_UPackage__Script_ZenohBridge, TEXT("/Script/ZenohBridge"), Z_Registration_Info_UPackage__Script_ZenohBridge, CONSTRUCT_RELOAD_VERSION_INFO(FPackageReloadVersionInfo, 0xE2E5C2E8, 0x9BB4868A));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
