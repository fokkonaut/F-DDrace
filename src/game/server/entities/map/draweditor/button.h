// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_MAP_DRAWEDITOR_BUTTON_H
#define GAME_SERVER_ENTITIES_MAP_DRAWEDITOR_BUTTON_H

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
	void ResetCollision(bool Remove = false) override;
	void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_ENTITIES_MAP_DRAWEDITOR_BUTTON_H
