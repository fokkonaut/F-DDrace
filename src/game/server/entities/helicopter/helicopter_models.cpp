//
// Created by Matq on 12/05/2025.
//

#include "../../gamecontext.h"
#include "helicopter_models.h"

// Default helicopter model

void CHelicopterModel::InitBody()
{
	CBone aBones[NUM_BONES_BODY] = {
		// Base
		CBone(Entity(), Server()->SnapNewID(), 70, 45, 50, 60, 4),
		CBone(Entity(), Server()->SnapNewID(), -60, 60, 50, 60, 4),
		CBone(Entity(), Server()->SnapNewID(), -25, 40, -30, 60, 3),
		CBone(Entity(), Server()->SnapNewID(), 25, 40, 30, 60, 3),
		CBone(Entity(), Server()->SnapNewID(), 0, -40, 0, -60, 4),
		// Top propeller rotor
		CBone(Entity(), Server()->SnapNewID(), 35, 40, -35, 40, 3),
		// Body
		CBone(Entity(), Server()->SnapNewID(), 60, 10, 35, 40, 4),
		CBone(Entity(), Server()->SnapNewID(), 25, -40, 60, 10, 4),
		CBone(Entity(), Server()->SnapNewID(), -35, -40, 25, -40, 4),
		CBone(Entity(), Server()->SnapNewID(), -45, 0, -35, -40, 4),
		// Tail
		CBone(Entity(), Server()->SnapNewID(), -100, 0, -45, 0, 4),
		CBone(Entity(), Server()->SnapNewID(), -120, -30, -100, 0, 4),
		CBone(Entity(), Server()->SnapNewID(), -105, 20, -120, -30, 4),
		CBone(Entity(), Server()->SnapNewID(), -35, 40, -105, 20, 4),
	};
	mem_copy(Body(), aBones, sizeof(CBone) * NUM_BONES_BODY);
}

void CHelicopterModel::InitPropellers()
{
	vec2 UPDATE_POS_LATER = vec2(0, 0);
	CBone aBlades[NUM_BONES_PROPELLERS] = {
		CBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, UPDATE_POS_LATER, 3, LASERTYPE_DOOR),
		CBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, UPDATE_POS_LATER, 3, LASERTYPE_DOOR),
		CBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, vec2(-110.0f, -10.f), 3, LASERTYPE_RIFLE),
		CBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, vec2(-110.0f, -10.f), 3, LASERTYPE_RIFLE),
	};
	memcpy(Blades(), aBlades, sizeof(CBone) * NUM_BONES_PROPELLERS);

	// Link

	Propellers()[0] = SPropeller(PROPELLER_HORIZONTAL, &Blades()[0], &Blades()[1], vec2(0, -60), 100.f, 0.23f);
	Propellers()[1] = SPropeller(PROPELLER_CIRCULAR, &Blades()[2], &Blades()[3], vec2(-110, -10), 30.f, 0.2f);
	Trails()[0] = CTrailNode(Entity(), Server()->SnapNewID(), &Blades()[2].m_From);
	Trails()[1] = CTrailNode(Entity(), Server()->SnapNewID(), &Blades()[3].m_From);
}

void CHelicopterModel::InitSeats()
{
	SSeat aSeats[NUM_SEATS] = { SSeat(vec2(0, 0)) };
	memcpy(Seats(), aSeats, sizeof(SSeat) * NUM_SEATS);
}

void CHelicopterModel::InitModel()
{
	InitBody();
	InitPropellers();
	InitSeats();

	SpinPropellers(); //
	UpdateLastPropellerPositions();
}

CHelicopterModel::CHelicopterModel(CEntity *pEntity)
	: IHelicopterModel(pEntity, NUM_BONES, NUM_TRAILS, NUM_PROPELLERS, NUM_SEATS)
{
}

// void CHelicopterModel::ApplyScale(float Scale)
// {
// 	ApplyScaleBones(Scale);
// 	ApplyScalePropellers(Scale);
// }

// Attack helicopter model

CHelicopterApacheModel::CHelicopterApacheModel(CEntity *pEntity)
	: IHelicopterModel(pEntity, NUM_BONES, NUM_TRAILS, NUM_PROPELLERS, NUM_SEATS)
{
}

void CHelicopterApacheModel::InitBody()
{
	CBone aBones[NUM_BONES_BODY] = {
		// Base
		CBone(Entity(), Server()->SnapNewID(), -165, 60, -135, 60, 4), // left tire
		CBone(Entity(), Server()->SnapNewID(), 15, 60, 45, 60, 4), // right tire
		CBone(Entity(), Server()->SnapNewID(), -145, 35, -150, 60, 3), // left connector
		CBone(Entity(), Server()->SnapNewID(), 25, 40, 30, 60, 3), // right connector
		// Top propeller rotor
		CBone(Entity(), Server()->SnapNewID(), 0, -40, 0, -60, 4), // top propeller rotor
		// Tail
		CBone(Entity(), Server()->SnapNewID(), -180, 10, -215, -35, 4), // tail mini segment
		CBone(Entity(), Server()->SnapNewID(), -45, 0, -180, 10, 4), // top tail connection
		// Body
		CBone(Entity(), Server()->SnapNewID(), -40, -35, -45, -0, 4), // back window
		CBone(Entity(), Server()->SnapNewID(), 15, -40, -40, -35, 4), // roof
		CBone(Entity(), Server()->SnapNewID(), 55, -25, 15, -40, 4), // windshield top
		CBone(Entity(), Server()->SnapNewID(), 105, 20, 55, -25, 1), // windshield bottom
		CBone(Entity(), Server()->SnapNewID(), 95, 40, 105, 20, 4), // nose
		CBone(Entity(), Server()->SnapNewID(), -195, 30, 95, 40, 3), // floor
		// Tail
		CBone(Entity(), Server()->SnapNewID(), -215, -35, -195, 30, 4), // tail longer segment
	};
	mem_copy(Body(), aBones, sizeof(CBone) * NUM_BONES_BODY);

	for (int i = 0; i < NUM_BONES_BODY; i++)
		Body()[i].m_InitColor = LASERTYPE_FREEZE;
	Body()[10].m_InitColor = LASERTYPE_RIFLE; // windshield bottom
}

void CHelicopterApacheModel::InitPropellers()
{
	vec2 UPDATE_POS_LATER = vec2(0, 0);
	CBone aBlades[NUM_BONES_PROPELLERS] = {
		CBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, UPDATE_POS_LATER, 3, LASERTYPE_DOOR),
		CBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, UPDATE_POS_LATER, 3, LASERTYPE_DOOR),
		CBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, vec2(-200.0f, -10.f), 3, LASERTYPE_FREEZE),
		CBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, vec2(-200.0f, -10.f), 3, LASERTYPE_FREEZE),
	};
	memcpy(Blades(), aBlades, sizeof(CBone) * NUM_BONES_PROPELLERS);

	// Link

	Propellers()[0] = SPropeller(PROPELLER_HORIZONTAL, &Blades()[0], &Blades()[1], vec2(0, -60), 170.f, 0.23f);
	Propellers()[1] = SPropeller(PROPELLER_CIRCULAR, &Blades()[2], &Blades()[3], vec2(-200, -10), 40.f, 0.3f);
	Trails()[0] = CTrailNode(Entity(), Server()->SnapNewID(), &Blades()[2].m_From);
	Trails()[1] = CTrailNode(Entity(), Server()->SnapNewID(), &Blades()[3].m_From);
}

void CHelicopterApacheModel::InitSeats()
{
	SSeat aSeats[NUM_SEATS] = {
		SSeat(vec2(0, 0)),
		SSeat(vec2(50, 10))
	};
	memcpy(Seats(), aSeats, sizeof(SSeat) * NUM_SEATS);
}

void CHelicopterApacheModel::InitModel()
{
	InitBody();
	InitPropellers();
	InitSeats();

	SpinPropellers(); //
	UpdateLastPropellerPositions();
}

// Transport helicopter model

CHelicopterChinookModel::CHelicopterChinookModel(CEntity *pEntity)
	: IHelicopterModel(pEntity, NUM_BONES, NUM_TRAILS, NUM_PROPELLERS, NUM_SEATS)
{
}

void CHelicopterChinookModel::InitBody()
{
	CBone aBones[NUM_BONES_BODY] = {
		// Base
		CBone(Entity(), Server()->SnapNewID(), -125, 60, -95, 60, 4), // left tire
		CBone(Entity(), Server()->SnapNewID(), 45, 60, 75, 60, 4), // right tire
		CBone(Entity(), Server()->SnapNewID(), -100, 35, -110, 60, 3), // left connector
		CBone(Entity(), Server()->SnapNewID(), 50, 30, 60, 60, 3), // right connector
		// Body
		CBone(Entity(), Server()->SnapNewID(), -110, -55, -180, -45, 4), // left roof
		CBone(Entity(), Server()->SnapNewID(), -90, -30, -110, -55, 4), // left slope
		CBone(Entity(), Server()->SnapNewID(), 45, -40, -90, -30, 4), // roof
		CBone(Entity(), Server()->SnapNewID(), 60, -55, 45, -40, 4), // right slope
		CBone(Entity(), Server()->SnapNewID(), 120, -60, 60, -55, 4), // right roof
		CBone(Entity(), Server()->SnapNewID(), 135, -15, 120, -60, 4), // windshield
		CBone(Entity(), Server()->SnapNewID(), 160, 0, 135, -15, 1), // nose top
		CBone(Entity(), Server()->SnapNewID(), 150, 20, 160, 0, 4), // nose bottom
		CBone(Entity(), Server()->SnapNewID(), -110, 40, 150, 20, 3), // floor
		CBone(Entity(), Server()->SnapNewID(), -180, 10, -110, 40, 4), // tail bottom
		CBone(Entity(), Server()->SnapNewID(), -180, -45, -180, 10, 4), // tail
	};
	mem_copy(Body(), aBones, sizeof(CBone) * NUM_BONES_BODY);

	for (int i = 0; i < NUM_BONES_BODY; i++)
		Body()[i].m_InitColor = LASERTYPE_FREEZE;
	Body()[9].m_InitColor = LASERTYPE_RIFLE; // windshield bottom
}

void CHelicopterChinookModel::InitPropellers()
{
	vec2 UPDATE_POS_LATER = vec2(0, 0);
	CBone aBlades[NUM_BONES_PROPELLERS] = {
		CBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, UPDATE_POS_LATER, 3, LASERTYPE_DOOR),
		CBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, UPDATE_POS_LATER, 3, LASERTYPE_DOOR),
		CBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, UPDATE_POS_LATER, 3, LASERTYPE_DOOR),
		CBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, UPDATE_POS_LATER, 3, LASERTYPE_DOOR),
	};
	memcpy(Blades(), aBlades, sizeof(CBone) * NUM_BONES_PROPELLERS);

	// Link

	Propellers()[0] = SPropeller(PROPELLER_HORIZONTAL, &Blades()[0], &Blades()[1], vec2(90, -65), 120.f, 0.1f);
	Propellers()[1] = SPropeller(PROPELLER_HORIZONTAL, &Blades()[2], &Blades()[3], vec2(-145, -60), 120.f, 0.11f);
}

void CHelicopterChinookModel::InitSeats()
{
	SSeat aSeats[NUM_SEATS] = {
		SSeat(vec2(90, -10)),
		SSeat(vec2(30, -5)),
		SSeat(vec2(-30, 5)),
		SSeat(vec2(-90, 10)),
	};
	memcpy(Seats(), aSeats, sizeof(SSeat) * NUM_SEATS);
}

void CHelicopterChinookModel::InitModel()
{
	InitBody();
	InitPropellers();
	InitSeats();

	SpinPropellers(); //
	UpdateLastPropellerPositions();
}
