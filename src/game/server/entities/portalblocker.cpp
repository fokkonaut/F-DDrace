// made by fokkonaut

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include "portalblocker.h"

CPortalBlocker::CPortalBlocker(CGameWorld *pGameWorld, vec2 Pos, int Owner)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PORTAL_BLOCKER, Pos)
{
	m_Owner = Owner;
	m_Lifetime = Config()->m_SvPortalBlockerDetonation * Server()->TickSpeed();

	m_HasStartPos = false;
	m_HasEndPos = false;

	// We always want the single ball to be on top of the laser edge
	for (int i = 0; i < 2; i++)
		m_aID[i] = Server()->SnapNewID();
	std::sort(std::begin(m_aID), std::end(m_aID));

	GameWorld()->InsertEntity(this);
}

CPortalBlocker::~CPortalBlocker()
{
	for (int i = 0; i < 2; i++)
		Server()->SnapFreeID(m_aID[i]);
}

void CPortalBlocker::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if (!m_HasEndPos)
	{
		if (!pOwner || !pOwner->m_IsPortalBlocker)
		{
			if (pOwner)
				pOwner->m_pPortalBlocker = 0;
			MarkForDestroy();
			return;
		}

		vec2 CursorPos = pOwner->GetCursorPos();
		if (m_HasStartPos)
		{
			GameServer()->Collision()->IntersectLine(m_StartPos, CursorPos, 0, &CursorPos);

			float Amount = 1.f;
			if (Config()->m_SvPortalBlockerMaxLength)
			{
				// Clamp the length
				float Multiples = distance(m_StartPos, CursorPos) / (Config()->m_SvPortalBlockerMaxLength * 32.f);
				Amount = min(1.0f, 1 / Multiples);
			}
			m_Pos = mix(m_StartPos, CursorPos, Amount);
		}
		else
		{
			m_Pos = CursorPos;
		}
		return;
	}

	if (--m_Lifetime <= 0 || (m_Owner != -1 && !GameServer()->m_apPlayers[m_Owner]))
	{
		Mask128 TeamMask = pOwner ? pOwner->TeamMask() : Mask128();
		GameServer()->CreateDeath(m_StartPos, m_Owner, TeamMask);
		GameServer()->CreateDeath(m_Pos, m_Owner, TeamMask);

		MarkForDestroy();
		return;
	}
}

bool CPortalBlocker::OnPlace()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if (!pOwner)
		return false;

	// Not out of the map and not too far away from the owner
	if (GameLayerClipped(m_Pos) || distance(pOwner->GetPos(), m_Pos) > Config()->m_SvPortalMaxDistance)
		return false;

	if (!m_HasStartPos)
	{
		// For the first position we check this relative to the owner's position, the second position gets clamped to the closest pos to a wall if it's cutting through
		if (GameServer()->Collision()->IntersectLine(pOwner->GetPos(), m_Pos, 0, 0))
			return false;

		m_StartPos = m_Pos;
		m_HasStartPos = true;
	}
	else
	{
		m_HasEndPos = true;
		
		vec2 CenterPos = (m_Pos + m_StartPos) / 2;
		GameServer()->CreateSound(CenterPos, SOUND_WEAPON_SPAWN, pOwner->TeamMask());

		int AccID = pOwner->GetPlayer()->GetAccID();
		if (AccID >= ACC_START)
		{
			GameServer()->m_Accounts[AccID].m_PortalBlocker--;
			if (!GameServer()->m_Accounts[AccID].m_PortalBlocker)
				pOwner->m_IsPortalBlocker = false;
		}

		// Create a new portal blocker ready to be placed for the owner so we get detached from him aswell
		pOwner->m_pPortalBlocker = new CPortalBlocker(GameWorld(), pOwner->GetPos(), m_Owner);
		pOwner->UpdateWeaponIndicator();
	}

	return true;
}

void CPortalBlocker::Snap(int SnappingClient)
{
	// Preview for now only
	if (!m_HasEndPos)
	{
		if (SnappingClient != m_Owner)
			return;

		// For the currently creating guy
		CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
		if (pOwner && distance(pOwner->GetPos(), m_Pos) > Config()->m_SvPortalMaxDistance)
			return;
	}

	if (NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if (pOwner && !CmaskIsSet(pOwner->TeamMask(), SnappingClient))
		return;

	for (int i = 0; i < 2; i++)
	{
		vec2 To = m_Pos;
		vec2 From = m_Pos;

		if (i == 0)
		{
			if (!m_HasStartPos)
				continue;
			To = m_StartPos;
		}

		if(GameServer()->GetClientDDNetVersion(SnappingClient) >= VERSION_DDNET_MULTI_LASER)
		{
			CNetObj_DDNetLaser *pLaser = static_cast<CNetObj_DDNetLaser *>(Server()->SnapNewItem(NETOBJTYPE_DDNETLASER, m_aID[i], sizeof(CNetObj_DDNetLaser)));
			if(!pLaser)
				return;

			int Owner = m_Owner;
			if (!Server()->Translate(Owner, SnappingClient))
				Owner = -1;

			pLaser->m_ToX = round_to_int(To.x);
			pLaser->m_ToY = round_to_int(To.y);
			pLaser->m_FromX = round_to_int(From.x);
			pLaser->m_FromY = round_to_int(From.y);
			pLaser->m_StartTick = Server()->Tick() - 3;
			pLaser->m_Owner = Owner;
			pLaser->m_Type = LASERTYPE_SHOTGUN;
		}
		else
		{
			CNetObj_Laser *pButton = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_aID[i], sizeof(CNetObj_Laser)));
			if (!pButton)
				return;

			pButton->m_X = round_to_int(To.x);
			pButton->m_Y = round_to_int(To.y);
			pButton->m_FromX = round_to_int(From.x);
			pButton->m_FromY = round_to_int(From.y);
			pButton->m_StartTick = Server()->Tick() - 3;
		}
	}
}
