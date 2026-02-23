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
	m_TeamMask = Mask128();

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
	m_TeamMask = pOwner ? pOwner->TeamMask() : Mask128();

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
		GameServer()->CreateDeath(m_StartPos, m_Owner, m_TeamMask);
		GameServer()->CreateDeath(m_Pos, m_Owner, m_TeamMask);

		MarkForDestroy();
		return;
	}
}

bool CPortalBlocker::CanPlace()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if (!pOwner)
		return false;

	// Not out of the map and not too far away from the owner
	if (GameLayerClipped(m_Pos) || distance(pOwner->GetPos(), m_Pos) > Config()->m_SvPortalMaxDistance)
		return false;

	// For the first position we check this relative to the owner's position, the second position gets clamped to the closest pos to a wall if it's cutting through
	if (!m_HasStartPos && GameServer()->Collision()->IntersectLine(pOwner->GetPos(), m_Pos, 0, 0))
		return false;

	// if we have start position already we validated that the general position is reachable and dont need to validate intersectline
	return true;
}

bool CPortalBlocker::OnPlace()
{
	if (!CanPlace())
		return false;

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if (!pOwner)
		return false;

	if (!m_HasStartPos)
	{
		m_StartPos = m_Pos;
		m_HasStartPos = true;
	}
	else
	{
		m_HasEndPos = true;
		
		vec2 CenterPos = (m_Pos + m_StartPos) / 2;
		GameServer()->CreateSound(CenterPos, SOUND_WEAPON_SPAWN, m_TeamMask);

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
	// Preview for now only, for the currently creating guy
	if (!m_HasEndPos && (SnappingClient != m_Owner || !CanPlace()))
		return;

	if (NetworkClipped(SnappingClient, m_Pos) && NetworkClipped(SnappingClient, m_StartPos))
		return;

	if (!CmaskIsSet(m_TeamMask, SnappingClient))
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

		int SnappingClientVersion = GameServer()->GetClientDDNetVersion(SnappingClient);
		GameServer()->SnapLaserObject(CSnapContext(SnappingClientVersion, Server()->IsSevendown(SnappingClient), SnappingClient), m_aID[i],
			To, From, Server()->Tick() - 3, m_Owner, LASERTYPE_SHOTGUN, -1, -1, LASERFLAG_NO_PREDICT);
	}
}
