#include <game/server/gamecontext.h>
#include "meteor.h"
#include "character.h"
#include <game/server/player.h>

CMeteor::CMeteor(CGameWorld *pGameWorld, vec2 Pos, int Owner, bool Infinite)
: CStableProjectile(pGameWorld, WEAPON_SHOTGUN, Owner, Pos, true)
{
	m_Vel = vec2(0.1f, 0.1f);
	m_Owner = Owner;
	m_Infinite = Infinite;

	m_TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(m_Pos));
}

void CMeteor::Reset()
{
	GameWorld()->DestroyEntity(this);
}

void CMeteor::Tick()
{
	CCharacter* pChr = GameServer()->GetPlayerChar(m_Owner);
	if (
		!GameServer()->m_apPlayers[m_Owner]
		|| (!pChr && !m_Infinite)
		|| (m_Infinite && pChr && !pChr->GetPlayer()->m_InfMeteors)
		|| (!m_Infinite && pChr && !pChr->m_Meteors)
		)
	{
		Reset();
	}

	float Friction;
	float MaxAccel;
	float AccelPreserve;
	if (!m_TuneZone)
	{
		Friction = GameServer()->Tuning()->m_MeteorFriction / 1000000.f;
		MaxAccel = GameServer()->Tuning()->m_MeteorMaxAccel / 1000.f;
		AccelPreserve = GameServer()->Tuning()->m_MeteorAccelPreserve / 1000.f;
	}
	else
	{
		Friction = GameServer()->TuningList()[m_TuneZone].m_MeteorFriction / 1000000.f;
		MaxAccel = GameServer()->TuningList()[m_TuneZone].m_MeteorMaxAccel / 1000.f;
		AccelPreserve = GameServer()->TuningList()[m_TuneZone].m_MeteorAccelPreserve / 1000.f;
	}

	if(pChr)
	{
		vec2 CharPos = pChr->GetPos();
		m_Vel += normalize(CharPos - m_Pos) * (MaxAccel*AccelPreserve / (distance(CharPos, m_Pos) + AccelPreserve));
	}
	m_Pos += m_Vel;
	m_Vel *= 1.f - Friction;
}
