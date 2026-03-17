//
// Created by Matq on 12/05/2025.
//

#include "../../gamecontext.h"
#include "helicopter_models.h"

void SHelicopterModel::InitBody()
{
	SBone aBones[NUM_BONES_BODY] = {
		// Base
		SBone(Entity(), Server()->SnapNewID(), 70, 45, 50, 60, 4),
		SBone(Entity(), Server()->SnapNewID(), -60, 60, 50, 60, 4),
		SBone(Entity(), Server()->SnapNewID(), -25, 40, -30, 60, 3),
		SBone(Entity(), Server()->SnapNewID(), 25, 40, 30, 60, 3),
		SBone(Entity(), Server()->SnapNewID(), 0, -40, 0, -60, 4),
		// Top propeller rotor
		SBone(Entity(), Server()->SnapNewID(), 35, 40, -35, 40, 3),
		// Body
		SBone(Entity(), Server()->SnapNewID(), 60, 10, 35, 40, 4),
		SBone(Entity(), Server()->SnapNewID(), 25, -40, 60, 10, 4),
		SBone(Entity(), Server()->SnapNewID(), -35, -40, 25, -40, 4),
		SBone(Entity(), Server()->SnapNewID(), -45, 0, -35, -40, 4),
		// Tail
		SBone(Entity(), Server()->SnapNewID(), -100, 0, -45, 0, 4),
		SBone(Entity(), Server()->SnapNewID(), -120, -30, -100, 0, 4),
		SBone(Entity(), Server()->SnapNewID(), -105, 20, -120, -30, 4),
		SBone(Entity(), Server()->SnapNewID(), -35, 40, -105, 20, 4),
	};
	mem_copy(Body(), aBones, sizeof(SBone) * NUM_BONES_BODY);
}

void SHelicopterModel::InitModel()
{
	InitBody();
	InitPropellers();
	SpinPropellers(); //
}

void SHelicopterModel::InitPropellers()
{
	vec2 UPDATE_POS_LATER = vec2(0, 0);
	SBone aBlades[NUM_BONES_PROPELLERS] = {
		SBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, UPDATE_POS_LATER, 3, LASERTYPE_DOOR),
		SBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, UPDATE_POS_LATER, 3, LASERTYPE_DOOR),
		SBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, vec2(-110.0f, -10.f), 3, LASERTYPE_RIFLE),
		SBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, vec2(-110.0f, -10.f), 3, LASERTYPE_RIFLE),
	};
	memcpy(Blades(), aBlades, sizeof(SBone) * NUM_BONES_PROPELLERS);

	// Link

	Propellers()[0] = SPropeller(PROPELLER_HORIZONTAL, &Blades()[0], &Blades()[1], vec2(0, -60), 100.f, 0.57f);
	Propellers()[1] = SPropeller(PROPELLER_CIRCULAR, &Blades()[2], &Blades()[3], vec2(-110, -10), 30.f, 0.2f);
	Trails()[0] = STrail(Entity(), Server()->SnapNewID(), &Blades()[2].m_From);
	Trails()[1] = STrail(Entity(), Server()->SnapNewID(), &Blades()[3].m_From);
}

SHelicopterModel::SHelicopterModel(CEntity *pEntity)
	: IHelicopterModel(pEntity, NUM_BONES, NUM_TRAILS, NUM_PROPELLERS)
{

}

void SHelicopterModel::ApplyScale(float Scale)
{
	ApplyScaleBones(Scale);
	ApplyScalePropellers(Scale);
}

SHelicopterApacheModel::SHelicopterApacheModel(CEntity *pEntity)
	: IHelicopterModel(pEntity, NUM_BONES, NUM_TRAILS, NUM_PROPELLERS)
{

}

void SHelicopterApacheModel::InitBody()
{
	SBone aBones[NUM_BONES_BODY] = {
		// Base
		SBone(Entity(), Server()->SnapNewID(), -165, 60, -135, 60, 4), // left tire
		SBone(Entity(), Server()->SnapNewID(), 15, 60, 45, 60, 4), // right tire
		SBone(Entity(), Server()->SnapNewID(), -145, 35, -150, 60, 3), // left connector
		SBone(Entity(), Server()->SnapNewID(), 25, 40, 30, 60, 3), // right connector
		// Top propeller rotor
		SBone(Entity(), Server()->SnapNewID(), 0, -40, 0, -60, 4), // top propeller rotor
		// Tail
		SBone(Entity(), Server()->SnapNewID(), -180, 10, -215, -35, 4), // tail mini segment
		SBone(Entity(), Server()->SnapNewID(), -45, 0, -180, 10, 4), // top tail connection
		// Body
		SBone(Entity(), Server()->SnapNewID(), -40, -35, -45, -0, 4), // back window
		SBone(Entity(), Server()->SnapNewID(), 15, -40, -40, -35, 4), // roof
		SBone(Entity(), Server()->SnapNewID(), 55, -25, 15, -40, 4), // windshield top
		SBone(Entity(), Server()->SnapNewID(), 105, 20, 55, -25, 1), // windshield bottom
		SBone(Entity(), Server()->SnapNewID(), 95, 40, 105, 20, 4), // nose
		SBone(Entity(), Server()->SnapNewID(), -195, 30, 95, 40, 3), // floor
		// Tail
		SBone(Entity(), Server()->SnapNewID(), -215, -35, -195, 30, 4), // tail longer segment
	};
	mem_copy(Body(), aBones, sizeof(SBone) * NUM_BONES_BODY);

	for (int i = 0; i < NUM_BONES_BODY; i++)
		Body()[i].m_InitColor = LASERTYPE_FREEZE;
	Body()[10].m_InitColor = LASERTYPE_RIFLE; // windshield bottom
}

void SHelicopterApacheModel::InitModel()
{
	InitBody();
	InitPropellers();
	SpinPropellers(); //
}

void SHelicopterApacheModel::InitPropellers()
{
	vec2 UPDATE_POS_LATER = vec2(0, 0);
	SBone aBlades[NUM_BONES_PROPELLERS] = {
		SBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, UPDATE_POS_LATER, 3, LASERTYPE_DOOR),
		SBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, UPDATE_POS_LATER, 3, LASERTYPE_DOOR),
		SBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, vec2(-200.0f, -10.f), 3, LASERTYPE_FREEZE),
		SBone(Entity(), Server()->SnapNewID(), UPDATE_POS_LATER, vec2(-200.0f, -10.f), 3, LASERTYPE_FREEZE),
	};
	memcpy(Blades(), aBlades, sizeof(SBone) * NUM_BONES_PROPELLERS);

	// Link

	Propellers()[0] = SPropeller(PROPELLER_HORIZONTAL, &Blades()[0], &Blades()[1], vec2(0, -60), 170.f, 0.57f);
	Propellers()[1] = SPropeller(PROPELLER_CIRCULAR, &Blades()[2], &Blades()[3], vec2(-200, -10), 40.f, 0.3f);
	Trails()[0] = STrail(Entity(), Server()->SnapNewID(), &Blades()[2].m_From);
	Trails()[1] = STrail(Entity(), Server()->SnapNewID(), &Blades()[3].m_From);
}

void SHelicopterApacheModel::ApplyScale(float Scale)
{
	ApplyScaleBones(Scale);
	ApplyScalePropellers(Scale);
}