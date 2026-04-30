// made by fokkonaut and Matq, somewhere around 2021 and in 2025

#ifndef GAME_SERVER_ENTITIES_VEHICLE_HELICOPTER_H
#define GAME_SERVER_ENTITIES_VEHICLE_HELICOPTER_H

#include "base/vehicle.h"

#define HELICOPTER_DEFAULT_SCALE 0.64f
#define HELICOPTER_MIN_SCALE 1.0f
#define HELICOPTER_MAX_SCALE 5.0f
#define HELICOPTER_PHYSSIZE vec2(80, 128)

enum
{
	HELICOPTER_DEFAULT,
	HELICOPTER_APACHE,
	HELICOPTER_CHINOOK,
	NUM_HELICOPTER_TYPES,
};

namespace Helicopters
{
extern SVehicleMeta aHelicopterMetadata[NUM_HELICOPTER_TYPES];
}

class CHelicopter : public IVehicle
{
private:
	enum
	{
		// Snap IDs
		MAX_HEARTS = 4,
		MAX_ARMOR = 4,
		NUM_BUILD_IDS = 3, // sparkles/particles

		// Other
		MAX_BONES_SORT = 128,
	};

	int m_HelicopterType;
	bool m_Strafing;

	void ResetAndTurnOff();

	void HandleRotationBasedOnVelocity();
	void ApplyAcceleration() override;
	void HandleFlipping();
	void HandleSeat(SSeat& Seat, int PassengerCID, CCharacter *pChar) override;
	void DriversDismounted() override;
	void DriversFrozen() override;

	// Tile respawn
	bool TryRespawnNewVehicle() override;

public:
	CHelicopter(
		CGameWorld *pGameWorld,
		int HelicopterType,
		int Spawner,
		int Team,
		vec2 Pos,
		float Scale = 1.f,
		int BuildTime = 0,
		// 0 for no build animation
		int Number = -1,
		int DelayTurretType = -1
	);
	virtual ~CHelicopter();

	// Getting
	int GetHelicopterType() { return m_HelicopterType; }

	// Ticking & Events
	void Tick() override;
	void Snap(int SnappingClient) override;
	void Reset() override;

	bool OnInput(CNetObj_PlayerInput *pNewInput, CCharacter *pControllerChar) override;
};

#endif // GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_H
