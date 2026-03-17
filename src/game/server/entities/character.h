/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_CHARACTER_H
#define GAME_SERVER_ENTITIES_CHARACTER_H

#include <engine/antibot.h>
#include <generated/protocol.h>

#include <game/gamecore.h>
#include <game/server/entity.h>
#include <game/server/player.h>
#include <game/server/draweditor.h>
#include <game/server/snake.h>

#include "pickup.h"
#include "lightsaber.h"
#include "stable_projectile.h"
#include "game/server/entities/helicopter/helicopter.h"
#include "portalblocker.h"
#include "grog.h"

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
	TEE_CONTROL,
	SNAKE,
	LOVELY,
	ROTATING_BALL,
	EPIC_CIRCLE,
	STAFF_IND,
	RAINBOW_NAME,
	CONFETTI,
	SPARKLE,
	NUM_EXTRAS
};

enum Backup
{
	BACKUP_SPOOKY_GHOST,
	BACKUP_INGAME,
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

class CAntibot;
class CGameTeams;
struct CAntibotCharacterData;

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
	virtual void TickDeferred();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);
	void SnapCharacter(int SnappingClient, int ID);
	void SnapDDNetCharacter(int SnappingClient, int ID);
	virtual void PostSnap();

	bool CanSnapCharacter(int SnappingClient);
	bool IsSnappingCharacterInView(int SnappingClientId);

	bool IsGrounded(bool CheckDoor = false);

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
	bool IsIdle();
	void UpdateMovementTick(CNetObj_PlayerInput *pNewInput);
	void FireWeapon();

	void Die(int Weapon = WEAPON_SELF, bool UpdateTeeControl = true, bool OnArenaDie = true);
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
	void DropMoney(int64 Amount, int Dir = -3, bool GlobalPickupDelay = false, bool OnDeath = false);
	void DropFlag(int Dir = -3);
	bool CanDropWeapon(int Type);
	void DropWeapon(int WeaponID, bool OnDeath, float Dir = -3);
	void DropPickup(int Type, int Amount = 1);
	void DropLoot(int Weapon);
	void DropBattery(int WeaponID, int Amount, bool OnDeath = false, float Dir = -3);
	bool DropGrog(float Dir = -3, bool OnDeath = false);

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
	void TeeControl(bool Set = true, int ForcedID = -1, int FromID = -1, bool Silent = false);
	void Snake(bool Set = true, int FromID = -1, bool Silent = false);
	void Lovely(bool Set = true, int FromID = -1, bool Silent = false);
	void RotatingBall(bool Set = true, int FromID = -1, bool Silent = false);
	void EpicCircle(bool Set = true, int FromID = -1, bool Silent = false);
	void StaffInd(bool Set = true, int FromID = -1, bool Silent = false);
	void RainbowName(bool Set = true, int FromID = -1, bool Silent = false);
	void Confetti(bool Set = true, int FromID = -1, bool Silent = false);
	void Sparkle(bool Set = true, int FromID = -1, bool Silent = false);
	void SetJumps(int NewJumps, bool Silent = false);

	// vip classic
	void OnRainbowVIP();
	void OnBloodyVIP();
	void OnAtomVIP();
	void OnTrailVIP();
	void OnSpreadGunVIP();
	// vip plus
	void OnRainbowHookVIP();
	void OnRotatingBallVIP();
	void OnEpicCircleVIP();
	void OnLovelyVIP();
	void OnRainbowNameVIP();
	void OnSparkleVIP();

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
	int m_QueuedWeapon; // -2 = any available, preferred gun

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
	CNetObj_PlayerInput m_LatestPrevPrevInput;
	CNetObj_PlayerInput m_LatestPrevInput;
	CNetObj_PlayerInput m_LatestInput;

	// input
	CNetObj_PlayerInput m_SavedInput;
	CNetObj_PlayerInput m_PrevInput;
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
	int CheckMaskableTile(int TileIndex, bool CurrentState);
	float m_Time;
	int m_LastBroadcast;
	void DDraceInit();
	void HandleSkippableTiles(int Index);
	void DDraceTick();
	void DDracePostCoreTick();
	void HandleBroadcast();
	void HandleTuneLayer();
	void SendTuneMsg(const char *pMessage);
	IAntibot *Antibot();

	bool m_SetSavePos;
	vec2 m_PrevSavePos;
	bool m_Solo;

	void FDDraceTick();
	void DummyTick();
	void FDDraceInit();
	void HandleLastIndexTiles();
	bool GrogTick();

public:
	CGameTeams* Teams();
	CTuningParams *Tuning();
	void ApplyLockedTunings(bool SendTuningParams = true);
	void FillAntibot(CAntibotCharacterData *pData);
	void Pause(bool Pause);
	bool Freeze(float Seconds);
	bool Freeze();
	bool UnFreeze();
	void GiveAllWeapons();
	int m_DDRaceState;
	int Team();
	Mask128 TeamMask(bool SevendownOnly = false);
	Mask128 TeamMaskExceptSelf(bool SevendownOnly = false);
	bool CanCollide(int ClientID, bool CheckPassive = true, bool CheckInGame = true);
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
	int m_HitSaved;
	int m_TuneZone;
	int m_TuneZoneOld;
	LOCKED_TUNES m_LockedTunings;
	LOCKED_TUNES m_LastLockedTunings;
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
	int GetActiveWeapon() { return max(0, m_ActiveWeapon); };
	int GetActiveWeaponUnclamped() { return m_ActiveWeapon; };
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
	int GetNinjaCurrentMoveTime() { return m_Ninja.m_CurrentMoveTime; };
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
	void SetCoreHookedPlayer(int HookedPlayer) { m_Core.SetHookedPlayer(HookedPlayer); }
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
	// for ghost and portal blocker
	int m_TabDoubleClickCount;
	bool m_IsPortalBlocker;
	int m_PortalBlockerIndSnapID;

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
	int m_PassiveSnapID;
	int m_Item;
	CPickup* m_pItem;
	bool m_PoliceHelper;
	enum
	{
		MONEYTILE_NONE = 0,
		MONEYTILE_NORMAL,
		MONEYTILE_POLICE,
		MONEYTILE_EXTRA,
	};
	int m_MoneyTile;
	bool m_LastPoliceFarmActive;
	bool m_ProcessedMoneyTile;
	bool m_ProcessedDurakSeatTile;
	int64 m_RoomAntiSpamTick;
	CSnake m_Snake;
	bool m_InSnake;
	bool m_Lovely;
	bool m_RotatingBall;
	bool m_EpicCircle;
	bool m_StaffInd;
	bool m_Confetti;

	void ResetOnlyFirstPortal();
	int64 m_LastLinkedPortals;
	bool m_CollectedPortalRifle;
	int64 m_LastBatteryDrop;
	CPortalBlocker *m_pPortalBlocker;

	int64 m_LastPoliceFarmBroadcast;

	int64 m_VipPlusAntiSpamTick;

	int SendDroppedFlagCooldown(int SnappingClient);

	int m_HookPower;
	bool m_IsRainbowHooked;
	int GetPowerHooked();
	int m_PowerHookedID;

	int m_MaxJumps;

	// weapon money reward
	bool m_aHadWeapon[NUM_WEAPONS];
	void WeaponMoneyReward(int Weapon);

	// editor
	CDrawEditor m_DrawEditor;

	std::vector<int> m_vLastButtonNumbers;
	int m_LastInOutTeleporter;

	// no bonus area
	void IncreaseNoBonusScore(int Summand = 1);
	bool OnNoBonusArea(bool Enter, bool Silent = false);
	struct SNoBonusContext
	{
		bool m_InArea = false;
		int m_Score = 0;
		int64 m_LastAlertTick = 0;

		struct SNoBonusSave
		{
			bool NonEmpty()
			{
				return m_EndlessHook || m_InfiniteJumps || m_Jumps > m_NoBonusMaxJumps;
			}
			bool m_EndlessHook = false;
			bool m_InfiniteJumps = false;
			int m_Jumps = 0;
			// Config()->m_SvNoBonusMaxJumps
			int m_NoBonusMaxJumps = 0;
		} m_SavedBonus;
	};
	SNoBonusContext m_NoBonusContext;
	int64 m_LastNoBonusTick;
	int m_LastJumpedTotal;
	int64 m_HookExceededTick;

	// helicopter
	CHelicopter *m_pHelicopter;
	int m_HelicopterSeat;
	int m_SeatSwitchedTick;
	bool TryMountHelicopter();
	bool CanSwitchSeats();

	int GetCurrentTilePlotID(bool CheckDoor = false);
	void TeleOutOfPlot(int PlotID);

	// returns bitwise specials on weapons
	int GetWeaponSpecial(int Type);

	float GetFireDelay(int Weapon);

	//weapon indicator
	void UpdateWeaponIndicator();
	bool IsWeaponIndicator();
	int64 m_LastWeaponIndTick;

	// 128p
	enum EUntranslatedMap
	{
		ID_HOOK,
		ID_WEAPON,
		NUM_IDS
	};
	int m_aUntranslatedID[EUntranslatedMap::NUM_IDS];

	// redirect tile
	bool TrySafelyRedirectClient(int Port, bool Force = false);
	bool TrySafelyRedirectClientImpl(int Port);
	bool LoadRedirectTile(int Port);
	int m_RedirectTilePort;
	int64 m_PassiveEndTick;
	bool UpdatePassiveEndTick(int64 NewEndTick);

	std::vector< std::pair<int, int> > m_vCheckpoints;
	void AddCheckpointList(int Port, int Checkpoint);
	void SetCheckpointList(std::vector< std::pair<int, int> > vCheckpoints);

	// Grog
	bool AddGrog();
	int64 m_LastGrogHoldMsg;
	int m_NumGrogsHolding;
	CGrog *m_pGrog;
	//int m_Permille; // Moved to player from character, because jail should keep permille xd
	int64 m_FirstPermilleTick;
	int m_GrogSpirit;
	int DetermineGrogSpirit();
	void IncreasePermille(int Permille);
	int GetPermilleLimit();
	int64 m_DeadlyPermilleDieTick;
	int64 GetNextGrogActionTick();
	int64 m_NextGrogEmote;
	int64 m_NextGrogDirDelay;
	int64 m_GrogDirDelayEnd;
	int64 m_NextGrogBalance;
	int m_GrogDirection;
	float m_GrogBalancePosX;
	enum
	{
		GROG_BALANCE_POS_UNSET = -200
	};

	//others
	int HasFlag();
	void CheckMoved();

	int GetAliveState();

	int64 m_SpawnTick;
	int64 m_WeaponChangeTick;
	int m_KillStreak;
	int64 m_LastWantedLogout;

	bool m_GotLasered;
	bool m_IsGrounded;

	int64 m_LastTaserUse;
	int GetTaserStrength();

	bool TryCatchingWanted(int TargetCID, vec2 EffectPos);
	int GetCorruptionScore();

	int64 m_BirthdayGiftEndTick;
	int64 m_LastBirthdayMsg;
	bool m_IsZombie;
	bool SetZombieHuman(bool Zombie, int HitHumanID = -1);
	bool TryHumanTransformation(CCharacter *pTarget);
	void SetBirthdayJetpack(bool Set);
	bool SetSafeArea(bool Enter, bool Silent = false);
	int64 m_LastSetInGame;
	struct SInGameSave
	{
		bool NonEmpty()
		{
			return m_IsZombie || m_EndlessHook || m_InfiniteJumps || m_Jetpack || m_Jumps != 2;
		}
		bool m_IsZombie = false;
		bool m_EndlessHook = false;
		bool m_InfiniteJumps = false;
		bool m_Jetpack = false;
		int m_Jumps = 2;
		bool m_aSpawnWeaponActive[3] = { 0, 0, 0 };

	} m_SavedInGame;
	bool IsInSafeArea();
	void SetInGame(bool Set);

	bool TryInitializeSpawnWeapons(bool Spawn = false);

	// broadcast and ddrace hud
	bool ShowAmmoHud();
	int NumDDraceHudRows();
	int GetDDNetCharacterFlags(int SnappingClient = -1);
	int GetDDNetCharacterNinjaActivationTick();
	void SendBroadcastHud(const char *pMessage);

	// Experience broadcast
	char m_aLineExp[64];
	char m_aLineMoney[64];

	// ResetNumInputs() gets called when player is paused or when (un)setting teecontrol. its to prevent weird shooting and weapon switching after unpause/(un)setting teecontrol
	void ResetNumInputs() { m_NumInputs = 0; };

	// EntityEx fills up the snap quite fast, so we try to avoid sending it as much as possible
	bool SendExtendedEntity(CEntity *pEntity);

	// spawnweapons
	bool m_InitializedSpawnWeapons;
	bool m_aSpawnWeaponActive[3];
	int GetSpawnWeaponIndex(int Weapon);

	// cursor
	CStableProjectile* m_pTeeControlCursor;
	void SetTeeControlCursor();
	void RemoveTeeControlCursor();

	int m_ViewCursorSnapID;
	void HandleCursor();
	vec2 GetCursorPos();
	vec2 m_CursorPos;
	vec2 m_CursorPosZoomed;
	void CalculateCursorPosZoomed();
	bool m_DynamicCamera;
	float m_CameraMaxLength;

	// special race
	bool m_HasFinishedSpecialRace;

	// money xp bomb
	bool m_GotMoneyXPBomb;

	bool m_IsDoubleXp;

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

	// AntiPing
	// https://github.com/ddnet/ddnet/blob/bca9a344dd257c62dc337d7e199213b43c15dde2/src/game/client/prediction/entities/character.cpp#L1501
	// Dont show hammer as activeweapon
	int m_AntiPingHideHammerTicks;

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
