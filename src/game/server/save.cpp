#include <new>
#include <cstdio>

#include "save.h"
#include "./entities/character.h"
#include "teams.h"
#include "./gamemodes/DDRace.h"
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>

CSaveTee::CSaveTee(bool ShutdownSave)
{
	m_ShutdownSave = (int)ShutdownSave;
}

CSaveTee::~CSaveTee()
{
}

void CSaveTee::TeleOutOfPlot(vec2 ToTele)
{
	m_Pos = m_PrevPos = m_CorePos = ToTele;
	m_aWeapons[WEAPON_DRAW_EDITOR].m_Got = false;
}

bool CSaveTee::SaveFile(const char *pFileName, CCharacter *pChr)
{
	IOHANDLE File = pChr->GameServer()->Storage()->OpenFile(pFileName, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(File)
	{
		Save(pChr);
		io_write(File, GetString(), str_length(GetString()));
		io_write_newline(File);
		io_close(File);
		return true;
	}
	return false;
}

bool CSaveTee::LoadFile(const char *pFileName, CCharacter *pChr)
{
	IOHANDLE File = pChr->GameServer()->Storage()->OpenFile(pFileName, IOFLAG_READ, IStorage::TYPE_SAVE);
	if (File)
	{
		CLineReader lr;
		lr.Init(File);

		char *pString = lr.Get();
		if (!pString)
		{
			io_close(File);
			return false;
		}

		LoadString(pString);
		Load(pChr, 0);

		io_close(File);
		return true;
	}
	return false;
}

void CSaveTee::Save(CCharacter *pChr)
{
	str_copy(m_aName, pChr->Server()->ClientName(pChr->GetPlayer()->GetCID()), sizeof(m_aName));

	m_Alive = pChr->IsAlive();
	m_Paused = abs(pChr->GetPlayer()->IsPaused());

	m_TeeFinished = pChr->Teams()->TeeFinished(pChr->GetPlayer()->GetCID());
	m_IsSolo = pChr->IsSolo();

	for(int i = 0; i< NUM_WEAPONS; i++)
	{
		m_aWeapons[i].m_AmmoRegenStart = pChr->GetWeaponAmmoRegenStart(i);
		m_aWeapons[i].m_Ammo = pChr->GetWeaponAmmo(i);
		m_aWeapons[i].m_Got = pChr->GetWeaponGot(i);
	}

	m_LastWeapon = pChr->GetLastWeapon();
	m_QueuedWeapon = pChr->GetQueuedWeapon();

	m_SuperJump = pChr->m_SuperJump;
	m_Jetpack = pChr->m_Jetpack;
	m_NinjaJetpack = pChr->m_NinjaJetpack;
	m_FreezeTime = pChr->m_FreezeTime;
	m_FreezeTick = pChr->Server()->Tick() - pChr->m_FreezeTick;

	m_DeepFreeze = pChr->m_DeepFreeze;
	m_EndlessHook = pChr->m_EndlessHook;
	m_DDRaceState = pChr->m_DDRaceState;

	m_Hit = pChr->m_Hit;
	m_TuneZone = pChr->m_TuneZone;
	m_TuneZoneOld = pChr->m_TuneZoneOld;

	if(pChr->m_StartTime)
		m_Time = pChr->Server()->Tick() - pChr->m_StartTime;
	else
		m_Time = 0;

	m_Pos = pChr->GetPos();
	m_PrevPos = pChr->m_PrevPos;
	m_TeleCheckpoint = pChr->m_TeleCheckpoint;
	m_LastPenalty = pChr->m_LastPenalty;

	if(pChr->m_CpTick)
		m_CpTime = pChr->Server()->Tick() - pChr->m_CpTick;

	m_CpActive = pChr->m_CpActive;
	m_CpLastBroadcast = pChr->m_CpLastBroadcast;

	for(int i = 0; i < 25; i++)
		m_CpCurrent[i] = pChr->m_CpCurrent[i];

	m_NotEligibleForFinish = pChr->GetPlayer()->m_NotEligibleForFinish;

	m_ActiveWeapon = pChr->GetActiveWeapon();

	// Core
	m_CorePos = pChr->GetCore().m_Pos;
	m_Vel = pChr->GetCore().m_Vel;
	m_Hook = pChr->GetCore().m_Hook;
	m_Collision = pChr->GetCore().m_Collision;
	m_Jumped = pChr->GetCore().m_Jumped;
	m_JumpedTotal = pChr->GetCore().m_JumpedTotal;
	m_Jumps = pChr->GetCore().m_Jumps;
	m_HookPos = pChr->GetCore().m_HookPos;
	m_HookDir = pChr->GetCore().m_HookDir;
	m_HookTeleBase = pChr->GetCore().m_HookTeleBase;

	m_HookTick = pChr->GetCore().m_HookTick;

	m_HookState = pChr->GetCore().m_HookState;

	FormatUuid(pChr->GameServer()->GameUuid(), aGameUuid, sizeof(aGameUuid));

	// F-DDrace
	// character
	m_Health = pChr->GetHealth();
	m_Armor = pChr->GetArmor();
	m_Invisible = pChr->m_Invisible;
	m_Rainbow = pChr->m_Rainbow;
	m_Atom = pChr->m_Atom;
	m_Trail = pChr->m_Trail;
	m_Meteors = pChr->m_Meteors;
	m_Bloody = pChr->m_Bloody;
	m_StrongBloody = pChr->m_StrongBloody;
	m_ScrollNinja = pChr->m_ScrollNinja;
	m_HookPower = pChr->m_HookPower;
	for (int i = 0; i < NUM_WEAPONS; i++)
	{
		m_aSpreadWeapon[i] = pChr->m_aSpreadWeapon[i];
		m_aHadWeapon[i] = pChr->m_aHadWeapon[i];
	}
	m_FakeTuneCollision = pChr->m_FakeTuneCollision;
	m_OldFakeTuneCollision = pChr->m_OldFakeTuneCollision;
	m_Passive = pChr->m_Passive;
	m_PoliceHelper = pChr->m_PoliceHelper;
	m_Item = pChr->m_Item;
	m_DoorHammer = pChr->m_DoorHammer;
	m_AlwaysTeleWeapon = pChr->m_AlwaysTeleWeapon;
	m_FreezeHammer = pChr->m_FreezeHammer;
	m_SavedGamemode = pChr->m_SavedGamemode;
	for (int i = 0; i < 3; i++)
		m_aSpawnWeaponActive[i] = pChr->m_aSpawnWeaponActive[i];
	m_HasFinishedSpecialRace = pChr->m_HasFinishedSpecialRace;
	m_GotMoneyXPBomb = pChr->m_GotMoneyXPBomb;
	m_SpawnTick = pChr->Server()->Tick() - pChr->m_SpawnTick;
	m_KillStreak = pChr->m_KillStreak;
	m_MaxJumps = pChr->m_MaxJumps;
	m_CarriedFlag = pChr->HasFlag();

	// core
	m_SpinBot = pChr->Core()->m_SpinBot;
	m_SpinBotSpeed = pChr->Core()->m_SpinBotSpeed;
	m_AimClosest = pChr->Core()->m_AimClosest;
	m_MoveRestrictionExtraCanEnterRoom = pChr->Core()->m_MoveRestrictionExtra.m_CanEnterRoom;

	// player
	m_Gamemode = pChr->GetPlayer()->m_Gamemode;
	m_Minigame = pChr->GetPlayer()->m_Minigame;
	if (m_ShutdownSave)
		m_WalletMoney = pChr->GetPlayer()->GetWalletMoney();
	else
		m_WalletMoney = 0;
	m_RainbowSpeed = pChr->GetPlayer()->m_RainbowSpeed;
	m_InfRainbow = pChr->GetPlayer()->m_InfRainbow;
	m_InfMeteors = pChr->GetPlayer()->m_InfMeteors;
	m_HasSpookyGhost = pChr->GetPlayer()->m_HasSpookyGhost;
	m_PlotSpawn = pChr->GetPlayer()->m_PlotSpawn;
	m_HasRoomKey = pChr->GetPlayer()->m_HasRoomKey;
	m_JailTime = pChr->GetPlayer()->m_JailTime;
	m_EscapeTime = pChr->GetPlayer()->m_EscapeTime;

	// account
	// '$' is not a valid username character, thats why we use it here (str_check_special_chars)
	// we cant just set it to 0 or '\0' because that would fuck up the LoadString() as it is a null terminator
	if (pChr->GameServer()->m_ShutdownSave.m_aUsername[0] == '\0')
		str_copy(m_aAccUsername, "$", sizeof(m_aAccUsername));
	else
		str_copy(m_aAccUsername, pChr->GameServer()->m_ShutdownSave.m_aUsername, sizeof(m_aAccUsername));
}

void CSaveTee::Load(CCharacter *pChr, int Team)
{
	pChr->GetPlayer()->Pause(m_Paused, true);

	pChr->SetAlive(m_Alive);

	pChr->Teams()->SetForceCharacterTeam(pChr->GetPlayer()->GetCID(), Team);
	pChr->Teams()->SetFinished(pChr->GetPlayer()->GetCID(), m_TeeFinished);

	for(int i = 0; i< NUM_WEAPONS; i++)
	{
		pChr->SetWeaponAmmoRegenStart(i, m_aWeapons[i].m_AmmoRegenStart);
		pChr->SetWeaponAmmo(i, m_aWeapons[i].m_Ammo);
		pChr->SetWeaponGot(i, m_aWeapons[i].m_Got);
	}

	pChr->SetLastWeapon(m_LastWeapon);
	pChr->SetQueuedWeapon(m_QueuedWeapon);

	pChr->m_SuperJump = m_SuperJump;
	pChr->m_Jetpack = m_Jetpack;
	pChr->m_NinjaJetpack = m_NinjaJetpack;
	pChr->m_FreezeTime = m_FreezeTime;
	pChr->m_FreezeTick = pChr->Server()->Tick() - m_FreezeTick;

	pChr->m_DeepFreeze = m_DeepFreeze;
	pChr->m_EndlessHook = m_EndlessHook;
	pChr->m_DDRaceState = m_DDRaceState;

	pChr->m_Hit = m_Hit;
	pChr->m_TuneZone = m_TuneZone;
	pChr->m_TuneZoneOld = m_TuneZoneOld;

	if(m_Time)
		pChr->m_StartTime = pChr->Server()->Tick() - m_Time;

	pChr->SetPos(m_Pos);
	pChr->SetPrevPos(m_PrevPos);
	pChr->m_TeleCheckpoint = m_TeleCheckpoint;
	pChr->m_LastPenalty = m_LastPenalty;

	if(m_CpTime)
		pChr->m_CpTick = pChr->Server()->Tick() - m_CpTime;

	pChr->m_CpActive  = m_CpActive;
	pChr->m_CpLastBroadcast = m_CpLastBroadcast;

	for(int i = 0; i < 25; i++)
		pChr->m_CpCurrent[i] = m_CpCurrent[i];

	pChr->GetPlayer()->m_NotEligibleForFinish = pChr->GetPlayer()->m_NotEligibleForFinish || m_NotEligibleForFinish;

	pChr->SetActiveWeapon(m_ActiveWeapon);

	// Core
	pChr->SetCorePos(m_CorePos);
	pChr->SetCoreVel(m_Vel);
	pChr->SetCoreHook(m_Hook);
	pChr->SetCoreCollision(m_Collision);
	pChr->SetCoreJumped(m_Jumped);
	pChr->SetCoreJumpedTotal(m_JumpedTotal);
	pChr->SetCoreJumps(m_Jumps);
	pChr->SetCoreHookPos(m_HookPos);
	pChr->SetCoreHookDir(m_HookDir);
	pChr->SetCoreHookTeleBase(m_HookTeleBase);

	pChr->SetCoreHookTick(m_HookTick);

	if(m_HookState == HOOK_GRABBED)
	{
		pChr->SetCoreHookState(HOOK_FLYING);
		pChr->SetCoreHookedPlayer(-1);
	}
	else
	{
		pChr->SetCoreHookState(m_HookState);
	}

	pChr->SetSolo(m_IsSolo);

	// F-DDrace
	// character
	pChr->SetHealth(m_Health);
	pChr->SetArmor(m_Armor);
	pChr->m_Invisible = m_Invisible;
	pChr->m_Rainbow = m_Rainbow;
	pChr->Atom(m_Atom, -1, true);
	pChr->Trail(m_Trail, -1, true);
	pChr->Meteor(m_Meteors, -1, false, true);
	pChr->m_Bloody = m_Bloody;
	pChr->m_StrongBloody = m_StrongBloody;
	pChr->ScrollNinja(m_ScrollNinja, -1, true);
	pChr->m_HookPower = m_HookPower;
	for (int i = 0; i < NUM_WEAPONS; i++)
	{
		pChr->m_aSpreadWeapon[i] = m_aSpreadWeapon[i];
		pChr->m_aHadWeapon[i] = m_aHadWeapon[i];
	}
	pChr->m_FakeTuneCollision = m_FakeTuneCollision;
	pChr->m_OldFakeTuneCollision = m_OldFakeTuneCollision;
	pChr->Passive(m_Passive, -1, true);
	pChr->m_PoliceHelper = m_PoliceHelper;
	pChr->Item(m_Item, -1, true);
	pChr->m_DoorHammer = m_DoorHammer;
	pChr->m_AlwaysTeleWeapon = m_AlwaysTeleWeapon;
	pChr->m_FreezeHammer = m_FreezeHammer;
	pChr->m_SavedGamemode = m_SavedGamemode;
	for (int i = 0; i < 3; i++)
		pChr->m_aSpawnWeaponActive[i] = m_aSpawnWeaponActive[i];
	pChr->m_HasFinishedSpecialRace = m_HasFinishedSpecialRace;
	pChr->m_GotMoneyXPBomb = m_GotMoneyXPBomb;
	pChr->m_SpawnTick = pChr->Server()->Tick() - m_SpawnTick;
	pChr->m_KillStreak = m_KillStreak;
	pChr->m_MaxJumps = m_MaxJumps;
	if (m_CarriedFlag != -1 && m_ShutdownSave)
	{
		CGameControllerDDRace *pController = ((CGameControllerDDRace *)pChr->GameServer()->m_pController);
		CFlag *pFlag = pController->m_apFlags[m_CarriedFlag];
		if (pFlag && !pFlag->GetCarrier())
			pController->ForceFlagOwner(pChr->GetPlayer()->GetCID(), m_CarriedFlag);
	}

	// core
	pChr->Core()->m_SpinBot = m_SpinBot;
	pChr->Core()->m_SpinBotSpeed = m_SpinBotSpeed;
	pChr->Core()->m_AimClosest = m_AimClosest;
	pChr->Core()->m_MoveRestrictionExtra.m_CanEnterRoom = m_MoveRestrictionExtraCanEnterRoom;

	// player
	pChr->GetPlayer()->m_Gamemode = m_Gamemode;
	pChr->GetPlayer()->m_Minigame = m_Minigame;
	if (m_ShutdownSave)
		pChr->GetPlayer()->SetWalletMoney(m_WalletMoney);
	pChr->GetPlayer()->m_RainbowSpeed = m_RainbowSpeed;
	pChr->GetPlayer()->m_InfRainbow = m_InfRainbow;
	pChr->Meteor(m_InfMeteors, -1, true, true);
	pChr->GetPlayer()->m_HasSpookyGhost = m_HasSpookyGhost;
	pChr->GetPlayer()->m_PlotSpawn = m_PlotSpawn;
	pChr->GetPlayer()->m_HasRoomKey = m_HasRoomKey;
	if (m_JailTime)
		pChr->GameServer()->JailPlayer(pChr->GetPlayer()->GetCID(), m_JailTime/pChr->Server()->TickSpeed());
	pChr->GetPlayer()->m_EscapeTime = m_EscapeTime;

	// account
	if (m_aAccUsername[0] != '$') // explanation: see CSaveTee::Save() @ m_aAccUsername
		pChr->GameServer()->Login(pChr->GetPlayer()->GetCID(), m_aAccUsername, "", false);
}

char* CSaveTee::GetString()
{
	str_format(m_aString, sizeof(m_aString),
		"%s\t%d\t%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t\
		%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t\
		%d\t%d\t%d\t%d\t\
		%d\t%d\t\
		%d\t%d\t%f\t%f\t\
		%d\t%d\t%d\t%d\t\
		%d\t%d\t%f\t%f\t%d\t%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%f\t%f\t%f\t%f\t%f\t\
		%f\t%f\t%f\t%f\t%f\t\
		%f\t%f\t%f\t%f\t%f\t\
		%f\t%f\t%f\t%f\t%f\t\
		%f\t%f\t%f\t%f\t%f\t\
		%d\t%s"
		/* F-DDrace */
		"\t%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t\
		%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t\
		%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t\
		%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%lld\t%d\t%d\t%d\t\
		%d\t%d\t%d\t%d\t\
		%d\t%d\t%lld\t%d\t%d\t%d\t%d\t%d\t%d\t\
		%lld\t%lld\t%s\t",
		m_aName, m_Alive, m_Paused, m_TeeFinished, m_IsSolo,
		m_aWeapons[0].m_AmmoRegenStart, m_aWeapons[0].m_Ammo, m_aWeapons[0].m_Got,
		m_aWeapons[1].m_AmmoRegenStart, m_aWeapons[1].m_Ammo, m_aWeapons[1].m_Got,
		m_aWeapons[2].m_AmmoRegenStart, m_aWeapons[2].m_Ammo, m_aWeapons[2].m_Got,
		m_aWeapons[3].m_AmmoRegenStart, m_aWeapons[3].m_Ammo, m_aWeapons[3].m_Got,
		m_aWeapons[4].m_AmmoRegenStart, m_aWeapons[4].m_Ammo, m_aWeapons[4].m_Got,
		m_aWeapons[5].m_AmmoRegenStart, m_aWeapons[5].m_Ammo, m_aWeapons[5].m_Got,
		m_LastWeapon, m_QueuedWeapon,
		m_SuperJump, m_Jetpack, m_NinjaJetpack, m_FreezeTime, m_FreezeTick, m_DeepFreeze, m_EndlessHook, m_DDRaceState, m_Hit, m_Collision, m_TuneZone, m_TuneZoneOld, m_Hook, m_Time,
		(int)m_Pos.x, (int)m_Pos.y, (int)m_PrevPos.x, (int)m_PrevPos.y,
		m_TeleCheckpoint, m_LastPenalty,
		(int)m_CorePos.x, (int)m_CorePos.y, m_Vel.x, m_Vel.y,
		m_ActiveWeapon, m_Jumped, m_JumpedTotal, m_Jumps,
		(int)m_HookPos.x, (int)m_HookPos.y, m_HookDir.x, m_HookDir.y, (int)m_HookTeleBase.x, (int)m_HookTeleBase.y, m_HookTick, m_HookState,
		m_CpTime, m_CpActive, m_CpLastBroadcast,
		m_CpCurrent[0], m_CpCurrent[1], m_CpCurrent[2], m_CpCurrent[3], m_CpCurrent[4],
		m_CpCurrent[5], m_CpCurrent[6], m_CpCurrent[7], m_CpCurrent[8], m_CpCurrent[9],
		m_CpCurrent[10], m_CpCurrent[11], m_CpCurrent[12], m_CpCurrent[13], m_CpCurrent[14],
		m_CpCurrent[15], m_CpCurrent[16], m_CpCurrent[17], m_CpCurrent[18], m_CpCurrent[19],
		m_CpCurrent[20], m_CpCurrent[21], m_CpCurrent[22], m_CpCurrent[23], m_CpCurrent[24],
		m_NotEligibleForFinish, aGameUuid,
		/* F-DDrace */
		m_ShutdownSave, m_Health, m_Armor,
		m_aWeapons[6].m_AmmoRegenStart, m_aWeapons[6].m_Ammo, m_aWeapons[6].m_Got,
		m_aWeapons[7].m_AmmoRegenStart, m_aWeapons[7].m_Ammo, m_aWeapons[7].m_Got,
		m_aWeapons[8].m_AmmoRegenStart, m_aWeapons[8].m_Ammo, m_aWeapons[8].m_Got,
		m_aWeapons[9].m_AmmoRegenStart, m_aWeapons[9].m_Ammo, m_aWeapons[9].m_Got,
		m_aWeapons[10].m_AmmoRegenStart, m_aWeapons[10].m_Ammo, m_aWeapons[10].m_Got,
		m_aWeapons[11].m_AmmoRegenStart, m_aWeapons[11].m_Ammo, m_aWeapons[11].m_Got,
		m_aWeapons[12].m_AmmoRegenStart, m_aWeapons[12].m_Ammo, m_aWeapons[12].m_Got,
		m_aWeapons[13].m_AmmoRegenStart, m_aWeapons[13].m_Ammo, m_aWeapons[13].m_Got,
		m_aWeapons[14].m_AmmoRegenStart, m_aWeapons[14].m_Ammo, m_aWeapons[14].m_Got,
		m_aWeapons[15].m_AmmoRegenStart, m_aWeapons[15].m_Ammo, m_aWeapons[15].m_Got,
		m_Invisible, m_Rainbow, m_Atom, m_Trail, m_Meteors, m_Bloody, m_StrongBloody, m_ScrollNinja, m_HookPower,
		m_aSpreadWeapon[0], m_aSpreadWeapon[1], m_aSpreadWeapon[2], m_aSpreadWeapon[3], m_aSpreadWeapon[4], m_aSpreadWeapon[5], m_aSpreadWeapon[6], m_aSpreadWeapon[7],
		m_aSpreadWeapon[8], m_aSpreadWeapon[9], m_aSpreadWeapon[10], m_aSpreadWeapon[11], m_aSpreadWeapon[12], m_aSpreadWeapon[13], m_aSpreadWeapon[14], m_aSpreadWeapon[15],
		m_aHadWeapon[0], m_aHadWeapon[1], m_aHadWeapon[2], m_aHadWeapon[3], m_aHadWeapon[4], m_aHadWeapon[5], m_aHadWeapon[6], m_aHadWeapon[7],
		m_aHadWeapon[8], m_aHadWeapon[9], m_aHadWeapon[10], m_aHadWeapon[11], m_aHadWeapon[12], m_aHadWeapon[13], m_aHadWeapon[14], m_aHadWeapon[15],
		m_FakeTuneCollision, m_OldFakeTuneCollision, m_Passive, m_PoliceHelper, m_Item, m_DoorHammer, m_AlwaysTeleWeapon, m_FreezeHammer, m_SavedGamemode,
		m_aSpawnWeaponActive[0], m_aSpawnWeaponActive[1], m_aSpawnWeaponActive[2],
		m_HasFinishedSpecialRace, m_GotMoneyXPBomb, m_SpawnTick, m_KillStreak, m_MaxJumps, m_CarriedFlag,
		m_SpinBot, m_SpinBotSpeed, m_AimClosest, m_MoveRestrictionExtraCanEnterRoom,
		m_Gamemode, m_Minigame, m_WalletMoney, m_RainbowSpeed, m_InfRainbow, m_InfMeteors, m_HasSpookyGhost, m_PlotSpawn, m_HasRoomKey,
		m_JailTime, m_EscapeTime, m_aAccUsername
	);
	return m_aString;
}

int CSaveTee::LoadString(char* String)
{
	int Num;
	Num = sscanf(String,
		"%[^\t]\t%d\t%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t\
		%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t\
		%f\t%f\t%f\t%f\t\
		%d\t%d\t\
		%f\t%f\t%f\t%f\t\
		%d\t%d\t%d\t%d\t\
		%f\t%f\t%f\t%f\t%f\t%f\t%d\t%d\t\
		%d\t%d\t%d\t\
		%f\t%f\t%f\t%f\t%f\t\
		%f\t%f\t%f\t%f\t%f\t\
		%f\t%f\t%f\t%f\t%f\t\
		%f\t%f\t%f\t%f\t%f\t\
		%f\t%f\t%f\t%f\t%f\t\
		%d\t%36s"
		/* F-DDrace */
		"\t%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t\
		%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t\
		%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t\
		%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t\
		%d\t%d\t%d\t\
		%d\t%d\t%lld\t%d\t%d\t%d\t\
		%d\t%d\t%d\t%d\t\
		%d\t%d\t%lld\t%d\t%d\t%d\t%d\t%d\t%d\t\
		%lld\t%lld\t%s\t",
		m_aName, &m_Alive, &m_Paused, &m_TeeFinished, &m_IsSolo,
		&m_aWeapons[0].m_AmmoRegenStart, &m_aWeapons[0].m_Ammo, &m_aWeapons[0].m_Got,
		&m_aWeapons[1].m_AmmoRegenStart, &m_aWeapons[1].m_Ammo, &m_aWeapons[1].m_Got,
		&m_aWeapons[2].m_AmmoRegenStart, &m_aWeapons[2].m_Ammo, &m_aWeapons[2].m_Got,
		&m_aWeapons[3].m_AmmoRegenStart, &m_aWeapons[3].m_Ammo, &m_aWeapons[3].m_Got,
		&m_aWeapons[4].m_AmmoRegenStart, &m_aWeapons[4].m_Ammo, &m_aWeapons[4].m_Got,
		&m_aWeapons[5].m_AmmoRegenStart, &m_aWeapons[5].m_Ammo, &m_aWeapons[5].m_Got,
		&m_LastWeapon, &m_QueuedWeapon,
		&m_SuperJump, &m_Jetpack, &m_NinjaJetpack, &m_FreezeTime, &m_FreezeTick, &m_DeepFreeze, &m_EndlessHook, &m_DDRaceState, &m_Hit, &m_Collision, &m_TuneZone, &m_TuneZoneOld, &m_Hook, &m_Time,
		&m_Pos.x, &m_Pos.y, &m_PrevPos.x, &m_PrevPos.y,
		&m_TeleCheckpoint, &m_LastPenalty,
		&m_CorePos.x, &m_CorePos.y, &m_Vel.x, &m_Vel.y,
		&m_ActiveWeapon, &m_Jumped, &m_JumpedTotal, &m_Jumps,
		&m_HookPos.x, &m_HookPos.y, &m_HookDir.x, &m_HookDir.y, &m_HookTeleBase.x, &m_HookTeleBase.y, &m_HookTick, &m_HookState,
		&m_CpTime, &m_CpActive, &m_CpLastBroadcast,
		&m_CpCurrent[0], &m_CpCurrent[1], &m_CpCurrent[2], &m_CpCurrent[3], &m_CpCurrent[4],
		&m_CpCurrent[5], &m_CpCurrent[6], &m_CpCurrent[7], &m_CpCurrent[8], &m_CpCurrent[9],
		&m_CpCurrent[10], &m_CpCurrent[11], &m_CpCurrent[12], &m_CpCurrent[13], &m_CpCurrent[14],
		&m_CpCurrent[15], &m_CpCurrent[16], &m_CpCurrent[17], &m_CpCurrent[18], &m_CpCurrent[19],
		&m_CpCurrent[20], &m_CpCurrent[21], &m_CpCurrent[22], &m_CpCurrent[23], &m_CpCurrent[24],
		&m_NotEligibleForFinish, aGameUuid,
		/* F-DDrace */
		&m_ShutdownSave, &m_Health, &m_Armor,
		&m_aWeapons[6].m_AmmoRegenStart, &m_aWeapons[6].m_Ammo, &m_aWeapons[6].m_Got,
		&m_aWeapons[7].m_AmmoRegenStart, &m_aWeapons[7].m_Ammo, &m_aWeapons[7].m_Got,
		&m_aWeapons[8].m_AmmoRegenStart, &m_aWeapons[8].m_Ammo, &m_aWeapons[8].m_Got,
		&m_aWeapons[9].m_AmmoRegenStart, &m_aWeapons[9].m_Ammo, &m_aWeapons[9].m_Got,
		&m_aWeapons[10].m_AmmoRegenStart, &m_aWeapons[10].m_Ammo, &m_aWeapons[10].m_Got,
		&m_aWeapons[11].m_AmmoRegenStart, &m_aWeapons[11].m_Ammo, &m_aWeapons[11].m_Got,
		&m_aWeapons[12].m_AmmoRegenStart, &m_aWeapons[12].m_Ammo, &m_aWeapons[12].m_Got,
		&m_aWeapons[13].m_AmmoRegenStart, &m_aWeapons[13].m_Ammo, &m_aWeapons[13].m_Got,
		&m_aWeapons[14].m_AmmoRegenStart, &m_aWeapons[14].m_Ammo, &m_aWeapons[14].m_Got,
		&m_aWeapons[15].m_AmmoRegenStart, &m_aWeapons[15].m_Ammo, &m_aWeapons[15].m_Got,
		&m_Invisible, &m_Rainbow, &m_Atom, &m_Trail, &m_Meteors, &m_Bloody, &m_StrongBloody, &m_ScrollNinja, &m_HookPower,
		&m_aSpreadWeapon[0], &m_aSpreadWeapon[1], &m_aSpreadWeapon[2], &m_aSpreadWeapon[3], &m_aSpreadWeapon[4], &m_aSpreadWeapon[5], &m_aSpreadWeapon[6], &m_aSpreadWeapon[7],
		&m_aSpreadWeapon[8], &m_aSpreadWeapon[9], &m_aSpreadWeapon[10], &m_aSpreadWeapon[11], &m_aSpreadWeapon[12], &m_aSpreadWeapon[13], &m_aSpreadWeapon[14], &m_aSpreadWeapon[15],
		&m_aHadWeapon[0], &m_aHadWeapon[1], &m_aHadWeapon[2], &m_aHadWeapon[3], &m_aHadWeapon[4], &m_aHadWeapon[5], &m_aHadWeapon[6], &m_aHadWeapon[7],
		&m_aHadWeapon[8], &m_aHadWeapon[9], &m_aHadWeapon[10], &m_aHadWeapon[11], &m_aHadWeapon[12], &m_aHadWeapon[13], &m_aHadWeapon[14], &m_aHadWeapon[15],
		&m_FakeTuneCollision, &m_OldFakeTuneCollision, &m_Passive, &m_PoliceHelper, &m_Item, &m_DoorHammer, &m_AlwaysTeleWeapon, &m_FreezeHammer, &m_SavedGamemode,
		&m_aSpawnWeaponActive[0], &m_aSpawnWeaponActive[1], &m_aSpawnWeaponActive[2],
		&m_HasFinishedSpecialRace, &m_GotMoneyXPBomb, &m_SpawnTick, &m_KillStreak, &m_MaxJumps, &m_CarriedFlag,
		&m_SpinBot, &m_SpinBotSpeed, &m_AimClosest, &m_MoveRestrictionExtraCanEnterRoom,
		&m_Gamemode, &m_Minigame, &m_WalletMoney, &m_RainbowSpeed, &m_InfRainbow, &m_InfMeteors, &m_HasSpookyGhost, &m_PlotSpawn, &m_HasRoomKey,
		&m_JailTime, &m_EscapeTime, m_aAccUsername
	);

	switch(Num) // Don't forget to update this when you save / load more / less.
	{
	case 91:
		return 0;
	case 199: // F-DDrace extra vars
		return 0;
	default:
		dbg_msg("load", "failed to load tee-string");
		dbg_msg("load", "loaded %d vars", Num);
		return Num + 1; // never 0 here
	}
}

CSaveTeam::CSaveTeam(IGameController* Controller)
{
	m_pController = Controller;
	m_Switchers = 0;
	m_apSavedTees = 0;
}

CSaveTeam::~CSaveTeam()
{
	if(m_Switchers)
		delete[] m_Switchers;
	if(m_apSavedTees)
		delete[] m_apSavedTees;
}

int CSaveTeam::save(int Team)
{
	if(m_pController->Config()->m_SvTeam == 3 || (Team > 0 && Team < MAX_CLIENTS))
	{
		CGameTeams* Teams = &(((CGameControllerDDRace*)m_pController)->m_Teams);

		m_MembersCount = Teams->Count(Team);
		if(m_MembersCount <= 0)
		{
			return 2;
		}

		m_TeamState = Teams->GetTeamState(Team);

		if(m_TeamState != CGameTeams::TEAMSTATE_STARTED)
		{
			return 4;
		}

		m_NumSwitchers = m_pController->GameServer()->Collision()->m_NumSwitchers;
		m_TeamLocked = Teams->TeamLocked(Team);
		m_Practice = Teams->IsPractice(Team);

		m_apSavedTees = new CSaveTee[m_MembersCount];
		int j = 0;
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if(Teams->m_Core.Team(i) == Team)
			{
				if(m_pController->GameServer()->m_apPlayers[i] && m_pController->GameServer()->m_apPlayers[i]->GetCharacter())
					m_apSavedTees[j].Save(m_pController->GameServer()->m_apPlayers[i]->GetCharacter());
				else
					return 3;
				j++;
			}
		}

		if(m_pController->GameServer()->Collision()->m_NumSwitchers)
		{
			m_Switchers = new SSimpleSwitchers[m_pController->GameServer()->Collision()->m_NumSwitchers+1];

			for(int i=1; i < m_pController->GameServer()->Collision()->m_NumSwitchers+1; i++)
			{
				m_Switchers[i].m_Status = m_pController->GameServer()->Collision()->m_pSwitchers[i].m_Status[Team];
				if(m_pController->GameServer()->Collision()->m_pSwitchers[i].m_EndTick[Team])
					m_Switchers[i].m_EndTime = m_pController->Server()->Tick() - m_pController->GameServer()->Collision()->m_pSwitchers[i].m_EndTick[Team];
				else
					m_Switchers[i].m_EndTime = 0;
				m_Switchers[i].m_Type = m_pController->GameServer()->Collision()->m_pSwitchers[i].m_Type[Team];
			}
		}
		return 0;
	}
	else
		return 1;
}

int CSaveTeam::load(int Team)
{
	if(Team <= 0 || Team >= MAX_CLIENTS)
		return 1;

	CGameTeams* pTeams = &(((CGameControllerDDRace*)m_pController)->m_Teams);

	if(pTeams->Count(Team) > m_MembersCount)
		return 2;

	CCharacter *pChr;

	for (int i = 0; i < m_MembersCount; i++)
	{
		int ID = MatchPlayer(m_apSavedTees[i].GetName());
		if(ID == -1) // first check if team can be loaded / do not load half teams
		{
			return i+10; // +10 to leave space for other return-values
		}
		if(m_pController->GameServer()->m_apPlayers[ID] && m_pController->GameServer()->m_apPlayers[ID]->GetCharacter() && m_pController->GameServer()->m_apPlayers[ID]->GetCharacter()->m_DDRaceState)
		{
			return i+100; // +100 to leave space for other return-values
		}
		if(Team != pTeams->m_Core.Team(ID))
		{
			return i+200; // +100 to leave space for other return-values
		}
	}

	pTeams->ChangeTeamState(Team, m_TeamState);
	pTeams->SetTeamLock(Team, m_TeamLocked);
	if(m_Practice)
		pTeams->EnablePractice(Team);

	for (int i = 0; i < m_MembersCount; i++)
	{
		pChr = MatchCharacter(m_apSavedTees[i].GetName(), i);
		if(pChr)
		{
			m_apSavedTees[i].Load(pChr, Team);
		}
	}

	if(m_pController->GameServer()->Collision()->m_NumSwitchers)
		for(int i=1; i < m_pController->GameServer()->Collision()->m_NumSwitchers+1; i++)
		{
			m_pController->GameServer()->Collision()->m_pSwitchers[i].m_Status[Team] = m_Switchers[i].m_Status;
			if(m_Switchers[i].m_EndTime)
				m_pController->GameServer()->Collision()->m_pSwitchers[i].m_EndTick[Team] = m_pController->Server()->Tick() - m_Switchers[i].m_EndTime;
			m_pController->GameServer()->Collision()->m_pSwitchers[i].m_Type[Team] = m_Switchers[i].m_Type;
		}
	return 0;
}

int CSaveTeam::MatchPlayer(char name[16])
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if(str_comp(m_pController->Server()->ClientName(i), name) == 0)
		{
			return i;
		}
	}
	return -1;
}

CCharacter* CSaveTeam::MatchCharacter(char name[16], int SaveID)
{
	int ID = MatchPlayer(name);
	if(ID >= 0 && m_pController->GameServer()->m_apPlayers[ID])
	{
		if(m_pController->GameServer()->m_apPlayers[ID]->GetCharacter())
			return m_pController->GameServer()->m_apPlayers[ID]->GetCharacter();
		else
			return m_pController->GameServer()->m_apPlayers[ID]->ForceSpawn(m_apSavedTees[SaveID].GetPos());
	}
	return 0;
}

char* CSaveTeam::GetString()
{
	str_format(m_String, sizeof(m_String), "%d\t%d\t%d\t%d\t%d", m_TeamState, m_MembersCount, m_NumSwitchers, m_TeamLocked, m_Practice);

	for(int i = 0; i < m_MembersCount; i++)
	{
		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "\n%s", m_apSavedTees[i].GetString());
		str_append(m_String, aBuf, sizeof(m_String));
	}

	if(m_Switchers && m_NumSwitchers)
	{
		for(int i=1; i < m_NumSwitchers+1; i++)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "\n%d\t%d\t%d", m_Switchers[i].m_Status, m_Switchers[i].m_EndTime, m_Switchers[i].m_Type);
			str_append(m_String, aBuf, sizeof(m_String));
		}
	}

	return m_String;
}

int CSaveTeam::LoadString(const char* String)
{
	char TeamStats[MAX_CLIENTS];
	char Switcher[64];
	char SaveTee[1024];

	char* CopyPos;
	unsigned int Pos = 0;
	unsigned int LastPos = 0;
	unsigned int StrSize;

	str_copy(m_String, String, sizeof(m_String));

	while (m_String[Pos] != '\n' && Pos < sizeof(m_String) && m_String[Pos]) // find next \n or \0
		Pos++;

	CopyPos = m_String + LastPos;
	StrSize = Pos - LastPos + 1;
	if(m_String[Pos] == '\n')
	{
		Pos++; // skip \n
		LastPos = Pos;
	}

	if(StrSize <= 0)
	{
		dbg_msg("load", "savegame: wrong format (couldn't load teamstats)");
		return 1;
	}

	if(StrSize < sizeof(TeamStats))
	{
		str_copy(TeamStats, CopyPos, StrSize);
		int Num = sscanf(TeamStats, "%d\t%d\t%d\t%d\t%d", &m_TeamState, &m_MembersCount, &m_NumSwitchers, &m_TeamLocked, &m_Practice);
		switch(Num) // Don't forget to update this when you save / load more / less.
		{
			case 4:
				m_Practice = false;
				// fallthrough
			case 5:
				break;
			default:
				dbg_msg("load", "failed to load teamstats");
				dbg_msg("load", "loaded %d vars", Num);
				return Num + 1; // never 0 here
		}
	}
	else
	{
		dbg_msg("load", "savegame: wrong format (couldn't load teamstats, too big)");
		return 1;
	}

	if(m_apSavedTees)
	{
		delete [] m_apSavedTees;
		m_apSavedTees = 0;
	}

	if(m_MembersCount)
		m_apSavedTees = new CSaveTee[m_MembersCount];

	for (int n = 0; n < m_MembersCount; n++)
	{
		while (m_String[Pos] != '\n' && Pos < sizeof(m_String) && m_String[Pos]) // find next \n or \0
			Pos++;

		CopyPos = m_String + LastPos;
		StrSize = Pos - LastPos + 1;
		if(m_String[Pos] == '\n')
		{
			Pos++; // skip \n
			LastPos = Pos;
		}

		if(StrSize <= 0)
		{
			dbg_msg("load", "savegame: wrong format (couldn't load tee)");
			return 1;
		}

		if(StrSize < sizeof(SaveTee))
		{
			str_copy(SaveTee, CopyPos, StrSize);
			int Num = m_apSavedTees[n].LoadString(SaveTee);
			if(Num)
			{
				dbg_msg("load", "failed to load tee");
				dbg_msg("load", "loaded %d vars", Num-1);
				return 1;
			}
		}
		else
		{
			dbg_msg("load", "savegame: wrong format (couldn't load tee, too big)");
			return 1;
		}
	}

	if(m_Switchers)
	{
		delete [] m_Switchers;
		m_Switchers = 0;
	}

	if(m_NumSwitchers)
		m_Switchers = new SSimpleSwitchers[m_NumSwitchers+1];

	for (int n = 1; n < m_NumSwitchers+1; n++)
		{
			while (m_String[Pos] != '\n' && Pos < sizeof(m_String) && m_String[Pos]) // find next \n or \0
				Pos++;

			CopyPos = m_String + LastPos;
			StrSize = Pos - LastPos + 1;
			if(m_String[Pos] == '\n')
			{
				Pos++; // skip \n
				LastPos = Pos;
			}

			if(StrSize <= 0)
			{
				dbg_msg("load", "savegame: wrong format (couldn't load switcher)");
				return 1;
			}

			if(StrSize < sizeof(Switcher))
			{
				str_copy(Switcher, CopyPos, StrSize);
				int Num = sscanf(Switcher, "%d\t%d\t%d", &(m_Switchers[n].m_Status), &(m_Switchers[n].m_EndTime), &(m_Switchers[n].m_Type));
				if(Num != 3)
				{
					dbg_msg("load", "failed to load switcher");
					dbg_msg("load", "loaded %d vars", Num-1);
				}
			}
			else
			{
				dbg_msg("load", "savegame: wrong format (couldn't load switcher, too big)");
				return 1;
			}
		}

	return 0;
}
