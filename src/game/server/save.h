#ifndef GAME_SERVER_SAVE_H
#define GAME_SERVER_SAVE_H

// Moved to save.cpp so we can include this file in player.h
//#include "./entities/character.h"
#include <engine/shared/protocol.h>
#include <game/server/gamecontroller.h>
#include "teeinfo.h"

class CCharacter;
class CGameContext;

// F-DDrace
struct SSavedIdentity
{
	char m_aAccUsername[32];
	NETADDR m_Addr;
	char m_aTimeoutCode[64];

	char m_aName[MAX_NAME_LENGTH];
	CTeeInfo m_TeeInfo;

	time_t m_ExpireDate;
};

enum
{
	SAVE_WALLET = 1<<0, // saves and loads wallet money
	SAVE_FLAG = 1<<1, // saves and gives back flag if no one else has it
	SAVE_IDENTITY = 1<<2, // saves the identity of a player aswell as logs him back in when this save gets loaded
	SAVE_JAIL = 1<<3, // only saves and loads escape and jail time of a player
};

class CSaveTee
{
public:
	CSaveTee(int Flags = 0);
	~CSaveTee();
	void Save(CCharacter* pchr);
	void Load(CCharacter* pchr, int Team);
	char* GetString();
	int LoadString(char* String);
	vec2 GetPos() { return m_Pos; }
	char* GetName() { return m_aName; }

	// F-DDrace
	int GetMinigame() { return m_Minigame; }
	SSavedIdentity GetIdentity() { return m_Identity; }
	bool HasSavedIdentity() { return m_Flags&SAVE_IDENTITY; }
	void TeleOutOfPlot(vec2 ToTele);
	void StopPlotEditing();

	bool SaveFile(const char *pFileName, CCharacter *pChr);
	bool LoadFile(const char *pFileName, CCharacter *pChr, CGameContext *pGameContext = 0);

private:

	char m_aString[2048];
	char m_aName[16];

	int m_Alive;
	int m_Paused;

	// Teamstuff
	int m_TeeFinished;
	int m_IsSolo;

	struct WeaponStat
	{
		int m_AmmoRegenStart;
		int m_Ammo;
		int m_Got;

	} m_aWeapons[NUM_WEAPONS];

	int m_LastWeapon;
	int m_QueuedWeapon;

	int m_SuperJump;
	int m_Jetpack;
	int m_NinjaJetpack;
	int m_FreezeTime;
	int m_FreezeTick;
	int m_DeepFreeze;
	int m_EndlessHook;
	int m_DDRaceState;

	int m_Hit;
	int m_Collision;
	int m_TuneZone;
	int m_TuneZoneOld;
	int m_Hook;
	int m_Time;
	vec2 m_Pos;
	vec2 m_PrevPos;
	int m_TeleCheckpoint;
	int m_LastPenalty;

	int m_CpTime;
	int m_CpActive;
	int m_CpLastBroadcast;
	float m_CpCurrent[25];

	int m_NotEligibleForFinish;

	// Core
	vec2 m_CorePos;
	vec2 m_Vel;
	int m_ActiveWeapon;
	int m_Jumped;
	int m_JumpedTotal;
	int m_Jumps;
	vec2 m_HookPos;
	vec2 m_HookDir;
	vec2 m_HookTeleBase;
	int m_HookTick;
	int m_HookState;

	char aGameUuid[16];

	// F-DDrace
	int m_Flags;

	// character
	int m_Health;
	int m_Armor;

	int m_Invisible;
	int m_Rainbow;
	int m_Atom;
	int m_Trail;
	int m_Meteors;
	int m_Bloody;
	int m_StrongBloody;
	int m_ScrollNinja;
	int m_HookPower;
	int m_aSpreadWeapon[NUM_WEAPONS];
	int m_aHadWeapon[NUM_WEAPONS];
	int m_FakeTuneCollision;
	int m_OldFakeTuneCollision;
	int m_Passive;
	int m_PoliceHelper;
	int m_Item;
	int m_DoorHammer;
	int m_AlwaysTeleWeapon;
	int m_FreezeHammer;
	int m_aSpawnWeaponActive[3];
	int m_HasFinishedSpecialRace;
	int m_GotMoneyXPBomb;
	int64 m_SpawnTick;
	int m_KillStreak;
	int m_MaxJumps;
	int m_CarriedFlag;
	int m_CollectedPortalRifle;

	// core
	int m_MoveRestrictionExtraCanEnterRoom;

	// player
	int m_Gamemode;
	int m_SavedGamemode;
	int m_Minigame;
	int64 m_WalletMoney;
	int m_RainbowSpeed;
	int m_InfRainbow;
	int m_InfMeteors;
	int m_HasSpookyGhost;
	int m_PlotSpawn;
	int m_HasRoomKey;
	int64 m_JailTime;
	int64 m_EscapeTime;

	SSavedIdentity m_Identity;
};

class CSaveTeam
{
public:
	CSaveTeam(IGameController* Controller);
	~CSaveTeam();
	char* GetString();
	int GetMembersCount() { return m_MembersCount; }
	int LoadString(const char* String);
	int save(int Team);
	int load(int Team);
	CSaveTee* m_apSavedTees;

private:
	int MatchPlayer(char name[16]);
	CCharacter* MatchCharacter(char name[16], int SaveID);

	IGameController* m_pController;

	char m_String[65536];

	struct SSimpleSwitchers
	{
		int m_Status;
		int m_EndTime;
		int m_Type;
	};
	SSimpleSwitchers* m_Switchers;

	int m_TeamState;
	int m_MembersCount;
	int m_NumSwitchers;
	int m_TeamLocked;
	int m_Practice;
};

#endif // GAME_SERVER_SAVE_H
