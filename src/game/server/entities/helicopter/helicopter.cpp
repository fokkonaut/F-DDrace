// made by fokkonaut

#include "engine/server.h"
#include "game/server/gamecontext.h"
#include "helicopter.h"

bool MovingCircleHitsMovingSegment_Analytical(
	vec2 circleLast, vec2 circleNow, float radius,
	vec2 lineLastA, vec2 lineNowA,
	vec2 lineLastB, vec2 lineNowB)
{
	// Step 1: Relative motion
	vec2 circleVel = circleNow - circleLast;
	vec2 lineVelA = lineNowA - lineLastA;
	vec2 lineVelB = lineNowB - lineLastB;
	vec2 lineVel = (lineVelA + lineVelB) * 0.5f;
	vec2 relVel = circleVel - lineVel;

	vec2 C0 = circleLast;
//	vec2 C1 = C0 + relVel;

	vec2 L0 = lineLastA;
	vec2 L1 = lineLastB;
	vec2 d = L1 - L0;
	vec2 m = C0 - L0;

	float a = dot(relVel, relVel);
	float b = dot(relVel, d);
	float c = dot(d, d);
	float d1 = dot(relVel, m);
	float e = dot(d, m);

	float denom = a * c - b * b;

	float t, s;

	if (denom != 0.0f)
	{
		t = (b * e - c * d1) / denom;
	}
	else
	{
		t = 0.0f; // Parallel
	}

	t = clamp(t, 0.0f, 1.0f);

	// Get point on circle path at time t
	vec2 circlePosAtT = C0 + relVel * t;

	// Find closest point on segment
	float segmentLenSq = dot(d, d);
	if (segmentLenSq != 0.0f)
	{
		s = clamp(dot(circlePosAtT - L0, d) / segmentLenSq, 0.0f, 1.0f);
	}
	else
	{
		s = 0.0f;
	}

	vec2 closestOnSegment = L0 + d * s;

	vec2 diff = circlePosAtT - closestOnSegment;
	float distSq = dot(diff, diff);

	return distSq <= radius * radius;
}

CHelicopter::CHelicopter(CGameWorld *pGameWorld, int Team, vec2 Pos, float HelicopterScale)
	: CAdvancedEntity(pGameWorld, CGameWorld::ENTTYPE_HELICOPTER, Pos, vec2(80, 128))
{
	m_AllowVipPlus = false;
	m_Elasticity = 0.f;
	m_DDTeam = Team;

	m_InputDirection = 0;
	m_Health = 100.f;
	m_EngineOn = false;

	m_Scale = 1.f;
	m_Flipped = false;
	m_Angle = 0.f;
	m_Accel = vec2(0.f, 0.f);

	m_pTurret = nullptr;

	memset(m_aFlungCharacters, 0, sizeof(m_aFlungCharacters));
	m_BackPropellerRadius = 25.f;
	m_TopPropellerRadius = 100.f;

	m_ExplosionsLeft = -1;

	InitBody();
	InitPropellers();
	GetFullPropellerPositions(m_LastTopPropellerA, m_LastTopPropellerB);

	SortBones();
	ApplyScale(0.8f);
	ApplyScale(HelicopterScale); // from before (example: HelicopterScale = 1.f results in 0.8f * 1.f)

	GameWorld()->InsertEntity(this);
}

CHelicopter::~CHelicopter()
{
	for (int i = 0; i < NUM_BONES; i++)
		Server()->SnapFreeID(m_aBones[i].m_ID);
	for (int i = 0; i < NUM_TRAILS; i++)
		Server()->SnapFreeID(m_aTrails[i].m_ID);
	DestroyTurret();
}

void CHelicopter::Reset()
{
	Dismount();
	GameWorld()->DestroyEntity(this);
}

bool CHelicopter::AttachTurret(CVehicleTurret *helicopterTurret)
{
	DestroyTurret();

	if (helicopterTurret == nullptr) // Just remove the current turret
		return true;

	if (helicopterTurret->TryBindHelicopter(this)) // Attempt to pass ownership
	{
		m_pTurret = helicopterTurret;
		SortBones();
		return true;
	}
	return false;
}

void CHelicopter::DestroyTurret()
{
	delete m_pTurret;
	m_pTurret = nullptr;
}

void CHelicopter::FlingTee(CCharacter *pChar)
{
	if (!pChar->IsAlive())
		return;

	GameServer()->CreateSound(pChar->GetPos(), SOUND_PLAYER_PAIN_SHORT, pChar->TeamMask());
	GameServer()->CreateDeath(pChar->GetPos(), pChar->GetPlayer()->GetCID(), pChar->TeamMask());
	pChar->SetEmote(EMOTE_PAIN, Server()->Tick() + 500 * Server()->TickSpeed() / 1000);

	float helicopterVelocity = length(m_Vel);
	float teeVelocity = length(pChar->GetCore().m_Vel);

	vec2 directionAwayFromBlades = normalize(pChar->m_PrevPos - m_Pos);
	// Known at compile time
	constexpr float teeMass = 10.f;
	constexpr float helicopterMass = 18.f;
	constexpr float transferForceTee = helicopterMass / teeMass; // POOR THING :SKULL: 💀
	constexpr float transferForceHelicopter = teeMass / helicopterMass;
	//
	float totalVelocity = clamp((helicopterVelocity + teeVelocity) * 0.75f, 5.f, 25.f);
	vec2 teeAcceleration = directionAwayFromBlades * transferForceTee * totalVelocity;
	vec2 helicopterAcceleration = -directionAwayFromBlades * transferForceHelicopter * totalVelocity;

	pChar->TakeDamage(m_Vel * max(0.001f, 0.f), m_Vel * -1, 1.f, m_Owner, WEAPON_PLAYER);

	pChar->SetCoreVel(pChar->GetCore().m_Vel + teeAcceleration);
	m_Vel += helicopterAcceleration;
}

void CHelicopter::ApplyScale(float HelicopterScale)
{
	// Experimental

	GetProximityRadius();
	m_Scale *= HelicopterScale;

	m_Size *= HelicopterScale;
	m_BackPropellerRadius *= HelicopterScale;
	m_TopPropellerRadius *= HelicopterScale;
	for (SBone& Bone : m_aBones)
		Bone.Scale(HelicopterScale);
	//
}

void CHelicopter::Explode()
{
	m_ExplosionsLeft = 6;

	Dismount();

	// Freeze characters near explosion
	CCharacter *aVictims[20];
	int numFound = GameWorld()->FindEntities(m_Pos, min(GetSize().x, GetSize().y) / 2, (CEntity **)aVictims, 20, CGameWorld::ENTTYPE_CHARACTER, m_DDTeam);
	if (!numFound)
		return;

	for (int i = 0; i < numFound; i++)
	{
		CCharacter* pChar = aVictims[i];
		pChar->Freeze(3);
	}
}

void CHelicopter::Tick()
{
	if (!IsExploding())
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

		FlingTeesInPropellersPath();
		ApplyAcceleration();
		SpinPropellers();

		if (m_pTurret)
			m_pTurret->Tick();

		if (m_Health <= 0.f)
			Explode();
	}

	// Entity destroyed at m_ExplosionsLeft == 0
	HandleExplosions();

	m_PrevPos = m_Pos;
	GetFullPropellerPositions(m_LastTopPropellerA, m_LastTopPropellerB);
}

void CHelicopter::OnInput(CNetObj_PlayerInput *pNewInput)
{
	// Movement controls
	if (!GetOwner() || GetOwner()->m_FreezeTime)
	{
		m_Accel = vec2(0.f, 0.f);
		return;
	}

	m_InputDirection = pNewInput->m_Direction;
	m_Accel.x = (float)pNewInput->m_Direction;

	bool Rise = pNewInput->m_Jump;
	bool Sink = pNewInput->m_Hook;
	if (Rise == Sink)
		m_Accel.y = 0.f;
	else
		m_Accel.y = Rise ? -1 : 1;

	// Weapon controls
	if (m_pTurret)
		m_pTurret->OnInput(pNewInput);
}

void CHelicopter::ApplyAcceleration()
{
	if (!m_EngineOn)
		m_Accel = vec2(0.f, 0.f);

	float strafeFactor = (m_Flipped == (m_Vel.x > 0.f)) ? 0.4f : 1.f; // Accelerate slower when moving backwards
	m_Vel.x += m_Accel.x * 0.4f * strafeFactor;
	m_Vel.y += m_Accel.y * 0.5f;
	m_Vel.y *= 0.95f;

	// Prevent flipping when not going the opposite direction OR when shooting the opposite direction
	if (((m_InputDirection == -1 && !m_Flipped && m_Vel.x < 0.f) ||
		(m_InputDirection == 1 && m_Flipped && m_Vel.x > 0.f)) &&
		(!m_pTurret || !m_pTurret->m_Shooting ||
		(m_Flipped != (m_pTurret->m_AimPosition.x < 0.f))))
		Flip();

	SetAngle(m_Vel.x);
}

void CHelicopter::FlingTeesInPropellersPath()
{
	if (!m_EngineOn || !GetOwner()) // Eh i th
		return;

	CCharacter *aPossibleCollisions[10];
	int numFound = GameWorld()->FindEntities(m_Pos, m_TopPropellerRadius + 200.f, (CEntity **)aPossibleCollisions, 10, CGameWorld::ENTTYPE_CHARACTER, m_DDTeam);
	if (!numFound)
		return;

	for (int i = 0; i < numFound; i++)
	{
		CCharacter *pChar = aPossibleCollisions[i];
		if (pChar == GetOwner())
			continue;

		int cID = pChar->GetPlayer()->GetCID();
		if (Server()->Tick() - m_aFlungCharacters[cID] <= 10)
			continue;

		vec2 propellerPosA, propellerPosB;
		GetFullPropellerPositions(propellerPosA, propellerPosB);
		bool collisionDetected = MovingCircleHitsMovingSegment_Analytical(
			pChar->m_PrevPos - m_Pos,pChar->GetPos() - m_Pos,
			pChar->GetProximityRadius(),
			m_LastTopPropellerA, propellerPosA,
			m_LastTopPropellerB, propellerPosB);
		if (collisionDetected)
		{
			m_aFlungCharacters[cID] = Server()->Tick();
			FlingTee(pChar);
		}
	}

}

void CHelicopter::GetFullPropellerPositions(vec2& outPosA, vec2& outPosB)
{
	vec2 bladeSpan = normalize(TopPropeller()[0].m_To - TopPropeller()[0].m_From) * m_TopPropellerRadius;
	outPosA = TopPropeller()[0].m_To + bladeSpan;
	outPosB = TopPropeller()[1].m_To - bladeSpan;
}

void CHelicopter::HandleExplosions()
{
	if (m_ExplosionsLeft < 0 || Server()->Tick() % 5 != 0)
		return;

	int Diameter = (int)min(GetSize().x, GetSize().y) + 150; // 150 seems fine minimum range for the explosion
	int Radius = Diameter / 2;
	for (int i = 0; i < 3; i++)
	{
		vec2 nearbyPos = m_Pos + vec2((float)(rand() % Diameter - Radius), (float)(rand() % Diameter - Radius));
		GameServer()->CreateExplosion(nearbyPos,
			m_Owner,
			WEAPON_GRENADE,
			m_Owner == -1,
			m_DDTeam, m_TeamMask);
		GameServer()->CreateSound(nearbyPos, SOUND_GRENADE_EXPLODE, m_TeamMask);
	}

	// F-DDrace
//    if (pTargetChr)
//        pTargetChr->TakeDamage(m_Vel * max(0.001f, 0.f), m_Vel * -1, 69.f, m_Owner, WEAPON_PLAYER);

	if (m_ExplosionsLeft == 0)
		GameWorld()->DestroyEntity(this);

	m_ExplosionsLeft--;
}

void CHelicopter::SpinPropellers()
{
	if (!m_EngineOn || Server()->Tick() % 2 != 0)
		return;

	if (!GetOwner() && IsGrounded()) // Reset top propellers when grounded without driver
	{
		if (m_EngineOn)
			ResetAndTurnOff();
		return;
	}

	for (int i = 0; i < NUM_BONES_PROPELLERS_BACK; i++)
		BackPropeller()[i].m_From = rotate_around_point(BackPropeller()[i].m_From, BackPropeller()[i].m_To, 50.f / pi);

	float Len = m_TopPropellerRadius;
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
	if (m_Owner != -1 || IsExploding())
		return false;

	m_EngineOn = true;
	m_Gravity = false;
	m_GroundVel = false;
	m_Owner = ClientID;
	if (GetOwner())
	{
		GetOwner()->m_pHelicopter = this;
		GetOwner()->SetWeapon(-1);
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
	m_Accel.y = 0;
	if (GetOwner())
	{
		GetOwner()->m_pHelicopter = nullptr;
		GetOwner()->SetWeapon(GetOwner()->GetLastWeapon());
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

	if (m_pTurret)
		m_pTurret->SetFlipped(m_Flipped);
}

void CHelicopter::Rotate(float Angle)
{
	m_Angle += Angle;
	for (int i = 0; i < NUM_BONES; i++)
		m_aBones[i].Rotate(Angle);

	if (m_pTurret)
		m_pTurret->Rotate(Angle);
}

void CHelicopter::SetAngle(float Angle)
{
	Rotate(-m_Angle);
	Rotate(Angle);
}

void CHelicopter::Snap(int SnappingClient)
{
	if (IsExploding())
		return;

	if (NetworkClipped(SnappingClient) || !CmaskIsSet(m_TeamMask, SnappingClient))
		return;

	// Draw body
	for (SBone& Bone : m_aBones)
		Bone.Snap(SnappingClient);

	// Draw back particles
	if (GetOwner() || !IsGrounded())
		for (STrail& m_aTrail : m_aTrails)
			m_aTrail.Snap(SnappingClient);

	// Draw guns
	if (m_pTurret)
		m_pTurret->Snap(SnappingClient);
}

void CHelicopter::SortBones()
{ // meant for adding turret later, not specifically at initialization
	int numBones = NUM_BONES;
	if (m_pTurret)
		numBones += m_pTurret->GetNumBones();

	// Get current IDs
	int *apIDs = new int[numBones];
	for (int i = 0; i < NUM_BONES; i++)
		apIDs[i] = m_aBones[i].m_ID;
	if (m_pTurret)
		for (int i = 0; i < m_pTurret->GetNumBones(); i++)
			apIDs[NUM_BONES + i] = m_pTurret->Bones()[i].m_ID;

	// Sort IDs
	std::sort(apIDs, apIDs + numBones);

	// Update sorted IDs (low -> high | ascending)
	for (int i = 0; i < NUM_BONES; i++)
		m_aBones[i].m_ID = apIDs[i];
	if (m_pTurret)
		for (int i = 0; i < m_pTurret->GetNumBones(); i++)
			m_pTurret->Bones()[i].m_ID = apIDs[NUM_BONES + i];
	delete[] apIDs;
}

void CHelicopter::InitBody()
{
	SBone aBones[NUM_BONES_BODY] = {
		// Base
		SBone(this, Server()->SnapNewID(), 70, 45, 50, 60, 4),
		SBone(this, Server()->SnapNewID(), -55, 60, 50, 60, 4),
		SBone(this, Server()->SnapNewID(), -25, 40, -30, 60, 3),
		SBone(this, Server()->SnapNewID(), 25, 40, 30, 60, 3),
		SBone(this, Server()->SnapNewID(), 0, -40, 0, -60, 4),
		// Top propeller rotor
		SBone(this, Server()->SnapNewID(), 35, 40, -35, 40, 3),
		// Body
		SBone(this, Server()->SnapNewID(), 60, 10, 35, 40, 4),
		SBone(this, Server()->SnapNewID(), 25, -40, 60, 10, 4),
		SBone(this, Server()->SnapNewID(), -35, -40, 25, -40, 4),
		SBone(this, Server()->SnapNewID(), -45, 0, -35, -40, 4),
		SBone(this, Server()->SnapNewID(), -100, 0, -45, 0, 4),
		// Tail
		SBone(this, Server()->SnapNewID(), -120, -30, -100, 0, 4),
		SBone(this, Server()->SnapNewID(), -105, 20, -120, -30, 4),
		SBone(this, Server()->SnapNewID(), -35, 40, -105, 20, 4),
	};
	mem_copy(Body(), aBones, sizeof(SBone) * NUM_BONES_BODY);
}

void CHelicopter::InitPropellers()
{
	float Radius = m_TopPropellerRadius;
	for (int i = 0; i < NUM_BONES_PROPELLERS_TOP; i++)
	{
		TopPropeller()[i] = SBone(this, Server()->SnapNewID(), vec2(Radius, -60.f), vec2(0, -60.f), 3);
		TopPropeller()[i].m_Color = LASERTYPE_DOOR;
		Radius *= -1;
	}

	Radius = m_BackPropellerRadius;
	for (int i = 0; i < NUM_BONES_PROPELLERS_BACK; i++)
	{
		BackPropeller()[i] = SBone(this, Server()->SnapNewID(), vec2(-110.f + Radius, -10.f), vec2(-110.f, -10.f), 3);
		Radius *= -1;
		m_aTrails[i] = STrail(this, Server()->SnapNewID(), &BackPropeller()[i].m_From);
	}
}

void CHelicopter::ResetAndTurnOff()
{
	// Dismounted & grounded
	m_EngineOn = false;

	// Reset propellers to full length
	float Radius = m_TopPropellerRadius;
	vec2 Direction = normalize(TopPropeller()[0].m_From - TopPropeller()[0].m_To);
	for (int i = 0; i < NUM_BONES_PROPELLERS_TOP; i++)
	{
		TopPropeller()[i].m_From = TopPropeller()[i].m_To + Direction * Radius;
		Radius *= -1;
	}
}