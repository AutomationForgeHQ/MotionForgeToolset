#include "MotionForgeToolsetModule.h"

#include "MotionForgeToolset.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

DEFINE_LOG_CATEGORY(LogMotionForgeToolset);

#define LOCTEXT_NAMESPACE "FMotionForgeToolsetModule"

void FMotionForgeToolsetModule::StartupModule()
{
	if (!UToolsetRegistry::IsAvailable())
	{
		// Expected outside the editor, and whenever the experimental plugins are off. Not an error.
		UE_LOG(LogMotionForgeToolset, Log,
			TEXT("Toolset registry unavailable - MotionForge tools not registered."));
		return;
	}

	if (UToolsetRegistry::IsToolsetClassRegistered(UMotionForgeToolset::StaticClass()))
	{
		bRegistered = true;
		return;
	}

	UToolsetRegistry::RegisterToolsetClass(UMotionForgeToolset::StaticClass());
	bRegistered = UToolsetRegistry::IsToolsetClassRegistered(UMotionForgeToolset::StaticClass());

	UE_LOG(LogMotionForgeToolset, Log, TEXT("MotionForge toolset %s."),
		bRegistered ? TEXT("registered") : TEXT("failed to register"));
}

void FMotionForgeToolsetModule::ShutdownModule()
{
	if (bRegistered && UToolsetRegistry::IsAvailable())
	{
		UToolsetRegistry::UnregisterToolsetClass(UMotionForgeToolset::StaticClass());
		bRegistered = false;
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMotionForgeToolsetModule, MotionForgeToolset)
