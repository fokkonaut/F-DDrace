// made by fokkonaut and Matq, somewhere around 2021 and in 2025

#include "engine/server.h"
#include "game/server/gamecontext.h"
#include "helicopter.h"
#include "generated/server_data.h"
#include "helicopter_models.h"
#include "engine/server.h"
#include "helicopter.h"

bool MovingCircleHitsMovingSegment_Analytical(
	vec2 circleLast,
	vec2 circleNow,
	float radius,
	vec2 lineLastA,
	vec2 lineNowA,
	vec2 lineLastB,
	vec2 lineNowB)
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

	// Find the closest point on segment
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

IServer *Server();

SHelicopterMeta aHelicopterMetadata[NUM_HELICOPTER_TYPES] = {
	{ "Helicopter", 60.0f, 4, vec2(0.75f, 0.6f), { vec2(0.f, 0.f) }, 1 },
	{ "Attack Helicopter", 250.0f, 6, vec2(1.1f, 1.1f), { vec2(0.0f, 0.0f), vec2(40.0f, 8.0f) }, 2 }
};

CHelicopter::CHelicopter(
	CGameWorld *pGameWorld,
	int HelicopterType,
	int Spawner,
	int Team,
	vec2 Pos,
	float HelicopterScale,
	bool Build,
	int Number,
	int DelayTurretType
)
	: CAdvancedEntity(pGameWorld, CGameWorld::ENTTYPE_HELICOPTER, Pos, HELICOPTER_PHYSSIZE * HelicopterScale)
{
	m_HelicopterType = HelicopterType;
	for (int i = 0; i < NUM_MAX_SEATS; i++)
		m_aPassengers[i] = -1;
	m_NumPassengers = 0;

	m_AllowVipPlus = false;
	m_Elasticity = vec2(0.f, 0.f);
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
	m_NumHearts = 0;
	m_BaseAccel = vec2(0.5f, 0.5f);
	m_pName = "Helicopter";
	m_NumSeats = 0;
	m_EngineOn = false;

	m_Scale = 1.f;
	m_Flipped = false;
	m_Angle = 0.f;
	m_Accel = vec2(0.f, 0.f);

	m_pModel = nullptr;
	m_pTurret = nullptr;

	for (int i = 0; i < MAX_CLIENTS; i++)
		m_aFlungCharacters[i] = -1;

	m_ExplosionsLeft = -1;

	m_ShowHeartsUntil = 0;
	m_LastDamage = 0;
	m_LastEnvironmentalDamage = 0;
	m_LastKnownOwner = Spawner;

	m_BroadcastingTick = 0;

	m_Build = Build;
	m_BuildHeight = 0.f;

	SetClassAtributes(HelicopterType, true);
	InitModel();
	m_pModel->UpdateLastPropellerPositions(); // Thop

	UpdateHeartsIndicator();

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
	delete m_pModel; // Also deletes laser and particle ids

	for (int i = 0; i < m_NumHearts; i++)
		Server()->SnapFreeID(m_aHearts[i].m_ID);
	for (int i = 0; i < NUM_BUILD_IDS; i++)
		if (m_aBuildIDs[i] != -1) // should only pass if deleted while crafting a helicopter
			Server()->SnapFreeID(m_aBuildIDs[i]);

	DestroyTurret();
}

void CHelicopter::Reset()
{
	Dismount(-1);
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
	return m_Health != m_MaxHealth && m_LastDamage && m_LastDamage + Server()->TickSpeed() * 60 < Server()->Tick();
}

CCharacter *CHelicopter::GetDriver()
{
	int driverID = m_aPassengers[0];
	if (driverID != -1)
		return GameServer()->GetPlayerChar(driverID);
	return nullptr;
}

CCharacter *CHelicopter::GetGunner()
{
	if (m_NumSeats == 1 && m_aPassengers[0] != -1)
		return GameServer()->GetPlayerChar(m_aPassengers[0]);
	if (m_NumSeats >= 2 && m_aPassengers[1] != -1)
		return GameServer()->GetPlayerChar(m_aPassengers[1]);
	return nullptr;
}

void CHelicopter::SetNumHeartsIndicator(int NumHearts)
{
	int CurrentHearts = m_NumHearts;
	int DeltaHearts = NumHearts - CurrentHearts;

	if (DeltaHearts > 0)
	{
		for (int i = CurrentHearts; i < NumHearts; i++)
			m_aHearts[i] = SHeart(this, Server()->SnapNewID(), vec2(0.f, 0.f));
	}
	else if (DeltaHearts < 0)
	{
		for (int i = CurrentHearts - 1; i >= NumHearts; i--) // when going from 10 to 8 hearts, remove indexes 9 8
		{
			Server()->SnapFreeID(m_aHearts[i].m_ID);
			m_aHearts[i].m_ID = -1;
		}
	}

	m_NumHearts = NumHearts;

	if (DeltaHearts != 0)
		UpdateHeartsIndicator();
}

void CHelicopter::SetClassAtributes(int HelicopterType, bool SetFullHealth)
{
	if (HelicopterType < 0 || HelicopterType >= NUM_HELICOPTER_TYPES)
		return; // invalid class

	SHelicopterMeta& metadata = aHelicopterMetadata[HelicopterType];
	m_pName = metadata.m_pName;
	m_MaxHealth = metadata.m_BaseHealth;
	// m_NumHearts = metadata.m_NumHeartsIndicator;
	SetNumHeartsIndicator(metadata.m_NumHeartsIndicator);
	m_BaseAccel = metadata.m_BaseAccel;

	memcpy(m_aSeats, metadata.m_aSeats, sizeof(m_aSeats));
	m_NumSeats = metadata.m_NumSeats;

	if (SetFullHealth)
		m_Health = m_MaxHealth;
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
	m_pModel->ApplyScale(HelicopterScale);
	m_BuildHeight *= HelicopterScale;
	//

	UpdateHeartsIndicator();
}

void CHelicopter::Explode()
{
	m_ExplosionsLeft = (m_HelicopterType == HELICOPTER_APACHE) ? 12 : 6;

	Dismount(-1);

	// Freeze characters near explosion
	CCharacter *aVictims[MAX_CLIENTS];
	int numFound = GameWorld()->FindEntities(m_Pos, GetProximityRadius(), (CEntity **)aVictims, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER, m_DDTeam);
	if (!numFound)
		return;

	for (int i = 0; i < numFound; i++)
	{
		CCharacter *pChar = aVictims[i];
		if (pChar->GetNinjaCurrentMoveTime())
			continue; // Ninjas get freeze immunity while slashing during an explosion

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

void CHelicopter::Heal(float Health)
{
	m_Health = min(m_Health + Health, m_MaxHealth);
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

		HandleSeats();

		FlingTeesInPropellersPath();
		ApplyAcceleration();
		HandlePropellers();

		DamageInWall(); // just in case
		DamageInFreeze();

		UpdateHeartsIndicator();
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
	m_pModel->UpdateLastPropellerPositions();
}

void CHelicopter::OnInput(CNetObj_PlayerInput *pNewInput, CCharacter *pController)
{
	if (!pController)
		return;

	bool isDriver = (pController->m_HelicopterSeat == 0);
	bool isGunner = (pController->m_HelicopterSeat == 1 || m_NumSeats == 1);

	// Movement controls
	if (isDriver)
	{
		if (pController->m_FreezeTime)
		{
			m_Accel = vec2(0.f, 0.f);
		}
		else
		{
			m_InputDirection = pNewInput->m_Direction;
			m_Accel.x = (float)pNewInput->m_Direction;

			bool Rise = pNewInput->m_Jump;
			bool Sink = pNewInput->m_Hook;
			if (Rise == Sink)
				m_Accel.y = 0.f;
			else
				m_Accel.y = Rise ? -1 : 1;
		}
	}

	// Weapon controls
	if (isGunner && m_pTurret)
		m_pTurret->OnInput(pNewInput, pController);
}

void CHelicopter::ApplyAcceleration()
{
	if (!m_EngineOn)
		m_Accel = vec2(0.f, 0.f);

	for (int i = 0; i < m_NumSeats; i++)
	{
		int passengerCID = m_aPassengers[i];
		if (passengerCID == -1)
			continue;

		CCharacter *pPassenger = GameServer()->GetPlayerChar(passengerCID);

		// Hook acceleration applied to vehicle
		for (int HookerCID : pPassenger->Core()->m_AttachedPlayers)
		{
			CCharacter *pChr = GameServer()->GetPlayerChar(HookerCID);
			if (!pChr)
				continue;

			float Distance = distance(pChr->GetPos(), pPassenger->GetPos());
			vec2 Dir = normalize(pChr->GetPos() - pPassenger->GetPos());

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
	m_Vel.x += m_BaseAccel.x * m_Accel.x * strafeFactor;
	m_Vel.y += m_BaseAccel.y * m_Accel.y;
	m_Vel.y *= 0.95f;

	// Prevent flipping when not going the opposite direction OR when shooting the opposite direction
	if (((m_InputDirection == -1 && !m_Flipped && m_Vel.x < 0.f) ||
			(m_InputDirection == 1 && m_Flipped && m_Vel.x > 0.f)) &&
		(!m_pTurret || !m_pTurret->m_Shooting || (m_Flipped != (m_pTurret->m_TargetPosition.x < 0))))
		Flip();

	SetAngle(m_Vel.x);
}

void CHelicopter::FlingTeesInPropellersPath()
{
	// be careful teleporting helicopter, update last propeller data
	if (!m_EngineOn || !GetDriver())
		return;

	SPropeller *aPropellers = m_pModel->Propellers();
	for (int j = 0; j < m_pModel->m_NumPropellers; j++)
	{
		CCharacter *aPossibleCollisions[10];
		int numFound = GameWorld()->FindEntities(m_Pos + aPropellers[j].GetCenter(),
		                                         aPropellers[j].m_Radius + 200.f,
		                                         (CEntity **)aPossibleCollisions, 10, CGameWorld::ENTTYPE_CHARACTER, m_DDTeam);
		if (!numFound)
			return;

		for (int i = 0; i < numFound; i++)
		{
			CCharacter *pChar = aPossibleCollisions[i];
			if (pChar->m_pHelicopter == this)
				continue;

			int cID = pChar->GetPlayer()->GetCID();
			if (Server()->Tick() - m_aFlungCharacters[cID] <= 10)
				continue;

			vec2 PosA, PosB;
			aPropellers[j].GetFullPropellerPositions(PosA, PosB);
			bool collisionDetected = MovingCircleHitsMovingSegment_Analytical(
				pChar->m_PrevPos - m_Pos, pChar->GetPos() - m_Pos,
				pChar->GetProximityRadius(),
				aPropellers[j].m_LastA, PosA,
				aPropellers[j].m_LastB, PosB);
			if (collisionDetected)
			{
				m_aFlungCharacters[cID] = Server()->Tick();
				FlingTee(pChar);
			}
		}
	}
}

void CHelicopter::InitModel()
{
	switch (m_HelicopterType)
	{
		case HELICOPTER_DEFAULT:
		{
			m_pModel = new SHelicopterModel(this);
			break;
		}
		case HELICOPTER_APACHE:
		{
			m_pModel = new SHelicopterApacheModel(this);
			break;
		}
	}

	m_pModel->PostConstruction();
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

// void CHelicopter::InitHearts()
// {
// 	for (int i = 0; i < m_NumHearts; i++)
// 		m_aHearts[i] = SHeart(this, Server()->SnapNewID(), vec2(0.f, 0.f));
//
// 	UpdateHearts();
// }

void CHelicopter::UpdateHeartsIndicator()
{
	if (m_pModel == nullptr)
		return;

	float HealthPercentage = m_Health / m_MaxHealth; // Health 0..1
	float HeartsPrecise = (float)m_NumHearts * HealthPercentage; // Hearts like 3.7
	int HeartsFull = floor(HeartsPrecise); // Hearts like 3.0
	int ShowNumHearts = ceil(HeartsPrecise); // How many are shown, including fraction heart (3 + 0.7 = 4)

	float CurrentHeart = HeartsPrecise - (float)HeartsFull; // Last heart 0..1, example 0.7
	bool FlashingHalfHeart = CurrentHeart > 0.0f && CurrentHeart <= 0.5f; // Flash or not, if last heart below half
	for (int i = 0; i < m_NumHearts; i++)
	{
		if (i < ShowNumHearts - FlashingHalfHeart) //
			m_aHearts[i].m_Enabled = true;
		else if (i == HeartsFull && FlashingHalfHeart) // Flash heart if last was below 0.5
			m_aHearts[i].m_Enabled = (Server()->Tick() / 4) % 2 == 0;
		else
			m_aHearts[i].m_Enabled = false;
	}

	float Gap = 40.f;
	float HeartsY = m_pModel->m_BoundTop - 30.f;
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

	m_Health = min(m_Health + 0.01f, m_MaxHealth);
}

void CHelicopter::UpdateVisualDamage()
{
	float HealthPercentage = 1.f - m_Health / m_MaxHealth;

	SBone *aBones = m_pModel->Bones();
	for (int i = 0; i < m_pModel->m_NumBones; i++)
		aBones[i].m_Thickness = aBones[i].m_InitThickness - (int)(sinf(Server()->Tick() / 4 + i) * HealthPercentage * (float)aBones[i].m_InitThickness);
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
	if (m_NumPassengers == 0 || (Server()->Tick() - m_BroadcastingTick) % Server()->TickSpeed() != 0)
		return;

	char aMsg[128];
	const char *RegenerationText = IsRegenerating() ? " +1regen" : "";
	str_format(aMsg, sizeof(aMsg), "> %s <\nHealth [%d]%s\nSeats %i/%i", m_pName, (int)m_Health, RegenerationText, m_NumPassengers, m_NumSeats);

	for (int i = 0; i < m_NumSeats; i++)
	{
		int passengerCID = m_aPassengers[i];
		if (passengerCID == -1)
			continue;

		CCharacter *pPassenger = GameServer()->GetPlayerChar(passengerCID);
		pPassenger->SendBroadcastHud(aMsg);
	}
}

void CHelicopter::HandleSeats()
{
	for (int i = 0; i < m_NumSeats; i++)
	{
		int passengerCID = m_aPassengers[i];
		if (passengerCID == -1)
			continue;

		CCharacter *pCharacter = GameServer()->GetPlayerChar(passengerCID);
		bool isDriver = (i == 0);

		vec2 relativeSeatPos = m_aSeats[i];
		relativeSeatPos.x *= (m_Flipped ? -1.0f : 1.0f);

		vec2 seatPos = m_Pos + relativeSeatPos;
		pCharacter->ForceSetPos(seatPos);
		pCharacter->Core()->m_Vel = vec2(0, 0);

		if (pCharacter->m_DeepFreeze)
			Dismount(passengerCID);

		if (isDriver)
		{
			m_Gravity = (pCharacter->m_FreezeTime > 0);
			m_GroundVel = (pCharacter->m_FreezeTime || m_Accel.x == 0.f); // forgot what is ground vel
		}
	}
}

void CHelicopter::HandlePropellers()
{
	if (!m_EngineOn || Server()->Tick() % 2 != 0)
		return;

	if (!GetOwner() && IsGrounded()) // Reset top propellers when grounded without driver
	{
		if (m_EngineOn)
			ResetAndTurnOff();
		return;
	}

	m_pModel->SpinPropellers(); //
}

bool CHelicopter::Mount(int ClientID)
{
	if (ClientID < 0 || ClientID > MAX_CLIENTS || m_NumPassengers >= m_NumSeats || IsExploding() || IsBuilding()) // used to check m_Owner != -1 (aka driver)
		return false;

	// scale specific mount condition
	CCharacter *pCharacter = GameServer()->GetPlayerChar(ClientID);
	if (distance(pCharacter->GetPos(), m_Pos) > pCharacter->GetProximityRadius() + GetProximityRadius())
		return false;

	bool isDriver = false;
	for (int i = 0; i < m_NumSeats; i++)
	{
		// Slot is taken
		if (m_aPassengers[i] != -1)
			continue;

		isDriver = (i == 0);
		m_aPassengers[i] = ClientID;
		pCharacter->m_HelicopterSeat = i;
		m_NumPassengers++;
		break;
	}

	if (isDriver)
	{
		m_Owner = ClientID;
		m_LastKnownOwner = ClientID;
		m_EngineOn = true;
		m_Gravity = false;
		m_GroundVel = false;
	}

	m_ShowHeartsUntil = Server()->Tick() + Server()->TickSpeed() * 3;
	pCharacter->m_pHelicopter = this;
	pCharacter->SetWeapon(-1);
	GameServer()->SendTuningParams(m_Owner, pCharacter->m_TuneZone);
	m_BroadcastingTick = Server()->Tick() + 1; // Start updating broadcast next tick

	return true;
}

void CHelicopter::Dismount(int ClientID)
{
	for (int i = 0; i < m_NumSeats; i++)
	{
		int passengerCID = m_aPassengers[i];
		if (passengerCID == -1)
			continue;

		bool isDriver = (i == 0);
		if (ClientID == -1 || passengerCID == ClientID)
		{
			CCharacter *pCharacter = GameServer()->GetPlayerChar(passengerCID);
			if (isDriver)
			{
				m_Gravity = true;
				m_GroundVel = true;
				m_Accel.y = 0;
				m_Owner = -1;
			}

			if (pCharacter)
			{
				pCharacter->m_pHelicopter = nullptr;
				pCharacter->SetWeapon(pCharacter->GetLastWeapon());
				GameServer()->SendTuningParams(m_Owner, pCharacter->m_TuneZone);
				pCharacter->SendBroadcastHud(""); // ?
			}

			m_aPassengers[i] = -1;
			m_NumPassengers--;

			// Dismount only one player
			if (ClientID != -1)
				break;
		}
	}
}

void CHelicopter::Flip()
{
	m_Flipped = !m_Flipped;
	// m_Angle *= -1.f;
	// m_pModel->Flip();

	// if (m_pTurret)
	// 	m_pTurret->SetFlipped(m_Flipped);
}

void CHelicopter::SetRotation(float NewRotation)
{
	m_Angle = NewRotation;
	m_pModel->SetRotation(NewRotation * (m_Flipped ? -1.0f : 1.0f));

	if (m_pTurret)
		m_pTurret->SetRotation(NewRotation, m_pTurret->GetTurretRotation());
}

void CHelicopter::SetAngle(float Angle)
{
	SetRotation(Angle);
}

void CHelicopter::Snap(int SnappingClient)
{
	if (IsExploding() || IsSpawning())
		return;

	if (NetworkClipped(SnappingClient) || !CmaskIsSet(m_TeamMask, SnappingClient))
		return;

	CCharacter *pChar = GameServer()->GetPlayerChar(SnappingClient);
	// auto tick = Server()->Tick();

	if (IsBuilding())
	{
		for (int i = 0; i < NUM_BUILD_IDS; i++)
		{
			CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_aBuildIDs[i], sizeof(CNetObj_Projectile)));
			if (!pObj)
				continue;

			pObj->m_X = round_to_int(m_Pos.x + m_pModel->m_BoundLeft + rand() % (int)m_pModel->m_TotalWidth);
			pObj->m_Y = round_to_int(m_Pos.y + m_BuildHeight);
			pObj->m_VelX = 0;
			pObj->m_VelY = 0;
			pObj->m_StartTick = Server()->Tick();
			pObj->m_Type = WEAPON_HAMMER;
		}
	}
	else if ((m_NumPassengers == 0 || (pChar && pChar->m_pHelicopter == this)) && // Show hearts when no passengers or only to passengers
		((m_ShowHeartsUntil && m_ShowHeartsUntil > Server()->Tick()) || // Show until time specified (on mount, damage, etc.)
			m_Health != m_MaxHealth) && // Show while not full health
		(!m_LastDamage || ((m_LastDamage && (m_LastDamage + Server()->TickSpeed() / 2 <= Server()->Tick())) || // If been damaged too long ago, show normally
			(m_LastDamage + Server()->TickSpeed() / 2 > Server()->Tick() && (Server()->Tick() / 4) % 2 == 0)))) // If been damaged recently, show flashing
	{
		// Draw hearts
		for (int i = 0; i < m_NumHearts; i++)
			m_aHearts[i].Snap(SnappingClient);
	}

	// Draw helicopter
	m_pModel->Snap(SnappingClient, m_EngineOn, m_Flipped, (int)((1.0f - (float)m_Health / (float)m_MaxHealth) * 10));

	// Draw guns
	if (m_pTurret)
		m_pTurret->Snap(SnappingClient, m_Flipped);
}

void CHelicopter::InitUnbuilt()
{
	// Only use in constructor
	for (int i = 0; i < NUM_BUILD_IDS; i++)
		m_aBuildIDs[i] = Server()->SnapNewID();

	m_BuildHeight = m_pModel->m_BoundBottom;
	m_pModel->InitBuildAnimation();
}

void CHelicopter::BuildHelicopter()
{
	if (!m_Build)
		return;

	m_BuildHeight -= 0.5f;
	SBone *aBones = m_pModel->Bones();
	for (int i = 0; i < m_pModel->m_NumBones; i++)
	{
		const SBone& Bone = aBones[i];
		vec2 From = Bone.m_InitFrom;
		vec2 To = Bone.m_InitTo;

		bool FromUnder = From.y > m_BuildHeight;
		bool ToUnder = To.y > m_BuildHeight;

		aBones[i].m_Enabled = FromUnder || ToUnder;

		if (aBones[i].m_Enabled)
		{
			float FullLength = distance(Bone.m_InitFrom, Bone.m_InitTo);
			float VisibleLength = distance(aBones[i].m_From, aBones[i].m_To);
			float Fraction = FullLength > 0.0f ? clamp(VisibleLength / FullLength, 0.0f, 1.0f) : 0.0f;
			aBones[i].m_Thickness = round_to_int(-3 + Fraction * ((float)Bone.m_InitThickness + 3.f));

			if (!FromUnder && !ToUnder)
			{
				// Both points are above the build height — fully hidden
				aBones[i].m_Enabled = false;
			}
			else if (FromUnder && ToUnder)
			{
				// Both points are below — keep the full original line
				aBones[i].m_From = From;
				aBones[i].m_To = To;
			}
			else if (FromUnder) // && !ToUnder)
			{
				// From is under, To is above — keep From, clip To
				float t = (m_BuildHeight - From.y) / (To.y - From.y);
				vec2 Intersect = {
					From.x + (To.x - From.x) * t,
					m_BuildHeight
				};
				aBones[i].m_From = From;
				aBones[i].m_To = Intersect;
			}
			else // if (!FromUnder && ToUnder)
			{
				// To is under, From is above — keep To, clip From
				float t = (m_BuildHeight - To.y) / (From.y - To.y);
				vec2 Intersect = {
					To.x + (From.x - To.x) * t,
					m_BuildHeight
				};
				aBones[i].m_From = Intersect;
				aBones[i].m_To = To;
			}
		}
	}

	STrail *aTrails = m_pModel->Trails();
	for (int i = 0; i < m_pModel->m_NumTrails; i++)
		aTrails[i].m_Enabled = aTrails[i].m_pPos->y > m_BuildHeight;

	if (m_BuildHeight < m_pModel->m_BoundTop)
	{
		// Finished building, no more animation required
		m_Build = false;
		for (int i = 0; i < NUM_BUILD_IDS; i++)
		{
			Server()->SnapFreeID(m_aBuildIDs[i]);
			m_aBuildIDs[i] = -1;
		}

		for (int i = 0; i < m_pModel->m_NumBones; i++)
			aBones[i].m_Color = aBones[i].m_InitColor;
	}
}

void CHelicopter::SortBones()
{
	// meant for adding turret later, not specifically at initialization
	SBone *aBones = m_pModel->Bones();
	int NumBonesModel = m_pModel->m_NumBones;

	int NumBonesTotal = NumBonesModel + (m_pTurret ? m_pTurret->GetNumBones() : 0);

	// Get current IDs
	int *apIDs = new int[NumBonesTotal];
	for (int i = 0; i < NumBonesModel; i++)
		apIDs[i] = aBones[i].m_ID;
	if (m_pTurret)
		for (int i = 0; i < m_pTurret->GetNumBones(); i++)
			apIDs[NumBonesModel + i] = m_pTurret->Bones()[i].m_ID;

	// Sort IDs
	std::sort(apIDs, apIDs + NumBonesTotal);

	// Update sorted IDs (low -> high | ascending)
	for (int i = 0; i < NumBonesModel; i++)
		aBones[i].m_ID = apIDs[i];
	if (m_pTurret)
		for (int i = 0; i < m_pTurret->GetNumBones(); i++)
			m_pTurret->Bones()[i].m_ID = apIDs[NumBonesModel + i];
	delete[] apIDs;
}

void CHelicopter::ResetAndTurnOff()
{
	// Dismounted & grounded
	m_EngineOn = false;

	m_pModel->ResetPropellers(); // Thop
}
