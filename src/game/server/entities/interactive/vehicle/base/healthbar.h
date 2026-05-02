//
// Created by Matq on 18/04/2026.
//

#ifndef GAME_SERVER_ENTITIES_INTERACTIVE_VEHICLE_BASE_HEALTHBAR_H
#define GAME_SERVER_ENTITIES_INTERACTIVE_VEHICLE_BASE_HEALTHBAR_H

#include <game/server/entities/misc/bone/base/bone.h>

class IVehicle;
class CHealthBar
{
private:
	IVehicle *m_pVehicle;
	bool m_VerticalLayout;

	CPickupNode *m_apHeartsIndicator;
	int m_NumHeartsIndicator;
	CPickupNode *m_apArmorIndicator;
	int m_NumArmorIndicator;

	void SetNumIndicator(CPickupNode *& apPickups, int& NumPickups, int NewNumPickups, int PowerupType);

public:
	CHealthBar(IVehicle *pVehicle, bool VerticalLayout = false);

	// Manipulating
	void SetNumHeartsIndicator(int NumHearts);
	void SetNumArmorIndicator(int NumArmor);
	void SetIndicator(int NumHearts, int NumArmor);
	void SetVertical(bool Vertical) { m_VerticalLayout = Vertical; }

	// Ticking
	void UpdateIndicator();
	void Snap(int SnappingClient);
};

#endif // GAME_SERVER_ENTITIES_INTERACTIVE_VEHICLE_BASE_HEALTHBAR_H
