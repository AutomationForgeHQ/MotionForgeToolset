#pragma once

#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

/** Filter the Output Log on "LogMotionForgeToolset" to follow tool registration and tool calls. */
MOTIONFORGETOOLSET_API DECLARE_LOG_CATEGORY_EXTERN(LogMotionForgeToolset, Log, All);

/**
 * Registers MotionForge's toolset with the engine's ToolsetRegistry on startup.
 *
 * Registration is conditional: if the registry is unavailable - a cooked build, a commandlet, an
 * editor started with the experimental plugins off - the module loads and does nothing rather than
 * failing. An adapter should never be the reason a project will not start.
 */
class FMotionForgeToolsetModule : public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

	bool bRegistered = false;
};
