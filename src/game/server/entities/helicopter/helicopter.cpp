// made by fokkonaut and Matq, somewhere around 2021 and in 2025

#include "engine/server.h"
#include "game/server/gamecontext.h"
#include "helicopter.h"
#include "generated/server_data.h"

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

CHelicopter::CHelicopter(CGameWorld *pGameWorld, int Spawner, int Team, vec2 Pos, float HelicopterScale, bool Build, int Number, int DelayTurretType)
	: CAdvancedEntity(pGameWorld, CGameWorld::ENTTYPE_HELICOPTER, Pos, HELICOPTER_PHYSSIZE * HelicopterScale)
{
	m_AllowVipPlus = false;
	m_Elasticity = 0.f;
	m_DDTeam = Team;

	m_Number = Number;
	m_DelayTurretType = DelayTurretType;
	m_NextSpawnTick = 0;
	m_InitialPosition = Pos;

	m_SpawnTick = -1;
	if (PlacedByTile())
	{
		m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * Config()->m_SvHeliRespawnTime;
		m_Layer = LAYER_SWITCH; // unused rn, but for completeness
	}

	m_InputDirection = 0;
	m_MaxHealth = 60.f;
	m_Health = m_MaxHealth;
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

	m_ShowHeartsUntil = 0;
	m_LastDamage = 0;
	m_LastEnvironmentalDamage = 0;
	m_LastKnownOwner = Spawner;

	m_BroadcastingTick = 0;

	m_Build = Build;
	m_BuildHeight = 0.f;
	m_BuildTop = 0.f;
	m_BuildBottom = 0.f;
	m_BuildTotalHeight = 0.f;

	InitBody();
	InitPropellers();
	GetFullPropellerPositions(m_LastTopPropellerA, m_LastTopPropellerB);

	InitBuild();
	InitHearts();

	// Order matters
	if (Build)
		InitUnbuilt();

	SortBones();

	HelicopterScale = clamp(HelicopterScale, 0.8f, 5.f);
	ApplyScale(HELICOPTER_DEFAULT_SCALE * HelicopterScale);

	GameWorld()->InsertEntity(this);
}

CHelicopter::~CHelicopter()
{
	for (int i = 0; i < NUM_BONES; i++)
		Server()->SnapFreeID(m_aBones[i].m_ID);
	for (int i = 0; i < NUM_TRAILS; i++)
		Server()->SnapFreeID(m_aTrails[i].m_ID);
	for (int i = 0; i < NUM_HEARTS; i++)
		Server()->SnapFreeID(m_aHearts[i].m_ID);
	for (int i = 0; i < NUM_BUILD_IDS; i++)
		if (m_aBuildIDs[i] != -1) // should only pass if deleted while crafting a helicopter
			Server()->SnapFreeID(m_aBuildIDs[i]);
	DestroyTurret();
}

void CHelicopter::Reset()
{
	Dismount();
	CAdvancedEntity::Reset();

	TryRespawnNewHelicopter();
}

bool CHelicopter::TryRespawnNewHelicopter()
{
	if (!PlacedByTile())
		return false;
	return GameServer()->SpawnHelicopter(-1, 0, m_InitialPosition, m_DelayTurretType, 1.f, false, m_Number);
}

bool CHelicopter::IsRegenerating()
{
	return m_Health != m_MaxHealth && m_LastDamage && m_LastDamage + Server()->TickSpeed() * 10 < Server()->Tick();
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
	m_Scale *= HelicopterScale;

//	m_Size *= HelicopterScale; // done in CHelicopter() : m_Size()
	m_BackPropellerRadius *= HelicopterScale;
	m_TopPropellerRadius *= HelicopterScale;
	for (SBone& Bone : m_aBones)
		Bone.Scale(HelicopterScale);

	m_BuildTop *= HelicopterScale;
	m_BuildBottom *= HelicopterScale;
	m_BuildLeft *= HelicopterScale;
	m_BuildRight *= HelicopterScale;

	m_BuildTotalWidth *= HelicopterScale;
	m_BuildTotalHeight *= HelicopterScale;

	m_BuildHeight *= HelicopterScale;
	//

	UpdateHearts();
}

void CHelicopter::Explode()
{
	m_ExplosionsLeft = 6;

	Dismount();

	// Freeze characters near explosion
	CCharacter *aVictims[MAX_CLIENTS];
	int numFound = GameWorld()->FindEntities(m_Pos, GetProximityRadius(), (CEntity **)aVictims, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER, m_DDTeam);
	if (!numFound)
		return;

	for (int i = 0; i < numFound; i++)
	{
		CCharacter *pChar = aVictims[i];
		pChar->m_FreezeTime = 0; // refreezing if exploded while tasered
		pChar->m_FreezeTick = 0;
		pChar->Freeze(3);
	}
}

void CHelicopter::TakeDamage(float Damage, vec2 HitPos, int FromID)
{
	if (IsBuilding() || IsExploding() || IsSpawning())
		return;

	m_Health -= Damage;
	m_ShowHeartsUntil = Server()->Tick() + Server()->TickSpeed() * 3;
	m_LastDamage = Server()->Tick();

	GameServer()->CreateSound(HitPos, SOUND_PLAYER_DIE, m_TeamMask);
	GameServer()->CreateDeath(HitPos, FromID);
}

void CHelicopter::ExplosionDamage(float Strength, vec2 Pos, int FromID)
{
	if (IsBuilding() || IsExploding() || IsSpawning()) // like this check only prevents changing mHealth
		return;

	// gamecontext.cpp : createxplosion
	float Radius = g_pData->m_Explosion.m_Radius;
	float InnerRadius = 48.0f;

	float DistanceFromExplosion = distance(m_Pos, Pos) - GetProximityRadius();
	float Close = 1 - clamp((DistanceFromExplosion - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);
	float Damage = Close * Strength;

	m_Health -= Damage;
	m_ShowHeartsUntil = Server()->Tick() + Server()->TickSpeed() * 3;
	m_LastDamage = Server()->Tick();

	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_DIE, m_TeamMask);
	GameServer()->CreateDeath(m_Pos, FromID);
}

void CHelicopter::Tick()
{
	CAdvancedEntity::Tick();

	if (m_LastKnownOwner >= 0 && !GameServer()->m_apPlayers[m_LastKnownOwner])
		m_LastKnownOwner = -1;

	if (PlacedByTile() && IsSpawning())
	{
		CCollision::SSwitchers *pSwitcher = m_Number > 0 ? &GameServer()->Collision()->m_pSwitchers[m_Number] : 0;
		if (pSwitcher && !pSwitcher->m_Status[0]) // always use team 0, we dont have management for other teams right now for tile-based helis
		{
			m_SpawnTick++;
		}

		if(Server()->Tick() > m_SpawnTick)
		{
			// respawn
			m_SpawnTick = -1;
			GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, m_TeamMask);
		}
		return;
	}

	BuildHelicopter();

	if (!IsBuilding() && !IsExploding())
	{
		HandleDropped();

		if (GetOwner())
		{
			GetOwner()->ForceSetPos(m_Pos);
			GetOwner()->Core()->m_Vel = vec2(0, 0);

			m_Gravity = GetOwner()->m_FreezeTime;
			m_GroundVel = GetOwner()->m_FreezeTime || m_Accel.x == 0.f; // when on floor and not moving

			if (GetOwner()->m_DeepFreeze)
				Dismount();
		}

		FlingTeesInPropellersPath();
		ApplyAcceleration();
		SpinPropellers();

		DamageInWall(); // just in case
		DamageInFreeze();

		UpdateHearts();
		RegenerateHelicopter();
		UpdateVisualDamage();

		if (m_pTurret)
			m_pTurret->Tick();

		if (m_Health <= 0.f)
			Explode();

		SendBroadcastIndicator();
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

	if (GetOwner())
	{ // Hook acceleration
		for (int Hooker : GetOwner()->Core()->m_AttachedPlayers)
		{
			CCharacter* pChr = GameServer()->GetPlayerChar(Hooker);
			if (!pChr)
				continue;

			float Distance = distance(pChr->GetPos(), m_Pos);
			vec2 Dir = normalize(pChr->GetPos() - m_Pos);

			if (Distance > GetProximityRadius() + CCharacterCore::PHYS_SIZE)
			{
				float Accel = pChr->Tuning()->m_HookDragAccel * (Distance / pChr->Tuning()->m_HookLength);
				float DragSpeed = pChr->Tuning()->m_HookDragSpeed;

				vec2 Temp;
				Temp.x = SaturatedAdd(-DragSpeed, DragSpeed, m_Vel.x, Accel * Dir.x * 0.025f);
				Temp.y = SaturatedAdd(-DragSpeed, DragSpeed, m_Vel.y, Accel * Dir.y * 0.05f);
				m_Vel = ClampVel(m_MoveRestrictions, Temp);
			}
		}
	}

	float strafeFactor = (m_Flipped == (m_Vel.x > 0.f)) ? 0.4f : 1.f; // Accelerate slower when moving backwards
	m_Vel.x += m_Accel.x * 0.6f * strafeFactor;
	m_Vel.y += m_Accel.y * 0.75f;
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
			pChar->m_PrevPos - m_Pos, pChar->GetPos() - m_Pos,
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
		Reset();

	m_ExplosionsLeft--;
}

void CHelicopter::InitHearts()
{
	for (int i = 0; i < NUM_HEARTS; i++)
		m_aHearts[i] = SHeart(this, Server()->SnapNewID(), vec2(0.f, 0.f));

	UpdateHearts();
}

void CHelicopter::UpdateHearts()
{
	float HealthPercentage = m_Health / m_MaxHealth; // Health 0.0 to 1.0
	float HeartsPrecise = NUM_HEARTS * HealthPercentage; // Hearts like 3.7
	int HeartsFull = floor(HeartsPrecise); // Hearts like 3
	int ShowNumHearts = ceil(HeartsPrecise); // How many are shown, including fraction heart (3 + 0.7 = 4)

	float CurrentHeart = HeartsPrecise - (float)HeartsFull; // Last heart 0.0 to 1.0, example 0.7
	bool FlashingHalfHeart = CurrentHeart > 0.0f && CurrentHeart <= 0.5f; // Flash or not, if last heart below half
	for (int i = 0; i < NUM_HEARTS; i++)
	{
		if (i < ShowNumHearts - FlashingHalfHeart) //
			m_aHearts[i].m_Enabled = true;
		else if (i == HeartsFull && FlashingHalfHeart) // Flash heart if last was below 0.5
			m_aHearts[i].m_Enabled = (Server()->Tick() / 4) % 2 == 0;
		else
			m_aHearts[i].m_Enabled = false;
	}

	float Gap = 40.f;
	float HeartsY = m_BuildTop - 30.f;
	float HeartsX = -(float)(ShowNumHearts - 1) / 2.f * Gap;
	for (int i = 0; i < ShowNumHearts; i++)
	{
		m_aHearts[i].m_Pos.x = HeartsX;
		m_aHearts[i].m_Pos.y = HeartsY;
		HeartsX += Gap;
	}
}

void CHelicopter::RegenerateHelicopter()
{
	if (!IsRegenerating())
		return;

	m_Health += 0.02f;
	if (m_Health > m_MaxHealth)
		m_Health = m_MaxHealth;
}

void CHelicopter::UpdateVisualDamage()
{
	float HealthPercentage = 1.f - m_Health / m_MaxHealth;
	for (int i = 0; i < NUM_BONES; i++)
		m_aBones[i].m_Thickness = m_aBones[i].m_InitThickness - (int)(sinf(Server()->Tick() / 4 + i) * HealthPercentage * (float)m_aBones[i].m_InitThickness);
}

void CHelicopter::DamageInFreeze()
{
	if (m_LastEnvironmentalDamage && m_LastEnvironmentalDamage + Server()->TickSpeed() > Server()->Tick())
		return;

	if ((((m_TileIndex == TILE_FREEZE) || (m_TileFIndex == TILE_FREEZE)) && // In freeze tile
		(!GetOwner() || (GetOwner() && !GetOwner()->m_Super))))
	{
		TakeDamage(2, m_Pos, m_LastKnownOwner);
		m_LastEnvironmentalDamage = Server()->Tick();
	}
}

void CHelicopter::DamageInWall()
{
	if ((!m_LastEnvironmentalDamage || m_LastEnvironmentalDamage + Server()->TickSpeed() < Server()->Tick()) && // Eligible to take freeze damage again
		GameServer()->Collision()->TestBoxBig(m_Pos, m_Size))
	{
		TakeDamage(20, m_Pos, m_LastKnownOwner);
		m_LastEnvironmentalDamage = Server()->Tick();
	}
}

void CHelicopter::SendBroadcastIndicator()
{
	if (!GetOwner() || (Server()->Tick() - m_BroadcastingTick) % Server()->TickSpeed() != 0)
		return;

	char aMsg[128];
	const char *RegenerationText = IsRegenerating() ? " +1regen" : "";
	str_format(aMsg, sizeof(aMsg), "> Helicopter <\nHealth [%d]%s", (int)m_Health, RegenerationText);
	GetOwner()->SendBroadcastHud(aMsg);
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
	if (m_Owner != -1 || IsExploding() || IsBuilding())
		return false;

	// scale specific mount condition
	CCharacter *pCharacter = GameServer()->GetPlayerChar(ClientID);
	if (distance(pCharacter->GetPos(), m_Pos) > pCharacter->GetProximityRadius() + GetProximityRadius())
		return false;

	m_ShowHeartsUntil = Server()->Tick() + Server()->TickSpeed() * 3;
	m_EngineOn = true;
	m_Gravity = false;
	m_GroundVel = false;
	m_Owner = ClientID;
	m_LastKnownOwner = ClientID;
	if (GetOwner())
	{
		GetOwner()->m_pHelicopter = this;
		GetOwner()->SetWeapon(-1);
		GameServer()->SendTuningParams(m_Owner, GetOwner()->m_TuneZone);

		m_BroadcastingTick = Server()->Tick() + 1; // Start updating broadcast next tick
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
		GetOwner()->SendBroadcastHud(""); // ?
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
	if (IsExploding() || IsSpawning())
		return;

	if (NetworkClipped(SnappingClient) || !CmaskIsSet(m_TeamMask, SnappingClient))
		return;

	if (IsBuilding())
	{
		for (int i = 0; i < NUM_BUILD_IDS; i++)
		{
			CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_aBuildIDs[i], sizeof(CNetObj_Projectile)));
			if (!pObj)
				continue;

			pObj->m_X = round_to_int(m_Pos.x + m_BuildLeft + rand() % (int)m_BuildTotalWidth);
			pObj->m_Y = round_to_int(m_Pos.y + m_BuildHeight);
			pObj->m_VelX = 0;
			pObj->m_VelY = 0;
			pObj->m_StartTick = Server()->Tick();
			pObj->m_Type = WEAPON_HAMMER;
		}
	}
	else if ((m_Owner == -1 || m_Owner == SnappingClient) && // Show hearts when no driver or only to driver
		((m_ShowHeartsUntil && m_ShowHeartsUntil > Server()->Tick()) || // Show until time specified (on mount, damage, etc.)
			m_Health != m_MaxHealth) && // Show while not full health
		(!m_LastDamage || ((m_LastDamage && (m_LastDamage + Server()->TickSpeed() / 2 <= Server()->Tick())) || // If been damaged too long ago, show normally
			(m_LastDamage + Server()->TickSpeed() / 2 > Server()->Tick() && (Server()->Tick() / 4) % 2 == 0)))) // If been damaged recently, show flashing
	{
		// Draw hearts
		for (int i = 0; i < NUM_HEARTS; i++)
			m_aHearts[i].Snap(SnappingClient);
	}

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

void CHelicopter::InitBuild()
{ // Hearts uses the values from this function
	bool NotFirst = false;
	float Lowest = 0.f;
	float Highest = 0.f;
	float Leftest = 0.f;
	float Rightest = 0.f;
	for (int i = 0; i < NUM_BONES; i++)
	{ // Flipped comparison signs, lowest means higher Y
		// y
		if (!NotFirst || m_aBones[i].m_From.y > Lowest)
		{
			NotFirst = true;
			Lowest = m_aBones[i].m_From.y;
		}
		if (!NotFirst || m_aBones[i].m_To.y > Lowest)
		{
			NotFirst = true;
			Lowest = m_aBones[i].m_To.y;
		}

		if (!NotFirst || m_aBones[i].m_From.y < Highest)
		{
			NotFirst = true;
			Highest = m_aBones[i].m_From.y;
		}
		if (!NotFirst || m_aBones[i].m_To.y < Highest)
		{
			NotFirst = true;
			Highest = m_aBones[i].m_To.y;
		}
		// x
		if (!NotFirst || m_aBones[i].m_From.x < Leftest)
		{
			NotFirst = true;
			Leftest = m_aBones[i].m_From.x;
		}
		if (!NotFirst || m_aBones[i].m_To.x < Leftest)
		{
			NotFirst = true;
			Leftest = m_aBones[i].m_To.x;
		}

		if (!NotFirst || m_aBones[i].m_From.x > Rightest)
		{
			NotFirst = true;
			Rightest = m_aBones[i].m_From.x;
		}
		if (!NotFirst || m_aBones[i].m_To.x > Rightest)
		{
			NotFirst = true;
			Rightest = m_aBones[i].m_To.x;
		}
	}

	m_BuildTop = Highest;
	m_BuildBottom = Lowest;
	m_BuildLeft = Leftest;
	m_BuildRight = Rightest;

	m_BuildTotalHeight = Lowest - Highest;
	m_BuildTotalWidth = Rightest - Leftest;

	m_BuildHeight = Lowest; // Build animation y
}

void CHelicopter::InitUnbuilt()
{ // Only use in constructor
	for (int i = 0; i < NUM_BUILD_IDS; i++)
		m_aBuildIDs[i] = Server()->SnapNewID();

	for (int i = 0; i < NUM_BONES; i++)
	{
		m_aBones[i].m_Enabled = false;
		m_aBones[i].m_Thickness = 0;
		m_aBones[i].m_Color = LASERTYPE_FREEZE;
	}
	for (int i = 0; i < NUM_TRAILS; i++)
		m_aTrails[i].m_Enabled = false;
}

void CHelicopter::BuildHelicopter()
{
	if (!m_Build)
		return;

	m_BuildHeight -= 0.5f;
	for (int i = 0; i < NUM_BONES; i++)
	{
		const SBone& Bone = m_aBones[i];
		vec2 From = Bone.m_InitFrom;
		vec2 To = Bone.m_InitTo;

		bool FromUnder = From.y > m_BuildHeight;
		bool ToUnder = To.y > m_BuildHeight;

		m_aBones[i].m_Enabled = FromUnder || ToUnder;

		if (m_aBones[i].m_Enabled)
		{
			float FullLength = distance(Bone.m_InitFrom, Bone.m_InitTo);
			float VisibleLength = distance(m_aBones[i].m_From, m_aBones[i].m_To);
			float Fraction = FullLength > 0.0f ? clamp(VisibleLength / FullLength, 0.0f, 1.0f) : 0.0f;
			m_aBones[i].m_Thickness = round_to_int(-3 + Fraction * ((float)Bone.m_InitThickness + 3.f));

			if (!FromUnder && !ToUnder)
			{
				// Both points are above the build height — fully hidden
				m_aBones[i].m_Enabled = false;
			}
			else if (FromUnder && ToUnder)
			{
				// Both points are below — keep the full original line
				m_aBones[i].m_From = From;
				m_aBones[i].m_To = To;
			}
			else if (FromUnder && !ToUnder)
			{
				// From is under, To is above — keep From, clip To
				float t = (m_BuildHeight - From.y) / (To.y - From.y);
				vec2 Intersect = {
					From.x + (To.x - From.x) * t,
					m_BuildHeight
				};
				m_aBones[i].m_From = From;
				m_aBones[i].m_To = Intersect;
			}
			else if (!FromUnder && ToUnder)
			{
				// To is under, From is above — keep To, clip From
				float t = (m_BuildHeight - To.y) / (From.y - To.y);
				vec2 Intersect = {
					To.x + (From.x - To.x) * t,
					m_BuildHeight
				};
				m_aBones[i].m_From = Intersect;
				m_aBones[i].m_To = To;
			}
		}
	}

	for (int i = 0; i < NUM_TRAILS; i++)
		m_aTrails[i].m_Enabled = m_aTrails[i].m_pPos->y > m_BuildHeight;

	if (m_BuildHeight < m_BuildTop)
	{ // Finished building, no more animation required
		m_Build = false;
		for (int i = 0; i < NUM_BUILD_IDS; i++)
		{
			Server()->SnapFreeID(m_aBuildIDs[i]);
			m_aBuildIDs[i] = -1;
		}

		for (int i = 0; i < NUM_BONES; i++)
			m_aBones[i].m_Color = m_aBones[i].m_InitColor;
	}
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
		TopPropeller()[i].m_InitColor = LASERTYPE_DOOR;
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
