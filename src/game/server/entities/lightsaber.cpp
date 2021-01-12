// made by fokkonaut

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include "lightsaber.h"
#include <game/server/teams.h>
#include "character.h"
#include <generated/server_data.h>

CLightsaber::CLightsaber(CGameWorld *pGameWorld, vec2 Pos, int Owner)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_LIGHTSABER, Pos)
{
	m_Owner = Owner;
	m_Pos = GameServer()->GetPlayerChar(Owner)->GetPos();
	m_EvalTick = Server()->Tick();
	m_SoundTick = 0;

	m_Length = RETRACTED_LENGTH;
	m_Extending = false;
	m_Retracting = false;

	for (int i = 0; i < MAX_CLIENTS; i++)
		m_LastHit[i] = 0;

	GameWorld()->InsertEntity(this);
}

void CLightsaber::Reset()
{
	if (m_pOwner)
		m_pOwner->m_pLightsaber = 0;
	GameWorld()->DestroyEntity(this);
}

void CLightsaber::Extend()
{
	m_Extending = true;
	m_Retracting = false;
}

void CLightsaber::Retract()
{
	m_Retracting = true;
	m_Extending = false;
}

void CLightsaber::PlaySound()
{
	if (m_pOwner && m_SoundTick < Server()->Tick())
	{
		GameServer()->CreateSound(m_Pos, SOUND_LASER_BOUNCE, m_pOwner->Teams()->TeamMask(m_pOwner->Team(), -1, m_Owner));
		m_SoundTick = Server()->Tick() + Server()->TickSpeed() / 10;
	}
}

bool CLightsaber::HitCharacter()
{
	std::list<CCharacter *> HitCharacters = GameWorld()->IntersectedCharacters(m_Pos, m_To, 0.0f, m_pOwner, m_Owner);
	if (HitCharacters.empty())
		return false;
	for (std::list<CCharacter *>::iterator i = HitCharacters.begin();
		i != HitCharacters.end(); i++)
	{
		CCharacter *pChr = *i;
		if (pChr && m_LastHit[pChr->GetPlayer()->GetCID()] < Server()->Tick())
		{
			m_LastHit[pChr->GetPlayer()->GetCID()] = Server()->Tick() + Server()->TickSpeed() / 4;
			pChr->TakeDamage(vec2(0.f, 0.f), vec2(0, 0), g_pData->m_Weapons.m_aId[WEAPON_GUN].m_Damage, m_Owner, WEAPON_LIGHTSABER);
		}
	}
	return true;
}

void CLightsaber::Tick()
{
	m_pOwner = 0;
	if (m_Owner != -1 && GameServer()->GetPlayerChar(m_Owner))
		m_pOwner = GameServer()->GetPlayerChar(m_Owner);

	if (!m_pOwner)
		Reset();
	else
		m_Pos = m_pOwner->GetPos();

	m_TeamMask = m_pOwner ? m_pOwner->Teams()->TeamMask(m_pOwner->Team(), -1, m_Owner) : Mask128();

	if (Server()->Tick() % int(Server()->TickSpeed() * 0.15f) == 0)
		m_EvalTick = Server()->Tick();
	Step();

	HitCharacter();

	if (m_Extending && m_Length < EXTENDED_LENGTH)
	{
		PlaySound();
		m_Length += SPEED;
		if (m_Length >= EXTENDED_LENGTH)
			m_Extending = false;
	}

	if (m_Retracting && m_Length > RETRACTED_LENGTH)
	{
		PlaySound();
		m_Length -= SPEED;
		if (m_Length <= RETRACTED_LENGTH)
			Reset();
	}
}

void CLightsaber::Step()
{
	if (!m_pOwner)
		return;

	vec2 to2 = m_Pos + normalize(vec2(m_pOwner->Input()->m_TargetX, m_pOwner->Input()->m_TargetY)) * m_Length;
	GameServer()->Collision()->IntersectLine(m_Pos, to2, &m_To, 0);
}

void CLightsaber::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient, m_Pos) && NetworkClipped(SnappingClient, m_To))
		return;

	if (GameServer()->GetPlayerChar(SnappingClient))
	{
		if (!CmaskIsSet(m_TeamMask, SnappingClient))
			return;
	}

	if (m_pOwner && m_pOwner->IsPaused())
		return;

	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
	if (!pObj)
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_FromX = (int)m_To.x;
	pObj->m_FromY = (int)m_To.y;

	int StartTick = m_EvalTick;
	if (StartTick < Server()->Tick() - 2)
		StartTick = Server()->Tick() - 2;
	else if (StartTick > Server()->Tick())
		StartTick = Server()->Tick();
	pObj->m_StartTick = StartTick;
}
