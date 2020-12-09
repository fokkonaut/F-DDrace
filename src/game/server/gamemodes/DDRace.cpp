#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include "DDRace.h"
#include "gamemode.h"


CGameControllerDDRace::CGameControllerDDRace(class CGameContext *pGameServer) :
		IGameController(pGameServer), m_Teams(pGameServer)
{
	m_apFlags[0] = 0;
	m_apFlags[1] = 0;

	m_pGameType = Config()->m_SvTestingCommands ? TEST_NAME : GAME_NAME;

	InitTeleporter();
}

CGameControllerDDRace::~CGameControllerDDRace()
{
	// Nothing to clean
}

void CGameControllerDDRace::Tick()
{
	IGameController::Tick();
}

void CGameControllerDDRace::InitTeleporter()
{
	if (!GameServer()->Collision()->Layers()->TeleLayer())
		return;
	int Width = GameServer()->Collision()->Layers()->TeleLayer()->m_Width;
	int Height = GameServer()->Collision()->Layers()->TeleLayer()->m_Height;

	for (int i = 0; i < Width * Height; i++)
	{
		int Number = GameServer()->Collision()->TeleLayer()[i].m_Number;
		int Type = GameServer()->Collision()->TeleLayer()[i].m_Type;
		if (Number > 0)
		{
			if (Type == TILE_TELEOUT)
			{
				m_TeleOuts[Number - 1].push_back(
						vec2(i % Width * 32 + 16, i / Width * 32 + 16));
			}
			else if (Type == TILE_TELECHECKOUT)
			{
				m_TeleCheckOuts[Number - 1].push_back(
						vec2(i % Width * 32 + 16, i / Width * 32 + 16));
			}
		}
	}
}

// F-DDrace

bool CGameControllerDDRace::OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number)
{
	if(IGameController::OnEntity(Index, Pos, Layer, Flags, Number))
		return true;

	int Team = -1;
	if (Index == ENTITY_FLAGSTAND_RED) Team = TEAM_RED;
	if (Index == ENTITY_FLAGSTAND_BLUE) Team = TEAM_BLUE;
	if (Team == -1 || m_apFlags[Team])
		return false;

	m_GameFlags |= GAMEFLAG_FLAGS;

	CFlag *F = new CFlag(&GameServer()->m_World, Team, Pos);
	m_apFlags[Team] = F;
	return true;
}

int CGameControllerDDRace::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID)
{
	int HadFlag = 0;

	// drop flags
	for (int i = 0; i < 2; i++)
	{
		CFlag *F = m_apFlags[i];
		if (!F || !pKiller || !pKiller->GetCharacter())
			continue;

		if (F->GetCarrier() == pKiller->GetCharacter())
			HadFlag |= 2;
		if (F->GetCarrier() == pVictim)
		{
			if (HasFlag(pKiller->GetCharacter()) == -1)
				ChangeFlagOwner(pVictim, pKiller->GetCharacter());
			else
				F->Drop();
			HadFlag |= 1;
		}
	}

	return HadFlag;
}

void CGameControllerDDRace::ChangeFlagOwner(CCharacter *pOldCarrier, CCharacter *pNewCarrier)
{
	for (int i = 0; i < 2; i++)
	{
		CFlag *F = m_apFlags[i];

		if (!F || !pNewCarrier)
			return;

		if (F->GetCarrier() == pOldCarrier && HasFlag(pNewCarrier) == -1)
			F->Grab(pNewCarrier->GetPlayer()->GetCID());
	}
}

void CGameControllerDDRace::ForceFlagOwner(int ClientID, int Team)
{
	CFlag *F = m_apFlags[Team];
	CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
	if (!F || (Team != TEAM_RED && Team != TEAM_BLUE) || (!pChr && ClientID >= 0))
		return;
	if (ClientID >= 0 && HasFlag(pChr) == -1)
	{
		if (F->GetCarrier())
			F->SetLastCarrier(F->GetCarrier()->GetPlayer()->GetCID());
		F->Grab(ClientID);
	}
	else if (ClientID == -1)
		F->Reset();
}

int CGameControllerDDRace::HasFlag(CCharacter *pChr)
{
	if (!pChr)
		return -1;

	for (int i = 0; i < 2; i++)
	{
		if (!m_apFlags[i])
			continue;

		if (m_apFlags[i]->GetCarrier() == pChr)
			return i;
	}
	return -1;
}

void CGameControllerDDRace::Snap(int SnappingClient)
{
	IGameController::Snap(SnappingClient);

	CNetObj_GameDataFlag *pGameDataFlag = static_cast<CNetObj_GameDataFlag*>(Server()->SnapNewItem(NETOBJTYPE_GAMEDATAFLAG, 0, sizeof(CNetObj_GameDataFlag)));
	if (!pGameDataFlag)
		return;

	int FlagDropTickRed = 0;
	int FlagDropTickBlue = 0;
	int FlagCarrierRed;
	int FlagCarrierBlue;

	if (m_apFlags[TEAM_RED])
	{
		if (m_apFlags[TEAM_RED]->IsAtStand())
			FlagCarrierRed = FLAG_ATSTAND;
		else if (m_apFlags[TEAM_RED]->GetCarrier() && m_apFlags[TEAM_RED]->GetCarrier()->GetPlayer())
			FlagCarrierRed = m_apFlags[TEAM_RED]->GetCarrier()->GetPlayer()->GetCID();
		else
		{
			FlagCarrierRed = FLAG_TAKEN;
			FlagDropTickRed = m_apFlags[TEAM_RED]->GetDropTick();
		}
	}
	else
		FlagCarrierRed = FLAG_MISSING;

	if (m_apFlags[TEAM_BLUE])
	{
		if (m_apFlags[TEAM_BLUE]->IsAtStand())
			FlagCarrierBlue = FLAG_ATSTAND;
		else if (m_apFlags[TEAM_BLUE]->GetCarrier() && m_apFlags[TEAM_BLUE]->GetCarrier()->GetPlayer())
			FlagCarrierBlue = m_apFlags[TEAM_BLUE]->GetCarrier()->GetPlayer()->GetCID();
		else
		{
			FlagCarrierBlue = FLAG_TAKEN;
			FlagDropTickBlue = m_apFlags[TEAM_BLUE]->GetDropTick();
		}
	}
	else
		FlagCarrierBlue = FLAG_MISSING;

	if (SnappingClient > -1 && FlagCarrierRed >= 0 && !Server()->Translate(FlagCarrierRed, SnappingClient))
		FlagCarrierRed = FLAG_TAKEN;
	if (SnappingClient > -1 && FlagCarrierBlue >= 0 && !Server()->Translate(FlagCarrierBlue, SnappingClient))
		FlagCarrierBlue = FLAG_TAKEN;

	if (Server()->IsSevendown(SnappingClient))
	{
		((int*)pGameDataFlag)[0] = 0;
		((int*)pGameDataFlag)[1] = 0;
		((int*)pGameDataFlag)[2] = FlagCarrierRed;
		((int*)pGameDataFlag)[3] = FlagCarrierBlue;
	}
	else
	{
		pGameDataFlag->m_FlagCarrierRed = FlagCarrierRed;
		pGameDataFlag->m_FlagCarrierBlue = FlagCarrierBlue;
		pGameDataFlag->m_FlagDropTickRed = FlagDropTickRed;
		pGameDataFlag->m_FlagDropTickBlue = FlagDropTickBlue;
	}
}
