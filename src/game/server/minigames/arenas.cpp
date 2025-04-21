// made by fokkonaut

#include "arenas.h"
#include <game/server/gamecontext.h>
#include <game/server/teams.h>
#include <engine/shared/config.h>
#include <game/server/gamemodes/DDRace.h>

CArenas::CArenas(CGameContext *pGameServer, int Type) : CMinigame(pGameServer, Type)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
		Reset(i);
	for (int i = 0; i < MAX_FIGHTS; i++)
		m_aFights[i].Reset();
	m_GlobalArena.Reset();

	for (int i = 0; i < 4; i++)
	{
		m_IDs.m_aBorder[i] = Server()->SnapNewID();
		m_IDs.m_aWeaponBox[i] = Server()->SnapNewID();
	}
	for (int i = 0; i < 2; i++)
		m_IDs.m_aSpawn[i] = Server()->SnapNewID();
	m_IDs.m_SelectedWeapon = Server()->SnapNewID();
	m_IDs.m_WeaponActivated = Server()->SnapNewID();
}

CArenas::~CArenas()
{
	for (int i = 0; i < 4; i++)
	{
		Server()->SnapFreeID(m_IDs.m_aBorder[i]);
		Server()->SnapFreeID(m_IDs.m_aWeaponBox[i]);
	}
	for (int i = 0; i < 2; i++)
		Server()->SnapFreeID(m_IDs.m_aSpawn[i]);
	Server()->SnapFreeID(m_IDs.m_SelectedWeapon);
	Server()->SnapFreeID(m_IDs.m_WeaponActivated);
}

void CArenas::Reset(int ClientID)
{
	m_aState[ClientID] = STATE_1VS1_NONE;
	m_aInFight[ClientID] = false;
	m_aLastDirection[ClientID] = 0;
	m_aLastJump[ClientID] = 0;
	m_aSelectedWeapon[ClientID] = -1;
	m_aFirstGroundedFreezeTick[ClientID] = 0;
}

int CArenas::GetFreeArena()
{
	for (int i = 0; i < MAX_FIGHTS; i++)
		if (!m_aFights[i].m_Active)
			return i;
	return -1;
}

bool CArenas::IsInArena(int Fight, vec2 Pos)
{
	return (Pos.x >= m_aFights[Fight].m_aCorners[POINT_TOP_LEFT].x
		&& Pos.x <= m_aFights[Fight].m_aCorners[POINT_BOTTOM_RIGHT].x
		&& Pos.y >= m_aFights[Fight].m_aCorners[POINT_TOP_LEFT].y
		&& Pos.y <= m_aFights[Fight].m_aCorners[POINT_BOTTOM_RIGHT].y);
}

vec2 CArenas::GetShowDistance(int ClientID)
{
	vec2 ShowDistance = GameServer()->m_apPlayers[ClientID]->m_ShowDistance;
	ShowDistance.x = clamp(ShowDistance.x*0.75f, 500.f, 8000.f);
	ShowDistance.y = clamp(ShowDistance.y*0.8f, 300.f, 4500.f);
	return ShowDistance;
}

bool CArenas::IsGrounded(CCharacter *pChr)
{
	if (!pChr)
		return false;
	int Fight = GetClientFight(pChr->GetPlayer()->GetCID());
	return pChr->IsGrounded(true) || GameServer()->Collision()->IsFightBorder(vec2(pChr->GetPos().x, pChr->GetPos().y), Fight)
		|| GameServer()->Collision()->IsFightBorder(vec2(pChr->GetPos().x, pChr->GetPos().y + pChr->GetProximityRadius() + 4), Fight);
}

int CArenas::GetClientFight(int ClientID, bool HasToBeJoined)
{
	if (ClientID == -1)
		return -1;

	for (int f = 0; f < MAX_FIGHTS; f++)
		if (m_aFights[f].m_Active)
			for (int i = 0; i < 2; i++)
				if (m_aFights[f].m_aParticipants[i].m_ClientID == ClientID && (!HasToBeJoined || HasJoined(f, i)))
					return f;
	return -1;
}

int CArenas::GetScoreLimit(int ClientID)
{
	int Fight = GetClientFight(ClientID);
	if (Fight < 0)
		return 0;
	return m_aFights[Fight].m_ScoreLimit;
}

int CArenas::GetClientScore(int ClientID)
{
	int Fight = GetClientFight(ClientID);
	if (Fight < 0)
		return 0;

	int Index = m_aFights[Fight].m_aParticipants[0].m_ClientID == ClientID ? 0 : 1;
	return m_aFights[Fight].m_aParticipants[Index].m_Score;
}

vec2 CArenas::GetSpawnPos(int ClientID)
{
	int Fight = GetClientFight(ClientID);
	if (Fight < 0)
		return vec2(-1, -1);

	int Index = m_aFights[Fight].m_aParticipants[0].m_ClientID == ClientID ? 0 : 1;
	return m_aFights[Fight].m_aSpawns[Index];
}

void CArenas::StartConfiguration(int ClientID, int Participant, int ScoreLimit, bool KillBorder)
{
	if (ClientID == Participant)
	{
		GameServer()->SendChatTarget(ClientID, GameServer()->m_apPlayers[ClientID]->Localize("You can't fight against yourself"));
		return;
	}

	if (Participant < 0 && Participant != PARTICIPANT_GLOBAL)
	{
		GameServer()->SendChatTarget(ClientID, GameServer()->m_apPlayers[ClientID]->Localize("Invalid participant"));
		return;
	}

	if (GetClientFight(ClientID) != -1)
	{
		GameServer()->SendChatTarget(ClientID, GameServer()->m_apPlayers[ClientID]->Localize("Leave the current fight in order to join another one"));
		return;
	}

	int FreeArena = GetFreeArena();
	if (FreeArena == -1)
	{
		GameServer()->SendChatTarget(ClientID, GameServer()->m_apPlayers[ClientID]->Localize("Too many 1vs1 arenas, try again later"));
		return;
	}

	m_aFights[FreeArena].m_Active = true;
	m_aFights[FreeArena].m_ScoreLimit = clamp(ScoreLimit, 0, 100);
	m_aFights[FreeArena].m_KillBorder = KillBorder;

	for (int i = 0; i < 2; i++)
	{
		m_aFights[FreeArena].m_aParticipants[i].m_ClientID = i == 0 ? ClientID : Participant;
		m_aFights[FreeArena].m_aParticipants[i].m_Status = i == 0 ? PARTICIPANT_OWNER : PARTICIPANT_WAITING;
		m_aFights[FreeArena].m_aParticipants[i].m_Score = 0;

		m_aFights[FreeArena].m_aSpawns[i] = vec2(-1, -1);
	}

	GameServer()->m_apPlayers[ClientID]->SetPlaying();
	m_aState[ClientID] = STATE_1VS1_PLACE_ARENA;
	GameServer()->SendTeamChange(ClientID, TEAM_SPECTATORS, true, Server()->Tick(), ClientID);
}

void CArenas::PlaceArena(int ClientID)
{
	int Fight = GetClientFight(ClientID);
	if (Fight < 0)
		return;

	m_aState[ClientID] = STATE_1VS1_PLACE_FIRST_SPAWN;
	SetArenaCollision(Fight, false);
}

void CArenas::PlaceSpawn(int ClientID)
{
	int Fight = GetClientFight(ClientID);
	if (Fight < 0)
		return;

	int Index = m_aState[ClientID] == STATE_1VS1_PLACE_FIRST_SPAWN ? 0 : 1;
	if (!ValidSpawnPos(m_aFights[Fight].m_aSpawns[Index]))
		return;

	m_aState[ClientID]++;
	GameServer()->CreateDeath(m_aFights[Fight].m_aSpawns[Index], ClientID, CmaskOne(ClientID));
}

void CArenas::SelectWeapon(int ClientID)
{
	int Fight = GetClientFight(ClientID);
	switch (m_aSelectedWeapon[ClientID])
	{
	case -1: FinishConfiguration(Fight, ClientID); break;
	case 0: m_aFights[Fight].m_Weapons.m_Hammer ^= 1; break;
	case 1: m_aFights[Fight].m_Weapons.m_Shotgun ^= 1; break;
	case 2: m_aFights[Fight].m_Weapons.m_Grenade ^= 1; break;
	case 3: m_aFights[Fight].m_Weapons.m_Laser ^= 1; break;
	}
}

void CArenas::FinishConfiguration(int Fight, int ClientID)
{
	m_aState[ClientID] = STATE_1VS1_DONE;
	GameServer()->SendBroadcast("", ClientID, false);
	GameServer()->SendTeamChange(ClientID, GameServer()->m_apPlayers[ClientID]->GetTeam(), true, Server()->Tick(), ClientID);

	int Invited = m_aFights[Fight].m_aParticipants[1].m_ClientID;

	if (Invited == PARTICIPANT_GLOBAL)
	{
		m_GlobalArena.m_Active = true;
		mem_copy(m_GlobalArena.m_aCorners, m_aFights[Fight].m_aCorners, sizeof(m_GlobalArena.m_aCorners));
		mem_copy(m_GlobalArena.m_aSpawns, m_aFights[Fight].m_aSpawns, sizeof(m_GlobalArena.m_aSpawns));
		m_GlobalArena.m_MiddlePos = m_aFights[Fight].m_MiddlePos;
		m_GlobalArena.m_ScoreLimit = m_aFights[Fight].m_ScoreLimit;
		m_GlobalArena.m_KillBorder = m_aFights[Fight].m_KillBorder;
		m_GlobalArena.m_Weapons.m_Hammer = m_aFights[Fight].m_Weapons.m_Hammer;
		m_GlobalArena.m_Weapons.m_Shotgun = m_aFights[Fight].m_Weapons.m_Shotgun;
		m_GlobalArena.m_Weapons.m_Grenade = m_aFights[Fight].m_Weapons.m_Grenade;
		m_GlobalArena.m_Weapons.m_Laser = m_aFights[Fight].m_Weapons.m_Laser;

		EndFight(Fight);
		GameServer()->SendChatTarget(ClientID, GameServer()->m_apPlayers[ClientID]->Localize("You successfully created a new global arena"));
	}
	else
	{
		m_aFights[Fight].m_aParticipants[1].m_Status = PARTICIPANT_INVITED;

		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), GameServer()->m_apPlayers[ClientID]->Localize("You invited '%s' to a fight"), Server()->ClientName(Invited));
		GameServer()->SendChatTarget(ClientID, aBuf);

		str_format(aBuf, sizeof(aBuf), GameServer()->m_apPlayers[Invited]->Localize("You have been invited to a fight by '%s', type '/1vs1 %s' to join"), Server()->ClientName(ClientID), Server()->ClientName(ClientID));
		GameServer()->SendChatTarget(Invited, aBuf);

		if (GameServer()->m_apPlayers[Invited] && GameServer()->m_apPlayers[Invited]->m_Minigame != MINIGAME_1VS1)
			GameServer()->SendChatTarget(Invited, GameServer()->m_apPlayers[Invited]->Localize("Join the 1vs1 lobby using '/1vs1' before you accept the fight"));
	}
}

void CArenas::OnInput(int ClientID, CNetObj_PlayerInput *pNewInput)
{
	if (pNewInput->m_Jump && m_aLastJump[ClientID] == 0)
	{
		switch (m_aState[ClientID])
		{
		case STATE_1VS1_PLACE_ARENA: PlaceArena(ClientID); break;
		case STATE_1VS1_PLACE_FIRST_SPAWN: PlaceSpawn(ClientID); break;
		case STATE_1VS1_PLACE_SECOND_SPAWN: PlaceSpawn(ClientID); break;
		case STATE_1VS1_SELECT_WEAPONS: SelectWeapon(ClientID); break;
		}
	}

	if (pNewInput->m_Direction != 0 && m_aLastDirection[ClientID] == 0)
	{
		if (m_aState[ClientID] == STATE_1VS1_SELECT_WEAPONS)
		{
			m_aSelectedWeapon[ClientID] += pNewInput->m_Direction;
			if (m_aSelectedWeapon[ClientID] > 3)
				m_aSelectedWeapon[ClientID] = -1;
			if (m_aSelectedWeapon[ClientID] < -1)
				m_aSelectedWeapon[ClientID] = 3;
		}
	}

	m_aLastJump[ClientID] = pNewInput->m_Jump;
	m_aLastDirection[ClientID] = pNewInput->m_Direction;
}

bool CArenas::ValidSpawnPos(vec2 Pos)
{
	int TileIndex = GameServer()->Collision()->GetTileRaw(Pos);
	int TileFIndex = GameServer()->Collision()->GetFTileRaw(Pos);
	int MapIndex = GameServer()->Collision()->GetPureMapIndex(Pos);
	bool IsTeleporter = GameServer()->Collision()->IsTeleport(MapIndex) || GameServer()->Collision()->IsEvilTeleport(MapIndex)
		|| GameServer()->Collision()->IsCheckTeleport(MapIndex) || GameServer()->Collision()->IsCheckEvilTeleport(MapIndex);

	if (GameServer()->Collision()->IsSolid(Pos.x, Pos.y) || TileIndex == TILE_FREEZE || TileFIndex == TILE_FREEZE
		|| TileIndex == TILE_DEATH || TileFIndex == TILE_DEATH || IsTeleporter)
		return false;
	return true;
}

void CArenas::UpdateSnapPositions(int ClientID)
{
	int Fight = GetClientFight(ClientID);
	if (Fight < 0)
		return;

	vec2 Pos = GameServer()->m_apPlayers[ClientID]->m_ViewPos;

	if (m_aState[ClientID] == STATE_1VS1_PLACE_FIRST_SPAWN || m_aState[ClientID] == STATE_1VS1_PLACE_SECOND_SPAWN)
	{
		vec2 TempPos = Pos;
		bool BorderBelow = false;
		do
		{
			if (TempPos.y > GameServer()->Collision()->GetHeight() * 32 || !ValidSpawnPos(TempPos))
				return;

			if (m_aFights[Fight].m_KillBorder)
			{
				if (GameServer()->Collision()->IsFightBorder(TempPos, Fight))
					return;
			}
			else
			{
				BorderBelow = GameServer()->Collision()->IsFightBorder(vec2(TempPos.x, TempPos.y + CCharacterCore::PHYS_SIZE + 4), Fight);
			}

			TempPos.y += 32.f;
		} while (!GameServer()->Collision()->IsSolid(TempPos.x, TempPos.y) && !BorderBelow);

		int Index = m_aState[ClientID] == STATE_1VS1_PLACE_FIRST_SPAWN ? 0 : 1;
		m_aFights[Fight].m_aSpawns[Index] = GameServer()->RoundPos(Pos);
	}
	else if (m_aState[ClientID] == STATE_1VS1_PLACE_ARENA)
	{
		m_aFights[Fight].m_MiddlePos = Pos;
		vec2 ShowDistance = GetShowDistance(ClientID);

		// TODO: fix floating point
		vec2 CornerPos;
		CornerPos = vec2(Pos.x - (ShowDistance.x / 2), Pos.y - (ShowDistance.y / 2));
		m_aFights[Fight].m_aCorners[POINT_TOP_LEFT] = GameServer()->RoundPos(CornerPos);
		CornerPos = vec2(Pos.x + (ShowDistance.x / 2), Pos.y - (ShowDistance.y / 2));
		m_aFights[Fight].m_aCorners[POINT_TOP_RIGHT] = GameServer()->RoundPos(CornerPos);
		CornerPos = vec2(Pos.x + (ShowDistance.x / 2), Pos.y + (ShowDistance.y / 2));
		m_aFights[Fight].m_aCorners[POINT_BOTTOM_RIGHT] = GameServer()->RoundPos(CornerPos);
		CornerPos = vec2(Pos.x - (ShowDistance.x / 2), Pos.y + (ShowDistance.y / 2));
		m_aFights[Fight].m_aCorners[POINT_BOTTOM_LEFT] = GameServer()->RoundPos(CornerPos);
	}
}

bool CArenas::ClampViewPos(int ClientID)
{
	if (m_aState[ClientID] == STATE_1VS1_DONE)
		return false;

	int Fight = GetClientFight(ClientID);
	if (m_aState[ClientID] == STATE_1VS1_SELECT_WEAPONS)
	{
		GameServer()->m_apPlayers[ClientID]->m_ViewPos = m_aFights[Fight].m_MiddlePos;
		return true;
	}

	vec2 TopLeft;
	vec2 BottomRight;
	if (m_aState[ClientID] == STATE_1VS1_PLACE_FIRST_SPAWN || m_aState[ClientID] == STATE_1VS1_PLACE_SECOND_SPAWN)
	{
		TopLeft = m_aFights[Fight].m_aCorners[POINT_TOP_LEFT];
		BottomRight = m_aFights[Fight].m_aCorners[POINT_BOTTOM_RIGHT];
	}
	else if (m_aState[ClientID] == STATE_1VS1_PLACE_ARENA)
	{
		vec2 ShowDistance = GetShowDistance(ClientID);
		ShowDistance = vec2(ShowDistance.x/2+32, ShowDistance.y/2+32);
		TopLeft = GameServer()->RoundPos(ShowDistance);
		BottomRight = GameServer()->RoundPos(vec2(GameServer()->Collision()->GetWidth()*32 - ShowDistance.x, GameServer()->Collision()->GetHeight()*32 - ShowDistance.y));
	}
	else
		return false;
	
	vec2 *pViewPos = &GameServer()->m_apPlayers[ClientID]->m_ViewPos;
	bool Clamp = false;

	if (pViewPos->x < TopLeft.x + 16)
	{
		pViewPos->x = TopLeft.x + 24;
		Clamp = true;
	}
	if (pViewPos->y < TopLeft.y + 16)
	{
		pViewPos->y = TopLeft.y + 24;
		Clamp = true;
	}
	if (pViewPos->x > BottomRight.x - 16)
	{
		pViewPos->x = BottomRight.x - 24;
		Clamp = true;
	}
	if (pViewPos->y > BottomRight.y - 16)
	{
		pViewPos->y = BottomRight.y - 24;
		Clamp = true;
	}

	return Clamp;
}

bool CArenas::AcceptFight(int Creator, int ClientID)
{
	int Fight = GetClientFight(Creator);
	if (Fight < 0)
		return false;

	int OwnFight = GetClientFight(ClientID);
	if (OwnFight >= 0)
	{
		if (FightStarted(ClientID))
			return false;
		else
			EndFight(OwnFight);
	}

	CFight *pFight = &m_aFights[Fight];
	bool Found = false;
	for (int i = 0; i < 2; i++)
	{
		if (pFight->m_aParticipants[i].m_ClientID == ClientID && pFight->m_aParticipants[i].m_Status == PARTICIPANT_INVITED)
		{
			pFight->m_aParticipants[i].m_Status = PARTICIPANT_ACCEPTED;
			Found = true;
			break;
		}
	}

	if (!Found)
		return false;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), GameServer()->m_apPlayers[ClientID]->Localize("You have accepted the invite by '%s'"), Server()->ClientName(Creator));
	GameServer()->SendChatTarget(ClientID, aBuf);

	str_format(aBuf, sizeof(aBuf), GameServer()->m_apPlayers[Creator]->Localize("'%s' has accepted your invite"), Server()->ClientName(ClientID));
	GameServer()->SendChatTarget(Creator, aBuf);

	StartFight(Fight);
	return true;
}

void CArenas::EndFight(int Fight)
{
	if (Fight < 0)
		return;

	if (m_aFights[Fight].m_aParticipants[1].m_ClientID != PARTICIPANT_GLOBAL)
		KillParticipants(Fight);
	SetArenaCollision(Fight, true);
	for (int i = 0; i < 2; i++)
	{
		int ClientID = m_aFights[Fight].m_aParticipants[i].m_ClientID;
		if (ClientID < 0 || !HasJoined(Fight, i))
			continue;

		if (IsConfiguring(ClientID))
		{
			GameServer()->SendBroadcast("", ClientID, false);
			GameServer()->SendTeamChange(ClientID, GameServer()->m_apPlayers[ClientID]->GetTeam(), true, Server()->Tick(), ClientID);
		}

		((CGameControllerDDRace *)GameServer()->m_pController)->m_Teams.SetForceCharacterTeam(ClientID, 0);
		GameServer()->m_pController->UpdateGameInfo(ClientID);
		Reset(ClientID);
	}
	m_aFights[Fight].Reset();
}

void CArenas::StartFight(int Fight)
{
	if (Fight < 0)
		return;

	CGameTeams *pTeams = &((CGameControllerDDRace *)GameServer()->m_pController)->m_Teams;
	int FirstFreeTeam = -1;
	for (int i = 1; i < MAX_CLIENTS; i++)
	{
		if (pTeams->Count(i) == 0)
		{
			FirstFreeTeam = i;
			break;
		}
	}

	if (FirstFreeTeam == -1)
	{
		EndFight(Fight);
		return;
	}

	KillParticipants(Fight);
	int aID[2] = { m_aFights[Fight].m_aParticipants[0].m_ClientID, m_aFights[Fight].m_aParticipants[1].m_ClientID };
	for (int i = 0; i < 2; i++)
	{
		m_aInFight[aID[i]] = true;
		((CGameControllerDDRace *)GameServer()->m_pController)->m_Teams.SetForceCharacterTeam(aID[i], FirstFreeTeam);
		GameServer()->m_pController->UpdateGameInfo(aID[i]);

		if (GameServer()->m_apPlayers[aID[i]])
			GameServer()->m_apPlayers[aID[i]]->SetPlaying();
	}
}

const char *CArenas::StartGlobalArenaFight(int ClientID1, int ClientID2)
{
	if (!GlobalArenaExists())
		return "There is no global arena, create one first";
	if (ClientID1 == ClientID2)
		return "Players cant fight against theirselves";
	if (ClientID1 < 0 || ClientID2 < 0 || !GameServer()->m_apPlayers[ClientID1] || !GameServer()->m_apPlayers[ClientID2])
		return "Both players have to be online";
	if (GameServer()->m_apPlayers[ClientID1]->m_Minigame != MINIGAME_1VS1 || GameServer()->m_apPlayers[ClientID2]->m_Minigame != MINIGAME_1VS1)
		return "Both players have to be in the 1vs1 minigame already";

	int FreeArena = GetFreeArena();
	if (FreeArena == -1)
		return "Too many 1vs1 arenas, try again later";

	// end current fights
	for (int i = 0; i < 2; i++)
		EndFight(GetClientFight(i == 0 ? ClientID1 : ClientID2));

	m_aFights[FreeArena].m_Active = true;
	mem_copy(m_aFights[FreeArena].m_aCorners, m_GlobalArena.m_aCorners, sizeof(m_aFights[FreeArena].m_aCorners));
	mem_copy(m_aFights[FreeArena].m_aSpawns, m_GlobalArena.m_aSpawns, sizeof(m_aFights[FreeArena].m_aSpawns));
	m_aFights[FreeArena].m_MiddlePos = m_GlobalArena.m_MiddlePos;
	m_aFights[FreeArena].m_ScoreLimit = m_GlobalArena.m_ScoreLimit;
	m_aFights[FreeArena].m_KillBorder = m_GlobalArena.m_KillBorder;
	m_aFights[FreeArena].m_Weapons.m_Hammer = m_GlobalArena.m_Weapons.m_Hammer;
	m_aFights[FreeArena].m_Weapons.m_Shotgun = m_GlobalArena.m_Weapons.m_Shotgun;
	m_aFights[FreeArena].m_Weapons.m_Grenade = m_GlobalArena.m_Weapons.m_Grenade;
	m_aFights[FreeArena].m_Weapons.m_Laser = m_GlobalArena.m_Weapons.m_Laser;

	for (int i = 0; i < 2; i++)
	{
		m_aFights[FreeArena].m_aParticipants[i].m_ClientID = i == 0 ? ClientID1 : ClientID2;
		m_aFights[FreeArena].m_aParticipants[i].m_Status = PARTICIPANT_ACCEPTED;
		m_aFights[FreeArena].m_aParticipants[i].m_Score = 0;
	}

	m_aFights[FreeArena].m_LongFreezeStart = true;
	StartFight(FreeArena);
	return "";
}

bool CArenas::CanSelfkill(int ClientID)
{
	int Fight = GetClientFight(ClientID);
	if (Fight < 0)
		return true;
	
	int Other = m_aFights[Fight].m_aParticipants[0].m_ClientID == ClientID ? 1 : 0;
	if (!HasJoined(Fight, Other))
		return true;

	CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
	int Seconds = m_aFights[Fight].m_LongFreezeStart ? 10 : 3;
	if (pChr->m_SpawnTick + Server()->TickSpeed() * Seconds > Server()->Tick()) // 3 sec freeze on arena join, dont kill there
		return false;

	int OtherID = m_aFights[Fight].m_aParticipants[Other].m_ClientID;
	CCharacter *pOther = GameServer()->GetPlayerChar(OtherID);
	if (pChr && pOther)
	{
		// only allow selfkill if no one touched the other one yet OR if the one who tries to kill is frozen and the other one is not, to speed up the killing process
		if ((pChr->Core()->m_Killer.m_ClientID != OtherID && pOther->Core()->m_Killer.m_ClientID != ClientID)
			|| (pChr->m_FreezeTime && !pOther->m_FreezeTime))
			return true;
	}
	return false;
}

void CArenas::KillParticipants(int Fight, int Killer)
{
	for (int i = 0; i < 2; i++)
	{
		if (!HasJoined(Fight, i))
			continue;

		int ClientID = m_aFights[Fight].m_aParticipants[i].m_ClientID;
		m_aFirstGroundedFreezeTick[ClientID] = 0;
		CCharacter *pChr = ClientID >= 0 ? GameServer()->GetPlayerChar(ClientID) : 0;
		if (pChr)
		{
			pChr->Die(i == Killer ? WEAPON_GAME : WEAPON_SELF, true, false); // WEAPON_SELF for the kill msg, so it will show the killer
			pChr->GetPlayer()->Respawn();
		}
	}
}

void CArenas::IncreaseScore(int Fight, int Index)
{
	if (!HasJoined(Fight, Index))
		return;

	int ClientID = m_aFights[Fight].m_aParticipants[Index].m_ClientID;
	int Other = Index == 0 ? 1 : 0;
	int OtherID = m_aFights[Fight].m_aParticipants[Other].m_ClientID;
	// dont count kill when we havent touched each other yet
	if (GameServer()->GetPlayerChar(OtherID) && GameServer()->GetPlayerChar(OtherID)->Core()->m_Killer.m_ClientID != ClientID)
		return;

	std::swap(m_aFights[Fight].m_aSpawns[0], m_aFights[Fight].m_aSpawns[1]); // swap spawn positions when we score

	if (GameServer()->GetPlayerChar(ClientID))
		GameServer()->CreateLaserText(m_aFights[Fight].m_aSpawns[Index], ClientID, "+1", 3);

	m_aFights[Fight].m_LongFreezeStart = false; // we only want the long freeze start on the initial round

	m_aFights[Fight].m_aParticipants[Index].m_Score++;
	if (m_aFights[Fight].m_aParticipants[Index].m_Score >= m_aFights[Fight].m_ScoreLimit)
	{
		// First, send to clients
		char aFormat[128];
		str_copy(aFormat, Localizable("'%s' won a 1vs1 round against '%s'! Final scores: %d - %d"), sizeof(aFormat));
		GameServer()->SendChatFormat(-1, CHAT_ALL, -1, CGameContext::CHATFLAG_ALL, aFormat, Server()->ClientName(ClientID), Server()->ClientName(OtherID),
			m_aFights[Fight].m_aParticipants[Index].m_Score, m_aFights[Fight].m_aParticipants[Other].m_Score);

		// Then fill untranslated format
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), aFormat, Server()->ClientName(ClientID), Server()->ClientName(OtherID),
			m_aFights[Fight].m_aParticipants[Index].m_Score, m_aFights[Fight].m_aParticipants[Other].m_Score);

		Server()->SendWebhookMessage(GameServer()->Config()->m_SvWebhook1vs1URL, aBuf, GameServer()->Config()->m_SvWebhook1vs1Name, GameServer()->Config()->m_SvWebhook1vs1AvatarURL);
		GameServer()->m_apPlayers[ClientID]->m_ConfettiWinEffectTick = Server()->Tick();

		EndFight(Fight);
	}
}

bool CArenas::OnCharacterSpawn(int ClientID)
{
	int Fight = GetClientFight(ClientID);
	if (Fight < 0 || !FightStarted(ClientID))
		return false;

	CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
	if (!pChr)
		return false;

	pChr->GiveWeapon(WEAPON_GUN);
	pChr->SetActiveWeapon(WEAPON_GUN);
	pChr->GiveWeapon(WEAPON_HAMMER, !m_aFights[Fight].m_Weapons.m_Hammer);
	pChr->GiveWeapon(WEAPON_SHOTGUN, !m_aFights[Fight].m_Weapons.m_Shotgun);
	pChr->GiveWeapon(WEAPON_GRENADE, !m_aFights[Fight].m_Weapons.m_Grenade);
	pChr->GiveWeapon(WEAPON_LASER, !m_aFights[Fight].m_Weapons.m_Laser);

	pChr->Freeze(m_aFights[Fight].m_LongFreezeStart ? 10 : 3);
	return true;
}

void CArenas::OnPlayerLeave(int ClientID, bool Disconnect)
{
	int Fight;
	while ((Fight = GetClientFight(ClientID, !Disconnect)) >= 0)
	{
		if (FightStarted(ClientID))
		{
			int Index = m_aFights[Fight].m_aParticipants[0].m_ClientID == ClientID ? 0 : 1;
			int Other = m_aFights[Fight].m_aParticipants[0].m_ClientID == ClientID ? 1 : 0;
			int OtherID = m_aFights[Fight].m_aParticipants[Other].m_ClientID;

			int FightScore = m_aFights[Fight].m_aParticipants[Index].m_Score;
			int OtherScore = m_aFights[Fight].m_aParticipants[Other].m_Score;

			// First, send to clients
			char aFormat[128];
			str_copy(aFormat, Localizable("'%s' left a 1vs1 round against '%s'! Current scores: %d - %d"), sizeof(aFormat));
			GameServer()->SendChatFormat(-1, CHAT_ALL, -1, CGameContext::CHATFLAG_ALL, aFormat, Server()->ClientName(ClientID), Server()->ClientName(OtherID),
				m_aFights[Fight].m_aParticipants[Index].m_Score, m_aFights[Fight].m_aParticipants[Other].m_Score);

			if(FightScore > 0 || OtherScore > 0)
			{
				// Then fill untranslated format
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), aFormat, Server()->ClientName(ClientID), Server()->ClientName(OtherID),
					m_aFights[Fight].m_aParticipants[Index].m_Score, m_aFights[Fight].m_aParticipants[Other].m_Score);
				Server()->SendWebhookMessage(GameServer()->Config()->m_SvWebhook1vs1URL, aBuf, GameServer()->Config()->m_SvWebhook1vs1Name, GameServer()->Config()->m_SvWebhook1vs1AvatarURL);
			}
			GameServer()->m_apPlayers[OtherID]->m_ConfettiWinEffectTick = Server()->Tick();
		}

		EndFight(Fight);
	}
}

void CArenas::OnPlayerDie(int ClientID)
{
	int Fight = GetClientFight(ClientID);
	if (Fight < 0)
		return;

	int Other = m_aFights[Fight].m_aParticipants[0].m_ClientID == ClientID ? 1 : 0;
	if (!HasJoined(Fight, Other))
		return;

	IncreaseScore(Fight, Other);

	CCharacter *pChr = GameServer()->GetPlayerChar(m_aFights[Fight].m_aParticipants[Other].m_ClientID);
	if (pChr)
	{
		pChr->Die(WEAPON_GAME, true, false);
		pChr->GetPlayer()->Respawn();
	}
}

void CArenas::Tick()
{
	for (int f = 0; f < MAX_FIGHTS; f++)
	{
		CFight *pFight = &m_aFights[f];
		if (!pFight->m_Active)
			continue;

		for (int i = 0; i < 2; i++)
		{
			int ClientID = pFight->m_aParticipants[i].m_ClientID;
			CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);

			if (HasJoined(f, i) && FightStarted(ClientID) && pChr)
			{
				int Other = i == 0 ? 1 : 0;
				if (!pChr->m_Super && !IsInArena(f, pChr->GetPos()))
				{
					IncreaseScore(f, Other);
					KillParticipants(f, Other);
					return;
				}

				if (pChr->GetPos() != pChr->m_PrevPos)
					m_aFirstGroundedFreezeTick[ClientID] = 0;

				if (!m_aFirstGroundedFreezeTick[ClientID])
				{
					if (pChr->m_IsFrozen && IsGrounded(pChr))
						m_aFirstGroundedFreezeTick[ClientID] = Server()->Tick();
				}
				else if (m_aFirstGroundedFreezeTick[ClientID] < Server()->Tick() - Server()->TickSpeed())
				{
					CCharacter *pOther = GameServer()->GetPlayerChar(pFight->m_aParticipants[Other].m_ClientID);
					if (pOther && !pOther->m_IsFrozen && (!pOther->m_FreezeTime || IsGrounded(pOther))) // draw -> no point
						IncreaseScore(f, Other);
					KillParticipants(f, Other);
					return;
				}
			}
			else if (IsConfiguring(ClientID) && Server()->Tick() % 50 == 0)
			{
				const char *pMsg = "";
				switch (m_aState[ClientID])
				{
				case STATE_1VS1_NONE: continue;
				case STATE_1VS1_PLACE_ARENA: pMsg = Localizable("Select your favourite part of the map\nZoom in or out to change the size\nPlace arena by pressing SPACE"); break;
				case STATE_1VS1_PLACE_FIRST_SPAWN: pMsg = Localizable("Set first spawn positions by pressing SPACE"); break;
				case STATE_1VS1_PLACE_SECOND_SPAWN: pMsg = Localizable("Set second spawn positions by pressing SPACE"); break;
				case STATE_1VS1_SELECT_WEAPONS: pMsg = Localizable("Choose a weapon with A/D\nToggle weapon usage by pressing SPACE\nPress SPACE on the heart to confirm"); break;
				}
				GameServer()->SendBroadcast(pMsg, ClientID, false);
			}
		}
	}
}

void CArenas::Snap(int SnappingClient)
{
	if (SnappingClient == -1)
		return;

	int ClientID = SnappingClient;
	CPlayer *pSnap = GameServer()->m_apPlayers[SnappingClient];
	if ((pSnap->GetTeam() == TEAM_SPECTATORS || pSnap->IsPaused()) && pSnap->GetSpectatorID() >= 0)
		ClientID = pSnap->GetSpectatorID();

	int Fight = GetClientFight(ClientID);
	if (Fight < 0)
		return;

	if (!IsConfiguring(SnappingClient) && !FightStarted(ClientID))
		return;

	if (m_aState[SnappingClient] != STATE_1VS1_SELECT_WEAPONS)
		UpdateSnapPositions(SnappingClient);

	CFight *pFight = &m_aFights[Fight];

	for (int i = 0; i < 4; i++)
	{
		CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_IDs.m_aBorder[i], sizeof(CNetObj_Laser)));
		if (!pObj)
			return;

		int To = i == POINT_BOTTOM_LEFT ? POINT_TOP_LEFT : i+1;
		pObj->m_X = round_to_int(pFight->m_aCorners[i].x);
		pObj->m_Y = round_to_int(pFight->m_aCorners[i].y);
		pObj->m_FromX = round_to_int(pFight->m_aCorners[To].x);
		pObj->m_FromY = round_to_int(pFight->m_aCorners[To].y);
		pObj->m_StartTick = Server()->Tick();
	}

	if (!IsConfiguring(SnappingClient))
		return;

	for (int i = 0; i < 2; i++)
	{
		if (pFight->m_aSpawns[i] != vec2(-1, -1))
		{
			CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_IDs.m_aSpawn[i], sizeof(CNetObj_Laser)));
			if (!pObj)
				return;

			pObj->m_X = round_to_int(pFight->m_aSpawns[i].x);
			pObj->m_Y = round_to_int(pFight->m_aSpawns[i].y);
			pObj->m_FromX = round_to_int(pFight->m_aSpawns[i].x);
			pObj->m_FromY = round_to_int(pFight->m_aSpawns[i].y);
			pObj->m_StartTick = Server()->Tick();
		}
	}

	if (m_aState[SnappingClient] != STATE_1VS1_SELECT_WEAPONS)
		return;

	vec2 Pos = pFight->m_MiddlePos;
	vec2 aBox[4] = { vec2(Pos.x-64, Pos.y-32), vec2(Pos.x+64, Pos.y-32), vec2(Pos.x+64, Pos.y+32), vec2(Pos.x-64, Pos.y+32) };
	for (int i = 0; i < 4; i++)
	{
		CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_IDs.m_aWeaponBox[i], sizeof(CNetObj_Laser)));
		if (!pObj)
			return;

		int To = i == POINT_BOTTOM_LEFT ? POINT_TOP_LEFT : i+1;
		pObj->m_X = round_to_int(aBox[i].x);
		pObj->m_Y = round_to_int(aBox[i].y);
		pObj->m_FromX = round_to_int(aBox[To].x);
		pObj->m_FromY = round_to_int(aBox[To].y);
		pObj->m_StartTick = Server()->Tick()-2;
	}

	int Size = Server()->IsSevendown(SnappingClient) ? 4*4 : sizeof(CNetObj_Pickup);
	CNetObj_Pickup *pPickup = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_IDs.m_SelectedWeapon, Size));
	if (!pPickup)
		return;

	int Type = POWERUP_WEAPON;
	int Subtype = 0;
	switch (m_aSelectedWeapon[SnappingClient])
	{
	case -1: Type = POWERUP_HEALTH; break;
	case 0: Subtype = WEAPON_HAMMER; break;
	case 1: Subtype = WEAPON_SHOTGUN; break;
	case 2: Subtype = WEAPON_GRENADE; break;
	case 3: Subtype = WEAPON_LASER; break;
	}

	pPickup->m_X = round_to_int(Pos.x);
	pPickup->m_Y = round_to_int(Pos.y);
	if (Server()->IsSevendown(SnappingClient))
	{
		pPickup->m_Type = Type;
		((int*)pPickup)[3] = Subtype;
	}
	else
		pPickup->m_Type = GameServer()->GetPickupType(Type, Subtype);

	// activated
	if (((Subtype == WEAPON_HAMMER && Type == POWERUP_WEAPON) && pFight->m_Weapons.m_Hammer)
		|| (Subtype == WEAPON_SHOTGUN && pFight->m_Weapons.m_Shotgun)
		|| (Subtype == WEAPON_GRENADE && pFight->m_Weapons.m_Grenade)
		|| (Subtype == WEAPON_LASER && pFight->m_Weapons.m_Laser))
	{
		CNetObj_Pickup *pShield = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_IDs.m_WeaponActivated, Size));
		if (!pShield)
			return;

		pShield->m_X = round_to_int(Pos.x);
		pShield->m_Y = round_to_int(Pos.y - 64);
		if (Server()->IsSevendown(SnappingClient))
		{
			pShield->m_Type = POWERUP_ARMOR;
			((int*)pShield)[3] = 0;
		}
		else
			pShield->m_Type = PICKUP_ARMOR;
	}
}

void CArenas::SetArenaCollision(int Fight, bool Remove)
{
	CFight *pFight = &m_aFights[Fight];
	vec2 Length = vec2(distance(pFight->m_aCorners[POINT_TOP_LEFT], pFight->m_aCorners[POINT_TOP_RIGHT]),
		distance(pFight->m_aCorners[POINT_TOP_LEFT], pFight->m_aCorners[POINT_BOTTOM_LEFT]));

	int Number = GameServer()->Collision()->GetNumAllSwitchers() + 1 + Fight;
	for (int i = 0; i < Length.x - 1; i++)
	{
		vec2 CurrentPos[2];
		CurrentPos[0] = vec2(pFight->m_aCorners[POINT_TOP_LEFT].x + i, pFight->m_aCorners[POINT_TOP_LEFT].y);
		CurrentPos[1] = vec2(pFight->m_aCorners[POINT_BOTTOM_LEFT].x + i, pFight->m_aCorners[POINT_BOTTOM_LEFT].y);

		for (int j = 0; j < 2; j++)
		{
			int Index = GameServer()->Collision()->GetPureMapIndex(CurrentPos[j]);
			if (Remove)
				GameServer()->Collision()->RemoveDoorTile(Index, TILE_STOPA, Number);
			else
				GameServer()->Collision()->AddDoorTile(Index, TILE_STOPA, Number);
		}
	}

	for (int i = 0; i < Length.y - 1; i++)
	{
		vec2 CurrentPos[2];
		CurrentPos[0] = vec2(pFight->m_aCorners[POINT_TOP_LEFT].x, pFight->m_aCorners[POINT_TOP_LEFT].y + i);
		CurrentPos[1] = vec2(pFight->m_aCorners[POINT_TOP_RIGHT].x, pFight->m_aCorners[POINT_TOP_RIGHT].y + i);

		for (int j = 0; j < 2; j++)
		{
			int Index = GameServer()->Collision()->GetPureMapIndex(CurrentPos[j]);
			if (Remove)
				GameServer()->Collision()->RemoveDoorTile(Index, TILE_STOPA, Number);
			else
				GameServer()->Collision()->AddDoorTile(Index, TILE_STOPA, Number);
		}
	}
}
