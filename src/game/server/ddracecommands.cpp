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
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	if (!CheckClientID(Victim))
		return;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (pChr && !pChr->m_Super)
	{
		pChr->m_Super = true;
		pChr->UnFreeze();
		pChr->m_TeamBeforeSuper = pChr->Team();
		pChr->Teams()->SetCharacterTeam(Victim, TEAM_SUPER);
		pChr->m_DDRaceState = DDRACE_CHEAT;
		if (pChr->m_Passive)
			pSelf->SendTuningParams(Victim, pChr->m_TuneZone);
	}
}

void CGameContext::ConUnSuper(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	if (!CheckClientID(Victim))
		return;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (pChr && pChr->m_Super)
	{
		pChr->m_Super = false;
		pChr->Teams()->SetForceCharacterTeam(Victim, pChr->m_TeamBeforeSuper);
		if (pChr->m_Passive)
			pSelf->SendTuningParams(Victim, pChr->m_TuneZone);
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
			pChr->GiveWeapon(WEAPON_PORTAL_RIFLE, Remove, Amount);
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
	int Tele = pResult->NumArguments() >= 1 ? pResult->GetVictim() : pResult->m_ClientID;
	int TeleTo = pResult->NumArguments() == 2 ? pResult->GetInteger(1) : pResult->m_ClientID;

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
					* pSelf->Config()->m_SvKillDelay
					> pSelf->Server()->Tick()))
		return;

	pPlayer->m_LastKill = pSelf->Server()->Tick();
	pPlayer->KillCharacter(WEAPON_SELF);
	//pPlayer->m_RespawnTick = pSelf->Server()->Tick() + pSelf->Server()->TickSpeed() * pSelf->Config()->m_SvSuicidePenalty;
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
		if (net_addr_comp(&m_aVoteMutes[i].m_Addr, pAddr, false) == 0)
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
		if (net_addr_comp(&m_aVoteMutes[i].m_Addr, pAddr, false) == 0)
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

bool CGameContext::TryMute(const NETADDR *pAddr, int Secs, const char *pReason)
{
	// find a matching mute for this ip, update expiration time if found
	for(int i = 0; i < m_NumMutes; i++)
	{
		if(net_addr_comp(&m_aMutes[i].m_Addr, pAddr, false) == 0)
		{
			m_aMutes[i].m_Expire = Server()->Tick()
							+ Secs * Server()->TickSpeed();
			str_copy(m_aMutes[i].m_aReason, pReason, sizeof(m_aMutes[i].m_aReason));
			return true;
		}
	}

	// nothing to update create new one
	if(m_NumMutes < MAX_MUTES)
	{
		m_aMutes[m_NumMutes].m_Addr = *pAddr;
		m_aMutes[m_NumMutes].m_Expire = Server()->Tick()
						+ Secs * Server()->TickSpeed();
		str_copy(m_aMutes[m_NumMutes].m_aReason, pReason, sizeof(m_aMutes[m_NumMutes].m_aReason));
		m_NumMutes++;
		return true;
	}
	// no free slot found
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", "mute array is full");
	return false;
}

void CGameContext::Mute(const NETADDR *pAddr, int Secs, const char *pDisplayName, const char *pReason)
{
	if (!TryMute(pAddr, Secs, pReason))
		return;

	if(!pDisplayName)
		return;

	char aBuf[128];
	if (pReason[0])
		str_format(aBuf, sizeof aBuf, "'%s' has been muted for %d seconds (%s)", pDisplayName, Secs, pReason);
	else
		str_format(aBuf, sizeof aBuf, "'%s' has been muted for %d seconds", pDisplayName, Secs);
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
			"Use either 'muteid <client_id> <seconds> <reason>' or 'muteip <ip> <seconds> <reason>'");
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

	const char *pReason = pResult->NumArguments() > 2 ? pResult->GetString(2) : "";
	pSelf->Mute(&Addr, clamp(pResult->GetInteger(1), 1, 86400),
			pSelf->Server()->ClientName(Victim), pReason);
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

	const char *pReason = pResult->NumArguments() > 2 ? pResult->GetString(2) : "";
	pSelf->Mute(&Addr, clamp(pResult->GetInteger(1), 1, 86400), NULL, pReason);
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
		str_format(aBuf, sizeof aBuf, "%d: \"%s\", %d seconds left (%s)", i, aIpBuf,
				(pSelf->m_aMutes[i].m_Expire - pSelf->Server()->Tick()) / pSelf->Server()->TickSpeed(), pSelf->m_aMutes[i].m_aReason);
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

	CCharacter* pChr = pSelf->GetPlayerChar(Target);

	if((pController->m_Teams.m_Core.Team(Target) && pController->m_Teams.GetDDRaceState(pSelf->m_apPlayers[Target]) == DDRACE_STARTED) || (pChr && pController->m_Teams.IsPractice(pChr->Team())))
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

void CGameContext::ConPortalRifle(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_PORTAL_RIFLE, false);
}

void CGameContext::ConUnPortalRifle(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_PORTAL_RIFLE, true);
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

void CGameContext::ConDrawEditor(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_DRAW_EDITOR, false);
}

void CGameContext::ConUnDrawEditor(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_DRAW_EDITOR, true);
}

void CGameContext::ConSetJumps(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (pChr)
		pChr->SetCoreJumps(pResult->NumArguments() > 1 ? pResult->GetInteger(1) : 2);
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
	if (pPlayer) pSelf->SendChat(Victim, pPlayer->m_LocalChat ? CHAT_LOCAL : CHAT_ALL, -1, pResult->GetString(1));
}

void CGameContext::ConPlayerInfo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	int ID = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ID);
	CPlayer *pPlayer = pSelf->m_apPlayers[ID];

	// first player vars
	if (!pPlayer)
		return;

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "==== [PLAYER INFO] '%s' ====", pSelf->Server()->ClientName(ID));
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	if (pSelf->Server()->GetAuthedState(ID))
	{
		str_format(aBuf, sizeof(aBuf), "Authed: %d", pSelf->Server()->GetAuthedState(ID));
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}
	str_format(aBuf, sizeof(aBuf), "ClientID: %d", ID);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	if (pChr)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Status: Ingame");
	else if (pPlayer->GetTeam() == TEAM_SPECTATORS)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Status: Spectator");
	else
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Status: Dead");
	if (pPlayer->GetAccID() >= ACC_START)
	{
		str_format(aBuf, sizeof(aBuf), "Account Name: %s", pSelf->m_Accounts[pPlayer->GetAccID()].m_Username);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}
	else
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Account: Not logged in");
	if (pPlayer->m_InfRainbow)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Infinite Rainbow: True");
	if (pPlayer->m_InfMeteors > 0)
	{
		str_format(aBuf, sizeof(aBuf), "Infinite Meteors: %d", pPlayer->m_InfMeteors);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}
	if (pPlayer->m_Gamemode == GAMEMODE_DDRACE)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Mode: DDrace");
	else if (pPlayer->m_Gamemode == GAMEMODE_VANILLA)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Mode: Vanilla");
	if (pPlayer->m_SpookyGhost)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Spooky Ghost: True");
	if (pPlayer->m_pControlledTee)
	{
		str_format(aBuf, sizeof(aBuf), "Tee Control: %s", pSelf->Server()->ClientName(pPlayer->m_pControlledTee->GetCID()));
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}
	else if (pPlayer->m_HasTeeControl)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Tee Control: True");

	// then character vars
	if (!pChr)
		return;

	if (pChr->HasFlag() == TEAM_RED)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Flag: Red");
	if (pChr->HasFlag() == TEAM_BLUE)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Flag: Blue");
	if (pChr->m_DeepFreeze)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Frozen: Deep");
	else if (pChr->m_IsFrozen)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Frozen: True");
	else if (pChr->m_FreezeTime)
	{
		str_format(aBuf, sizeof(aBuf), "Frozen: Freezetime: %d", pChr->m_FreezeTime);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}
	if (pChr->m_SuperJump)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "SuperJump: True");
	if (pChr->m_EndlessHook)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Endless: True");
	if (pChr->m_Jetpack)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Jetpack: True");
	if (pChr->m_Rainbow)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Rainbow: True");
	if (pChr->m_Atom)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Atom: True");
	if (pChr->m_Trail)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Trail: True");
	if (pChr->m_Bloody)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Bloody: True");
	if (pChr->m_StrongBloody)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Strong Bloody: True");
	if (pChr->m_Meteors > 0)
	{
		str_format(aBuf, sizeof(aBuf), "Meteors: %d", pChr->m_Meteors);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}
	if (pChr->m_Passive)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Passive Mode: True");
	if (pChr->m_PoliceHelper)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Police Helper: True");
	for (int i = 0; i < NUM_WEAPONS; i++)
	{
		if (pChr->m_aSpreadWeapon[i])
		{
			str_format(aBuf, sizeof(aBuf), "Spread %s: True", pSelf->GetWeaponName(i));
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
		}
	}
	if (pChr->m_Invisible)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Invisibility: True");
	if (pChr->m_HookPower != HOOK_NORMAL)
	{
		str_format(aBuf, sizeof(aBuf), "Hook Power: %s", pSelf->GetExtraName(pChr->m_HookPower));
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}
	if (pChr->m_Item != -3)
	{
		str_format(aBuf, sizeof(aBuf), "Item: %s", pSelf->GetWeaponName(pChr->m_Item));
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}
	if (pChr->m_DoorHammer)
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Door Hammer: True");
	str_format(aBuf, sizeof(aBuf), "Position: (%.2f/%.2f)", pChr->GetPos().x / 32, pChr->GetPos().y / 32);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
}

void CGameContext::ConListRcon(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char zerochar = 0;
	if(pResult->NumArguments() > 0)
		pSelf->List(-1, pResult->GetString(0));
	else
		pSelf->List(-1, &zerochar);
}

void CGameContext::ConLaserText(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (pChr) pSelf->CreateLaserText(pChr->GetPos(), Victim, pResult->GetString(1));
}

void CGameContext::ConSendMotd(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->GetVictim();
	if (!pSelf->m_apPlayers[Victim])
		return;
	const char *pText = pResult->GetString(2);
	if (pResult->GetInteger(1) == 1)
		pText = pSelf->FormatMotd(pText);
	pSelf->SendMotd(pText, Victim);
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
			pPlayer->SetDummyMode(pResult->GetInteger(1));
		else if (pResult->NumArguments() == 1)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "Dummymode of '%s': [%d]", pSelf->Server()->ClientName(Victim), pPlayer->GetDummyMode());
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
		}
	}
	else
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "~~~ Dummymodes ~~~");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "[0] Idle");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "[-6] BlmapV3 1o1");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "[23] ChillBlock5 Racer");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "[29] ChillBlock5 Blocker");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "[31] ChillBlock5 Police");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "[32] BlmapChill Police");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "[99] Shop Bot");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "[98] Plot Shop Bot");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "[97] Bank Bot");
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
		CTeeInfo Info(pResult->GetString(1));

		CPlayer* pFrom = pSelf->m_apPlayers[pResult->GetInteger(1)];
		char aInteger[4];
		str_format(aInteger, sizeof(aInteger), "%d", pResult->GetInteger(1));

		if (pFrom && Info.m_SkinID == -1 && !str_comp_nocase(aInteger, pResult->GetString(1)))
			pSelf->SendSkinChange(pFrom->m_TeeInfos, Victim, -1);
		else
			pPlayer->SetSkin(Info.m_SkinID, true);
	}
	else
		pPlayer->ResetSkin(true);
}

void CGameContext::ConAccLogoutPort(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->m_LogoutAccountsPort = pResult->GetInteger(0);
	pSelf->Storage()->ListDirectory(IStorage::TYPE_ALL, pSelf->Config()->m_SvAccFilePath, InitAccounts, pSelf);
}

void CGameContext::ConAccLogout(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;

	int ID = pSelf->GetAccount(pResult->GetString(0));
	if (ID < ACC_START)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Invalid account");
		return;
	}

	if (!pSelf->m_Accounts[ID].m_LoggedIn)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "This account is not marked as logged in");
		pSelf->FreeAccount(ID);
		return;
	}

	if (pSelf->m_Accounts[ID].m_ClientID >= 0)
		pSelf->SendChatTarget(pSelf->m_Accounts[ID].m_ClientID, "You have been logged out by an admin");

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "Logged out account '%s' (%s)", pSelf->m_Accounts[ID].m_Username, pSelf->m_Accounts[ID].m_ClientID >= 0 ? pSelf->Server()->ClientName(pSelf->m_Accounts[ID].m_ClientID) : "player not online");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);

	pSelf->Logout(ID);
}

void CGameContext::ConAccDisable(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;

	int ID = pSelf->GetAccount(pResult->GetString(0));
	if (ID < ACC_START)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Invalid account");
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

void CGameContext::ConAccInfo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	int ID = pSelf->GetAccount(pResult->GetString(0));
	if (ID < ACC_START)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Invalid account");
		return;
	}

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "==== [ACCOUNT INFO] '%s' ====", pResult->GetString(0));
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);

	for (int i = 0; i < NUM_ACCOUNT_VARIABLES; i++)
	{
		const char *pValue = pSelf->GetAccVarValue(ID, i);
		char aDate[64] = "";

		if (i == ACC_EXPIRE_DATE_VIP || i == ACC_EXPIRE_DATE_PORTAL_RIFLE)
		{
			if (str_comp_nocase(pValue, "0") != 0)
				str_format(aDate, sizeof(aDate), " (%s)", pSelf->GetDate(str_toint(pValue)));
		}

		str_format(aBuf, sizeof(aBuf), "%s: %s%s", pSelf->GetAccVarName(i), pValue, aDate);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}

	if (!pSelf->m_Accounts[ID].m_LoggedIn)
		pSelf->FreeAccount(ID);
}

void CGameContext::ConAccAddEuros(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	
	int ID = pSelf->GetAccount(pResult->GetString(0));
	if (ID < ACC_START)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Invalid account");
		return;
	}

	int Euros = pResult->GetInteger(1);
	char aBuf[256];
	pSelf->m_Accounts[ID].m_Euros += Euros;

	if (pSelf->m_Accounts[ID].m_ClientID >= 0)
	{
		str_format(aBuf, sizeof(aBuf), "You %s %d euros", Euros >= 0 ? "got" : "lost", Euros);
		pSelf->SendChatTarget(pSelf->m_Accounts[ID].m_ClientID, aBuf);
	}

	str_format(aBuf, sizeof(aBuf), "%d euros given to account '%s'", Euros, pSelf->m_Accounts[ID].m_Username);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);

	pSelf->WriteDonationFile(TYPE_DONATION, Euros, ID, "");

	if (pSelf->m_Accounts[ID].m_LoggedIn)
		pSelf->WriteAccountStats(ID);
	else
		pSelf->Logout(ID);
}

void CGameContext::ConAccEdit(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	
	int ID = pSelf->GetAccount(pResult->GetString(0));
	if (ID < ACC_START)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Invalid account");
		return;
	}

	const char *pVariable = pResult->GetString(1);
	int VariableID = -1;
	for (int i = 0; i < NUM_ACCOUNT_VARIABLES; i++)
	{
		if (!str_comp_nocase(pVariable, pSelf->GetAccVarName(i)))
		{
			VariableID = i;
			break;
		}
	}

	if (VariableID == -1)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Invalid variable");
		return;
	}

	char aBuf[128];
	if (pResult->NumArguments() <= 2 || VariableID == ACC_USERNAME)
	{
		str_format(aBuf, sizeof(aBuf), "Value: %s", pSelf->GetAccVarValue(ID, VariableID));
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}
	else
	{
		const char *pValue = pResult->GetString(2);

		if (VariableID == ACC_EXPIRE_DATE_VIP || VariableID == ACC_EXPIRE_DATE_PORTAL_RIFLE)
		{
			time_t ExpireDate = 0;
			pSelf->SetExpireDate(&ExpireDate, pResult->GetFloat(2));

			char aTime[64];
			str_format(aTime, sizeof(aTime), "%d", (int)ExpireDate);
			pValue = aTime;
		}

		str_format(aBuf, sizeof(aBuf), "Changed %s for %s from %s to %s", pSelf->GetAccVarName(VariableID), pSelf->m_Accounts[ID].m_Username, pSelf->GetAccVarValue(ID, VariableID), pValue);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
		pSelf->SetAccVar(ID, VariableID, pValue);
	}

	if (pSelf->m_Accounts[ID].m_LoggedIn)
		pSelf->WriteAccountStats(ID);
	else
		pSelf->Logout(ID);
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

void CGameContext::ConSetMinigame(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->GetVictim();
	const char *pMinigame = pResult->GetString(1);
	if (!pSelf->m_apPlayers[Victim])
		return;

	for (int i = 0; i < NUM_MINIGAMES; i++)
	{
		if (str_comp_nocase(pMinigame, pSelf->GetMinigameCommand(i)) == 0)
		{
			if (i == pSelf->m_apPlayers[Victim]->m_Minigame)
				break;

			// dont set minigame to none before setting the other one if we want to set it to none anyways
			if (i != MINIGAME_NONE && pSelf->m_apPlayers[Victim]->m_Minigame != MINIGAME_NONE)
				pSelf->SetMinigame(Victim, MINIGAME_NONE, true);

			pSelf->SetMinigame(Victim, i, true);
			break;
		}
	}
}

void CGameContext::ConToTelePlot(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->NumArguments() == 2 ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (!pChr)
		return;

	int PlotID = pResult->GetInteger(0);
	if (PlotID <= 0 || PlotID > pSelf->Collision()->m_NumPlots)
		return;

	pChr->Core()->m_Pos = pSelf->m_aPlots[PlotID].m_ToTele;
	pChr->SetPos(pSelf->m_aPlots[PlotID].m_ToTele);
	pChr->m_PrevPos = pSelf->m_aPlots[PlotID].m_ToTele;
	pChr->m_DDRaceState = DDRACE_CHEAT;
}

void CGameContext::ConClearPlot(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int PlotID = pResult->GetInteger(0);
	if (PlotID < 0 || PlotID > pSelf->Collision()->m_NumPlots)
		return;

	char aBuf[32];
	str_format(aBuf, sizeof(aBuf), "Cleared plot %d", PlotID);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "plot", aBuf);
	pSelf->ClearPlot(PlotID);
}

void CGameContext::ConPlotOwner(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;

	int NewID = pSelf->GetAccount(pResult->GetString(0));
	if (NewID < ACC_START)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Invalid account");
		return;
	}

	int PlotID = pResult->GetInteger(1);
	if (PlotID <= 0 || PlotID > pSelf->Collision()->m_NumPlots)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Invalid plot id");
		return;
	}

	if (pSelf->GetPlotID(NewID) != 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "This account owns a plot already");
		return;
	}

	int OldID = pSelf->GetAccIDByUsername(pSelf->m_aPlots[PlotID].m_aOwner);
	if (pSelf->m_Accounts[OldID].m_ClientID >= 0)
		pSelf->SendChatTarget(pSelf->m_Accounts[OldID].m_ClientID, "You lost your plot");

	char aBuf[128];
	if (pSelf->m_Accounts[NewID].m_ClientID >= 0)
	{
		str_format(aBuf, sizeof(aBuf), "You are now owner of plot %d", PlotID);
		pSelf->SendChatTarget(pSelf->m_Accounts[NewID].m_ClientID, aBuf);
	}

	if (pSelf->m_aPlots[PlotID].m_ExpireDate == 0)
		pSelf->SetPlotExpire(PlotID);
	pSelf->SetPlotInfo(PlotID, NewID);

	str_format(aBuf, sizeof(aBuf), "Changed owner of plot %d from '%s' to '%s'", PlotID, pSelf->m_Accounts[OldID].m_Username, pSelf->m_Accounts[NewID].m_Username);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
}

void CGameContext::ConPlotInfo(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int PlotID = pResult->GetInteger(0);
	if (PlotID <= 0 || PlotID > pSelf->Collision()->m_NumPlots)
		return;

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "Owner account: %s", pSelf->m_aPlots[PlotID].m_aOwner);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "plot", aBuf);
	str_format(aBuf, sizeof(aBuf), "Size: %s", pSelf->GetPlotSizeString(PlotID));
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "plot", aBuf);
	str_format(aBuf, sizeof(aBuf), "Expire date: %s", pSelf->m_aPlots[PlotID].m_ExpireDate == 0 ? "" : pSelf->GetDate(pSelf->m_aPlots[PlotID].m_ExpireDate));
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "plot", aBuf);
}
