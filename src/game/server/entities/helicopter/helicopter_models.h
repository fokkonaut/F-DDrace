//
// Created by Matq on 12/05/2025.
//

#ifndef GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_MODELS_H
#define GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_MODELS_H

#include "bone.h"

class IHelicopterModel : public SBoneModel
{
protected:
	float m_BackPropellerRadius;

	virtual void InitBody() = 0;
	virtual void InitPropellers() = 0;
	virtual void InitModel() = 0;
	virtual void InitLinkPropellers() = 0;

public:
	IHelicopterModel(CEntity *pEntity, int num_bones, int num_trails, int num_top_propellers)
		: SBoneModel(pEntity, num_bones, num_trails, num_top_propellers)
	{
		m_BackPropellerRadius = 25.0f; // overwrite
	};

	virtual void ApplyScale(float Scale) = 0;
	virtual void SpinPropellers() = 0;
};

class SHelicopterModel : public IHelicopterModel
{
private:
	enum
	{
		NUM_BONES_BODY = 14,
		NUM_BONES_PROPELLERS_TOP = 2,
		NUM_BONES_PROPELLERS_BACK = 2,
		NUM_BONES_PROPELLERS = NUM_BONES_PROPELLERS_TOP + NUM_BONES_PROPELLERS_BACK,
		NUM_BONES = NUM_BONES_BODY + NUM_BONES_PROPELLERS,
		NUM_TRAILS = 2,
	};

	float m_BackPropellerRadius;

	void InitBody() override;
	void InitPropellers() override;
	void InitModel() override;
	void InitLinkPropellers() override; // Thop

	SBone *Body() { return &m_aBones[0]; } // size: NUM_BONES_BODY
	SBone *TopPropeller() { return &m_aBones[NUM_BONES_BODY]; } // size: NUM_BONES_PROPELLERS_TOP
	SBone *BackPropeller() { return &m_aBones[NUM_BONES_BODY + NUM_BONES_PROPELLERS_TOP]; } // size: NUM_BONES_PROPELLERS_BACK

public:
	SHelicopterModel(CEntity *pEntity);

	// Getting
	//	float GetTopPropellerRadius() { return m_TopPropellerRadius; }

	// Manipulating
	void ApplyScale(float Scale) override;

	// Ticking & Events
	void SpinPropellers() override;
};

class SHelicopterApacheModel : public IHelicopterModel
{
private:
	enum
	{
		NUM_BONES_BODY = 14,
		NUM_BONES_PROPELLERS_TOP = 2,
		NUM_BONES_PROPELLERS_BACK = 2,
		NUM_BONES_PROPELLERS = NUM_BONES_PROPELLERS_TOP + NUM_BONES_PROPELLERS_BACK,
		NUM_BONES = NUM_BONES_BODY + NUM_BONES_PROPELLERS,
		NUM_TRAILS = 2,
	};

	void InitBody() override;
	void InitPropellers() override;
	void InitModel() override;
	void InitLinkPropellers() override; // Thop

	SBone *Body() { return &m_aBones[0]; } // size: NUM_BONES_BODY
	SBone *TopPropeller() { return &m_aBones[NUM_BONES_BODY]; } // size: NUM_BONES_PROPELLERS_TOP
	SBone *BackPropeller() { return &m_aBones[NUM_BONES_BODY + NUM_BONES_PROPELLERS_TOP]; } // size: NUM_BONES_PROPELLERS_BACK

public:
	SHelicopterApacheModel(CEntity *pEntity);

	// Manipulating
	void ApplyScale(float Scale) override;

	// Ticking & Events
	void SpinPropellers() override;
};

#endif // GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_MODELS_H
