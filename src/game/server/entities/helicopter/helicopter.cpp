// made by fokkonaut and Matq, somewhere around 2021 and in 2025

#include "engine/server.h"
#include "game/server/gamecontext.h"
#include "helicopter.h"
#include "generated/server_data.h"
#include "helicopter_models.h"

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

namespace Helicopters
{
static IHelicopterModel *CreateDefaultModel(CHelicopter *pHeli) { return new CHelicopterModel(pHeli); }
static IHelicopterModel *CreateApacheModel(CHelicopter *pHeli) { return new CHelicopterApacheModel(pHeli); }
static IHelicopterModel *CreateChinookModel(CHelicopter *pHeli) { return new CHelicopterChinookModel(pHeli); }

SHelicopterMeta aHelicopterMetadata[NUM_HELICOPTER_TYPES] = {
	{
		"Helicopter", // m_pName
		30, // m_BaseHealth
		0, // m_BaseArmorHealth
		4, // m_NumHeartsIndicator
		0, // m_NumArmorIndicator
		6, // m_NumExplosions
		vec2(80, 128),// m_BaseSize
		vec2(0.75f, 0.6f), // m_BaseAccel
		CreateDefaultModel, // m_pModelFactory
	},
	{
		"Attack Helicopter", // m_pName
		60, // m_BaseHealth
		30, // m_BaseArmorHealth
		4, // m_NumHeartsIndicator
		2, // m_NumArmorIndicator
		12, // m_NumExplosions
		vec2(170, 128),// m_BaseSize
		vec2(1.0f, 1.0f), // m_BaseAccel
		CreateApacheModel, // m_pModelFactory
	},
	{
		"Transport Helicopter", // m_pName
		30, // m_BaseHealth
		60, // m_BaseArmorHealth
		2, // m_NumHeartsIndicator
		4, // m_NumArmorIndicator
		12, // m_NumExplosions
		vec2(230, 128),// m_BaseSize
		vec2(0.65f, 0.65f), // m_BaseAccel
		CreateChinookModel, // m_pModelFactory
	},
};
}

CHelicopter::CHelicopter(
	CGameWorld *pGameWorld,
	int HelicopterType,
	int Spawner,
	int Team,
	vec2 Pos,
	float HelicopterScale,
	int BuildTime,
	int Number,
	int DelayTurretType
)
	: CAdvancedEntity(pGameWorld, CGameWorld::ENTTYPE_HELICOPTER, Pos, HELICOPTER_PHYSSIZE)
{
	m_HelicopterType = clamp(HelicopterType, 0, (int)NUM_HELICOPTER_TYPES - 1);

	SetFlags(EFlags::ALLOW_VIP_PLUS, false);
	m_Elasticity = vec2(0.f, 0.f);
	m_DDTeam = Team;

	m_InputDirection = 0;
	m_MaxHealth = 60;
	m_Health = m_MaxHealth;
	m_MaxArmor = 0;
	m_Armor = m_MaxArmor;
	m_NumHeartsIndicator = 0;
	m_NumArmorIndicator = 0;
	m_BaseAccel = vec2(0.5f, 0.5f);
	m_pName = "Helicopter";
	m_EngineOn = false;

	m_Scale = 1.f;
	m_Flipped = false;
	m_Angle = 0.f;
	m_Accel = vec2(0.f, 0.f);

	m_pModel = nullptr;
	m_pTurret = nullptr;

	for (int i = 0; i < MAX_CLIENTS; i++)
		m_aFlungCharacters[i] = -1;

	m_ExplosionsOnDeath = 1;
	m_ExplosionsLeft = 0;

	m_ShowHealthbarUntil = 0;
	m_LastDamage = 0;
	m_LastEnvironmentalDamage = 0;
	m_LastKnownOwner = Spawner;

	m_BroadcastingTick = 0;

	m_Build.m_StartTick = 0;
	m_Build.m_Duration = BuildTime;
	m_Build.m_Building = false;
	m_Build.m_CachedHeight = 0.0f;
	for (int i = 0; i < NUM_BUILD_IDS; i++)
		m_Build.m_aBuildIDs[i] = -1;

	SetClassType(HelicopterType, true);

	UpdateHealthbarIndicator();

	m_Number = Number;
	m_DelayTurretType = DelayTurretType;
	m_InitialPosition = Pos;
	if (PlacedByTile())
	{
		m_Build.m_Duration = Server()->TickSpeed() * Config()->m_SvHeliRespawnTime;
		m_Layer = LAYER_SWITCH; // unused rn, but for completeness
	}

	// Order matters
	if (m_Build.m_Duration)
		InitBuildAnimation();

	SortBones();

	HelicopterScale = clamp(HelicopterScale, 0.8f, 5.f);
	ApplyScale(HELICOPTER_DEFAULT_SCALE * HelicopterScale);

	GameWorld()->InsertEntity(this);
}

CHelicopter::~CHelicopter()
{
	delete m_pModel;

	for (int i = 0; i < m_NumHeartsIndicator; i++)
		Server()->SnapFreeID(m_aHeartsIndicator[i].m_ID);
	for (int i = 0; i < NUM_BUILD_IDS; i++)
		if (m_Build.m_aBuildIDs[i] != -1) // should only pass if deleted while crafting a helicopter
			Server()->SnapFreeID(m_Build.m_aBuildIDs[i]);

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
	return GameServer()->SpawnHelicopter(-1, 0, m_InitialPosition, GameServer()->GetHelicopterTileType(), m_DelayTurretType, 1.f, false, m_Number);
}

bool CHelicopter::CanRegenerateArmor()
{
	return m_Armor != m_MaxArmor && // Regenerate if needs to
	(!m_LastDamage || // Was not damaged
		m_LastDamage + Server()->TickSpeed() * 60 < Server()->Tick() || // Or damaged 60 seconds ago
		(m_Health == m_MaxHealth && m_LastDamage + Server()->TickSpeed() * 20 < Server()->Tick())); // Or damage 20 seconds ago, with full health
}

CCharacter *CHelicopter::GetDriver()
{
	if (m_pModel->NumSeats() >= 1)
	{
		int driverCID = m_pModel->Seats()[0].m_SeatedCID;
		if (driverCID != -1)
			return GameServer()->GetPlayerChar(driverCID);
	}

	return nullptr;
}

CCharacter *CHelicopter::GetGunner()
{
	// One seat = gunner
	if (m_pModel->NumSeats() == 1 && m_pModel->Seats()[0].m_SeatedCID != -1)
		return GameServer()->GetPlayerChar(m_pModel->Seats()[0].m_SeatedCID);

	// Two or more seats = second is gunner
	if (m_pModel->NumSeats() >= 2 && m_pModel->Seats()[1].m_SeatedCID != -1)
		return GameServer()->GetPlayerChar(m_pModel->Seats()[1].m_SeatedCID);

	return nullptr;
}

int CHelicopter::GetNextAvailableSeat(int AfterIndex)
{
	int NumSeats = m_pModel->NumSeats();
	for (int i = 0; i < NumSeats; i++)
	{
		int SeatID = (AfterIndex + i) % NumSeats;
		if (m_pModel->Seats()[SeatID].m_SeatedCID == -1)
			return SeatID;
	}

	return -1; // Full
}

void CHelicopter::SetNumIndicator(CPickupNode *aPickups, int& NumPickups, int NewNumPickups, int MaxPickups, int PowerupType)
{
	NewNumPickups = std::min(NewNumPickups, MaxPickups);

	int CurrentPickups = NumPickups;
	int DeltaPickups = NewNumPickups - CurrentPickups;

	if (DeltaPickups > 0)
	{
		for (int i = CurrentPickups; i < NewNumPickups; i++)
			aPickups[i] = CPickupNode(this, Server()->SnapNewID(), PowerupType, vec2(0.f, 0.f));
	}
	else if (DeltaPickups < 0)
	{
		for (int i = CurrentPickups - 1; i >= NewNumPickups; i--)
		{
			Server()->SnapFreeID(aPickups[i].m_ID);
			aPickups[i].m_ID = -1;
		}
	}

	NumPickups = NewNumPickups;

	if (DeltaPickups != 0)
		UpdateHealthbarIndicator();
}

void CHelicopter::SetNumHeartsIndicator(int NumHearts)
{
	SetNumIndicator(m_aHeartsIndicator, m_NumHeartsIndicator, NumHearts, MAX_HEARTS, POWERUP_HEALTH);
}

void CHelicopter::SetNumArmorIndicator(int NumArmor)
{
	SetNumIndicator(m_aArmorIndicator, m_NumArmorIndicator, NumArmor, MAX_ARMOR, POWERUP_ARMOR);
}

void CHelicopter::SetClassType(int HelicopterType, bool HealFullyToo)
{
	if (HelicopterType < 0 || HelicopterType >= NUM_HELICOPTER_TYPES)
		return; // invalid class

	Helicopters::SHelicopterMeta& metadata = Helicopters::aHelicopterMetadata[HelicopterType];
	m_pName = metadata.m_pName;
	m_MaxHealth = metadata.m_BaseHealth;
	m_MaxArmor = metadata.m_BaseArmorHealth;
	SetNumHeartsIndicator(metadata.m_NumHeartsIndicator);
	SetNumArmorIndicator(metadata.m_NumArmorIndicator);
	m_ExplosionsOnDeath = metadata.m_NumExplosions;
	SetSize(metadata.m_BaseSize);
	m_BaseAccel = metadata.m_BaseAccel;

	// Model
	IHelicopterModel *pNewModel = metadata.m_pModelFactory(this);
	if (pNewModel)
		pNewModel->PostConstructor();

	delete m_pModel;
	m_pModel = pNewModel;
	//

	if (HealFullyToo)
	{
		m_Health = m_MaxHealth;
		m_Armor = m_MaxArmor;
	}
}

bool CHelicopter::TryAttachTurret(CVehicleTurret *pNewTurret)
{
	DestroyTurret();

	if (pNewTurret == nullptr) // Just remove the current turret
		return true;

	if (pNewTurret->TryBindHelicopter(this)) // Attempt to pass ownership
	{
		m_pTurret = pNewTurret;
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
	m_Scale *= HelicopterScale;

	vec2 NewSize = m_Size * HelicopterScale;
	SetSize(NewSize);
	m_pModel->ApplyScale(HelicopterScale);

	UpdateHealthbarIndicator();
}

void CHelicopter::Explode()
{
	m_ExplosionsLeft = m_ExplosionsOnDeath;

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

	if (!m_ExplosionsLeft)
		Reset(); // Early
}

void CHelicopter::TakeDamage(float Damage, vec2 HitPos, int FromID)
{
	if (IsInvincible())
		return;

	m_Armor -= Damage;
	if (m_Armor < 0.0f)
	{
		m_Health += m_Armor;
		m_Armor = 0.0f;
	}
	// m_Health -= Damage;
	m_ShowHealthbarUntil = Server()->Tick() + Server()->TickSpeed() * 3;
	m_LastDamage = Server()->Tick();

	GameServer()->CreateSound(HitPos, SOUND_PLAYER_DIE, m_TeamMask);
	GameServer()->CreateDeath(HitPos, FromID);
}

void CHelicopter::ExplosionDamage(float Strength, vec2 Pos, int FromID)
{
	if (IsInvincible()) // like this check only prevents changing mHealth
		return;

	// gamecontext.cpp : createxplosion
	float Radius = g_pData->m_Explosion.m_Radius;
	float InnerRadius = 48.0f;

	float DistanceFromExplosion = distance(m_Pos, Pos) - GetProximityRadius();
	float Close = 1 - clamp((DistanceFromExplosion - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);
	float Damage = Close * Strength;

	TakeDamage(Damage, m_Pos, FromID);
}

void CHelicopter::Heal(float Health)
{
	m_Health = min(m_Health + Health, m_MaxHealth);
}

void CHelicopter::HealArmor(float Armor)
{
	m_Armor = min(m_Armor + Armor, m_MaxArmor);
}

void CHelicopter::Tick()
{
	CAdvancedEntity::Tick();

	if (m_LastKnownOwner >= 0 && !GameServer()->m_apPlayers[m_LastKnownOwner])
		m_LastKnownOwner = -1;

	BuildHelicopter();

	if (PlacedByTile() && IsBuilding())
	{
		CCollision::SSwitchers *pSwitcher = m_Number > 0 ? &GameServer()->Collision()->m_pSwitchers[m_Number] : 0;
		if (pSwitcher && !pSwitcher->m_Status[m_DDTeam])
		{
			m_Build.m_StartTick++;
		}
		return;
	}

	if (!IsInvincible())
	{
		HandleDropped();

		HandleSeats();

		FlingTeesInPropellersPath();
		ApplyAcceleration();
		HandlePropellers();

		DamageInWall(); // just in case
		DamageInFreeze();

		UpdateHealthbarIndicator();
		RegenerateHelicopterArmor();
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
	bool isGunner = (pController->m_HelicopterSeat == 1 || m_pModel->NumSeats() == 1);

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

	for (int i = 0; i < m_pModel->NumSeats(); i++)
	{
		int passengerCID = m_pModel->Seats()[i].m_SeatedCID;
		if (passengerCID == -1)
			continue;

		CCharacter *pPassenger = GameServer()->GetPlayerChar(passengerCID);

		// Hook acceleration applied to vehicle
		for (int HookerCID : pPassenger->Core()->m_AttachedPlayers)
		{
			CCharacter *pChr = GameServer()->GetPlayerChar(HookerCID);
			if (!pChr || pChr->GetCore().HookedPlayer() != passengerCID)
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
		m_Flipped = !m_Flipped;

	float targetAngle = (m_Vel.x != 0.0f) ? m_Vel.x / (abs(m_Vel.x) + 60) * 90 : 0.0f;
	m_VisualAngle += (targetAngle - m_VisualAngle) * 0.1f;
	SetRotation(m_VisualAngle);
}

void CHelicopter::FlingTeesInPropellersPath() // might be a bug, if propeller pivot x is not 0 (hehe)
{
	// be careful teleporting helicopter, update last propeller data
	CCharacter *pDriver = GetDriver();
	if (!m_EngineOn || !pDriver)
		return;

	int DriverCID = pDriver->GetPlayer()->GetCID();
	SPropeller *aPropellers = m_pModel->Propellers();
	for (int j = 0; j < m_pModel->NumPropellers(); j++)
	{
		if (m_pModel->Propellers()[j].PropellerType() == PROPELLER_CIRCULAR)
			continue;

		vec2 propellersCenter = aPropellers[j].GetCenter();
		if (m_Flipped)
			propellersCenter.x = -propellersCenter.x;

		static CCharacter *aPossibleCollisions[10];
		int numFound = GameWorld()->FindEntities(
			m_Pos + aPropellers[j].GetCenter(),
			aPropellers[j].m_Radius * 1.3f + 200,
			(CEntity **)aPossibleCollisions, 10,
			CGameWorld::ENTTYPE_CHARACTER, m_DDTeam
		);
		if (!numFound)
			return;

		for (int i = 0; i < numFound; i++)
		{
			CCharacter *pChar = aPossibleCollisions[i];
			if (pChar->m_pHelicopter == this || !pChar->CanCollide(DriverCID))
				continue;

			int cID = pChar->GetPlayer()->GetCID();
			if (Server()->Tick() - m_aFlungCharacters[cID] <= 10)
				continue;

			vec2 PosA, PosB;
			aPropellers[j].GetHorizontalPositions(PosA, PosB);
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

void CHelicopter::HandleExplosions()
{
	if (m_ExplosionsLeft <= 0 || Server()->Tick() % 5 != 0)
		return;

	m_ExplosionsLeft--;

	int Diameter = (int)min(GetSize().x, GetSize().y) + 150; // 150 seems fine minimum range for the explosion
	int Radius = Diameter / 2;
	for (int i = 0; i < 3; i++)
	{
		vec2 nearbyPos = m_Pos + vec2((float)(rand() % Diameter - Radius), (float)(rand() % Diameter - Radius));
		GameServer()->CreateExplosion(
			nearbyPos,
			m_Owner,
			WEAPON_GRENADE,
			m_Owner == -1,
			m_DDTeam, m_TeamMask
		);
		GameServer()->CreateSound(nearbyPos, SOUND_GRENADE_EXPLODE, m_TeamMask);
	}

	// F-DDrace
	//    if (pTargetChr)
	//        pTargetChr->TakeDamage(m_Vel * max(0.001f, 0.f), m_Vel * -1, 69.f, m_Owner, WEAPON_PLAYER);

	if (m_ExplosionsLeft == 0)
		Reset();
}

void CHelicopter::UpdateHealthbarIndicator()
{
	if (m_pModel == nullptr)
		return;

	float HealthPercentage = m_Health / m_MaxHealth; // Health 0..1
	float HeartsPrecise = (float)m_NumHeartsIndicator * HealthPercentage; // Hearts like 3.7
	int HeartsFull = floor(HeartsPrecise); // Hearts like 3.0
	int ShowNumHearts = ceil(HeartsPrecise); // How many are shown, including fraction heart (3 + 0.7 = 4)
	float CurrentHeart = HeartsPrecise - (float)HeartsFull; // Last heart 0..1, example 0.7
	bool FlashingHalfHeart = CurrentHeart > 0.0f && CurrentHeart <= 0.5f; // Flash or not, if last heart below half

	for (int i = 0; i < m_NumHeartsIndicator; i++)
	{
		if (i < ShowNumHearts - FlashingHalfHeart) //
			m_aHeartsIndicator[i].m_Enabled = true;
		else if (i == HeartsFull && FlashingHalfHeart) // Flash heart if last was below 0.5
			m_aHeartsIndicator[i].m_Enabled = (Server()->Tick() / 4) % 2 == 0;
		else
			m_aHeartsIndicator[i].m_Enabled = false;
	}

	float ArmorPercentage = m_MaxArmor > 0.0f ? m_Armor / m_MaxArmor : 0.0f; // Armor 0..1
	float ArmorPrecise = (float)m_NumArmorIndicator * ArmorPercentage; // Armor like 2.7
	int ArmorFull = floor(ArmorPrecise); // Armor like 2.0
	int ShowNumArmor = ceil(ArmorPrecise);
	float CurrentArmor = ArmorPrecise - (float)ArmorFull; // Last armor 0..1, example 0.7
	bool FlashingHalfArmor = CurrentArmor > 0.0f && CurrentArmor <= 0.5f;

	for (int i = 0; i < m_NumArmorIndicator; i++)
	{
		if (i < ShowNumArmor - FlashingHalfArmor) //
			m_aArmorIndicator[i].m_Enabled = true;
		else if (i == ArmorFull && FlashingHalfArmor) // Flash if last was below 0.5
			m_aArmorIndicator[i].m_Enabled = (Server()->Tick() / 4) % 2 == 0;
		else
			m_aArmorIndicator[i].m_Enabled = false;
	}

	// Positions yay

	float Gap = 40.f;
	float OffsetY = m_pModel->GetCachedBounds().m_Top - 30.f;
	float OffsetX = -(float)(ShowNumHearts + ShowNumArmor - 1) / 2.f * Gap;
	for (int i = 0; i < ShowNumHearts; i++)
	{
		m_aHeartsIndicator[i].m_Pos.x = OffsetX;
		m_aHeartsIndicator[i].m_Pos.y = OffsetY;
		OffsetX += Gap;
	}
	for (int i = 0; i < ShowNumArmor; i++)
	{
		m_aArmorIndicator[i].m_Pos.x = OffsetX;
		m_aArmorIndicator[i].m_Pos.y = OffsetY;
		OffsetX += Gap;
	}
}

void CHelicopter::RegenerateHelicopterArmor()
{
	if (!CanRegenerateArmor() || Server()->Tick() % Server()->TickSpeed() != 0)
		return;

	HealArmor(0.5f);
}

void CHelicopter::UpdateVisualDamage()
{
	float HealthPercentage = 1.f - m_Health / m_MaxHealth;

	CBone *aBones = m_pModel->Bones();
	for (int i = 0; i < m_pModel->NumBones(); i++)
		aBones[i].m_Thickness = aBones[i].m_InitThickness - (int)(sinf(Server()->Tick() / 4 + i) * HealthPercentage * (float)aBones[i].m_InitThickness);
}

void CHelicopter::DamageInFreeze()
{
	if (m_LastEnvironmentalDamage && m_LastEnvironmentalDamage + Server()->TickSpeed() > Server()->Tick())
		return;

	CCharacter *pDriver = GetDriver();
	if ((m_TileIndex == TILE_FREEZE || m_TileFIndex == TILE_FREEZE) && // In freeze tile
		(!pDriver || !pDriver->m_Super)) // No driver or they are super
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
	if (m_pModel->NumSeated() == 0 || (Server()->Tick() - m_BroadcastingTick) % Server()->TickSpeed() != 0)
		return;

	// CCharacter *pDriver = GetDriver();
	// CCharacter *pGunner = GetGunner();
	// const char *pDriverName = pDriver ? Server()->ClientName(pDriver->GetPlayer()->GetCID()) : "NONE";
	// const char *pGunnerName = pGunner ? Server()->ClientName(pGunner->GetPlayer()->GetCID()) : "NONE";

	const char *pArmorText = "";
	char aArmorMsg[32];
	if (m_Armor >= 1.0f)
	{
		const char *pRegenerationText = CanRegenerateArmor() ? " +1regen" : "";
		str_format(aArmorMsg, sizeof(aArmorMsg), "\nArmor [%i]%s",
		           (int)m_Armor, pRegenerationText);
		pArmorText = aArmorMsg;
	}

	char aMsg[128];
	str_format(aMsg, sizeof(aMsg), "> %s <\nHealth [%i]%s\n\nF3 - Switch seat\nF4 - Dismount",
	           m_pName, (int)m_Health, pArmorText);

	for (int i = 0; i < m_pModel->NumSeats(); i++)
	{
		int passengerCID = m_pModel->Seats()[i].m_SeatedCID;
		if (passengerCID == -1)
			continue;

		CCharacter *pPassenger = GameServer()->GetPlayerChar(passengerCID);
		pPassenger->SendBroadcastHud(aMsg);
	}
}

void CHelicopter::HandleSeats()
{
	for (int i = 0; i < m_pModel->NumSeats(); i++)
	{
		SSeat& Seat = m_pModel->Seats()[i];
		int passengerCID = Seat.m_SeatedCID;
		if (passengerCID == -1)
			continue;

		CCharacter *pCharacter = GameServer()->GetPlayerChar(passengerCID);
		bool isDriver = (i == 0);

		vec2 targetSeatPos = Seat.m_Seat;
		if (m_Flipped)
			targetSeatPos.x = -targetSeatPos.x;
		Seat.m_Interpolated += (targetSeatPos - Seat.m_Interpolated) * 0.1f;

		vec2 resultSeatPos = m_Pos + Seat.m_Interpolated;
		pCharacter->ForceSetPos(resultSeatPos);
		pCharacter->Core()->m_Vel = vec2(0, 0);

		if (isDriver)
		{
			SetFlags(EFlags::APPLY_GRAVITY, pCharacter->m_FreezeTime > 0);
			SetFlags(EFlags::APPLY_GROUND_VEL, pCharacter->m_FreezeTime || m_Accel.x == 0.f);
		}

		if (pCharacter->m_DeepFreeze)
			Dismount(passengerCID);
	}
}

void CHelicopter::HandlePropellers()
{
	if (!m_EngineOn)
		return;

	if (!GetDriver() && IsGrounded()) // Reset top propellers when grounded without driver
	{
		if (m_EngineOn)
			ResetAndTurnOff();

		return;
	}

	m_pModel->SpinPropellers(); //
}

bool CHelicopter::Mount(int ClientID, int WantedSeat)
{
	if (ClientID < 0 || ClientID > MAX_CLIENTS || m_pModel->NumSeated() >= m_pModel->NumSeats() || IsInvincible()) // used to check m_Owner != -1 (aka driver)
		return false;

	// scale specific mount condition
	CCharacter *pCharacter = GameServer()->GetPlayerChar(ClientID);
	if (pCharacter->m_pHelicopter || distance(pCharacter->GetPos(), m_Pos) > pCharacter->GetProximityRadius() + GetProximityRadius())
		return false;

	if (WantedSeat < 0 || WantedSeat >= m_pModel->NumSeats() || m_pModel->Seats()[WantedSeat].m_SeatedCID != -1)
		WantedSeat = -1; // Default to any available seat

	int gotSeatedAt = -1;
	for (int i = 0; i < m_pModel->NumSeats(); i++)
	{
		SSeat& Seat = m_pModel->Seats()[i];

		// Slot is taken
		//  or we have a wanted seat, but it isn't our seat yet
		if (Seat.m_SeatedCID != -1 || (WantedSeat != -1 && WantedSeat != i))
		{
			// The seat we wanted is full, give up :(
			if (WantedSeat == i)
				return false;

			continue;
		}

		gotSeatedAt = i;
		m_pModel->Seats()[i].m_SeatedCID = ClientID;
		pCharacter->m_HelicopterSeat = i;
		pCharacter->m_SeatSwitchedTick = Server()->Tick();
		m_pModel->SetNumSeated(m_pModel->NumSeated() + 1);

		// Previous
		// Seat.m_Interpolated = Seat.m_Transformed; // Teleport for mounting

		// Experimental
		Seat.m_Interpolated = pCharacter->GetPos() - m_Pos; // Swing from character position to the seat

		break;
	}

	if (gotSeatedAt == -1)
		return false;

	// Driver seat
	if (gotSeatedAt == 0)
	{
		m_Owner = ClientID;
		m_LastKnownOwner = ClientID;
		m_EngineOn = true;
		SetFlags(EFlags::APPLY_GRAVITY | EFlags::APPLY_GROUND_VEL, false);
	}

	m_ShowHealthbarUntil = Server()->Tick() + Server()->TickSpeed() * 3;
	pCharacter->m_pHelicopter = this;
	pCharacter->SetWeapon(-1);
	GameServer()->SendTuningParams(ClientID, pCharacter->m_TuneZone);
	m_BroadcastingTick = Server()->Tick() + 1; // Start updating broadcast next tick

	return true;
}

void CHelicopter::Dismount(int ClientID, bool ForceDismountAtHelicopter)
{
	for (int i = 0; i < m_pModel->NumSeats(); i++)
	{
		SSeat& Seat = m_pModel->Seats()[i];
		int passengerCID = Seat.m_SeatedCID;
		if (passengerCID == -1)
			continue;

		bool isDriver = (i == 0);
		bool isGunner = (i == 1);
		if (ClientID == -1 || passengerCID == ClientID)
		{
			CCharacter *pCharacter = GameServer()->GetPlayerChar(passengerCID);
			if (isDriver)
			{
				SetFlags(EFlags::APPLY_GRAVITY | EFlags::APPLY_GROUND_VEL, true);
				m_Accel.y = 0;
				m_Owner = -1;
			}
			if (isGunner && m_pTurret)
				m_pTurret->m_Shooting = false;

			if (pCharacter)
			{
				pCharacter->m_pHelicopter = nullptr;
				if (ForceDismountAtHelicopter)
				{
					// Dismount at the bottom when propellers running and you're not the only passenger
					vec2 dismountPos = (m_EngineOn && m_pModel->NumSeated() >= 2) ? vec2(0, m_Size.y / 2.0f - CCharacterCore::PHYS_SIZE / 2.0f - 1.0f) : vec2(0, 0);
					pCharacter->ForceSetPos(m_Pos + dismountPos);
				}
				pCharacter->SetWeapon(pCharacter->GetLastWeapon());
				GameServer()->SendTuningParams(ClientID, pCharacter->m_TuneZone);
				pCharacter->SendBroadcastHud("");
			}

			Seat.m_SeatedCID = -1;
			m_pModel->SetNumSeated(m_pModel->NumSeated() - 1);

			// Dismount only one player
			if (ClientID != -1)
				break;
		}
	}
}

void CHelicopter::SetRotation(float NewRotation)
{
	m_Angle = NewRotation;
	m_pModel->SetRotation(NewRotation * (m_Flipped ? -1.0f : 1.0f));

	if (m_pTurret)
		m_pTurret->SetRotation(NewRotation, m_pTurret->GetTurretRotation());
}

void CHelicopter::Snap(int SnappingClient)
{
	if (IsExploding())
		return;

	if (NetworkClipped(SnappingClient) || !CmaskIsSet(m_TeamMask, SnappingClient))
		return;

	CCharacter *pChar = GameServer()->GetPlayerChar(SnappingClient);
	if (IsBuilding())
	{
		// only show when constructing, switch is active
		CCollision::SSwitchers *pSwitcher = m_Number > 0 ? &GameServer()->Collision()->m_pSwitchers[m_Number] : 0;
		if (!PlacedByTile() || (pSwitcher && pSwitcher->m_Status[m_DDTeam]))
		{
			const SBounds& ModelBounds = m_pModel->GetCachedBounds();
			for (int i = 0; i < NUM_BUILD_IDS; i++)
			{
				CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_Build.m_aBuildIDs[i],
																								sizeof(CNetObj_Projectile)));
				if (!pObj)
					continue;

				pObj->m_X = round_to_int(m_Pos.x + ModelBounds.m_Left + rand() % (int)m_pModel->GetTotalSize().x);
				pObj->m_Y = round_to_int(m_Pos.y + m_Build.m_CachedHeight);
				pObj->m_VelX = 0;
				pObj->m_VelY = 0;
				pObj->m_StartTick = Server()->Tick();
				pObj->m_Type = WEAPON_HAMMER;
			}
		}
	}
	else if (
		(!m_pModel->NumSeats() || IsPassenger(pChar)) && // Show when no passengers or if you're a passenger
		(ShouldShowHealthbar() || !IsFullHealthAndArmor()) && // Show until specified (on mount, damage, etc.) or when not max
		(!m_LastDamage || (m_LastDamage && m_LastDamage + Server()->TickSpeed() / 2 <= Server()->Tick()) || // Show if damaged not long ago
			ShouldFlashDamagedHearts())) // Show flashing damaged recently
	{
		for (int i = 0; i < m_NumHeartsIndicator; i++)
			m_aHeartsIndicator[i].Snap(SnappingClient);
		for (int i = 0; i < m_NumArmorIndicator; i++)
			m_aArmorIndicator[i].Snap(SnappingClient);
	}

	// Draw helicopter
	CCharacter *pDriver = GetDriver();
	bool HelicopterRainbowMode = pDriver && (pDriver->m_Rainbow || pDriver->m_IsRainbowHooked);
	m_pModel->Snap(SnappingClient, m_EngineOn, m_Flipped, floor((1.0f - m_Health / m_MaxHealth) * 10), HelicopterRainbowMode);

	// Draw guns
	if (m_pTurret)
		m_pTurret->Snap(SnappingClient);
}

void CHelicopter::InitBuildAnimation()
{
	// Only use in constructor
	for (int i = 0; i < NUM_BUILD_IDS; i++)
		m_Build.m_aBuildIDs[i] = Server()->SnapNewID();

	m_Build.m_StartTick = Server()->Tick();
	m_Build.m_Building = true;
	m_pModel->InitBuildAnimation();
}

void CHelicopter::BuildHelicopter()
{
	if (!m_Build.m_Building || !m_Build.m_Duration)
		return;

	const SBounds& ModelBounds = m_pModel->GetCachedBounds();
	int builtTicks = Server()->Tick() - m_Build.m_StartTick;
	float builtProgress = std::min(1.0f, (float)builtTicks / (float)m_Build.m_Duration);
	float currentHeight = ModelBounds.m_Bottom - m_pModel->GetTotalSize().y * builtProgress;
	m_Build.m_CachedHeight = currentHeight;

	CBone *aBones = m_pModel->Bones();
	for (int i = 0; i < m_pModel->NumBones(); i++)
	{
		const CBone& Bone = aBones[i];
		vec2 From = Bone.m_InitFrom;
		vec2 To = Bone.m_InitTo;

		bool FromUnder = From.y >= currentHeight;
		bool ToUnder = To.y >= currentHeight;

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
				float t = (currentHeight - From.y) / (To.y - From.y);
				vec2 Intersect = {
					From.x + (To.x - From.x) * t,
					currentHeight
				};
				aBones[i].m_From = From;
				aBones[i].m_To = Intersect;
			}
			else // if (!FromUnder && ToUnder)
			{
				// To is under, From is above — keep To, clip From
				float t = (currentHeight - To.y) / (From.y - To.y);
				vec2 Intersect = {
					To.x + (From.x - To.x) * t,
					currentHeight
				};
				aBones[i].m_From = Intersect;
				aBones[i].m_To = To;
			}
		}
	}

	CTrailNode *aTrails = m_pModel->Trails();
	for (int i = 0; i < m_pModel->NumTrails(); i++)
		aTrails[i].m_Enabled = aTrails[i].m_pPos->y > currentHeight;

	if (builtTicks >= m_Build.m_Duration)
	{
		// Finished building, no more animation required
		m_Build.m_Building = false;
		for (int i = 0; i < NUM_BUILD_IDS; i++)
		{
			Server()->SnapFreeID(m_Build.m_aBuildIDs[i]);
			m_Build.m_aBuildIDs[i] = -1;
		}

		for (int i = 0; i < m_pModel->NumBones(); i++)
			aBones[i].m_Color = aBones[i].m_InitColor;
	}
}

bool CHelicopter::IsPassenger(CCharacter *pChar)
{
	return pChar && pChar->m_pHelicopter == this;
}

bool CHelicopter::ShouldShowHealthbar()
{
	return m_ShowHealthbarUntil && m_ShowHealthbarUntil > Server()->Tick();
}

bool CHelicopter::ShouldFlashDamagedHearts()
{
	return m_LastDamage + Server()->TickSpeed() / 2 > Server()->Tick() && // Within half a second
		(Server()->Tick() / 4) % 2 == 0; // flash rapidly
}

void CHelicopter::SortBones()
{
	// meant for adding turret on top of helicopter, might add more stuff later
	CBone *aModel = m_pModel->Bones();
	int NumModel = m_pModel->NumBones();

	CBone *aTurret = nullptr;
	int NumTurret = 0;
	if (m_pTurret)
	{
		aTurret = m_pTurret->Bones();
		NumTurret = m_pTurret->GetNumBones();
	}

	int NumTotal = NumModel + NumTurret;
	if (NumTotal > MAX_BONES_SORT)
	{
		dbg_msg("helicopter", "SortBones() exceeds the amount of bones allowed for sorting: %i/%i", NumTotal, MAX_BONES_SORT);
		return;
	}

	// Get current IDs
	static int aIDs[MAX_BONES_SORT];
	for (int i = 0; i < NumModel; i++)
		aIDs[i] = aModel[i].m_ID;
	for (int i = 0; i < NumTurret; i++)
		aIDs[NumModel + i] = aTurret[i].m_ID;

	// Sort IDs
	std::sort(aIDs, aIDs + NumTotal);

	// Update sorted IDs (low -> high | ascending)
	for (int i = 0; i < NumModel; i++)
		aModel[i].m_ID = aIDs[i];
	for (int i = 0; i < NumTurret; i++)
		aTurret[i].m_ID = aIDs[NumModel + i];
}

void CHelicopter::ResetAndTurnOff()
{
	// Dismounted & grounded
	m_EngineOn = false;

	m_pModel->ResetPropellers();
}
