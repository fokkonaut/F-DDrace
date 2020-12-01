#ifndef GAME_SERVER_SAVE_H
#define GAME_SERVER_SAVE_H

// Moved to save.cpp so we can include this file in player.
//#include "./entities/character.h"
#include <engine/shared/protocol.h>
class CCharacter;

#include <game/server/gamecontroller.h>

class CSaveTee
{
public:
	CSaveTee();
	~CSaveTee();
	void Save(CCharacter* pchr); // F-DDrace TODO: extra weapons and mod specific stuff is not saved or loaded with the string
	void Load(CCharacter* pchr, int Team);
	char* GetString();
	int LoadString(char* String);
	vec2 GetPos() { return m_Pos; }
	char* GetName() { return m_aName; }

	int GetMinigame() { return m_Minigame; }

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
	// character
	int m_Health;
	int m_Armor;

	bool m_Invisible;
	bool m_Rainbow;
	bool m_Atom;
	bool m_Trail;
	int m_Meteors;
	bool m_Bloody;
	bool m_StrongBloody;
	bool m_ScrollNinja;
	int m_HookPower;
	bool m_aSpreadWeapon[NUM_WEAPONS];
	bool m_aHadWeapon[NUM_WEAPONS];
	bool m_FakeTuneCollision;
	bool m_OldFakeTuneCollision;
	bool m_Passive;
	bool m_PoliceHelper;
	int m_Item;
	bool m_DoorHammer;
	bool m_AlwaysTeleWeapon;
	bool m_FreezeHammer;
	int m_SavedGamemode;
	bool m_aSpawnWeaponActive[3];
	bool m_HasFinishedSpecialRace;
	bool m_GotMoneyXPBomb;
	int64 m_SpawnTick;
	int m_KillStreak;
	int m_MaxJumps;

	// core
	bool m_SpinBot;
	int m_SpinBotSpeed;
	bool m_AimClosest;

	// player
	int m_Gamemode;
	int m_Minigame;
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
