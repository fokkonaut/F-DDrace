/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */
#include <engine/server.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/teams.h>

#include "gun.h"
#include "plasma.h"
#include "character.h"

//////////////////////////////////////////////////
// CGun
//////////////////////////////////////////////////
CGun::CGun(CGameWorld *pGameWorld, vec2 Pos, bool Freeze, bool Explosive, int Layer, int Number)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER_GUN, Pos)
{
	m_Layer = Layer;
	m_Number = Number;
	m_LastFire = Server()->Tick();
	m_Pos = Pos;
	m_EvalTick = Server()->Tick();
	m_Freeze = Freeze;
	m_Explosive = Explosive;

	GameWorld()->InsertEntity(this);
}


void CGun::Fire()
{
	CCharacter *Ents[MAX_CLIENTS];
	int IdInTeam[MAX_CLIENTS];
	int LenInTeam[MAX_CLIENTS];
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		IdInTeam[i] = -1;
		LenInTeam[i] = 0;
	}

	int Num = -1;
	Num = GameWorld()->FindEntities(m_Pos, Config()->m_SvPlasmaRange, (CEntity**)Ents, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

	for (int i = 0; i < Num; i++)
	{
		CCharacter *Target = Ents[i];
		//now gun doesn't affect on super
		if(Target->Team() == TEAM_SUPER)
			continue;
		if(m_Layer == LAYER_SWITCH && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[Target->Team()])
			continue;
		int res = GameServer()->Collision()->IntersectLine(m_Pos, Target->GetPos(),0,0);
		if (!res)
		{
			int Len = length(Target->GetPos() - m_Pos);
			if (LenInTeam[Target->Team()] == 0 || LenInTeam[Target->Team()] > Len)
			{
				LenInTeam[Target->Team()] = Len;
				IdInTeam[Target->Team()] = i;
			}
		}
	}
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if(IdInTeam[i] != -1)
		{
			CCharacter *Target = Ents[IdInTeam[i]];
			new CPlasma(GameWorld(), m_Pos, normalize(Target->GetPos() - m_Pos), m_Freeze, m_Explosive, i);
			m_LastFire = Server()->Tick();
		}
	}
	for (int i = 0; i < Num; i++)
	{
		CCharacter *Target = Ents[i];
		if (Target->IsAlive() && Target->Teams()->m_Core.GetSolo(Target->GetPlayer()->GetCID()))
		{
			if (IdInTeam[Target->Team()] != i)
			{
				int res = GameServer()->Collision()->IntersectLine(m_Pos, Target->GetPos(),0,0);
				if (!res)
				{
					new CPlasma(GameWorld(), m_Pos, normalize(Target->GetPos() - m_Pos), m_Freeze, m_Explosive, Target->Team());
					m_LastFire = Server()->Tick();
				}
			}
		}
	}

}

void CGun::Reset()
{

}

void CGun::Tick()
{
	if (Server()->Tick()%int(Server()->TickSpeed()*0.15f)==0)
	{
		int Flags;
		m_EvalTick=Server()->Tick();
		int index = GameServer()->Collision()->IsMover(m_Pos.x,m_Pos.y, &Flags);
		if (index)
		{
			m_Core=GameServer()->Collision()->CpSpeed(index, Flags);
		}
		m_Pos+=m_Core;
	}
	if (m_LastFire + Server()->TickSpeed() / Config()->m_SvPlasmaPerSec <= Server()->Tick())
		Fire();

}

void CGun::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *Char = GameServer()->GetPlayerChar(SnappingClient);

	if(SnappingClient > -1 && (GameServer()->m_apPlayers[SnappingClient]->GetTeam() == -1
				|| GameServer()->m_apPlayers[SnappingClient]->IsPaused())
			&& GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID() != -1)
		Char = GameServer()->GetPlayerChar(GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID());

	CNetObj_EntityEx *pEntData = 0;
	CCharacter *pChr = GameServer()->GetPlayerChar(SnappingClient);
	if (pChr && pChr->SendExtendedEntity(this))
		pEntData = static_cast<CNetObj_EntityEx *>(Server()->SnapNewItem(NETOBJTYPE_ENTITYEX, GetID(), sizeof(CNetObj_EntityEx)));

	if (pEntData)
	{
		pEntData->m_SwitchNumber = m_Number;
		pEntData->m_Layer = m_Layer;

		if(m_Explosive && !m_Freeze)
			pEntData->m_EntityClass = ENTITYCLASS_GUN_NORMAL;
		else if(m_Explosive && m_Freeze)
			pEntData->m_EntityClass = ENTITYCLASS_GUN_EXPLOSIVE;
		else if(!m_Explosive && m_Freeze)
			pEntData->m_EntityClass = ENTITYCLASS_GUN_FREEZE;
		else
			pEntData->m_EntityClass = ENTITYCLASS_GUN_UNFREEZE;
	}
	else
	{
		int Tick = (Server()->Tick()%Server()->TickSpeed())%11;
		if (Char && Char->IsAlive() && (m_Layer == LAYER_SWITCH && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[Char->Team()]) && (!Tick))
			return;
	}

	if(GameServer()->GetClientDDNetVersion(SnappingClient) >= VERSION_DDNET_MULTI_LASER)
	{
		CNetObj_DDNetLaser *pObj = static_cast<CNetObj_DDNetLaser *>(Server()->SnapNewItem(NETOBJTYPE_DDNETLASER, GetID(), sizeof(CNetObj_DDNetLaser)));
		if(!pObj)
			return;

		pObj->m_ToX = round_to_int(m_Pos.x);
		pObj->m_ToY = round_to_int(m_Pos.y);
		pObj->m_FromX = round_to_int(m_Pos.x);
		pObj->m_FromY = round_to_int(m_Pos.y);
		pObj->m_StartTick = m_EvalTick;
		pObj->m_Owner = -1;
		pObj->m_Type = m_Freeze ? LASERTYPE_FREEZE : LASERTYPE_RIFLE;
	}
	else
	{
		CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
		if(!pObj)
			return;

		pObj->m_X = round_to_int(m_Pos.x);
		pObj->m_Y = round_to_int(m_Pos.y);
		pObj->m_FromX = round_to_int(m_Pos.x);
		pObj->m_FromY = round_to_int(m_Pos.y);
		pObj->m_StartTick = m_EvalTick;
	}
}
