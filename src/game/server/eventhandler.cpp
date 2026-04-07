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
	int Cur = m_CurrentBuffer;
	int NumEvents = m_NumEvents[Cur];
	if(NumEvents == MAX_EVENTS)
		return 0;
	if(m_CurrentOffset[Cur] + Size >= MAX_DATASIZE)
		return 0;

	void *p = &m_aData[Cur][m_CurrentOffset[Cur]];
	m_aOffsets[Cur][NumEvents] = m_CurrentOffset[Cur];
	m_aTypes[Cur][NumEvents] = Type;
	m_aSizes[Cur][NumEvents] = Size;
	m_aClientMasks[Cur][NumEvents] = Mask;
	m_CurrentOffset[Cur] += Size;
	m_NumEvents[Cur]++;
	return p;
}

void CEventHandler::Clear()
{
	// Switch buffer, store previous events to send them to low bandwidth players
	// without resending them to highbandwidth players
	m_CurrentBuffer ^= 1;

	// Reset new current buffer
	m_NumEvents[m_CurrentBuffer] = 0;
	m_CurrentOffset[m_CurrentBuffer] = 0;
}

void CEventHandler::Snap(int SnappingClient)
{
	int Cur = m_CurrentBuffer;
	int Prev = Cur ^ 1;

	SnapBuffer(SnappingClient, Cur);
	if (!GameServer()->Server()->GetHighBandwidth(SnappingClient))
		SnapBuffer(SnappingClient, Prev);
}

void CEventHandler::SnapBuffer(int SnappingClient, int Buf)
{
	for(int i = 0; i < m_NumEvents[Buf]; i++)
	{
		if(SnappingClient == -1 || CmaskIsSet(m_aClientMasks[Buf][i], SnappingClient))
		{
			char *pData = &m_aData[Buf][m_aOffsets[Buf][i]];
			CNetEvent_Common *pEvent = (CNetEvent_Common *)pData;
			vec2 EventPos = vec2(pEvent->m_X, pEvent->m_Y);
			if(!NetworkClipped(GameServer(), SnappingClient, EventPos))
			{
				int Type = m_aTypes[Buf][i];
				if (Type == NETEVENTTYPE_SOUNDWORLD || Type == NETEVENTTYPE_HAMMERHIT || Type == NETEVENTTYPE_SPAWN)
				{
					CPlayer *pSnap = SnappingClient >= 0 ? GameServer()->m_apPlayers[SnappingClient] : 0;
					CCharacter *pChr = pSnap ? pSnap->GetCharacter() : 0;
					if (Type == NETEVENTTYPE_HAMMERHIT && pChr && pChr->m_pGrog && pChr->m_pGrog->m_LastNudgePos == EventPos)
						pChr->m_pGrog->m_LastNudgePos = vec2(-1, -1);
					else if (pSnap && pSnap->SilentFarmActive())
						continue;
				}

				const auto &&SnapEvent = [&]() {
					// offset: dont overlap ids, start where m_CurrentBuffer stopped and append previously missed events
					int SnapID = i;
					if (Buf != m_CurrentBuffer)
						SnapID += m_NumEvents[m_CurrentBuffer];

					int Size = m_aSizes[Buf][i];
					void *pItem = GameServer()->Server()->SnapNewItem(Type, SnapID, Size);
					if (pItem)
						mem_copy(pItem, pData, Size);
				};
				const auto &&SnapTranslateEvent = [&](int *pClientId) {
					int ClientId = *pClientId; // Save real Id
					if(GameServer()->Server()->Translate(*pClientId, SnappingClient))
					{
						SnapEvent();
						*pClientId = ClientId; // Reset Id for others
					}
				};

				if (Type == NETEVENTTYPE_DEATH)
				{
					CNetEvent_Death *pDeath = (CNetEvent_Death *)pData;
					SnapTranslateEvent(&pDeath->m_ClientID);
				}
				else if (Type == NETEVENTTYPE_DAMAGE)
				{
					CNetEvent_Damage *pDamage = (CNetEvent_Damage *)pData;
					SnapTranslateEvent(&pDamage->m_ClientID);
				}
				else
				{
					SnapEvent();
				}
			}
		}
	}
}
