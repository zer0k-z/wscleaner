#include <stdio.h>
#include "plugin.h"
#include "addons.h"
#include "filesystem/ifilesystem.h"
#include "engine2/iengineservicemgr.h"
#include "icommandline.h"
#include <string>

WSCleanerPlugin g_ThisPlugin;
PLUGIN_EXPOSE(WSCleanerPlugin, g_ThisPlugin);

// Cleanup must not run while a level is loading: the engine may still be reading the new map's vpk, and the
// workshop manager has already dropped its record of the map it just hosted. Wait until the map has run a while.
static const double kCleanupDelay = 10.0;
static double s_flCleanupTime = 0.0;
static std::string s_CurrentMap;

WSCleanerPlugin::WSCleanerPlugin() :
	m_GameFrame(&IServerGameDLL::GameFrame, this, nullptr, &WSCleanerPlugin::Hook_GameFrame)
{
}

bool WSCleanerPlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	// This should not run if the server is not dedicated.
	if (!CommandLine()->CheckParm("-dedicated"))
	{
		snprintf(error, maxlen, "This plugin can only be run on dedicated servers.");
		return false;
	}
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pEngineServer, IVEngineServer2, INTERFACEVERSION_VENGINESERVER);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pEngineServiceMgr, IEngineServiceMgr, ENGINESERVICEMGR_INTERFACE_VERSION);
	g_SMAPI->AddListener( this, this );
	ConVar_Register();

	m_GameFrame.Add(g_pSource2Server);
	return true;
}

bool WSCleanerPlugin::Unload(char *error, size_t maxlen)
{
	m_GameFrame.Remove(g_pSource2Server);
	return true;
}

void WSCleanerPlugin::AllPluginsLoaded()
{
	g_pEngineServer->ServerCommand("exec wscleaner/wscleaner");
}

void WSCleanerPlugin::OnLevelInit(char const *pMapName,
							char const *pMapEntities,
							char const *pOldLevel,
							char const *pLandmarkName,
							bool loadGame,
							bool background)
{
	s_CurrentMap = pMapName ? pMapName : "";
	s_flCleanupTime = Plat_FloatTime() + kCleanupDelay;
}

KHook::Return<void> WSCleanerPlugin::Hook_GameFrame(IServerGameDLL*, bool simulating, bool bFirstTick, bool bLastTick)
{
	if (s_flCleanupTime != 0.0 && Plat_FloatTime() >= s_flCleanupTime)
	{
		s_flCleanupTime = 0.0;
		CleanupWorkshopAddons(s_CurrentMap.c_str());
	}
	return { KHook::Action::Ignore };
}
