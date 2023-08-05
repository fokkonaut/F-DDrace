// made by fokkonaut

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include "helicopter.h"

CHelicopter::CHelicopter(CGameWorld *pGameWorld, vec2 Pos)
: CAdvancedEntity(pGameWorld, CGameWorld::ENTTYPE_HELICOPTER, Pos, vec2(80, 128))
{
	m_AllowVipPlus = false;
	m_Elasticity = 0.f;

	m_Flipped = false;
	m_Angle = 0.f;
	m_Accel = vec2(0.f, 0.f);

	InitBody();
	InitPropellers();

	for (int i = 0; i < NUM_BONES; i++)
		m_aBones[i].m_ID = Server()->SnapNewID();
	for (int i = 0; i < NUM_TRAILS; i++)
		m_aTrails[i].m_ID = Server()->SnapNewID();

	GameWorld()->InsertEntity(this);
}

CHelicopter::~CHelicopter()
{
	for (int i = 0; i < NUM_BONES; i++)
		Server()->SnapFreeID(m_aBones[i].m_ID);
	for (int i = 0; i < NUM_TRAILS; i++)
		Server()->SnapFreeID(m_aTrails[i].m_ID);
}

void CHelicopter::Reset()
{
	Dismount();
	GameWorld()->DestroyEntity(this);
}

void CHelicopter::Tick()
{
	CAdvancedEntity::Tick();
	HandleDropped();

	if (GetOwner())
	{
		GetOwner()->ForceSetPos(m_Pos);
		GetOwner()->Core()->m_Vel = vec2(0, 0);

		m_Gravity = GetOwner()->m_FreezeTime;
		m_GroundVel = GetOwner()->m_FreezeTime || m_Accel.x == 0.f; // when on floor and not moving
	}

	ApplyAcceleration();
	SpinPropellers();

	m_PrevPos = m_Pos;
}

void CHelicopter::OnInput(CNetObj_PlayerInput *pNewInput)
{
	if (!GetOwner() || GetOwner()->m_FreezeTime)
	{
		m_Accel = vec2(0.f, 0.f);
		return;
	}

	m_Accel.x = pNewInput->m_Direction;

	bool Rise = pNewInput->m_Jump;
	bool Sink = pNewInput->m_Hook;
	if (Rise == Sink)
		m_Accel.y = 0.f;
	else
		m_Accel.y = Rise ? -1 : 1;
}

void CHelicopter::ApplyAcceleration()
{
	m_Vel.x += m_Accel.x * 0.4f;
	m_Vel.y += m_Accel.y * 0.5f;
	m_Vel.y *= 0.95f;

	if ((m_Vel.x < 0.f && !m_Flipped) || (m_Vel.x > 0.f && m_Flipped))
		Flip();
	SetAngle(m_Vel.x);
}

void CHelicopter::SpinPropellers()
{
	if (Server()->Tick() % 2 != 0 || (!GetOwner() && IsGrounded()))
		return;

	for (int i = 0; i < NUM_BONES_PROPELLERS_BACK; i++)
		BackPropeller()[i].m_From = rotate_around_point(BackPropeller()[i].m_From, BackPropeller()[i].m_To, 50.f / pi);

	float Len = 100.f;
	float curLen = clamp((float)sin(Server()->Tick() / 5) * Len, -Len, Len);
	vec2 Diff = TopPropeller()[0].m_From - TopPropeller()[0].m_To;
	Diff = normalize(Diff) * curLen;
	for (int i = 0; i < NUM_BONES_PROPELLERS_TOP; i++)
	{
		TopPropeller()[i].m_From = TopPropeller()[i].m_To + Diff;
		Diff *= -1;
	}
}

bool CHelicopter::Mount(int ClientID)
{
	if (m_Owner != -1)
		return false;

	m_Gravity = false;
	m_GroundVel = false;
	m_Owner = ClientID;
	if (GetOwner())
	{
		GetOwner()->m_pHelicopter = this;
		GameServer()->SendTuningParams(m_Owner, GetOwner()->m_TuneZone);
	}
	return true;
}

void CHelicopter::Dismount()
{
	if (m_Owner == -1)
		return;

	m_Gravity = true;
	m_GroundVel = true;
	if (GetOwner())
	{
		GetOwner()->m_pHelicopter = 0;
		GameServer()->SendTuningParams(m_Owner, GetOwner()->m_TuneZone);
	}
	m_Owner = -1;
}

void CHelicopter::Flip()
{
	m_Flipped = !m_Flipped;
	m_Angle *= -1.f;
	for (int i = 0; i < NUM_BONES; i++)
		m_aBones[i].Flip();
}

void CHelicopter::Rotate(float Angle)
{
	m_Angle += Angle;
	for (int i = 0; i < NUM_BONES; i++)
		m_aBones[i].Rotate(Angle);
}

void CHelicopter::SetAngle(float Angle)
{
	Rotate(-m_Angle);
	Rotate(Angle);
}

void CHelicopter::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	if (GameServer()->GetPlayerChar(SnappingClient))
	{
		if (GetOwner() && !CmaskIsSet(GetOwner()->TeamMask(), SnappingClient))
			return;
	}

	for (int i = 0; i < NUM_BONES; i++)
		SnapBone(m_aBones[i]);

	if (GetOwner() || !IsGrounded())
		for (int i = 0; i < NUM_TRAILS; i++)
			SnapTrail(m_aTrails[i]);
}

void CHelicopter::SnapBone(SBone Bone)
{
	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, Bone.m_ID, sizeof(CNetObj_Laser)));
	if (!pObj)
		return;

	pObj->m_X = round_to_int(m_Pos.x + Bone.m_To.x);
	pObj->m_Y = round_to_int(m_Pos.y + Bone.m_To.y);
	pObj->m_FromX = round_to_int(m_Pos.x + Bone.m_From.x);
	pObj->m_FromY = round_to_int(m_Pos.y + Bone.m_From.y);
	pObj->m_StartTick = Server()->Tick()-1;
}

void CHelicopter::SnapTrail(STrail Trail)
{
	CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, Trail.m_ID, sizeof(CNetObj_Projectile)));
	if (!pObj)
		return;

	pObj->m_X = round_to_int(m_Pos.x + Trail.m_pPos->x);
	pObj->m_Y = round_to_int(m_Pos.y + Trail.m_pPos->y);
	pObj->m_VelX = 0;
	pObj->m_VelY = 0;
	pObj->m_StartTick = Server()->Tick();
	pObj->m_Type = WEAPON_HAMMER; 
}

void CHelicopter::InitBody()
{
	SBone aBones[NUM_BONES_BODY] = {
		SBone(70, 45, 50, 60),
		SBone(-55, 60, 50, 60),
		SBone(-25, 40, -30, 60),
		SBone(25, 40, 30, 60),
		SBone(0, -40, 0, -60),
		SBone(35, 40, -35, 40),
		SBone(60, 10, 35, 40),
		SBone(25, -40, 60, 10),
		SBone(-35, -40, 25, -40),
		SBone(-45, 0, -35, -40),
		SBone(-100, 0, -45, 0),
		SBone(-120, -30, -100, 0),
		SBone(-105, 20, -120, -30),
		SBone(-35, 40, -105, 20)
	}; // 0-4: base, 5: propeller bottom, 6-10: body, 11-13: tail
	mem_copy(Body(), aBones, sizeof(SBone)*NUM_BONES_BODY);
}

void CHelicopter::InitPropellers()
{
	float Radius = 100.f;
	for (int i = 0; i < NUM_BONES_PROPELLERS_TOP; i++)
	{
		TopPropeller()[i] = SBone(vec2(Radius, -60.f), vec2(0, -60.f));
		Radius *= -1;
	}

	Radius = 25.f;
	for (int i = 0; i < NUM_BONES_PROPELLERS_BACK; i++)
	{
		BackPropeller()[i] = SBone(vec2(-110.f+Radius, -10.f), vec2(-110.f, -10.f));
		Radius *= -1;
		m_aTrails[i].m_pPos = &BackPropeller()[i].m_From;
	}
}
