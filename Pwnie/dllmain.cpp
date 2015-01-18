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

//void __thiscall GameAPI::JoinGameServer(GameAPI *this, int id, bool transition)
//

Vector3* zeroVelocity = nullptr;
Vector3* aboveBearChest = nullptr;
Vector3* belowBearChest = nullptr;
Vector3* lockedPosition = nullptr;
bool lockPosition = false;
WorldVtbl WorldBackup;

typedef void(__thiscall* WorldChat_t)(World* pthis, Player*, std::string &);
typedef bool(__thiscall* WorldIsAuthority_t)(World* pthis);
typedef void(__thiscall *Tick_t)(World *pthis, float);

bool g_admin = false;

void updateLockedPosition(Vector3* src) {
	app.log("Changing locked position to = %f, %f, %f", src->x, src->y, src->z);
	lockedPosition->x = src->x;
	lockedPosition->y = src->y;
	lockedPosition->z = src->z;
}

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
	else if (tokens[0].compare("/admin") == 0) {
		g_admin = !g_admin;
		player->m_admin = g_admin;
	}
	else if (tokens[0].compare("/test") == 0) {
		for (auto i = gwr->GameWorld->m_actors.begin(); i != gwr->GameWorld->m_actors.end(); i++) {
			if (!(*i).m_object)
				continue;

			if ((IActor*)player == (*i).m_object)
				continue;

			(*i).m_object->vfptr->Damage((*i).m_object, (IActor*) player, player->m_equipped[0], INT_MAX, DamageType::PhysicalDamage);
		}
	}
	else if (tokens[0].compare("/down") == 0) {
		Vector3* playerPosition = new Vector3;
		IUE4Actor* localPlayer = (IUE4Actor*)(player->m_localPlayer);
		localPlayer->vfptr->GetPosition(localPlayer, playerPosition);
		playerPosition->z -= 200;	// move down 200 units
		updateLockedPosition(playerPosition);
		localPlayer->vfptr->SetPosition(localPlayer, playerPosition);
	}
	else if (tokens[0].compare("/up") == 0) {
		Vector3* playerPosition = new Vector3;
		IUE4Actor* localPlayer = (IUE4Actor*)(player->m_localPlayer);
		localPlayer->vfptr->GetPosition(localPlayer, playerPosition);
		playerPosition->z += 200;	// move up 200 units
		updateLockedPosition(playerPosition);
		localPlayer->vfptr->SetPosition(localPlayer, playerPosition);
	}
	else if (tokens[0].compare("/bears") == 0) {
		IUE4Actor* localPlayer = (IUE4Actor*)(player->m_localPlayer);
		// go above ground
		localPlayer->vfptr->SetPosition(localPlayer, aboveBearChest);
		updateLockedPosition(aboveBearChest);
		// lock position so that when we go underground in a moment, we don't fall all the way through the map
		lockPosition = true;
		localPlayer->vfptr->SetCharacterVelocity(localPlayer, zeroVelocity);
		// go below ground
		localPlayer->vfptr->SetPosition(localPlayer, belowBearChest);
		updateLockedPosition(belowBearChest);
	}
	else if (tokens[0].compare("/lock") == 0) {
		lockPosition = !lockPosition;
	}
	else {
		command = false;
	}

	// Don't output
	if (command)
		return;

	WorldBackup.Chat(pthis, player, text);
}

void __fastcall new_Tick(World* pthis, float tickvalue)
{
	if (lockPosition) {
		ILocalPlayer* localPlayer = pthis->m_localPlayer;
		localPlayer->vfptr->SetCharacterVelocity(localPlayer, zeroVelocity);
		localPlayer->vfptr->SetPosition(localPlayer, lockedPosition);
	}

	WorldBackup.Tick(pthis, tickvalue);
}

bool __fastcall new_WorldIsAuthority(World* pthis, void* _EDX)
{
	return g_admin;
}


//

DWORD WINAPI lpHackThread(LPVOID lpParam)
{
	// just above ground, near the bear chest
	aboveBearChest = new Vector3;
	aboveBearChest->x = -7893.926758;
	aboveBearChest->y = 64349.375000;
	aboveBearChest->z = 2655.879395;

	// just below ground, near the bear chest (basically just 200 units below aboveBearChest)
	belowBearChest = new Vector3;
	belowBearChest->x = -7893.926758;
	belowBearChest->y = 64349.375000;
	belowBearChest->z = 2455.911865;

	// if you just set your velocity to 0, you still move downward, probably because of gravity.
	// this still doesn't fix that completely. you actually move up slightly with this setting, but it's really slow.
	zeroVelocity = new Vector3;
	zeroVelocity->x = 0;
	zeroVelocity->y = 0;
	zeroVelocity->z = 9;

	lockedPosition = new Vector3;

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
	gwr->GameWorld->vfptr->IsAuthority = (WorldIsAuthority_t) new_WorldIsAuthority;
	gwr->GameWorld->vfptr->Tick = (Tick_t)new_Tick;

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
