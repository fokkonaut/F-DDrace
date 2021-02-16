/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include <engine/config.h>
#include <engine/server.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/teams.h>
#include <game/server/gamemodes/DDRace.h>
#include "dragger.h"
#include "character.h"

CDragger::CDragger(CGameWorld *pGameWorld, vec2 Pos, float Strength, bool NW, int Layer, int Number)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_DRAGGER, Pos)
{
	m_Layer = Layer;
	m_Number = Number;
	m_Pos = Pos;
	m_Strength = Strength;
	m_EvalTick = Server()->Tick();
	m_NW = NW;
	mem_zero(m_apTarget, sizeof(m_apTarget));
	mem_zero(m_aapSoloEnts, sizeof(m_aapSoloEnts));
	for (int i = 0; i < MAX_CLIENTS; i++)
		m_aIDs[i] = Server()->SnapNewID();
	GameWorld()->InsertEntity(this);
}

CDragger::~CDragger()
{
	for (int i = 0; i < MAX_CLIENTS; i++)
		Server()->SnapFreeID(m_aIDs[i]);
}

void CDragger::Move(int Team)
{
	if (m_apTarget[Team] && (!m_apTarget[Team]->IsAlive() || (m_apTarget[Team]->IsAlive()
		&& (m_apTarget[Team]->m_Super || m_apTarget[Team]->IsPaused() || (m_Layer == LAYER_SWITCH && m_Number && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[m_apTarget[Team]->Team()])))))
		m_apTarget[Team] = 0;

	mem_zero(m_aapSoloEnts[Team], sizeof(m_aapSoloEnts[Team]));
	CCharacter *apTempEnts[MAX_CLIENTS];

	int Num = GameWorld()->FindEntities(m_Pos, Config()->m_SvDraggerRange, (CEntity**)m_aapSoloEnts[Team], MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	mem_copy(apTempEnts, m_aapSoloEnts[Team], sizeof(apTempEnts));

	int Id = -1;
	int MinLen = 0;
	CCharacter *pTemp;
	for (int i = 0; i < Num; i++)
	{
		pTemp = m_aapSoloEnts[Team][i];
		if (pTemp->Team() != Team)
		{
			m_aapSoloEnts[Team][i] = 0;
			continue;
		}
		if (m_Layer == LAYER_SWITCH && m_Number && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[Team])
		{
			m_aapSoloEnts[Team][i] = 0;
			continue;
		}

		int Res = m_NW ? GameServer()->Collision()->IntersectNoLaserNW(m_Pos, pTemp->GetPos(), 0, 0)
			: GameServer()->Collision()->IntersectNoLaser(m_Pos, pTemp->GetPos(), 0, 0);

		if (Res == 0)
		{
			int Len = length(pTemp->GetPos() - m_Pos);
			if (MinLen == 0 || MinLen > Len)
			{
				MinLen = Len;
				Id = i;
			}

			if (!pTemp->Teams()->m_Core.GetSolo(pTemp->GetPlayer()->GetCID()))
				m_aapSoloEnts[Team][i] = 0;
		}
		else
		{
			m_aapSoloEnts[Team][i] = 0;
		}
	}

	if (!m_apTarget[Team])
		m_apTarget[Team] = Id != -1 ? apTempEnts[Id] : 0;

	if (m_apTarget[Team])
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (m_aapSoloEnts[Team][i] == m_apTarget[Team])
				m_aapSoloEnts[Team][i] = 0;
		}
	}
}

void CDragger::Drag(int Team)
{
	if (!m_apTarget[Team])
		return;

	CCharacter *pTarget = m_apTarget[Team];

	for (int i = -1; i < MAX_CLIENTS; i++)
	{
		if (i >= 0)
			pTarget = m_aapSoloEnts[Team][i];

		if (!pTarget)
			continue;

		int Res = m_NW ? GameServer()->Collision()->IntersectNoLaserNW(m_Pos, pTarget->GetPos(), 0, 0)
			: GameServer()->Collision()->IntersectNoLaser(m_Pos, pTarget->GetPos(), 0, 0);

		if (Res || length(m_Pos - pTarget->GetPos()) > Config()->m_SvDraggerRange)
		{
			pTarget = 0;
			if (i == -1)
				m_apTarget[Team] = 0;
			else
				m_aapSoloEnts[Team][i] = 0;
		}
		else if (length(m_Pos - pTarget->GetPos()) > 28)
		{
			vec2 Temp = pTarget->Core()->m_Vel + (normalize(GetPos() - pTarget->GetPos()) * m_Strength);
			pTarget->Core()->m_Vel = ClampVel(pTarget->m_MoveRestrictions, Temp);

			// F-DDrace
			pTarget->SetLastTouchedSwitcher(m_Number);
		}
	}
}

void CDragger::Reset()
{

}

void CDragger::Tick()
{
	bool Tick = Server()->Tick() % int(Server()->TickSpeed() * 0.15f) == 0;
	if (Tick)
	{
		int Flags;
		m_EvalTick = Server()->Tick();
		int index = GameServer()->Collision()->IsMover(m_Pos.x, m_Pos.y, &Flags);
		if (index)
		{
			m_Core = GameServer()->Collision()->CpSpeed(index, Flags);
		}
		m_Pos += m_Core;
	}

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (((CGameControllerDDRace *)GameServer()->m_pController)->m_Teams.GetTeamState(i) == CGameTeams::TEAMSTATE_EMPTY)
			continue;

		if (Tick)
			Move(i);
		Drag(i);
	}
}

void CDragger::Snap(int SnappingClient)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (((CGameControllerDDRace *)GameServer()->m_pController)->m_Teams.GetTeamState(i) != CGameTeams::TEAMSTATE_EMPTY)
			Snap(SnappingClient, i);
}

void CDragger::Snap(int SnappingClient, int Team)
{
	CCharacter *pTarget = m_apTarget[Team];

	for (int i = -1; i < MAX_CLIENTS; i++)
	{
		if (i >= 0)
		{
			pTarget = m_aapSoloEnts[Team][i];
			if (!pTarget)
				continue;
		}

		if (pTarget)
		{
			if (NetworkClipped(SnappingClient, m_Pos) && NetworkClipped(SnappingClient, pTarget->GetPos()))
				continue;
		}
		else if (NetworkClipped(SnappingClient, m_Pos))
			continue;

		CCharacter *pChr = GameServer()->GetPlayerChar(SnappingClient);

		if(SnappingClient > -1 && (GameServer()->m_apPlayers[SnappingClient]->GetTeam() == -1 || GameServer()->m_apPlayers[SnappingClient]->IsPaused())
			&& GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID() != -1)
			pChr = GameServer()->GetPlayerChar(GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID());

		int Tick = (Server()->Tick() % Server()->TickSpeed()) % 11;
		if (pChr && pChr->IsAlive() && (m_Layer == LAYER_SWITCH && m_Number	&& !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[pChr->Team()] && (!Tick)))
			continue;
		if (pChr && pChr->IsAlive())
		{
			if (pChr->Team() != Team)
				continue;
		}
		else
		{
			// send to spectators only active draggers and some inactive from team 0
			if (!((pTarget && pTarget->IsAlive()) || Team == 0))
				continue;
		}

		if (pChr && pChr->IsAlive() && pTarget && pTarget->IsAlive() && pTarget->GetPlayer()->GetCID() != pChr->GetPlayer()->GetCID() && !pChr->GetPlayer()->m_ShowOthers &&
			(pChr->Teams()->m_Core.GetSolo(SnappingClient) || pChr->Teams()->m_Core.GetSolo(pTarget->GetPlayer()->GetCID())))
		{
			continue;
		}

		int ID = pTarget ? m_aIDs[pTarget->GetPlayer()->GetCID()] : GetID();
		CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, ID, sizeof(CNetObj_Laser)));
		if (!pObj)
			continue;

		pObj->m_X = (int)m_Pos.x;
		pObj->m_Y = (int)m_Pos.y;
		if (pTarget)
		{
			pObj->m_FromX = (int)pTarget->GetPos().x;
			pObj->m_FromY = (int)pTarget->GetPos().y;
		}
		else
		{
			pObj->m_FromX = (int)m_Pos.x;
			pObj->m_FromY = (int)m_Pos.y;
		}

		int StartTick = m_EvalTick;
		if (StartTick < Server()->Tick() - 4)
			StartTick = Server()->Tick() - 4;
		else if (StartTick > Server()->Tick())
			StartTick = Server()->Tick();
		pObj->m_StartTick = StartTick;
	}
}
