/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_CHARACTER_H
#define GAME_SERVER_ENTITIES_CHARACTER_H

#include <generated/protocol.h>

#include <game/gamecore.h>
#include <game/server/entity.h>
#include <game/server/player.h>
#include <game/server/draweditor.h>

#include "pickup.h"
#include "lightsaber.h"
#include "stable_projectile.h"

#include "dummy/dummybase.h"

enum Extra
{
	HOOK_NORMAL,
	JETPACK,
	RAINBOW,
	INF_RAINBOW,
	ATOM,
	TRAIL,
	SPOOKY_GHOST,
	METEOR,
	INF_METEOR,
	PASSIVE,
	VANILLA_MODE,
	DDRACE_MODE,
	BLOODY,
	STRONG_BLOODY,
	SCROLL_NINJA,
	HOOK_POWER,
	ENDLESS_HOOK,
	INFINITE_JUMPS,
	SPREAD_WEAPON,
	FREEZE_HAMMER,
	INVISIBLE,
	ITEM,
	TELE_WEAPON,
	ALWAYS_TELE_WEAPON,
	DOOR_HAMMER,
	AIM_CLOSEST,
	SPIN_BOT,
	TEE_CONTROL,
	NUM_EXTRAS
};

enum Backup
{
	BACKUP_SPOOKY_GHOST,
	NUM_BACKUPS
};

enum WeaponSpecial
{
	SPECIAL_JETPACK = 1<<0,
	SPECIAL_SPREADWEAPON = 1<<1,
	SPECIAL_TELEWEAPON = 1<<2,
	SPECIAL_DOORHAMMER = 1<<3,
	SPECIAL_SCROLLNINJA = 1<<4,
};

class CGameTeams;

class CCharacter : public CEntity
{
	MACRO_ALLOC_POOL_ID()

public:
	//character's size
	static const int ms_PhysSize = 28;

	CCharacter(CGameWorld *pWorld);
	virtual ~CCharacter();

	virtual void Reset();
	virtual void Destroy();
	virtual void Tick();
	virtual void TickDefered();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);
	virtual void PostSnap();

	virtual int NetworkClipped(int SnappingClient, bool CheckShowAll = true);
	virtual int NetworkClipped(int SnappingClient, vec2 CheckPos, bool CheckShowAll = true);

	bool IsGrounded();

	void SetWeapon(int W);
	void SetSolo(bool Solo);
	bool IsSolo() { return m_Solo; }
	void HandleWeaponSwitch();
	void DoWeaponSwitch();

	void HandleWeapons();
	void HandleNinja();
	void HandleJetpack();

	void OnPredictedInput(CNetObj_PlayerInput *pNewInput);
	void OnDirectInput(CNetObj_PlayerInput *pNewInput);
	void ResetInput();
	void FireWeapon();

	void Die(int Weapon = WEAPON_SELF, bool UpdateTeeControl = true);
	bool TakeDamage(vec2 Force, vec2 Source, int Dmg, int From, int Weapon);

	bool Spawn(class CPlayer *pPlayer, vec2 Pos);
	bool Remove();

	bool IncreaseHealth(int Amount);
	bool IncreaseArmor(int Amount);

	void GiveWeapon(int Weapon, bool Remove = false, int Ammo = -1, bool PortalRifleByAcc = false);
	void GiveNinja();
	void RemoveNinja();

	void SetEmote(int Emote, int Tick);

	void Rescue();

	bool IsAlive() const { return m_Alive; }
	bool IsPaused() const { return m_Paused; }
	class CPlayer *GetPlayer() { return m_pPlayer; }

	//drops
	void DropMoney(int64 Amount, int Dir = -3);
	void DropFlag();
	void DropWeapon(int WeaponID, bool OnDeath, float Dir = -3);
	void DropPickup(int Type, int Amount = 1);
	void DropLoot(int Weapon);

	void SetAvailableWeapon(int PreferedWeapon = WEAPON_GUN);
	int GetAimDir() { return m_Input.m_TargetX < 0 ? -1 : 1; };

	void Jetpack(bool Set = true, int FromID = -1, bool Silent = false);
	void Rainbow(bool Set = true, int FromID = -1, bool Silent = false);
	void InfRainbow(bool Set = true, int FromID = -1, bool Silent = false);
	void Atom(bool Set = true, int FromID = -1, bool Silent = false);
	void Trail(bool Set = true, int FromID = -1, bool Silent = false);
	void SpookyGhost(bool Set = true, int FromID = -1, bool Silent = false);
	void Meteor(bool Set = true, int FromID = -1, bool Infinite = false, bool Silent = false);
	void Passive(bool Set = true, int FromID = -1, bool Silent = false);
	void VanillaMode(int FromID = -1, bool Silent = false);
	void DDraceMode(int FromID = -1, bool Silent = false);
	void Bloody(bool Set = true, int FromID = -1, bool Silent = false);
	void StrongBloody(bool Set = true, int FromID = -1, bool Silent = false);
	void ScrollNinja(bool Set = true, int FromID = -1, bool Silent = false);
	void HookPower(int Extra, int FromID = -1, bool Silent = false);
	void EndlessHook(bool Set = true, int FromID = -1, bool Silent = false);
	void InfiniteJumps(bool Set = true, int FromID = -1, bool Silent = false);
	void SpreadWeapon(int Type, bool Set = true, int FromID = -1, bool Silent = false);
	void FreezeHammer(bool Set = true, int FromID = -1, bool Silent = false);
	void Invisible(bool Set = true, int FromID = -1, bool Silent = false);
	void Item(int Item, int FromID = -1, bool Silent = false);
	void TeleWeapon(int Type, bool Set = true, int FromID = -1, bool Silent = false);
	void AlwaysTeleWeapon(bool Set = true, int FromID = -1, bool Silent = false);
	void DoorHammer(bool Set = true, int FromID = -1, bool Silent = false);
	void AimClosest(bool Set = true, int FromID = -1, bool Silent = false);
	void SpinBot(bool Set = true, int FromID = -1, bool Silent = false);
	void TeeControl(bool Set = true, int ForcedID = -1, int FromID = -1, bool Silent = false);

private:
	// player controlling this character
	class CPlayer *m_pPlayer;

	bool m_Alive;
	bool m_Paused;

	// weapon info
	CEntity *m_apHitObjects[10];
	int m_NumObjectsHit;

	struct WeaponStat
	{
		int m_AmmoRegenStart;
		int m_Ammo;
		bool m_Got;

	} m_aWeapons[NUM_WEAPONS];

	struct WeaponStat m_aPrevSaveWeapons[NUM_WEAPONS];

	int m_ActiveWeapon;
	int m_LastWeapon;
	int m_QueuedWeapon;

	int m_ReloadTimer;
	int m_AttackTick;

	int m_EmoteType;
	int m_EmoteStop;

	// F-DDrace
	int m_LastWantedWeapon;

	// last tick that the player took any action ie some input
	int m_LastAction;
	int m_LastNoAmmoSound;

	// these are non-heldback inputs
	CNetObj_PlayerInput m_LatestPrevInput;
	CNetObj_PlayerInput m_LatestInput;

	// input
	CNetObj_PlayerInput m_SavedInput;
	CNetObj_PlayerInput m_Input;
	int m_NumInputs;
	int m_Jumped;

	int m_Health;
	int m_Armor;

	int m_TriggeredEvents;

	// ninja
	struct
	{
		vec2 m_ActivationDir;
		int m_ActivationTick;
		int m_CurrentMoveTime;
		int m_OldVelAmount;
	} m_Ninja;

	// the player core for the physics
	CCharacterCore m_Core;

	// info for dead reckoning
	int m_ReckoningTick; // tick that we are performing dead reckoning From
	CCharacterCore m_SendCore; // core that we should send
	CCharacterCore m_ReckoningCore; // the dead reckoning core

	// F-DDrace

	static bool IsSwitchActiveCb(int Number, void *pUser);
	void HandleTiles(int Index);
	float m_Time;
	int m_LastBroadcast;
	void DDraceInit();
	void HandleSkippableTiles(int Index);
	void DDraceTick();
	void DDracePostCoreTick();
	void HandleBroadcast();
	void HandleTuneLayer();
	void SendZoneMsgs();

	bool m_SetSavePos;
	vec2 m_PrevSavePos;
	bool m_Solo;

	void FDDraceTick();
	void DummyTick();
	void FDDraceInit();
	void HandleLastIndexTiles();

public:
	CGameTeams* Teams();
	void Pause(bool Pause);
	bool Freeze(float Time);
	bool Freeze();
	bool UnFreeze();
	void GiveAllWeapons();
	int m_DDRaceState;
	int Team();
	bool CanCollide(int ClientID, bool CheckPassive = true);
	bool SameTeam(int ClientID);
	bool m_Super;
	bool m_SuperJump;
	bool m_Jetpack;
	bool m_NinjaJetpack;
	int m_TeamBeforeSuper;
	int64 m_FirstFreezeTick;
	int m_FreezeTime;
	int m_FreezeTick;
	bool m_FrozenLastTick;
	bool m_DeepFreeze;
	bool m_EndlessHook;
	bool m_FreezeHammer;
	enum
	{
		HIT_ALL = 0,
		DISABLE_HIT_HAMMER = 1,
		DISABLE_HIT_SHOTGUN = 2,
		DISABLE_HIT_GRENADE = 4,
		DISABLE_HIT_RIFLE = 8
	};
	int m_Hit;
	int m_TuneZone;
	int m_TuneZoneOld;
	int m_PainSoundTimer;
	int m_LastMove;
	int m_StartTime;
	vec2 m_PrevPos;
	int m_TeleCheckpoint;
	int m_CpTick;
	int m_CpActive;
	int m_CpLastBroadcast;
	float m_CpCurrent[25];
	int m_TileIndex;
	int m_TileFIndex;

	int m_MoveRestrictions;

	vec2 m_Intersection;
	int64 m_LastStartWarning;
	int64 m_LastRescue;
	bool m_LastRefillJumps;
	bool m_LastPenalty;
	bool m_LastBonus;
	bool m_HasTeleGun;
	bool m_HasTeleGrenade;
	bool m_HasTeleLaser;
	vec2 m_TeleGunPos;
	bool m_TeleGunTeleport;
	bool m_IsBlueTeleGunTeleport;
	int m_StrongWeakID;

	// Setters/Getters because i don't want to modify vanilla vars access modifiers
	int GetLastWeapon() { return m_LastWeapon; };
	void SetLastWeapon(int LastWeap) { m_LastWeapon = LastWeap; };
	int GetActiveWeapon() { return m_ActiveWeapon; };
	void SetActiveWeapon(int Weapon);
	void SetLastAction(int LastAction) { m_LastAction = LastAction; };
	int GetArmor() { return m_Armor; };
	void SetArmor(int Armor) { m_Armor = Armor; };
	int GetHealth() { return m_Health; }
	void SetHealth(int Health) { m_Health = Health; }
	CCharacterCore GetCore() { return m_Core; };
	void SetCore(CCharacterCore Core) { m_Core = Core; };
	CCharacterCore* Core() { return &m_Core; };
	int GetQueuedWeapon() { return m_QueuedWeapon; };
	void SetQueuedWeapon(int Weapon) { m_QueuedWeapon = Weapon; };
	bool GetWeaponGot(int Type) { return m_aWeapons[Type].m_Got; };
	void SetWeaponGot(int Type, bool Value);
	int GetWeaponAmmoRegenStart(int Type) { return m_aWeapons[Type].m_AmmoRegenStart; };
	void SetWeaponAmmoRegenStart(int Type, int Value) { m_aWeapons[Type].m_AmmoRegenStart = Value; };
	int GetWeaponAmmo(int Type);
	void SetWeaponAmmo(int Type, int Value);
	void SetEmoteType(int EmoteType) { m_EmoteType = EmoteType; };
	void SetEmoteStop(int EmoteStop) { m_EmoteStop = EmoteStop; };
	void SetNinjaActivationDir(vec2 ActivationDir) { m_Ninja.m_ActivationDir = ActivationDir; };
	void SetNinjaActivationTick(int ActivationTick) { m_Ninja.m_ActivationTick = ActivationTick; };
	void SetNinjaCurrentMoveTime(int CurrentMoveTime) { m_Ninja.m_CurrentMoveTime = CurrentMoveTime; };
	void SetAlive(bool Alive) { m_Alive = Alive; }

	void SetPos(vec2 Pos) { m_Pos = Pos; };
	void SetPrevPos(vec2 PrevPos) { m_PrevPos = PrevPos; };
	void ForceSetPos(vec2 Pos);

	void SetCoreHook(int Hook) { m_Core.m_Hook = Hook; }
	void SetCoreCollision(int Collision) { m_Core.m_Collision = Collision; }
	void SetCoreJumped(int Jumped) { m_Core.m_Jumped = Jumped; }
	void SetCoreJumpedTotal(int JumpedTotal) { m_Core.m_JumpedTotal = JumpedTotal; }
	void SetCoreJumps(int Jumps) { m_Core.m_Jumps = Jumps; }
	void SetCoreHookTick(int HookTick) { m_Core.m_HookTick = HookTick; }
	void SetCoreHookedPlayer(int HookedPlayer) { m_Core.m_HookedPlayer = HookedPlayer; }
	void SetCoreHookState(int HookState) { m_Core.m_HookState = HookState; }
	void SetCoreHookPos(vec2 HookPos) { m_Core.m_HookPos = HookPos; }
	void SetCoreHookDir(vec2 HookDir) { m_Core.m_HookDir = HookDir; }
	void SetCoreHookTeleBase(vec2 HookTeleBase) { m_Core.m_HookTeleBase = HookTeleBase; }

	void SetCorePos(vec2 Pos) { m_Core.m_Pos = Pos; };
	void SetCoreVel(vec2 Vel) { m_Core.m_Vel = Vel; };

	void SetAttackTick(int AttackTick) { m_AttackTick = AttackTick; }

	//last tile
	int m_LastIndexTile;
	int m_LastIndexFrontTile;

	//backups
	void BackupWeapons(int Type);
	void LoadWeaponBackup(int Type);
	int m_aWeaponsBackup[NUM_WEAPONS][NUM_BACKUPS];
	bool m_WeaponsBackupped[NUM_BACKUPS];
	bool m_aWeaponsBackupGot[NUM_WEAPONS][NUM_BACKUPS];

	//spooky ghost
	void SetSpookyGhost();
	void UnsetSpookyGhost();
	int m_NumGhostShots;

	//extras
	bool m_Rainbow;
	bool m_Atom;
	bool m_Trail;
	int m_Meteors;
	bool m_Bloody;
	bool m_StrongBloody;
	bool m_Invisible;
	bool m_ScrollNinja;
	bool m_aSpreadWeapon[NUM_WEAPONS];
	CEntity* m_pTelekinesisEntity;
	CLightsaber* m_pLightsaber;
	bool m_AlwaysTeleWeapon;
	bool m_DoorHammer;
	bool m_FakeTuneCollision;
	bool m_OldFakeTuneCollision;
	bool m_Passive;
	CPickup* m_pPassiveShield;
	int m_Item;
	CPickup* m_pItem;
	bool m_PoliceHelper;
	bool m_MoneyTile;
	int64 m_RoomAntiSpamTick;

	int64 m_LastLinkedPortals;
	bool m_CollectedPortalRifle;
	bool SendingPortalCooldown();

	int m_HookPower;
	bool m_IsRainbowHooked;

	int m_SavedGamemode;
	int m_MaxJumps;

	// weapon money reward
	bool m_aHadWeapon[NUM_WEAPONS];
	void WeaponMoneyReward(int Weapon);

	// editor
	CDrawEditor m_DrawEditor;

	int GetCurrentTilePlotID();
	void TeleOutOfPlot(int PlotID);

	// returns bitwise specials on weapons
	int GetWeaponSpecial(int Type);

	//weapon indicator
	void UpdateWeaponIndicator();

	//others
	int HasFlag();
	void CheckMoved();

	int GetAliveState();

	int64 m_SpawnTick;
	bool m_GotLasered;

	int m_KillStreak;

	int64 m_LastWantedLogout;

	// ResetNumInputs() gets called when player is paused or when (un)setting teecontrol. its to prevent weird shooting and weapon switching after unpause/(un)setting teecontrol
	void ResetNumInputs() { m_NumInputs = 0; };

	// spawnweapons
	bool m_InitializedSpawnWeapons;
	bool m_aSpawnWeaponActive[3];
	int GetSpawnWeaponIndex(int Weapon);

	// cursor
	CStableProjectile* m_pTeeControlCursor;
	void SetTeeControlCursor();
	void RemoveTeeControlCursor();

	vec2 m_CursorPos;

	// special race
	bool m_HasFinishedSpecialRace;

	// money xp bomb
	bool m_GotMoneyXPBomb;

	// minigame join/leave request
	bool MinigameRequestTick();
	bool RequestMinigameChange(int RequestedMinigame);
	int m_RequestedMinigame;
	int64 m_LastMinigameRequest;

	int64 m_LastMoneyDrop;

	// true if the character is constantly getting freezed by Freeze() function, e.g. on a freeze tile or while deepfrozen
	bool m_IsFrozen;

	void OnPlayerHook();
	void ReleaseHook(bool Other = true);

	// last
	void SetLastTouchedSwitcher(int Number);
	int m_LastTouchedSwitcher;
	int m_LastTouchedPortalBy;

	CNetObj_PlayerInput *Input() { return &m_Input; };
	CNetObj_PlayerInput *LatestInput() { return &m_LatestInput; };
	int GetReloadTimer() { return m_ReloadTimer; }

	// Handles dummymode stuff
	void CreateDummyHandle(int Dummymode);
	CDummyBase *m_pDummyHandle;


	/////////dummymode variables

	void Fire(bool Stroke = true);

	//dummymode 29 vars (ChillBlock5 blocker)
	int m_DummyFreezeBlockTrick;
	int m_DummyPanicDelay;
	bool m_DummyStartHook;
	bool m_DummySpeedRight;
	bool m_DummyTrick3Check;
	bool m_DummyTrick3Panic;
	bool m_DummyTrick3StartCount;
	bool m_DummyTrick3PanicLeft;
	bool m_DummyTrick4HasStartPos;
	bool m_DummyLockBored;
	bool m_DummyDoBalance;
	bool m_DummyAttackedOnSpawn;
	bool m_DummyMovementToBlockArea;
	bool m_DummyPlannedMovement;
	bool m_DummyJumped;
	bool m_DummyHooked;
	bool m_DummyMovedLeft;
	bool m_DummyHookDelay;
	bool m_DummyRuled;
	bool m_DummyEmergency;
	bool m_DummyLeftFreezeFull;
	bool m_DummyGetSpeed;
	bool m_DummyBored;
	bool m_DummySpecialDefend;
	bool m_DummySpecialDefendAttack;
	int m_DummyBoredCounter;
	int m_DummyBlockMode;

	//dummymode 23 vars
	int m_DummyHelpBeforeHammerfly;
	bool m_DummyHelpEmergency;
	bool m_DummyHelpNoEmergency;
	bool m_DummyHookAfterHammer;
	bool m_DummyHelpBeforeFly;
	bool m_DummyPanicWhileHelping;
	bool m_DummyPanicBalance;
	bool m_DummyMateFailed;
	bool m_DummyHook;
	bool m_DummyCollectedWeapons;
	bool m_DummyMateCollectedWeapons;
	bool m_DummyRocketJumped;
	bool m_DummyRaceHelpHook;
	bool m_DummyRaceHook;
	int m_DummyRaceState;
	int m_DummyRaceMode;
	int m_DummyNothingHappensCounter;
	int m_DummyPanicWeapon;
	int m_DummyMateHelpMode;
	int m_DummyMovementMode;
	bool m_DummyFreezed;
	int m_DummyEmoteTickNext;

	/////////dummymode variables
};

enum
{
	DDRACE_NONE = 0,
	DDRACE_STARTED,
	DDRACE_CHEAT, // no time and won't start again unless ordered by a mod or death
	DDRACE_FINISHED
};

#endif
