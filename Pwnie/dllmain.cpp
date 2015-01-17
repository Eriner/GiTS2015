#include "stdafx.h"

// This is some ghetto shit but leave it be
// They're all next to each other in memory so this is ez

struct GameWorldRef
{
	World* GameWorld;
	GameAPI* Game;
	MasterServerConnection* g_masterServer;
	ServerThread* g_serverThread;
	float g_masterServerHeartbeatTimer;
	bool g_init;
	bool g_transitioning;
	bool g_watcherProcess;
	bool g_adminCharacter;
	GameServerConnection* g_gameServer;
};

GameWorldRef* gwr = NULL;

// Hooks go here

WorldVtbl WorldBackup;

typedef void(__thiscall* WorldChat_t)(World* pthis, Player*, std::string &);

void __fastcall new_WorldChat(World* pthis, void* _EDX, Player* player, std::string &text) {
	if (!text.length())
		return; // wtf

	std::istringstream iss(text);

	std::vector<std::string> tokens
	{
		std::istream_iterator<std::string>{iss},
		std::istream_iterator<std::string>{}
	};

	if (tokens.size() == 0) {
		WorldBackup.Chat(pthis, player, text);
		return;
	}

	bool command = true;

	if (tokens[0].compare("/sws") == 0 && tokens.size() > 1) {
		player->m_walkingSpeed = atof(tokens[1].c_str());
	}
	else if (tokens[0].compare("/sjs") == 0 && tokens.size() > 1) {
		player->m_jumpSpeed = atoi(tokens[1].c_str());
	}
	else if (tokens[0].compare("admin") == 0) {
		player->m_admin = !player->m_admin;
	}
	else {
		command = false;
	}

	// Don't output
	if (command)
		return;

	WorldBackup.Chat(pthis, player, text);
}

//

DWORD WINAPI lpHackThread(LPVOID lpParam)
{
	HMODULE hGameLogic = NULL;

	while (hGameLogic == NULL) {
		hGameLogic = GetModuleHandle("GameLogic.dll");

		Sleep(100);
	}

	app.log("GameLogic.dll = 0x%X", hGameLogic);

	gwr = (GameWorldRef*)((DWORD)hGameLogic + 0x97D7C);

	app.log("gwr = 0x%X", gwr);

	if (gwr == NULL) {
		app.log("Unable to get GameWorldRef!");
		ExitProcess(0);
	}

	app.log("Waiting for game world...");

	while (gwr->GameWorld == NULL) {
		Sleep(100);
	}

	app.log("GameWorld: 0x%X", gwr->GameWorld);
	app.log("Game: 0x%X", gwr->Game);

	while (gwr->GameWorld == NULL || gwr->GameWorld->vfptr == NULL || gwr->GameWorld->vfptr->Chat == NULL)
		Sleep(100);

	ZeroMemory(&WorldBackup, sizeof(WorldVtbl));

	memcpy(&WorldBackup, gwr->GameWorld->vfptr, sizeof(WorldVtbl));

	MEMORY_BASIC_INFORMATION mbi;
	VirtualQuery((LPCVOID)gwr->GameWorld->vfptr, &mbi, sizeof(mbi));
	VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &mbi.Protect);
	
	gwr->GameWorld->vfptr->Chat = (WorldChat_t) new_WorldChat;

	VirtualProtect(mbi.BaseAddress, mbi.RegionSize, mbi.Protect, &mbi.Protect);

	app.log("Hooked World...");

	return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule, DWORD dwReason, LPVOID lpReserved )
{
	if (dwReason == DLL_PROCESS_ATTACH) {
		app.module(hModule);
		app.log("Loaded!");

		CreateThread(0, 0, lpHackThread, 0, 0, 0);
	}

	return TRUE;
}

extern "C"
{
	__declspec(dllexport) void stub()
	{
		
	}
}
