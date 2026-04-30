//
// Created by Matq on 12/05/2025.
//

#ifndef GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_MODELS_H
#define GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_MODELS_H

#include "vehicle_model.h"
#include <game/server/entities/misc/bone/base/bone_model.h>

// Default helicopter model

class CHelicopterModel : public IVehicleModel
{
private:
	enum
	{
		NUM_TRAILS = 2,
		NUM_PROPELLERS = 2,
		NUM_SEATS = 1,
		NUM_ROPES = 0,

		NUM_BONES_BODY = 14,
		NUM_BONES_PROPELLERS = NUM_PROPELLERS * 2,
		NUM_BONES = NUM_BONES_BODY + NUM_BONES_PROPELLERS,
	};

	void InitBody() override;
	void InitPropellers() override;
	void InitSeats() override;
	void InitRopes() override;

	void InitModel() override;

	CBone *Body() { return &m_aBones[0]; } // size: NUM_BONES_BODY
	CBone *Blades() { return &m_aBones[NUM_BONES_BODY]; } // size: NUM_BONES_PROPELLERS

public:
	CHelicopterModel(CEntity *pEntity);
};

// Attack helicopter model

class CHelicopterApacheModel : public IVehicleModel
{
private:
	enum
	{
		NUM_TRAILS = 2,
		NUM_PROPELLERS = 2,
		NUM_SEATS = 2,
		NUM_ROPES = 0,

		NUM_BONES_BODY = 14,
		NUM_BONES_PROPELLERS = NUM_PROPELLERS * 2,
		NUM_BONES = NUM_BONES_BODY + NUM_BONES_PROPELLERS,
	};

	void InitBody() override;
	void InitPropellers() override;
	void InitSeats() override;
	void InitRopes() override
	{
	}

	void InitModel() override;

	CBone *Body() { return &m_aBones[0]; } // size: NUM_BONES_BODY
	CBone *Blades() { return &m_aBones[NUM_BONES_BODY]; } // size: NUM_BONES_PROPELLERS

public:
	CHelicopterApacheModel(CEntity *pEntity);
};

// Transport helicopter model

class CHelicopterChinookModel : public IVehicleModel
{
private:
	enum
	{
		NUM_PROPELLERS = 2,
		NUM_TRAILS = 0,
		NUM_SEATS = 4,
		NUM_ROPES = 0,

		NUM_BONES_BODY = 15,
		NUM_BONES_PROPELLERS = NUM_PROPELLERS * 2,
		NUM_BONES = NUM_BONES_BODY + NUM_BONES_PROPELLERS,
	};

	void InitBody() override;
	void InitPropellers() override;
	void InitSeats() override;
	void InitRopes() override
	{
	}

	void InitModel() override;

	CBone *Body() { return &m_aBones[0]; } // size: NUM_BONES_BODY
	CBone *Blades() { return &m_aBones[NUM_BONES_BODY]; } // size: NUM_BONES_PROPELLERS

public:
	CHelicopterChinookModel(CEntity *pEntity);
};

#endif // GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_MODELS_H
