//
// Created by Matq on 02/04/2026.
//

#pragma once

#include "base/vehicle.h"
#include "game/server/entities/advanced_entity.h"
#include "game/server/entities/bone/spider_model.h"

#define SPIDER_MIN_SCALE 0.5f
#define SPIDER_MAX_SCALE 5.0f
#define SPIDER_DEFAULT_SCALE 0.64f
#define SPIDER_PHYSSIZE vec2(28.0f, 28.0f)

class CSpider : public IVehicle
{
private:
	CLegIK m_aLegs[8];
	int m_NumLegs;
	float m_PhaseTimer;
	float m_PhaseCompensation;
	vec2 m_TravelDirection;

	bool m_Attacking; //
	vec2 m_Attack;

	void ApplyAcceleration() override;
	void HandleRotation();
	void HandleSeat(SSeat& Seat, int PassengerCID, CCharacter *pChar) override;
	void DriversDismounted() override;
	void DriversFrozen() override;
	void SetRotation(float NewRotation) override;

	// Ticking
	void TickLegs();

public:
	CSpider(
		CGameWorld *pGameWorld,
		int Spawner,
		int Team,
		vec2 Pos,
		float Scale = 1.f,
		int BuildTime = 0,
		// 0 for no build animation
		int Number = -1
	);
	~CSpider();

	// Manipulating
	void ApplyScale(float VehicleScale) override;

	// Ticking & Events
	void Tick() override;
	// void Snap(int SnappingClient) override;
	void Reset() override;

	bool OnInput(CNetObj_PlayerInput *pNewInput, CCharacter *pController);
};
