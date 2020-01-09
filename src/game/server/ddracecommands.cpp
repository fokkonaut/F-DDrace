/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "gamecontext.h"
#include <engine/shared/config.h>
#include <game/server/teams.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/version.h>
#include <game/server/entities/character.h>

bool CheckClientID(int ClientID);

void CGameContext::ConGoLeft(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	pSelf->MoveCharacter(pResult->m_ClientID, -1, 0);
}

void CGameContext::ConGoRight(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	pSelf->MoveCharacter(pResult->m_ClientID, 1, 0);
}

void CGameContext::ConGoDown(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	pSelf->MoveCharacter(pResult->m_ClientID, 0, 1);
}

void CGameContext::ConGoUp(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	pSelf->MoveCharacter(pResult->m_ClientID, 0, -1);
}

void CGameContext::ConMove(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	pSelf->MoveCharacter(pResult->m_ClientID, pResult->GetInteger(0),
			pResult->GetInteger(1));
}

void CGameContext::ConMoveRaw(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	pSelf->MoveCharacter(pResult->m_ClientID, pResult->GetInteger(0),
			pResult->GetInteger(1), true);
}

void CGameContext::MoveCharacter(int ClientID, int X, int Y, bool Raw)
{
	CCharacter* pChr = GetPlayerChar(ClientID);

	if (!pChr)
		return;

	pChr->Core()->m_Pos.x += ((Raw) ? 1 : 32) * X;
	pChr->Core()->m_Pos.y += ((Raw) ? 1 : 32) * Y;
	pChr->m_DDRaceState = DDRACE_CHEAT;
}

void CGameContext::ConKillPlayer(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->GetVictim();

	if (pSelf->m_apPlayers[Victim])
	{
		pSelf->m_apPlayers[Victim]->KillCharacter(WEAPON_GAME);
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "%s was killed by %s",
				pSelf->Server()->ClientName(Victim),
				pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChat(-1, CHAT_ALL, -1, aBuf);
	}
}

void CGameContext::ConNinja(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_NINJA, false);
}

void CGameContext::ConSuper(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (pChr && !pChr->m_Super)
	{
		pChr->m_Super = true;
		pChr->UnFreeze();
		pChr->m_TeamBeforeSuper = pChr->Team();
		pChr->Teams()->SetCharacterTeam(Victim, TEAM_SUPER);
		pChr->m_DDRaceState = DDRACE_CHEAT;
		if (pChr->m_Passive)
			pChr->PassiveCollision(false);
	}
}

void CGameContext::ConUnSuper(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (pChr && pChr->m_Super)
	{
		pChr->m_Super = false;
		pChr->Teams()->SetForceCharacterTeam(pResult->m_ClientID, pChr->m_TeamBeforeSuper);
		if (pChr->m_Passive)
			pChr->PassiveCollision(true);
	}
}

void CGameContext::ConUnSolo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->SetSolo(false);
}

void CGameContext::ConUnDeep(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->m_DeepFreeze = false;
}

void CGameContext::ConShotgun(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_SHOTGUN, false);
}

void CGameContext::ConGrenade(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_GRENADE, false);
}

void CGameContext::ConRifle(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_LASER, false);
}

void CGameContext::ConWeapons(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, -1, false);
}

void CGameContext::ConUnShotgun(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_SHOTGUN, true);
}

void CGameContext::ConUnGrenade(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_GRENADE, true);
}

void CGameContext::ConUnRifle(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_LASER, true);
}

void CGameContext::ConUnWeapons(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, -1, true);
}

void CGameContext::ConAddWeapon(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, pResult->GetInteger(0), false, true);
}

void CGameContext::ConRemoveWeapon(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, pResult->GetInteger(0), true, true);
}

void CGameContext::ModifyWeapons(IConsole::IResult* pResult, void* pUserData, int Weapon, bool Remove, bool AddRemoveCommand)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Offset = AddRemoveCommand ? 1 : 0;
	int Victim = pResult->NumArguments() > Offset ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = GetPlayerChar(Victim);
	if (!pChr)
		return;

	int NumWeapons = NUM_WEAPONS;
	if (clamp(Weapon, -3, NumWeapons) != Weapon)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info",
				"invalid weapon id");
		return;
	}

	int Amount = (pChr->GetPlayer()->m_Gamemode == GAMEMODE_VANILLA && Weapon != WEAPON_HAMMER && Weapon != WEAPON_TELEKINESIS && Weapon != WEAPON_LIGHTSABER) ? 10 : -1;

	bool Spread = Remove ? false : pResult->NumArguments() > 1 + Offset ? pResult->GetInteger(1 + Offset) : Weapon >= 0 ? pChr->m_aSpreadWeapon[Weapon] : false;

	if (Weapon < 0)
	{
		if (Weapon == -1 || Weapon == -3)
		{
			pChr->GiveWeapon(WEAPON_SHOTGUN, Remove, Amount);
			pChr->GiveWeapon(WEAPON_GRENADE, Remove, Amount);
			pChr->GiveWeapon(WEAPON_LASER, Remove, Amount);
			if (!Remove)
			{
				pChr->GiveWeapon(WEAPON_HAMMER, Remove);
				pChr->GiveWeapon(WEAPON_GUN, Remove, Amount);
			}
			for (int i = 0; i < WEAPON_NINJA; i++)
				if (pChr->m_aSpreadWeapon[i] != Spread)
					pChr->SpreadWeapon(i, Spread, pResult->m_ClientID);
		}
		if (Weapon == -2 || Weapon == -3)
		{
			pChr->GiveWeapon(WEAPON_HEART_GUN, Remove, Amount);
			pChr->GiveWeapon(WEAPON_PLASMA_RIFLE, Remove, Amount);
			pChr->GiveWeapon(WEAPON_STRAIGHT_GRENADE, Remove, Amount);
			pChr->GiveWeapon(WEAPON_TELEKINESIS, Remove);
			pChr->GiveWeapon(WEAPON_LIGHTSABER, Remove);
			pChr->GiveWeapon(WEAPON_TELE_RIFLE, Remove, Amount);
			pChr->GiveWeapon(WEAPON_PROJECTILE_RIFLE, Remove, Amount);
			pChr->GiveWeapon(WEAPON_BALL_GRENADE, Remove, Amount);

			for (int i = WEAPON_NINJA; i < NUM_WEAPONS; i++)
				if (pChr->m_aSpreadWeapon[i] != Spread)
					pChr->SpreadWeapon(i, Spread, pResult->m_ClientID);
		}
	}
	else
	{
		if (Weapon == WEAPON_NINJA && pChr->m_ScrollNinja && Remove)
			pChr->ScrollNinja(false);
		pChr->GiveWeapon(Weapon, Remove, Amount);

		if (pChr->m_aSpreadWeapon[Weapon] != Spread)
			pChr->SpreadWeapon(Weapon, Spread, pResult->m_ClientID);
	}

	pChr->m_DDRaceState = DDRACE_CHEAT;
}

void CGameContext::ConToTeleporter(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	unsigned int TeleTo = pResult->GetInteger(0);

	if (((CGameControllerDDRace*)pSelf->m_pController)->m_TeleOuts[TeleTo-1].size())
	{
		int Num = ((CGameControllerDDRace*)pSelf->m_pController)->m_TeleOuts[TeleTo-1].size();
		vec2 TelePos = ((CGameControllerDDRace*)pSelf->m_pController)->m_TeleOuts[TeleTo-1][(!Num)?Num:rand() % Num];
		CCharacter* pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
		if (pChr)
		{
			pChr->Core()->m_Pos = TelePos;
			pChr->SetPos(TelePos);
			pChr->m_PrevPos = TelePos;
			pChr->m_DDRaceState = DDRACE_CHEAT;
		}
	}
}

void CGameContext::ConToCheckTeleporter(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	unsigned int TeleTo = pResult->GetInteger(0);

	if (((CGameControllerDDRace*)pSelf->m_pController)->m_TeleCheckOuts[TeleTo-1].size())
	{
		int Num = ((CGameControllerDDRace*)pSelf->m_pController)->m_TeleCheckOuts[TeleTo-1].size();
		vec2 TelePos = ((CGameControllerDDRace*)pSelf->m_pController)->m_TeleCheckOuts[TeleTo-1][(!Num)?Num:rand() % Num];
		CCharacter* pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
		if (pChr)
		{
			pChr->Core()->m_Pos = TelePos;
			pChr->SetPos(TelePos);
			pChr->m_PrevPos = TelePos;
			pChr->m_DDRaceState = DDRACE_CHEAT;
			pChr->m_TeleCheckpoint = TeleTo;
		}
	}
}

void CGameContext::ConTeleport(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	int Tele = pResult->GetVictim();
	int TeleTo = pResult->NumArguments() == 2 ? pResult->GetInteger(1) : pResult->m_ClientID;
	if (pResult->NumArguments() < 2 && Tele != pResult->m_ClientID)
		return;

	CCharacter *pChr = pSelf->GetPlayerChar(Tele);
	if(pChr && pSelf->GetPlayerChar(TeleTo))
	{
		pChr->Core()->m_Pos = pSelf->m_apPlayers[TeleTo]->m_ViewPos;
		pChr->SetPos(pSelf->m_apPlayers[TeleTo]->m_ViewPos);
		pChr->m_PrevPos = pSelf->m_apPlayers[TeleTo]->m_ViewPos;
		pChr->m_DDRaceState = DDRACE_CHEAT;
	}
}

void CGameContext::ConKill(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];

	if (!pPlayer
			|| (pPlayer->m_LastKill
					&& pPlayer->m_LastKill
					+ pSelf->Server()->TickSpeed()
					* g_Config.m_SvKillDelay
					> pSelf->Server()->Tick()))
		return;

	pPlayer->m_LastKill = pSelf->Server()->Tick();
	pPlayer->KillCharacter(WEAPON_SELF);
	//pPlayer->m_RespawnTick = pSelf->Server()->Tick() + pSelf->Server()->TickSpeed() * g_Config.m_SvSuicidePenalty;
}

void CGameContext::ConForcePause(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->GetVictim();
	int Seconds = 0;
	if (pResult->NumArguments() > 1)
		Seconds = clamp(pResult->GetInteger(1), 0, 360);

	CPlayer *pPlayer = pSelf->m_apPlayers[Victim];
	if (!pPlayer)
		return;

	pPlayer->ForcePause(Seconds);
}

bool CGameContext::TryVoteMute(const NETADDR* pAddr, int Secs)
{
	// find a matching vote mute for this ip, update expiration time if found
	for (int i = 0; i < m_NumVoteMutes; i++)
	{
		if (net_addr_comp_noport(&m_aVoteMutes[i].m_Addr, pAddr) == 0)
		{
			m_aVoteMutes[i].m_Expire = Server()->Tick()
				+ Secs * Server()->TickSpeed();
			return true;
		}
	}

	// nothing to update create new one
	if (m_NumVoteMutes < MAX_VOTE_MUTES)
	{
		m_aVoteMutes[m_NumVoteMutes].m_Addr = *pAddr;
		m_aVoteMutes[m_NumVoteMutes].m_Expire = Server()->Tick()
			+ Secs * Server()->TickSpeed();
		m_NumVoteMutes++;
		return true;
	}
	// no free slot found
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "votemute", "vote mute array is full");
	return false;
}

bool CGameContext::VoteMute(const NETADDR* pAddr, int Secs, const char* pDisplayName, int AuthedID)
{
	if (!TryVoteMute(pAddr, Secs))
		return false;

	if (!pDisplayName)
		return true;

	char aBuf[128];
	str_format(aBuf, sizeof aBuf, "'%s' banned '%s' for %d seconds from voting.",
		Server()->ClientName(AuthedID), pDisplayName, Secs);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "votemute", aBuf);
	return true;
}

bool CGameContext::VoteUnmute(const NETADDR* pAddr, const char* pDisplayName, int AuthedID)
{
	for (int i = 0; i < m_NumVoteMutes; i++)
	{
		if (net_addr_comp_noport(&m_aVoteMutes[i].m_Addr, pAddr) == 0)
		{
			m_NumVoteMutes--;
			m_aVoteMutes[i] = m_aVoteMutes[m_NumVoteMutes];
			if (pDisplayName)
			{
				char aBuf[128];
				str_format(aBuf, sizeof aBuf, "'%s' unbanned '%s' from voting.",
					Server()->ClientName(AuthedID), pDisplayName);
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "voteunmute", aBuf);
			}
			return true;
		}
	}
	return false;
}

bool CGameContext::TryMute(const NETADDR *pAddr, int Secs)
{
	// find a matching mute for this ip, update expiration time if found
	for(int i = 0; i < m_NumMutes; i++)
	{
		if(net_addr_comp_noport(&m_aMutes[i].m_Addr, pAddr) == 0)
		{
			m_aMutes[i].m_Expire = Server()->Tick()
							+ Secs * Server()->TickSpeed();
			return true;
		}
	}

	// nothing to update create new one
	if(m_NumMutes < MAX_MUTES)
	{
		m_aMutes[m_NumMutes].m_Addr = *pAddr;
		m_aMutes[m_NumMutes].m_Expire = Server()->Tick()
						+ Secs * Server()->TickSpeed();
		m_NumMutes++;
		return true;
	}
	// no free slot found
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", "mute array is full");
	return false;
}

void CGameContext::Mute(const NETADDR *pAddr, int Secs, const char *pDisplayName)
{
	if (!TryMute(pAddr, Secs))
		return;

	if(!pDisplayName)
		return;

	char aBuf[128];
	str_format(aBuf, sizeof aBuf, "'%s' has been muted for %d seconds.",
			pDisplayName, Secs);
	SendChat(-1, CHAT_ALL, -1, aBuf);
}

void CGameContext::ConVoteMute(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->GetVictim();

	if (Victim < 0 || Victim > MAX_CLIENTS || !pSelf->m_apPlayers[Victim])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "votemute", "Client ID not found");
		return;
	}

	NETADDR Addr;
	pSelf->Server()->GetClientAddr(Victim, &Addr);

	int Seconds = clamp(pResult->GetInteger(1), 1, 86400);
	bool Found = pSelf->VoteMute(&Addr, Seconds, pSelf->Server()->ClientName(Victim), pResult->m_ClientID);

	if (Found)
	{
		char aBuf[128];
		str_format(aBuf, sizeof aBuf, "'%s' banned '%s' for %d seconds from voting.",
			pSelf->Server()->ClientName(pResult->m_ClientID), pSelf->Server()->ClientName(Victim), Seconds);
		pSelf->SendChat(-1, CHAT_ALL, -1, aBuf);
	}
}

void CGameContext::ConVoteUnmute(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->GetVictim();

	if (Victim < 0 || Victim > MAX_CLIENTS || !pSelf->m_apPlayers[Victim])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "voteunmute", "Client ID not found");
		return;
	}

	NETADDR Addr;
	pSelf->Server()->GetClientAddr(Victim, &Addr);

	bool Found = pSelf->VoteUnmute(&Addr, pSelf->Server()->ClientName(Victim), pResult->m_ClientID);
	if (Found)
	{
		char aBuf[128];
		str_format(aBuf, sizeof aBuf, "'%s' unbanned '%s' from voting.",
			pSelf->Server()->ClientName(pResult->m_ClientID), pSelf->Server()->ClientName(Victim));
		pSelf->SendChat(-1, CHAT_ALL, -1, aBuf);
	}
}

void CGameContext::ConVoteMutes(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;

	if (pSelf->m_NumVoteMutes <= 0)
	{
		// Just to make sure.
		pSelf->m_NumVoteMutes = 0;
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "votemutes",
			"There are no active vote mutes.");
		return;
	}

	char aIpBuf[64];
	char aBuf[128];
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "votemutes",
		"Active vote mutes:");
	for (int i = 0; i < pSelf->m_NumVoteMutes; i++)
	{
		net_addr_str(&pSelf->m_aVoteMutes[i].m_Addr, aIpBuf, sizeof(aIpBuf), false);
		str_format(aBuf, sizeof aBuf, "%d: \"%s\", %d seconds left", i,
			aIpBuf, (pSelf->m_aVoteMutes[i].m_Expire - pSelf->Server()->Tick()) / pSelf->Server()->TickSpeed());
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "votemutes", aBuf);
	}
}

void CGameContext::ConMute(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"mutes",
			"Use either 'muteid <client_id> <seconds>' or 'muteip <ip> <seconds>'");
}

// mute through client id
void CGameContext::ConMuteID(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	int Victim = pResult->GetVictim();

	if (Victim < 0 || Victim > MAX_CLIENTS || !pSelf->m_apPlayers[Victim])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "muteid", "Client id not found.");
		return;
	}

	NETADDR Addr;
	pSelf->Server()->GetClientAddr(Victim, &Addr);

	pSelf->Mute(&Addr, clamp(pResult->GetInteger(1), 1, 86400),
			pSelf->Server()->ClientName(Victim));
}

// mute through ip, arguments reversed to workaround parsing
void CGameContext::ConMuteIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	NETADDR Addr;
	if (net_addr_from_str(&Addr, pResult->GetString(0)))
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes",
				"Invalid network address to mute");
	}
	pSelf->Mute(&Addr, clamp(pResult->GetInteger(1), 1, 86400), NULL);
}

// unmute by mute list index
void CGameContext::ConUnmute(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	char aIpBuf[64];
	char aBuf[64];
	int Victim = pResult->GetVictim();

	if (Victim < 0 || Victim >= pSelf->m_NumMutes)
		return;

	pSelf->m_NumMutes--;
	pSelf->m_aMutes[Victim] = pSelf->m_aMutes[pSelf->m_NumMutes];

	net_addr_str(&pSelf->m_aMutes[Victim].m_Addr, aIpBuf, sizeof(aIpBuf), false);
	str_format(aBuf, sizeof(aBuf), "Unmuted %s", aIpBuf);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", aBuf);
}

// list mutes
void CGameContext::ConMutes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;

	if (pSelf->m_NumMutes <= 0)
	{
		// Just to make sure.
		pSelf->m_NumMutes = 0;
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes",
			"There are no active mutes.");
		return;
	}

	char aIpBuf[64];
	char aBuf[128];
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes",
			"Active mutes:");
	for (int i = 0; i < pSelf->m_NumMutes; i++)
	{
		net_addr_str(&pSelf->m_aMutes[i].m_Addr, aIpBuf, sizeof(aIpBuf), false);
		str_format(aBuf, sizeof aBuf, "%d: \"%s\", %d seconds left", i, aIpBuf,
				(pSelf->m_aMutes[i].m_Expire - pSelf->Server()->Tick()) / pSelf->Server()->TickSpeed());
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", aBuf);
	}
}

void CGameContext::ConSetDDRTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CGameControllerDDRace*pController = (CGameControllerDDRace*)pSelf->m_pController;

	int Target = pResult->GetVictim();
	int Team = pResult->GetInteger(1);

	if (Team < TEAM_FLOCK || Team >= TEAM_SUPER)
		return;

	if(pController->m_Teams.m_Core.Team(Target) && pController->m_Teams.GetDDRaceState(pSelf->m_apPlayers[Target]) == DDRACE_STARTED)
		pSelf->m_apPlayers[Target]->KillCharacter(WEAPON_SELF);

	pController->m_Teams.SetForceCharacterTeam(Target, Team);
}

void CGameContext::ConUninvite(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CGameControllerDDRace*pController = (CGameControllerDDRace*)pSelf->m_pController;

	pController->m_Teams.SetClientInvited(pResult->GetInteger(1), pResult->GetVictim(), false);
}

// F-DDrace

void CGameContext::ConAllWeapons(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, -3, false);
}

void CGameContext::ConUnAllWeapons(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, -3, true);
}

void CGameContext::ConExtraWeapons(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, -2, false);
}

void CGameContext::ConUnExtraWeapons(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, -2, true);
}

void CGameContext::ConHammer(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_HAMMER, false);
}

void CGameContext::ConUnHammer(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_HAMMER, true);
}

void CGameContext::ConGun(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_GUN, false);
}

void CGameContext::ConUnGun(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_GUN, true);
}

void CGameContext::ConPlasmaRifle(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_PLASMA_RIFLE, false);
}

void CGameContext::ConUnPlasmaRifle(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_PLASMA_RIFLE, true);
}

void CGameContext::ConHeartGun(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_HEART_GUN, false);
}

void CGameContext::ConUnHeartGun(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_HEART_GUN, true);
}

void CGameContext::ConStraightGrenade(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_STRAIGHT_GRENADE, false);
}

void CGameContext::ConUnStraightGrenade(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_STRAIGHT_GRENADE, true);
}

void CGameContext::ConTelekinesis(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_TELEKINESIS, false);
}

void CGameContext::ConUnTelekinesis(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_TELEKINESIS, true);
}

void CGameContext::ConLightsaber(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_LIGHTSABER, false);
}

void CGameContext::ConUnLightsaber(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_LIGHTSABER, true);
}

void CGameContext::ConTeleRifle(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_TELE_RIFLE, false);
}

void CGameContext::ConUnTeleRifle(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_TELE_RIFLE, true);
}

void CGameContext::ConProjectileRifle(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_PROJECTILE_RIFLE, false);
}

void CGameContext::ConUnProjectileRifle(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_PROJECTILE_RIFLE, true);
}

void CGameContext::ConBallGrenade(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_BALL_GRENADE, false);
}

void CGameContext::ConUnBallGrenade(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_BALL_GRENADE, true);
}

void CGameContext::ConScrollNinja(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->ScrollNinja(!pChr->m_ScrollNinja, pResult->m_ClientID);
}

void CGameContext::ConInfiniteJumps(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->InfiniteJumps(!pChr->m_SuperJump, pResult->m_ClientID);
}

void CGameContext::ConEndlessHook(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->EndlessHook(!pChr->m_EndlessHook, pResult->m_ClientID);
}

void CGameContext::ConJetpack(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->Jetpack(!pChr->m_Jetpack, pResult->m_ClientID);
}

void CGameContext::ConRainbowSpeed(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CPlayer *pPlayer = pSelf->m_apPlayers[Victim];
	if (!pPlayer)
		return;

	char aBuf[64];
	if (pResult->NumArguments() < 2)
	{
		str_format(aBuf, sizeof(aBuf), "Value: %d", pPlayer->m_RainbowSpeed);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}
	else
	{
		int Speed = clamp(pResult->GetInteger(1), 1, 20);
		str_format(aBuf, sizeof(aBuf), "Rainbow speed for '%s' changed to %d", pSelf->Server()->ClientName(Victim), Speed);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
		pPlayer->m_RainbowSpeed = Speed;
	}
}

void CGameContext::ConRainbow(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->Rainbow(!(pChr->m_Rainbow || pChr->GetPlayer()->m_InfRainbow), pResult->m_ClientID);
}

void CGameContext::ConInfRainbow(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->InfRainbow(!(pChr->m_Rainbow || pChr->GetPlayer()->m_InfRainbow), pResult->m_ClientID);
}

void CGameContext::ConAtom(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->Atom(!pChr->m_Atom, pResult->m_ClientID);
}

void CGameContext::ConTrail(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->Trail(!pChr->m_Trail, pResult->m_ClientID);
}

void CGameContext::ConSpookyGhost(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->SpookyGhost(!pChr->GetPlayer()->m_HasSpookyGhost, pResult->m_ClientID);
}

void CGameContext::ConSpooky(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->Spooky(!pChr->m_Spooky, pResult->m_ClientID);
}

void CGameContext::ConAddMeteor(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->Meteor(true, pResult->m_ClientID);
}

void CGameContext::ConAddInfMeteor(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->Meteor(true, pResult->m_ClientID, true);
}

void CGameContext::ConRemoveMeteors(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->Meteor(false, pResult->m_ClientID);
}

void CGameContext::ConInvisible(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->Invisible(!pChr->m_Invisible, pResult->m_ClientID);
}

void CGameContext::ConPassive(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->Passive(!pChr->m_Passive, pResult->m_ClientID);
}

void CGameContext::ConItem(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->Item(pResult->GetInteger(1), pResult->m_ClientID);
}

void CGameContext::ConVanillaMode(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->VanillaMode(pResult->m_ClientID);
}

void CGameContext::ConDDraceMode(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->DDraceMode(pResult->m_ClientID);
}

void CGameContext::ConBloody(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->Bloody(!(pChr->m_Bloody || pChr->m_StrongBloody), pResult->m_ClientID);
}

void CGameContext::ConStrongBloody(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->StrongBloody(!pChr->m_StrongBloody, pResult->m_ClientID);
}

void CGameContext::ConHookPower(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() > 1 ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	bool ShowInfo = false;
	if (pChr)
	{
		int Power = -1;
		for (int i = 0; i < NUM_EXTRAS; i++)
		{
			if (!str_comp_nocase(pResult->GetString(0), pSelf->GetExtraName(i)))
				if (pSelf->IsValidHookPower(i))
					Power = i;
		}
		if (Power == -1)
			ShowInfo = true;
		else
		{
			if (pChr->m_HookPower == Power)
				Power = HOOK_NORMAL;
			pChr->HookPower(Power, pResult->m_ClientID);
		}
	}
	if (!pResult->NumArguments() || ShowInfo)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "~~~ Hook Powers ~~~");
		for (int i = 0; i < NUM_EXTRAS; i++)
		{
			if (pSelf->IsValidHookPower(i))
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", pSelf->GetExtraName(i));
		}
	}
}

void CGameContext::ConFreezeHammer(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->FreezeHammer(!pChr->m_FreezeHammer, pResult->m_ClientID);
}

void CGameContext::ConForceFlagOwner(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() > 1 ? pResult->GetInteger(1) : pResult->m_ClientID;
	((CGameControllerDDRace*)pSelf->m_pController)->ForceFlagOwner(Victim, pResult->GetInteger(0));
}

void CGameContext::ConSayBy(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->GetVictim();
	CPlayer* pPlayer = pSelf->m_apPlayers[Victim];
	if (pPlayer) pSelf->SendChat(Victim, CHAT_ALL, -1, pResult->GetString(1));
}

void CGameContext::ConPlayerInfo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	int ID = pSelf->GetCIDByName(pResult->GetString(0));
	if (ID < 0)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "Invalid player");
		return;
	}

	CCharacter *pChr = pSelf->GetPlayerChar(ID);
	CPlayer *pPlayer = pSelf->m_apPlayers[ID];

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "==== [PLAYER INFO] '%s' ====", pResult->GetString(0));
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	if (pSelf->Server()->GetAuthedState(ID))
	{
		str_format(aBuf, sizeof(aBuf), "Authed: %d", pSelf->Server()->GetAuthedState(ID));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	str_format(aBuf, sizeof(aBuf), "ClientID: %d", ID);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	if (pChr)
		pSelf->SendChatTarget(pResult->m_ClientID, "Status: Ingame");
	else if (pPlayer->GetTeam() == TEAM_SPECTATORS)
		pSelf->SendChatTarget(pResult->m_ClientID, "Status: Spectator");
	else
		pSelf->SendChatTarget(pResult->m_ClientID, "Status: Dead");
	if (pPlayer->GetAccID() >= ACC_START)
	{
		str_format(aBuf, sizeof(aBuf), "Account Name: %s", pSelf->m_Accounts[pPlayer->GetAccID()].m_Username);
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	else
		pSelf->SendChatTarget(pResult->m_ClientID, "Account: Not logged in");
	if (pPlayer->m_InfRainbow)
		pSelf->SendChatTarget(pResult->m_ClientID, "Infinite Rainbow: True");
	if (pPlayer->m_InfMeteors > 0)
	{
		str_format(aBuf, sizeof(aBuf), "Infinite Meteors: %d", pPlayer->m_InfMeteors);
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	if (pPlayer->m_Gamemode == GAMEMODE_DDRACE)
		pSelf->SendChatTarget(pResult->m_ClientID, "Mode: DDrace");
	else if (pPlayer->m_Gamemode == GAMEMODE_VANILLA)
		pSelf->SendChatTarget(pResult->m_ClientID, "Mode: Vanilla");
	if (pPlayer->m_SpookyGhost)
		pSelf->SendChatTarget(pResult->m_ClientID, "Spooky Ghost: True");
	if (pPlayer->m_pControlledTee)
	{
		str_format(aBuf, sizeof(aBuf), "Tee Control: %s", pSelf->Server()->ClientName(pPlayer->m_pControlledTee->GetCID()));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	else if (pPlayer->m_HasTeeControl)
		pSelf->SendChatTarget(pResult->m_ClientID, "Tee Control: True");

	if (pChr)
	{
		if (pChr->HasFlag() == TEAM_RED)
			pSelf->SendChatTarget(pResult->m_ClientID, "Flag: Red");
		if (pChr->HasFlag() == TEAM_BLUE)
			pSelf->SendChatTarget(pResult->m_ClientID, "Flag: Blue");
		if (pChr->m_DeepFreeze)
			pSelf->SendChatTarget(pResult->m_ClientID, "Frozen: Deep");
		else if (pChr->m_IsFrozen)
			pSelf->SendChatTarget(pResult->m_ClientID, "Frozen: True");
		else if (pChr->m_FreezeTime)
		{
			str_format(aBuf, sizeof(aBuf), "Frozen: Freezetime: %d", pChr->m_FreezeTime);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
		}
		if (pChr->m_SuperJump)
			pSelf->SendChatTarget(pResult->m_ClientID, "SuperJump: True");
		if (pChr->m_EndlessHook)
			pSelf->SendChatTarget(pResult->m_ClientID, "Endless: True");
		if (pChr->m_Jetpack)
			pSelf->SendChatTarget(pResult->m_ClientID, "Jetpack: True");
		if (pChr->m_Rainbow)
			pSelf->SendChatTarget(pResult->m_ClientID, "Rainbow: True");
		if (pChr->m_Atom)
			pSelf->SendChatTarget(pResult->m_ClientID, "Atom: True");
		if (pChr->m_Trail)
			pSelf->SendChatTarget(pResult->m_ClientID, "Trail: True");
		if (pChr->m_Bloody)
			pSelf->SendChatTarget(pResult->m_ClientID, "Bloody: True");
		if (pChr->m_StrongBloody)
			pSelf->SendChatTarget(pResult->m_ClientID, "Strong Bloody: True");
		if (pChr->m_Meteors > 0)
		{
			str_format(aBuf, sizeof(aBuf), "Meteors: %d", pChr->m_Meteors);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
		}
		if (pChr->m_Passive)
			pSelf->SendChatTarget(pResult->m_ClientID, "Passive Mode: True");
		if (pChr->m_PoliceHelper)
			pSelf->SendChatTarget(pResult->m_ClientID, "Police Helper: True");
		for (int i = 0; i < NUM_WEAPONS; i++)
		{
			if (pChr->m_aSpreadWeapon[i])
			{
				str_format(aBuf, sizeof(aBuf), "Spread %s: True", pSelf->GetWeaponName(i));
				pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			}
		}
		if (pChr->m_Invisible)
			pSelf->SendChatTarget(pResult->m_ClientID, "Invisibility: True");
		if (pChr->m_HookPower != HOOK_NORMAL)
		{
			str_format(aBuf, sizeof(aBuf), "Hook Power: %s", pSelf->GetExtraName(pChr->m_HookPower));
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
		}
		if (pChr->m_Item != -3)
		{
			str_format(aBuf, sizeof(aBuf), "Item: %s", pSelf->GetWeaponName(pChr->m_Item));
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
		}
		if (pChr->m_DoorHammer)
			pSelf->SendChatTarget(pResult->m_ClientID, "Door Hammer: True");
		str_format(aBuf, sizeof(aBuf), "Position: (%.2f/%.2f)", pChr->GetPos().x / 32, pChr->GetPos().y / 32);
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
}

void CGameContext::ConLaserText(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pSelf->CreateLaserText(pChr->GetPos(), Victim, pResult->GetString(1));
}

void CGameContext::ConConnectDummy(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Amount = pResult->NumArguments() > 0 ? pResult->GetInteger(0) : 1;
	int Dummymode = pResult->NumArguments() == 2 ? pResult->GetInteger(1) : DUMMYMODE_IDLE;
	for (int i = 0; i < Amount; i++)
		pSelf->ConnectDummy(Dummymode);
}

void CGameContext::ConDisconnectDummy(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ID = pResult->GetVictim();
	if (ID >= 0 && ID < MAX_CLIENTS && pSelf->m_apPlayers[ID] && pSelf->m_apPlayers[ID]->m_IsDummy)
		pSelf->Server()->DummyLeave(ID);
}

void CGameContext::ConDummymode(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->GetVictim();
	CPlayer *pPlayer = pSelf->m_apPlayers[Victim];

	if (pResult->NumArguments() > 0)
	{
		if (!pPlayer || !pPlayer->m_IsDummy)
			return;

		if (pResult->NumArguments() == 2)
			pPlayer->m_Dummymode = pResult->GetInteger(1);
		else if (pResult->NumArguments() == 1)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "Dummymode of '%s': [%d]", pSelf->Server()->ClientName(pResult->GetInteger(0)), pPlayer->m_Dummymode);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
		}
	}
	else
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "~~~ Dummymodes ~~~");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "[0] Calm");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "[-6] BlmapV3 1o1");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "[23] ChillBlock5 Racer");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "[29] ChillBlock5 Blocker");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "[31] ChillBlock5 Police");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "[32] BlmapChill Police");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "[99] Shop Bot");
	}
}

void CGameContext::ConConnectDefaultDummies(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ConnectDefaultDummies();
}

void CGameContext::ConSound(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->CreateSoundGlobal(pResult->GetInteger(0));
}

void CGameContext::ConPlayerName(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->GetVictim();
	if (!pSelf->m_apPlayers[Victim])
		return;
	const char* pName = pResult->NumArguments() > 1 ? pResult->GetString(1) : pSelf->Server()->ClientName(Victim);
	pSelf->m_apPlayers[Victim]->SetName(pName);
	pSelf->m_apPlayers[Victim]->UpdateInformation();
}

void CGameContext::ConPlayerClan(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->GetVictim();
	if (!pSelf->m_apPlayers[Victim])
		return;
	const char* pClan = pResult->NumArguments() > 1 ? pResult->GetString(1) : pSelf->Server()->ClientClan(Victim);
	pSelf->m_apPlayers[Victim]->SetClan(pClan);
	pSelf->m_apPlayers[Victim]->UpdateInformation();
}

void CGameContext::ConPlayerSkin(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->GetVictim();
	CPlayer* pPlayer = pSelf->m_apPlayers[Victim];
	if (!pPlayer)
		return;

	if (pResult->NumArguments() > 1)
	{
		int Skin = pSelf->m_pSkins->GetSkinID(pResult->GetString(1));
		CPlayer* pFrom = pSelf->m_apPlayers[pResult->GetInteger(1)];
		char aInteger[4];
		str_format(aInteger, sizeof(aInteger), "%d", pResult->GetInteger(1));

		if (pFrom && Skin == -1 && !str_comp_nocase(aInteger, pResult->GetString(1)))
			pSelf->SendSkinChange(pFrom->m_CurrentTeeInfos, Victim, -1);
		else
			pPlayer->SetSkin(Skin, true);
	}
	else
		pPlayer->ResetSkin(true);
}

void CGameContext::ConAccLogout(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->GetVictim();
	CPlayer* pPlayer = pSelf->m_apPlayers[Victim];
	if (!pPlayer)
		return;
	if (pPlayer->GetAccID() >= ACC_START)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "Logged out account '%s' (%s)", pSelf->m_Accounts[pPlayer->GetAccID()].m_Username, pSelf->Server()->ClientName(Victim));
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
		pSelf->SendChatTarget(Victim, "You have been logged out by an admin");
		pSelf->Logout(pPlayer->GetAccID());
	}
}

void CGameContext::ConAccDisable(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;

	int ID = pSelf->GetAccount(pResult->GetString(0));
	if (ID < ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "Invalid account");
		return;
	}

	if (pSelf->m_Accounts[ID].m_ClientID >= 0)
		pSelf->SendChatTarget(pSelf->m_Accounts[ID].m_ClientID, "You have been logged out due to account deactivation by an admin");

	pSelf->m_Accounts[ID].m_Disabled = !pSelf->m_Accounts[ID].m_Disabled;

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%s account '%s'", pSelf->m_Accounts[ID].m_Disabled ? "Disabled" : "Enabled", pSelf->m_Accounts[ID].m_Username);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);

	pSelf->Logout(ID);
}

void CGameContext::ConAccVIP(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	
	int ID = pSelf->GetAccount(pResult->GetString(0));
	if (ID < ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "Invalid account");
		return;
	}

	char aBuf[64];
	pSelf->m_Accounts[ID].m_VIP = !pSelf->m_Accounts[ID].m_VIP;

	if (pSelf->m_Accounts[ID].m_ClientID >= 0)
	{
		str_format(aBuf, sizeof(aBuf), "You %s VIP", pSelf->m_Accounts[ID].m_VIP ? "got" : "lost");
		pSelf->SendChatTarget(pSelf->m_Accounts[ID].m_ClientID, aBuf);
	}

	str_format(aBuf, sizeof(aBuf), "VIP for account '%s' has been %s", pSelf->m_Accounts[ID].m_Username, pSelf->m_Accounts[ID].m_VIP ? "enabled" : "disabled");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);

	if (!pSelf->m_Accounts[ID].m_LoggedIn)
		pSelf->Logout(ID);
}

void CGameContext::ConAccInfo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	int ID = pSelf->GetAccount(pResult->GetString(0));
	if (ID < ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "Invalid account");
		return;
	}

	CGameContext::AccountInfo* Account = &pSelf->m_Accounts[ID];

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "==== [ACCOUNT INFO] '%s' ====", pResult->GetString(0));
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Port: %d", (*Account).m_Port);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Logged in: %d", (int)(*Account).m_LoggedIn);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Disabled: %d", (int)(*Account).m_Disabled);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Password: %s", (*Account).m_Password);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Username: %s", (*Account).m_Username);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "ClientID: %d", (*Account).m_ClientID);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Level: %d", (*Account).m_Level);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "XP: %d", (*Account).m_XP);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Money: %llu", (*Account).m_Money);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Kills: %d", (*Account).m_Kills);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Deaths: %d", (*Account).m_Deaths);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Police Level: %d", (*Account).m_PoliceLevel);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Survival Kills: %d", (*Account).m_SurvivalKills);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Survival Wins: %d", (*Account).m_SurvivalWins);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Spooky Ghost: %d", (int)(*Account).m_SpookyGhost);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Last Money Transaction 0: %s", (*Account).m_aLastMoneyTransaction[0]);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Last Money Transaction 1: %s", (*Account).m_aLastMoneyTransaction[1]);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Last Money Transaction 2: %s", (*Account).m_aLastMoneyTransaction[2]);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Last Money Transaction 3: %s", (*Account).m_aLastMoneyTransaction[3]);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Last Money Transaction 4: %s", (*Account).m_aLastMoneyTransaction[4]);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "VIP: %d", (int)(*Account).m_VIP);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Block Points: %d", (*Account).m_BlockPoints);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Instagib Kills: %d", (*Account).m_InstagibKills);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Instagib Wins: %d", (*Account).m_InstagibWins);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Spawn Shotgun: %d", (*Account).m_SpawnWeapon[0]);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Spawn Grenade: %d", (*Account).m_SpawnWeapon[1]);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Spawn Rifle: %d", (*Account).m_SpawnWeapon[2]);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Ninjajetpack: %d", (*Account).m_Ninjajetpack);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Last Player Name: %s", (*Account).m_aLastPlayerName);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Survival Deaths: %d", (*Account).m_SurvivalDeaths);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Instagib Deaths: %d", (*Account).m_InstagibDeaths);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Taser Level: %d", (*Account).m_TaserLevel);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Killing Spree Record: %d", (*Account).m_KillingSpreeRecord);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);

	if (!pSelf->m_Accounts[ID].m_LoggedIn)
		pSelf->FreeAccount(ID);
}

void CGameContext::ConAlwaysTeleWeapon(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->AlwaysTeleWeapon(!pChr->m_AlwaysTeleWeapon, pResult->m_ClientID);
}

void CGameContext::ConTeleGun(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->TeleWeapon(WEAPON_GUN, !pChr->m_HasTeleGun, pResult->m_ClientID);
}

void CGameContext::ConTeleGrenade(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->TeleWeapon(WEAPON_GRENADE, !pChr->m_HasTeleGrenade, pResult->m_ClientID);
}

void CGameContext::ConTeleLaser(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->TeleWeapon(WEAPON_LASER, !pChr->m_HasTeleLaser, pResult->m_ClientID);
}

void CGameContext::ConDoorHammer(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->DoorHammer(!pChr->m_DoorHammer, pResult->m_ClientID);
}

void CGameContext::ConAimClosest(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->AimClosest(!pChr->Core()->m_AimClosest, pResult->m_ClientID);
}

void CGameContext::ConSpinBot(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pChr->SpinBot(!pChr->Core()->m_SpinBot, pResult->m_ClientID);
}

void CGameContext::ConSpinBotSpeed(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (!pChr)
		return;

	char aBuf[64];
	if (pResult->NumArguments() < 2)
	{
		str_format(aBuf, sizeof(aBuf), "Value: %d", pChr->Core()->m_SpinBotSpeed);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}
	else
	{
		int Speed = clamp(pResult->GetInteger(1), 1, 500);
		str_format(aBuf, sizeof(aBuf), "Spinbot speed for '%s' changed to %d", pSelf->Server()->ClientName(Victim), Speed);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
		pChr->Core()->m_SpinBotSpeed = Speed;
	}
}

void CGameContext::ConTeeControl(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (!pChr)
		return;

	int ForcedID = -1;
	if (pResult->NumArguments() == 2)
	{
		if (pSelf->m_apPlayers[pResult->GetInteger(1)])
			ForcedID = pResult->GetInteger(1);
		else
			ForcedID = -2;
	}
	if (ForcedID != -2)
		pChr->TeeControl(!pChr->GetPlayer()->m_HasTeeControl, ForcedID, pResult->m_ClientID);
}
