// made by fokkonaut

#ifndef GAME_SERVER_DRAWEDITOR_H
#define GAME_SERVER_DRAWEDITOR_H

#include <generated/protocol.h>
#include "entity.h"

class CCharacter;
struct SSelectedArea;

class CDrawEditor
{
	enum
	{
		// do not re-sort draw pickups, order is important for SetPickup()
		// Pickup
		DRAW_PICKUP_HEART = 0,
		DRAW_PICKUP_SHIELD,
		DRAW_PICKUP_HAMMER,
		DRAW_PICKUP_GUN,
		DRAW_PICKUP_SHOTGUN,
		DRAW_PICKUP_GRENADE,
		DRAW_PICKUP_LASER,
		NUM_DRAW_PICKUPS,

		// Wall
		LASERWALL_COLLISION = 0,
		LASERWALL_THICKNESS,
		NUM_LASERWALL_SETTINGS,

		// Door
		LASERDOOR_NUMBER = 0,
		LASERDOOR_MODE,
		NUM_LASERDOOR_SETTINGS,

		// Speedup
		SPEEDUP_FORCE = 0,
		SPEEDUP_MAXSPEED,
		NUM_SPEEDUP_SETTINGS,

		// Teleporters
		TELEPORTER_NUMBER = 0,
		TELEPORTER_MODE,
		TELEPORTER_EVIL,
		NUM_TELEPORTERS_SETTINGS,

		TELE_MODE_TOGGLE = 0,
		TELE_MODE_IN,
		TELE_MODE_OUT,
		TELE_MODE_WEAPON,
		TELE_MODE_HOOK,
		NUM_TELE_MODES,

		// Transform
		TRANSFORM_MOVE = 0,
		TRANSFORM_COPY,
		TRANSFORM_ERASE,
		NUM_TRANSFORM_SETTINGS,

		TRANSFORM_STATE_SETTING_FIRST = 0,
		TRANSFORM_STATE_SETTING_SECOND,
		TRANSFORM_STATE_CONFIRM,
		TRANSFORM_STATE_RUNNING,

		// Categories
		CAT_UNINITIALIZED = -1,
		CAT_PICKUPS,
		CAT_LASERWALLS,
		CAT_LASERDOORS,
		CAT_SPEEDUPS,
		CAT_TELEPORTER,
		CAT_TRANSFORM,
		NUM_DRAW_CATEGORIES,
	};

	CGameContext *GameServer() const;
	IServer *Server() const;

	CCharacter *m_pCharacter;
	int GetCID();

	CEntity *CreateEntity(bool Preview = false);
	CEntity *CreateTransformEntity(CEntity *pTemplate, bool Preview);

	bool IsCategoryLaser() { return m_Category == CAT_LASERWALLS || m_Category == CAT_LASERDOORS; }
	void SetAngle(float Angle);
	void AddAngle(float Add);
	void AddLength(float Add);

	void HandleInput();
	struct DrawInput
	{
		int m_Jump;
		int m_Hook;
		int m_Direction;
	};
	DrawInput m_Input;
	DrawInput m_PrevInput;
	int m_PrevPlotID;

	bool CanPlace(bool Remove = false, CEntity *pEntity = 0);
	bool CanRemove(CEntity *pEntity);
	int GetPlotID();
	int CurrentPlotID();
	int GetCursorPlotID();

	int GetNumMaxDoors();
	int GetNumMaxTeleporters();
	int GetFirstFreeNumber();
	int GetNumSpeedups(int PlotID);

	bool IsCategoryAllowed(int Category);
	const char *GetCategoryListName(int Category);

	vec2 m_Pos;
	int m_Entity;
	bool m_RoundPos;

	bool m_Erasing;
	bool m_Selecting;
	int64 m_EditStartTick;

	void SendWindow();
	const char *GetCategory(int Category);
	void SetCategory(int Category);
	int m_Category;

	int GetNumSettings();
	void SetSetting(int Setting);
	const char *FormatSetting(const char *pSetting, int Setting);
	int m_Setting;

	const char *GetPickup(int Pickup);
	void SetPickup(int Pickup);

	const char *GetTeleporterMode();
	int GetTeleporterType();

	void StopTransform();
	void RemoveEntity(CEntity *pEntity);

	struct
	{
		int m_Type;
		int m_SubType;
	} m_Pickup;

	struct
	{
		float m_Length;
		float m_Angle;
		// Walls
		bool m_Collision;
		int m_Thickness;
		// Doors
		int m_Number;
		bool m_ButtonMode;
	} m_Laser;

	struct
	{
		int m_Force;
		int m_MaxSpeed;
		int m_Angle;
	} m_Speedup;

	struct
	{
		int m_Number;
		bool m_Evil;
		int m_Mode;
	} m_Teleporter;

	struct SSelectedEnt
	{
		CEntity *m_pEnt;
		vec2 m_Offset;
	};
	struct
	{
		int m_State;
		SSelectedArea m_Area;
		std::vector<SSelectedEnt> m_vPreview;
		std::vector<CEntity *> m_vSelected;
	} m_Transform;

	// preview
	void SetPreview();
	void RemovePreview();
	void UpdatePreview();
	CEntity *m_pPreview;

public:
	~CDrawEditor();
	void Init(CCharacter *pChr);
	void Tick();
	void Snap();

	bool Active();
	bool Selecting() { return Active() && m_Selecting; }

	void OnPlayerFire();
	void OnWeaponSwitch();
	void OnPlayerDeath();
	void OnPlayerKill();
	void OnInput(CNetObj_PlayerInput *pNewInput);

	// used in snap functions of available entities to draw, returns true if the SnappingClient is not able to see the preview
	bool OnSnapPreview(CEntity *pEntity);
};
#endif //GAME_SERVER_DRAWEDITOR_H
