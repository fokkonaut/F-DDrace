//
// Created by Matq on 12/05/2025.
//

#include "../../gamecontext.h"
#include "helicopter_models.h"

void SHelicopterModel::InitBody()
{
	SBone aBones[NUM_BONES_BODY] = {
		// Base
		SBone(Entity(), Server()->SnapNewID(), 70, 45, 50, 60, 4), // toe
		SBone(Entity(), Server()->SnapNewID(), -55, 60, 50, 60, 4), // base
		SBone(Entity(), Server()->SnapNewID(), -25, 40, -30, 60, 3),  // left connector
		SBone(Entity(), Server()->SnapNewID(), 25, 40, 30, 60, 3), // right connector
		SBone(Entity(), Server()->SnapNewID(), 0, -40, 0, -60, 4), // wtf is this
		// Top propeller rotor
		SBone(Entity(), Server()->SnapNewID(), 35, 40, -35, 40, 3),
		// Body
		SBone(Entity(), Server()->SnapNewID(), 60, 10, 35, 40, 4), // windshield?
		SBone(Entity(), Server()->SnapNewID(), 25, -40, 60, 10, 4), // below windshield
		SBone(Entity(), Server()->SnapNewID(), -35, -40, 25, -40, 4), // floor
		SBone(Entity(), Server()->SnapNewID(), -45, 0, -35, -40, 4), // back window?
		// Tail
		SBone(Entity(), Server()->SnapNewID(), -100, 0, -45, 0, 4), // top tail connection
		SBone(Entity(), Server()->SnapNewID(), -120, -30, -100, 0, 4), // tail mini segment
		SBone(Entity(), Server()->SnapNewID(), -105, 20, -120, -30, 4), // tail longer segment
		SBone(Entity(), Server()->SnapNewID(), -35, 40, -105, 20, 4), // bottom tail connection
	};
	mem_copy(Body(), aBones, sizeof(SBone) * NUM_BONES_BODY);
}

void SHelicopterModel::InitPropellers()
{
	for (int i = 0; i < m_NumPropellers; i++)
	{
		float Radius = m_aPropellers[i].m_Radius;
		TopPropeller()[i] = SBone(Entity(), Server()->SnapNewID(), vec2(Radius, -60.f), vec2(0, -60.f), 3);
		TopPropeller()[i + 1] = SBone(Entity(), Server()->SnapNewID(), vec2(-Radius, -60.f), vec2(0, -60.f), 3);
		TopPropeller()[i].m_InitColor = LASERTYPE_DOOR;
		TopPropeller()[i + 1].m_InitColor = LASERTYPE_DOOR;
		TopPropeller()[i].m_Color = LASERTYPE_DOOR;
		TopPropeller()[i + 1].m_Color = LASERTYPE_DOOR;

		m_aPropellers[i].m_pBoneA = &TopPropeller()[i];
		m_aPropellers[i].m_pBoneB = &TopPropeller()[i + 1];
	}

	float Radius = m_BackPropellerRadius;
	for (int i = 0; i < NUM_BONES_PROPELLERS_BACK; i++)
	{
		BackPropeller()[i] = SBone(Entity(), Server()->SnapNewID(), vec2(-110.f + Radius, -10.f), vec2(-110.f, -10.f), 3);
		Radius *= -1;
		m_aTrails[i] = STrail(Entity(), Server()->SnapNewID(), &BackPropeller()[i].m_From);
	}
}

void SHelicopterModel::InitModel()
{
	// Run inside constructor
	InitBody();
	// Propellers are initialized later ... PostConstructor() -> InitLinkPropellers() -> InitPropellers()
}

void SHelicopterModel::InitLinkPropellers()
{
	Propellers()[0] = SPropeller(nullptr, nullptr, 100.f);
	InitPropellers();
}

SHelicopterModel::SHelicopterModel(CEntity *pEntity)
	: IHelicopterModel(pEntity, NUM_BONES, NUM_TRAILS, 1)
{
	m_BackPropellerRadius = 25.f;
}

void SHelicopterModel::ApplyScale(float Scale)
{
	ApplyScaleBones(Scale);
	ApplyScalePropellers(Scale);
	m_BackPropellerRadius *= Scale;
}

void SHelicopterModel::SpinPropellers()
{
	// Advance propeller animation
	for (int i = 0; i < NUM_BONES_PROPELLERS_BACK; i++)
		BackPropeller()[i].m_From = rotate_around_point(BackPropeller()[i].m_From, BackPropeller()[i].m_To, 50.f / pi);

	for (int j = 0; j < m_NumPropellers; j++)
	{
		float Len = m_aPropellers[j].m_Radius;

		float curLen = clamp((float)sin(Server()->Tick() / 5) * Len, -Len, Len);
		vec2 Diff = TopPropeller()[0].m_From - TopPropeller()[0].m_To;
		Diff = normalize(Diff) * curLen;
		for (int i = 0; i < NUM_BONES_PROPELLERS_TOP; i++)
		{
			TopPropeller()[i].m_From = TopPropeller()[i].m_To + Diff;
			Diff *= -1;
		}
	}
}

SHelicopterApacheModel::SHelicopterApacheModel(CEntity *pEntity)
	: IHelicopterModel(pEntity, NUM_BONES, NUM_TRAILS, 1)
{
	m_BackPropellerRadius = 40.0f;
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
		// Body
		SBone(Entity(), Server()->SnapNewID(), 95, 40, -195, 30, 3), // floor
		SBone(Entity(), Server()->SnapNewID(), 105, 20, 95, 40, 4), // nose
		SBone(Entity(), Server()->SnapNewID(), 55, -25, 105, 20, 4), // windshield bottom
		SBone(Entity(), Server()->SnapNewID(), 15, -40, 55, -25, 4), // windshield top
		SBone(Entity(), Server()->SnapNewID(), -50, -35, 15, -40, 4), // roof
		SBone(Entity(), Server()->SnapNewID(), -55, -0, -50, -35, 4), // back window
		// Tail
		SBone(Entity(), Server()->SnapNewID(), -180, 10, -55, 0, 4), // top tail connection
		SBone(Entity(), Server()->SnapNewID(), -215, -35, -180, 10, 4), // tail mini segment
		SBone(Entity(), Server()->SnapNewID(), -195, 30, -215, -35, 4), // tail longer segment
		// SBone(Entity(), Server()->SnapNewID(), -35, 40, -195, 40, 4), // bottom tail connection
	};
	mem_copy(Body(), aBones, sizeof(SBone) * NUM_BONES_BODY);

	for (int i = 0; i < NUM_BONES_BODY; i++)
		Body()[i].m_InitColor = LASERTYPE_FREEZE;
}

void SHelicopterApacheModel::InitPropellers()
{
	for (int i = 0; i < m_NumPropellers; i++)
	{
		float Radius = m_aPropellers[i].m_Radius;
		TopPropeller()[i] = SBone(Entity(), Server()->SnapNewID(), vec2(Radius, -60.f), vec2(0, -60.f), 3);
		TopPropeller()[i + 1] = SBone(Entity(), Server()->SnapNewID(), vec2(-Radius, -60.f), vec2(0, -60.f), 3);
		TopPropeller()[i].m_InitColor = LASERTYPE_DOOR;
		TopPropeller()[i + 1].m_InitColor = LASERTYPE_DOOR;
		// TopPropeller()[i].m_Color = LASERTYPE_DOOR;
		// TopPropeller()[i + 1].m_Color = LASERTYPE_DOOR;

		m_aPropellers[i].m_pBoneA = &TopPropeller()[i];
		m_aPropellers[i].m_pBoneB = &TopPropeller()[i + 1];
	}

	float Radius = m_BackPropellerRadius;
	for (int i = 0; i < NUM_BONES_PROPELLERS_BACK; i++)
	{
		BackPropeller()[i] = SBone(Entity(), Server()->SnapNewID(), vec2(-200.f + Radius, -10.f), vec2(-200.f, -10.f), 3);
		BackPropeller()[i].m_InitColor = LASERTYPE_FREEZE;
		Radius *= -1;
		m_aTrails[i] = STrail(Entity(), Server()->SnapNewID(), &BackPropeller()[i].m_From);
	}
}

void SHelicopterApacheModel::InitModel()
{
	InitBody();
}

void SHelicopterApacheModel::InitLinkPropellers()
{
	Propellers()[0] = SPropeller(nullptr, nullptr, 170.f);
	InitPropellers();
}

void SHelicopterApacheModel::ApplyScale(float Scale)
{
	ApplyScaleBones(Scale);
	ApplyScalePropellers(Scale);
	m_BackPropellerRadius *= Scale;
}

void SHelicopterApacheModel::SpinPropellers()
{
	// Advance propeller animation
	for (int i = 0; i < NUM_BONES_PROPELLERS_BACK; i++)
		BackPropeller()[i].m_From = rotate_around_point(BackPropeller()[i].m_From, BackPropeller()[i].m_To, 50.f / pi);

	for (int j = 0; j < m_NumPropellers; j++)
	{
		float Len = m_aPropellers[j].m_Radius;

		float curLen = clamp((float)sin(Server()->Tick() / 5) * Len, -Len, Len);
		vec2 Diff = TopPropeller()[0].m_From - TopPropeller()[0].m_To;
		Diff = normalize(Diff) * curLen;
		for (int i = 0; i < NUM_BONES_PROPELLERS_TOP; i++)
		{
			TopPropeller()[i].m_From = TopPropeller()[i].m_To + Diff;
			Diff *= -1;
		}
	}
}
