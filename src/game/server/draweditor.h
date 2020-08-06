// made by fokkonaut

#ifndef GAME_SERVER_DRAWEDITOR_H
#define GAME_SERVER_DRAWEDITOR_H

#include <generated/protocol.h>
#include "entity.h"

class CCharacter;

// do not re-sort modes, order is important for SetDrawMode()
enum DrawMode
{
	DRAW_UNINITIALIZED = -1,
	DRAW_HEART,
	DRAW_SHIELD,
	DRAW_HAMMER,
	DRAW_GUN,
	DRAW_SHOTGUN,
	DRAW_GRENADE,
	DRAW_LASER,
	DRAW_WALL,
	NUM_DRAW_MODES
};

class CDrawEditor
{
	CGameContext *GameServer() const;
	IServer *Server() const;

	CCharacter *m_pCharacter;
	int GetCID();

	CEntity *CreateEntity(bool Preview = false);

	void SetAngle(float Angle);
	void AddAngle(int Add);
	void AddLength(int Add);

	bool CanPlace();
	bool CanRemove(CEntity *pEnt);
	int GetPlotID();

	vec2 m_Pos;
	int m_Entity;
	bool m_RoundPos;

	bool m_Erasing;
	bool m_Selecting;
	int64 m_EditStartTick;

	void SendWindow();
	const char *GetMode(int Mode);
	void SetDrawMode(int Mode);
	int m_DrawMode;

	union
	{
		struct
		{
			int m_Type;
			int m_SubType;
		} m_Pickup;

		struct
		{
			float m_Length;
			float m_Angle;
		} m_Laser;
	} m_Data;

	// preview
	void SetPreview();
	void RemovePreview();
	CEntity *m_pPreview;

public:
	CDrawEditor(CCharacter *pChr);

	void Tick();

	bool Active();
	bool Selecting() { return Active() && m_Selecting; }

	void OnPlayerFire();
	void OnWeaponSwitch();
	void OnPlayerDeath();
	void OnPlayerKill();
	void OnInput(CNetObj_PlayerInput *pNewInput);

	// used in snap functions of available entities to draw, returns true if the SnappingClient is not able to see the preview
	bool OnSnapPreview(int SnappingClient);
};
#endif //GAME_SERVER_DRAWEDITOR_H
