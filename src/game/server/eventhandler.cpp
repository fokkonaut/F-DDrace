/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include "eventhandler.h"
#include "gamecontext.h"
#include "player.h"

//////////////////////////////////////////////////
// Event handler
//////////////////////////////////////////////////
CEventHandler::CEventHandler()
{
	m_pGameServer = 0;
	Clear();
}

void CEventHandler::SetGameServer(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
}

void *CEventHandler::Create(int Type, int Size, Mask128 Mask)
{
	if(m_NumEvents == MAX_EVENTS)
		return 0;
	if(m_CurrentOffset+Size >= MAX_DATASIZE)
		return 0;

	void *p = &m_aData[m_CurrentOffset];
	m_aOffsets[m_NumEvents] = m_CurrentOffset;
	m_aTypes[m_NumEvents] = Type;
	m_aSizes[m_NumEvents] = Size;
	m_aClientMasks[m_NumEvents] = Mask;
	m_CurrentOffset += Size;
	m_NumEvents++;
	return p;
}

void CEventHandler::Clear()
{
	m_NumEvents = 0;
	m_CurrentOffset = 0;
}

void CEventHandler::Snap(int SnappingClient)
{
	for(int i = 0; i < m_NumEvents; i++)
	{
		if(SnappingClient == -1 || CmaskIsSet(m_aClientMasks[i], SnappingClient))
		{
			CNetEvent_Common *ev = (CNetEvent_Common *)&m_aData[m_aOffsets[i]];
			if(SnappingClient == -1 || distance(GameServer()->m_apPlayers[SnappingClient]->m_ViewPos, vec2(ev->m_X, ev->m_Y)) < 1500.0f)
			{
				if (m_aTypes[i] == NETEVENTTYPE_DEATH)
				{
					CNetEvent_Death *pDeath = (CNetEvent_Death *)&m_aData[m_aOffsets[i]];
					// save real id
					int ClientID = pDeath->m_ClientID;

					// translate id
					int id = pDeath->m_ClientID;
					if (!GameServer()->Server()->Translate(id, SnappingClient))
						continue;
					pDeath->m_ClientID = id;

					// create event
					void *d = GameServer()->Server()->SnapNewItem(m_aTypes[i], i, m_aSizes[i]);
					if(d)
						mem_copy(d, &m_aData[m_aOffsets[i]], m_aSizes[i]);

					// reset id for others
					pDeath->m_ClientID = ClientID;
				}
				else if (m_aTypes[i] == NETEVENTTYPE_DAMAGE)
				{
					CNetEvent_Damage *pDamage = (CNetEvent_Damage *)&m_aData[m_aOffsets[i]];
					// save real id
					int ClientID = pDamage->m_ClientID;

					// translate id
					int id = pDamage->m_ClientID;
					if (!GameServer()->Server()->Translate(id, SnappingClient))
						continue;
					pDamage->m_ClientID = id;

					// create event
					void *d = GameServer()->Server()->SnapNewItem(m_aTypes[i], i, m_aSizes[i]);
					if(d)
						mem_copy(d, &m_aData[m_aOffsets[i]], m_aSizes[i]);

					// reset id for others
					pDamage->m_ClientID = ClientID;
				}
				else if (m_aTypes[i] == NETEVENTTYPE_SOUNDWORLD && SnappingClient >= 0 && GameServer()->m_apPlayers[SnappingClient]->m_SilentFarm
					&& GameServer()->m_apPlayers[SnappingClient]->GetCharacter() && GameServer()->m_apPlayers[SnappingClient]->GetCharacter()->m_MoneyTile
					&& !GameServer()->m_apPlayers[SnappingClient]->IsPaused() && GameServer()->m_apPlayers[SnappingClient]->GetTeam() != TEAM_SPECTATORS)
				{
					continue;
				}
				else
				{
					void* d = GameServer()->Server()->SnapNewItem(m_aTypes[i], i, m_aSizes[i]);
					if (d)
						mem_copy(d, &m_aData[m_aOffsets[i]], m_aSizes[i]);
				}
			}
		}
	}
}
