//
// Created by Matq on 14/04/2026.
//

#include "vehicle.h"
#include <game/server/entities/interactive/vehicle/spider.h>
#include <game/server/gamecontext.h>
#include "generated/server_data.h"
#include "engine/server.h"

vec2 IVehicle::MinimumVehicleHitbox(vec2 Hitbox)
{
	return { maximum(Hitbox.x, CCharacterCore::PHYS_SIZE), maximum(Hitbox.y, CCharacterCore::PHYS_SIZE) };
}

IVehicle::IVehicle(CGameWorld *pGameWorld, int VehicleType, int Objtype, vec2 Pos, vec2 BaseSize, int Owner, int Team, int Number, int BuildTime)
	: CAdvancedEntity(pGameWorld, Objtype, Pos, BaseSize, Owner), m_HealthBar(this)
{
	SetFlags(EFlags::ALLOW_VIP_PLUS, false);
	m_Elasticity = vec2(0.f, 0.f);
	m_DDTeam = Team;
	m_Number = Number;
	m_SwitchDelay = -1;
	m_NextSpawnTick = 0;
	m_InitialPosition = Pos;

	m_VehicleType = VehicleType;
	m_BaseSize = BaseSize;
	m_InputDirection = 0;
	m_MaxHealth = 100;
	m_Health = m_MaxHealth;
	m_MaxArmor = 0;
	m_Armor = m_MaxArmor;
	m_pName = "Vehicle";
	m_Scale = 1.f;
	m_EngineOn = false;
	m_pModel = nullptr;
	m_apAttachments = nullptr;
	m_NumAttachments = 0;
	m_Flipped = false;
	m_Angle = 0.0f;
	m_VisualAngle = 0.0f;

	// for (int i = 0; i < NumAttachments; i++)
	// 	m_apAttachments[i] = nullptr;
	//
	// for (int i = 0; i < NumAttachments; i++)
	// {
	// 	IVehicleTurret *pNewTurret = new CLauncherTurret();
	// 	pNewTurret->TryBindVehicle(this);
	// 	m_apAttachments[i] = pNewTurret;
	// }

	m_ExplosionsOnDeath = 1;
	m_ExplosionsLeft = 0;
	m_DriverFreezeTime = 0;
	m_ShowHealthbarUntil = Server()->Tick() + Server()->TickSpeed() * 5;
	m_LastDamage = 0;
	m_LastEnvironmentalDamage = 0;
	m_LastKnownOwner = Owner;
	m_BroadcastingTick = 0;

	m_Build.m_StartTick = 0;
	m_Build.m_Duration = BuildTime;
	m_Build.m_Building = false;
	m_Build.m_CachedHeight = 0.0f;
	for (int i = 0; i < NUM_BUILD_IDS; i++)
		m_Build.m_aBuildIDs[i] = -1;

	//
}

IVehicle::~IVehicle()
{
	m_HealthBar.SetIndicator(0, 0); // aka SnapFreeID()

	for (int i = 0; i < m_NumAttachments; i++)
	{
		delete m_apAttachments[i];
		m_apAttachments[i] = nullptr;
	}
	delete[] m_apAttachments;
	m_apAttachments = nullptr;
	delete m_pModel;
	m_pModel = nullptr;

	for (int i = 0; i < NUM_BUILD_IDS; i++)
		if (m_Build.m_aBuildIDs[i] != -1) // should only pass if deleted while crafting a vehicle
			Server()->SnapFreeID(m_Build.m_aBuildIDs[i]);
}

void IVehicle::Reset()
{
	Dismount(-1);
	CAdvancedEntity::Reset();
}

bool IVehicle::CanRegenerateArmor()
{
	return m_Armor != m_MaxArmor && // Regenerate if needs to
	(!m_LastDamage || // Was not damaged
		m_LastDamage + Server()->TickSpeed() * 60 < Server()->Tick() || // Or damaged 60 seconds ago
		(m_Health == m_MaxHealth && m_LastDamage + Server()->TickSpeed() * 20 < Server()->Tick())); // Or damage 20 seconds ago, with full health
}

CCharacter *IVehicle::GetDriver()
{
	for (int i = 0; i < m_pModel->NumSeats(); i++)
	{
		int passengerCID = m_pModel->Seats()[i].m_SeatedCID;
		if (passengerCID == -1)
			continue;

		if (m_pModel->Seats()[i].m_Type == SEATTYPE_DRIVER)
			return GameServer()->GetPlayerChar(passengerCID);
	}
	return nullptr;
}

int IVehicle::GetNextAvailableSeat(int CheckFromIndex)
{
	int NumSeats = m_pModel->NumSeats();
	for (int i = 0; i < NumSeats; i++)
	{
		int SeatID = (CheckFromIndex + i) % NumSeats;
		if (m_pModel->Seats()[SeatID].m_SeatedCID == -1)
			return SeatID;
	}

	return -1; // Full
}

bool IVehicle::TryAttach(IVehicleTurret *pNewTurret)
{
	if (!pNewTurret || m_NumAttachments >= m_AttachmentsCap)
		return false;

	if (pNewTurret->TryBindVehicle(this)) // Attempt to pass ownership
	{
		m_apAttachments[m_NumAttachments] = pNewTurret;
		m_NumAttachments++;

		SortBones();
		return true;
	}

	return false;
}

void IVehicle::AllocateNumAttachments(int NumAttachments)
{
	for (int i = 0; i < m_NumAttachments; i++)
		delete m_apAttachments[i];

	delete[] m_apAttachments;
	m_apAttachments = new IVehicleTurret *[NumAttachments]();
	m_AttachmentsCap = NumAttachments;
	m_NumAttachments = 0;
}

void IVehicle::SetVehicleMetadata(const SVehicleMeta& metadata, bool HealFullyToo)
{
	m_pName = metadata.m_pName;
	m_MaxHealth = metadata.m_BaseHealth;
	m_MaxArmor = metadata.m_BaseArmorHealth;
	m_HealthBar.SetVertical(metadata.m_VerticalHealthbar);
	m_HealthBar.SetIndicator(metadata.m_NumHeartsIndicator, metadata.m_NumArmorIndicator);
	m_ExplosionsOnDeath = metadata.m_NumExplosions;
	SetSize(metadata.m_BaseSize);
	m_BaseAccel = metadata.m_BaseAccel;

	// Model
	IVehicleModel *pNewModel = metadata.m_pModelFactory(this);
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

	m_HealthBar.UpdateIndicator();
}

void IVehicle::ApplyScale(float VehicleScale)
{
	m_Scale *= VehicleScale;

	vec2 NewSize = MinimumVehicleHitbox(m_BaseSize * m_Scale);
	SetSize(NewSize);
	m_pModel->ApplyScale(VehicleScale);

	for (int i = 0; i < m_NumAttachments; i++)
		if (m_apAttachments[i])
			m_apAttachments[i]->ApplyScale(VehicleScale);

	m_HealthBar.UpdateIndicator();
}

void IVehicle::Explode()
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

void IVehicle::TakeDamage(float Damage, vec2 HitPos, int FromID)
{
	if (IsInvincible())
		return;

	m_Armor -= Damage;
	if (m_Armor < 0.0f)
	{
		m_Health += m_Armor;
		m_Armor = 0.0f;
	}
	m_ShowHealthbarUntil = Server()->Tick() + Server()->TickSpeed() * 3;
	m_LastDamage = Server()->Tick();

	GameServer()->CreateSound(HitPos, SOUND_PLAYER_DIE, m_TeamMask);
	GameServer()->CreateDeath(HitPos, FromID);
}

void IVehicle::ExplosionDamage(float Strength, vec2 Pos, int FromID)
{
	if (IsInvincible()) // like this check only prevents changing mHealth
		return;

	// gamecontext.cpp : createexplosion
	float Radius = g_pData->m_Explosion.m_Radius;
	float InnerRadius = 48.0f;

	float DistanceFromExplosion = distance(m_Pos, Pos) - GetProximityRadius();
	float Close = 1 - clamp((DistanceFromExplosion - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);
	float Damage = Close * Strength;

	TakeDamage(Damage, m_Pos, FromID);
}

void IVehicle::HealHealth(float Health)
{
	m_Health = minimum(m_Health + maximum(Health, 0.0f), m_MaxHealth);
}

void IVehicle::HealArmor(float Armor)
{
	m_Armor = minimum(m_Armor + maximum(Armor, 0.0f), m_MaxArmor);
}

void IVehicle::Tick()
{
	CAdvancedEntity::Tick();

	if (m_LastKnownOwner >= 0 && !GameServer()->m_apPlayers[m_LastKnownOwner])
		m_LastKnownOwner = -1;

	if (HandleBuilding())
		return;
}

bool IVehicle::OnInput(CNetObj_PlayerInput *pNewInput, CCharacter *pControllerChar)
{
	if (!m_pModel || !pControllerChar || pControllerChar->m_pVehicle != this)
		return false;

	int CharSeat = pControllerChar->m_VehicleSeat;
	if (CharSeat < 0 || CharSeat >= m_pModel->NumSeats())
		return false;

	SSeat& Seat = m_pModel->Seats()[CharSeat];
	SSeat::SInputs& SeatInputs = Seat.m_Inputs;
	SeatInputs.m_WalkDirection = pNewInput->m_Direction;
	SeatInputs.m_Fire = pNewInput->m_Fire;
	SeatInputs.m_Hook = pNewInput->m_Hook;
	SeatInputs.m_Jump = pNewInput->m_Jump;
	SeatInputs.m_MouseX = pNewInput->m_TargetX;
	SeatInputs.m_MouseY = pNewInput->m_TargetY;

	SeatInputs.m_HeldWalkDirection = SeatInputs.m_WalkDirection;
	SeatInputs.m_HeldFire = SeatInputs.m_Fire % 2 == 1;
	SeatInputs.m_HeldHook = SeatInputs.m_Hook % 2 == 1;
	SeatInputs.m_HeldJump = SeatInputs.m_Jump % 2 == 1;

	return true;
}

void IVehicle::TickAttachments()
{
	for (int i = 0; i < m_NumAttachments; i++)
		if (m_apAttachments[i])
			m_apAttachments[i]->Tick();
}

void IVehicle::SetRotation(float NewRotation)
{
	m_Angle = NewRotation;
	m_pModel->SetRotation(NewRotation * (m_Flipped ? -1.0f : 1.0f));

	for (int i = 0; i < m_NumAttachments; i++)
	{
		IVehicleTurret *pTurret = m_apAttachments[i];
		if (!pTurret)
			continue;

		pTurret->SetRotation(NewRotation, pTurret->GetTurretRotation());
	}
}

void IVehicle::HandleHookedPassengers()
{
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
}

void IVehicle::ApplyAcceleration()
{
	if (!m_EngineOn)
		m_Accel = vec2(0.f, 0.f);

	m_Vel.x += m_BaseAccel.x * m_Accel.x;
	m_Vel.y += m_BaseAccel.y * m_Accel.y;
	m_Vel.y *= 0.95f;
}

void IVehicle::FlingTeesInPropellersPath() // might be a bug, if propeller pivot x is not 0 (hehe)
{
	// be careful teleporting helicopter, update last propeller data
	CCharacter *pDriver = GetDriver();
	if (!m_EngineOn || !pDriver)
		return;

	int DriverCID = pDriver->GetPlayer()->GetCID();
	for (int j = 0; j < m_pModel->NumPropellers(); j++)
	{
		SPropeller& Propeller = m_pModel->Propellers()[j];
		if (Propeller.PropellerType() == PROPELLER_CIRCULAR)
			continue;

		vec2 propellersCenter = Propeller.GetCenter();
		if (m_Flipped)
			propellersCenter.x = -propellersCenter.x;

		static CCharacter *aPossibleCollisions[10];
		int numFound = GameWorld()->FindEntities(
			m_Pos + Propeller.GetCenter(),
			Propeller.m_Radius * 1.3f + 200,
			(CEntity **)aPossibleCollisions, 10,
			CGameWorld::ENTTYPE_CHARACTER, m_DDTeam
		);
		if (!numFound)
			return;

		for (int i = 0; i < numFound; i++)
		{
			CCharacter *pChar = aPossibleCollisions[i];
			if (pChar->m_pVehicle == this || !pChar->CanCollide(DriverCID))
				continue;

			int cID = pChar->GetPlayer()->GetCID();
			if (Server()->Tick() - m_aFlungCharacters[cID] <= 10)
				continue;

			vec2 PosA, PosB;
			Propeller.GetHorizontalPositions(PosA, PosB);
			bool collisionDetected = MovingCircleHitsMovingSegment_Analytical(
				pChar->m_PrevPos - m_Pos, pChar->GetPos() - m_Pos,
				pChar->GetProximityRadius(),
				Propeller.m_LastA, PosA,
				Propeller.m_LastB, PosB);
			if (collisionDetected)
			{
				m_aFlungCharacters[cID] = Server()->Tick();
				Propeller.FlingTee(pChar, this);
			}
		}
	}
}

bool IVehicle::HandleSpawning()
{
	if (PlacedByTile() && IsBuilding())
	{
		CCollision::SSwitchers *pSwitcher = m_Number > 0 ? &GameServer()->Collision()->m_pSwitchers[m_Number] : 0;
		if (pSwitcher && !pSwitcher->m_Status[m_DDTeam])
		{
			m_Build.m_StartTick++;
		}
		return true;
	}

	return false;
}

bool IVehicle::TryRespawnNewVehicle()
{
	return false; // override
}

void IVehicle::HandleExplosions()
{
	if (m_ExplosionsLeft <= 0 || Server()->Tick() % 5 != 0)
		return;

	m_ExplosionsLeft--;

	int Diameter = (int)minimum(GetSize().x, GetSize().y) + 150; // 150 seems fine minimum range for the explosion
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
	//        pTargetChr->TakeDamage(m_Vel * maximum(0.001f, 0.f), m_Vel * -1, 69.f, m_Owner, WEAPON_PLAYER);

	if (m_ExplosionsLeft == 0)
		Reset();
}

void IVehicle::RegenerateArmor()
{
	if (!CanRegenerateArmor() || Server()->Tick() % Server()->TickSpeed() != 0)
		return;

	HealArmor(0.5f);
}

void IVehicle::TickVisualBoneDamage()
{
	if (Server()->Tick() % 2 == 1)
		return;

	float HealthPercentage = 1.f - m_Health / m_MaxHealth;
	CBone *aBones = m_pModel->Bones();
	for (int i = 0; i < m_pModel->NumBones(); i++)
		aBones[i].m_Thickness = aBones[i].m_InitThickness - (int)(sinf(Server()->Tick() / 4 + i) * HealthPercentage * (float)aBones[i].m_InitThickness);
}

void IVehicle::DamageInFreeze()
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

void IVehicle::DamageInWall()
{
	if ((!m_LastEnvironmentalDamage || m_LastEnvironmentalDamage + Server()->TickSpeed() < Server()->Tick()) && // Eligible to take freeze damage again
		GameServer()->Collision()->TestBoxBig(m_Pos, m_Size))
	{
		TakeDamage(20, m_Pos, m_LastKnownOwner);
		m_LastEnvironmentalDamage = Server()->Tick();
	}
}

bool IVehicle::CanSendBroadcastIndicator()
{
	return m_pModel->NumSeated() > 0 && (Server()->Tick() - m_BroadcastingTick) % Server()->TickSpeed() == 0;
}

void IVehicle::SendBroadcastIndicator()
{
	if (!CanSendBroadcastIndicator())
		return;

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
	const char* pSwitchSeatMsg = m_pModel->NumSeats() > 1 ? "F3 - Switch seat\n" : "";
	str_format(aMsg, sizeof(aMsg), "> %s <\nHealth [%i]%s\n\n%sF4 - Dismount", m_pName, (int)m_Health, pArmorText, pSwitchSeatMsg);
	SendBroadcastToPassengers(aMsg);
}

void IVehicle::SendBroadcastToPassengers(const char *pMsg)
{
	for (int i = 0; i < m_pModel->NumSeats(); i++)
	{
		int passengerCID = m_pModel->Seats()[i].m_SeatedCID;
		if (passengerCID == -1)
			continue;

		CCharacter *pPassenger = GameServer()->GetPlayerChar(passengerCID);
		pPassenger->SendBroadcastHud(pMsg);
	}
}

void IVehicle::HandleSeat(SSeat& Seat, int PassengerCID, CCharacter *pChar)
{
	if (Seat.m_Type == SEATTYPE_DRIVER)
	{
		if (!m_DriverFreezeTime)
			m_DriverFreezeTime = pChar->m_FreezeTime;
		else
			m_DriverFreezeTime = minimum(m_DriverFreezeTime, pChar->m_FreezeTime);
	}

	if (pChar->m_DeepFreeze || (pChar->m_FreezeTime && m_Armor <= 0.0f))
		Dismount(PassengerCID);
}

void IVehicle::HandleSeats()
{
	for (int i = 0; i < m_pModel->NumSeats(); i++)
	{
		SSeat& Seat = m_pModel->Seats()[i];
		int passengerCID = Seat.m_SeatedCID;
		if (passengerCID == -1)
			continue;

		CCharacter *pCharacter = GameServer()->GetPlayerChar(passengerCID);

		// Handle seat positions
		vec2 targetSeatPos = Seat.m_Seat;
		if (m_Flipped)
			targetSeatPos.x = -targetSeatPos.x;
		Seat.m_Interpolated += (targetSeatPos - Seat.m_Interpolated) * 0.1f;

		vec2 resultSeatPos = m_Pos + Seat.m_Interpolated;
		pCharacter->ForceSetPos(resultSeatPos);
		pCharacter->Core()->m_Vel = vec2(0, 0);

		HandleSeat(Seat, passengerCID, pCharacter);
	}
}

void IVehicle::DriversDismounted()
{
	SetFlags(EFlags::APPLY_GROUND_VEL, true);
	m_Owner = -1; //
}

void IVehicle::DriversFrozen()
{
	SetFlags(EFlags::APPLY_GRAVITY, true);
	SetFlags(EFlags::APPLY_GROUND_VEL, true);
	m_Accel = vec2(0.0f, 0.0f);
}

void IVehicle::TickRopes()
{
	for (int i = 0; i < m_pModel->NumRopes(); i++)
		m_pModel->Ropes()[i].Tick();
}

void IVehicle::TickPropellers()
{
	if (!m_EngineOn)
		return;

	m_pModel->SpinPropellers();
}

bool IVehicle::Mount(int ClientID, int WantedSeat)
{
	if (ClientID < 0 || ClientID > MAX_CLIENTS || m_pModel->NumSeated() >= m_pModel->NumSeats() || IsInvincible()) // used to check m_Owner != -1 (aka driver)
		return false;

	// scale specific mount condition
	CCharacter *pCharacter = GameServer()->GetPlayerChar(ClientID);
	if (pCharacter->m_pVehicle || distance(pCharacter->GetPos(), m_Pos) > pCharacter->GetProximityRadius() + GetProximityRadius())
		return false;

	if (WantedSeat < 0 || WantedSeat >= m_pModel->NumSeats() || m_pModel->Seats()[WantedSeat].m_SeatedCID != -1)
		WantedSeat = -1; // Default to any available seat

	SSeat *pSeatedAt = nullptr;
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

		pSeatedAt = &Seat;
		gotSeatedAt = i;
		m_pModel->Seats()[i].m_SeatedCID = ClientID;
		pCharacter->m_VehicleSeat = i;
		pCharacter->m_SeatSwitchedTick = Server()->Tick();
		m_pModel->SetNumSeated(m_pModel->NumSeated() + 1);
		UpdateAttachmentControllerCounts();

		// Previous
		// Seat.m_Interpolated = Seat.m_Transformed; // Teleport for mounting

		// Experimental
		Seat.m_Interpolated = pCharacter->GetPos() - m_Pos; // Swing from character position to the seat

		break;
	}

	if (gotSeatedAt == -1)
		return false;

	// Driver seat
	if (pSeatedAt->m_Type == SEATTYPE_DRIVER) //
	{
		m_Owner = ClientID;
		m_LastKnownOwner = ClientID;
		m_EngineOn = true;
		SetFlags(EFlags::APPLY_GRAVITY, false);
		SetFlags(EFlags::APPLY_GROUND_VEL, false);
	}

	m_ShowHealthbarUntil = Server()->Tick() + Server()->TickSpeed() * 3;
	pCharacter->m_pVehicle = this;
	pCharacter->SetWeapon(-1);
	GameServer()->SendTuningParams(ClientID, pCharacter->m_TuneZone);
	m_BroadcastingTick = Server()->Tick() + 1; // Start updating broadcast next tick

	return true;
}

void IVehicle::Dismount(int ClientID, bool ForceDismountAtHelicopter)
{
	int NumDrivers = 0;
	int NumDismountedDrivers = 0;
	int NumTotal = 0;
	int NumDismounted = 0;
	for (int i = 0; i < m_pModel->NumSeats(); i++)
	{
		SSeat& Seat = m_pModel->Seats()[i];
		int passengerCID = Seat.m_SeatedCID;
		if (passengerCID == -1)
			continue;

		NumTotal++;
		if (Seat.m_Type == SEATTYPE_DRIVER)
			NumDrivers++;

		if (ClientID == -1 || passengerCID == ClientID)
		{
			CCharacter *pCharacter = GameServer()->GetPlayerChar(passengerCID);
			NumDismounted++;
			if (Seat.m_Type == SEATTYPE_DRIVER)
				NumDismountedDrivers++;
			if (Seat.m_AttachmentID >= 0 && Seat.m_AttachmentID < m_NumAttachments && m_apAttachments[Seat.m_AttachmentID])
				m_apAttachments[Seat.m_AttachmentID]->Dismounted();

			if (pCharacter)
			{
				pCharacter->m_pVehicle = nullptr;
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

			Seat.Dismounted();
			m_pModel->SetNumSeated(m_pModel->NumSeated() - 1);

			// Dismount only one player
			if (ClientID != -1)
			{
				UpdateAttachmentControllerCounts();
				break;
			}
		}
	}

	if (ClientID == -1 && NumDismounted)
		UpdateAttachmentControllerCounts();

	if (NumTotal == NumDismounted)
		m_ShowHealthbarUntil = Server()->Tick() + Server()->TickSpeed() * 3;

	if (NumDrivers == NumDismountedDrivers)
		DriversDismounted();
}

void IVehicle::Snap(int SnappingClient)
{
	if (IsExploding())
		return;

	if (NetworkClipped(SnappingClient) || !CmaskIsSet(m_TeamMask, SnappingClient))
		return;

	SnapBuildingParticles(SnappingClient);
	SnapHealthBar(SnappingClient);

	// Draw vehicle
	CCharacter *pDriver = GetDriver();
	SBoneModelSnapping Options = {
		m_EngineOn, // m_SendTrails
		m_Flipped, // m_Flipped
		floor((1.0f - m_Health / m_MaxHealth) * 10), // m_VertexSnapping
		pDriver && (pDriver->m_Rainbow || pDriver->m_IsRainbowHooked) // m_RainbowMode
	};
	SnapModel(SnappingClient, Options);
	SnapAttachments(SnappingClient, Options);
}

void IVehicle::InitBuildAnimation()
{
	if (m_Build.m_Duration <= 0)
		return;

	// Only use in constructor
	for (int i = 0; i < NUM_BUILD_IDS; i++)
		m_Build.m_aBuildIDs[i] = Server()->SnapNewID();

	m_Build.m_StartTick = Server()->Tick();
	m_Build.m_Building = true;
	m_pModel->InitBuildAnimation();
}

bool IVehicle::HandleBuilding()
{
	if (!m_Build.m_Building || !m_Build.m_Duration)
		return false;

	const SBounds& ModelBounds = m_pModel->GetCachedBounds();
	int builtTicks = Server()->Tick() - m_Build.m_StartTick;
	float builtProgress = minimum(1.0f, (float)builtTicks / (float)m_Build.m_Duration);
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

		CTrailNode *aTrails = m_pModel->Trails();
		for (int i = 0; i < m_pModel->NumTrails(); i++)
			aTrails[i].m_Enabled = aTrails[i].m_InitEnabled;
	}

	return true;
}

bool IVehicle::IsPassenger(CCharacter *pChar)
{
	return pChar && pChar->m_pVehicle == this;
}

bool IVehicle::ShouldShowHealthbar()
{
	return m_ShowHealthbarUntil && m_ShowHealthbarUntil > Server()->Tick();
}

bool IVehicle::ShouldFlashDamagedHearts()
{
	return m_LastDamage + Server()->TickSpeed() / 2 > Server()->Tick() && // Within half a second
		(Server()->Tick() / 4) % 2 == 0; // flash rapidly
}

void IVehicle::SortBones()
{
	// meant for adding turret on top of helicopter, might add more stuff later
	CBone *aModel = m_pModel->Bones();
	int NumModel = m_pModel->NumBones();

	int NumTurret = 0;
	for (int i = 0; i < m_NumAttachments; i++)
		if (m_apAttachments[i])
			NumTurret += m_apAttachments[i]->NumBones();

	int NumTotal = NumModel + NumTurret;
	if (NumTotal > MAX_BONES_SORT)
	{
		dbg_msg("vehicle", "SortBones() exceeds the amount of bones allowed for sorting: %i/%i", NumTotal, MAX_BONES_SORT);
		return;
	}

	// Get current IDs
	static int aIDs[MAX_BONES_SORT];
	for (int i = 0; i < NumModel; i++)
		aIDs[i] = aModel[i].m_ID;

	int Passed = 0;
	for (int i = 0; i < m_NumAttachments; i++)
	{
		IVehicleTurret *pTurret = m_apAttachments[i];
		if (!pTurret)
			continue;

		for (int j = 0; j < pTurret->NumBones(); j++)
			aIDs[NumModel + Passed + j] = m_apAttachments[i]->Bones()[j].m_ID;
		Passed += pTurret->NumBones();
	}

	// Sort IDs
	std::sort(aIDs, aIDs + NumTotal);

	// Update sorted IDs (low -> high | ascending)
	for (int i = 0; i < NumModel; i++)
		aModel[i].m_ID = aIDs[i];

	Passed = 0;
	for (int i = 0; i < m_NumAttachments; i++)
	{
		IVehicleTurret *pTurret = m_apAttachments[i];
		if (!pTurret)
			continue;

		for (int j = 0; j < pTurret->NumBones(); j++)
			m_apAttachments[i]->Bones()[j].m_ID = aIDs[NumModel + Passed + j];
		Passed += pTurret->NumBones();
	}
}

void IVehicle::SnapBuildingParticles(int SnappingClient)
{
	if (!IsBuilding())
		return;

	// only show when constructing, switch is active
	CCollision::SSwitchers *pSwitcher = m_Number > 0 ? &GameServer()->Collision()->m_pSwitchers[m_Number] : 0;
	if (PlacedByTile() && (!pSwitcher || !pSwitcher->m_Status[m_DDTeam]))
		return;

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

void IVehicle::SnapHealthBar(int SnappingClient)
{
	if (IsBuilding())
		return;

	CCharacter *pChar = GameServer()->GetPlayerChar(SnappingClient);
	if ((!m_pModel->NumSeated() || IsPassenger(pChar)) && // Show when no passengers or if you're a passenger
		(ShouldShowHealthbar() || !IsFullHealthAndArmor()) && // Show until specified (on mount, damage, etc.) or when not max
		(!m_LastDamage || (m_LastDamage + Server()->TickSpeed() / 2 <= Server()->Tick()) || // Show if damaged not long ago
			ShouldFlashDamagedHearts())) // Show flashing damaged recently
		m_HealthBar.Snap(SnappingClient);
}

void IVehicle::SnapModel(int SnappingClient, const SBoneModelSnapping& Options)
{
	if (m_pModel)
		m_pModel->Snap(SnappingClient, Options);
}

void IVehicle::SnapAttachments(int SnappingClient, const SBoneModelSnapping& Options)
{
	for (int i = 0; i < m_NumAttachments; i++)
		if (m_apAttachments[i])
			m_apAttachments[i]->Snap(SnappingClient, Options);
}

void IVehicle::UpdateAttachmentControllerCounts()
{
	for (int i = 0; i < m_NumAttachments; i++)
		if (m_apAttachments[i])
			m_apAttachments[i]->CountControllers();
}
