// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_BUTTON_H
#define GAME_SERVER_ENTITIES_BUTTON_H

#include <game/server/entity.h>

class CButton : public CEntity
{
	enum
	{
		POINT_TOP,
		POINT_RIGHT,
		POINT_BOTTOM,
		POINT_LEFT,
		NUM_SIDES
	};

	struct
	{
		int m_ID;
		vec2 m_Pos;
	} m_aSides[NUM_SIDES];

public:
	CButton(CGameWorld *pGameWorld, vec2 Pos, int Number, bool Collision = true);
	virtual ~CButton();
	virtual void ResetCollision(bool Remove = false);
	virtual void Snap(int SnappingClient);
};

#endif // GAME_SERVER_ENTITIES_BUTTON_H
