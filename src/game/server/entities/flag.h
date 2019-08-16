/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#ifndef GAME_SERVER_ENTITIES_FLAG_H
#define GAME_SERVER_ENTITIES_FLAG_H

#include <game/server/entity.h>

class CFlag : public CEntity
{
private:
	int m_Team;
	int m_AtStand;
	int m_DropTick;
	int m_GrabTick;

	CCharacter *m_pCarrier;
	CCharacter *m_pLastCarrier;

	vec2 m_Vel;
	vec2 m_StandPos;
	vec2 m_PrevPos;

	int m_TeleCheckpoint;
	int m_TuneZone;

	void HandleTiles(int Index);
	int m_TileIndex;
	int m_TileFlags;
	int m_TileFIndex;
	int m_TileFFlags;
	int m_TileSIndex;
	int m_TileSFlags;
	int m_TileIndexL;
	int m_TileFlagsL;
	int m_TileFIndexL;
	int m_TileFFlagsL;
	int m_TileSIndexL;
	int m_TileSFlagsL;
	int m_TileIndexR;
	int m_TileFlagsR;
	int m_TileFIndexR;
	int m_TileFFlagsR;
	int m_TileSIndexR;
	int m_TileSFlagsR;
	int m_TileIndexT;
	int m_TileFlagsT;
	int m_TileFIndexT;
	int m_TileFFlagsT;
	int m_TileSIndexT;
	int m_TileSFlagsT;
	int m_TileIndexB;
	int m_TileFlagsB;
	int m_TileFIndexB;
	int m_TileFFlagsB;
	int m_TileSIndexB;
	int m_TileSFlagsB;

public:
	static const int ms_PhysSize = 14;

	int m_DropFreezeTick;

	CCharacter *GetCarrier() const { return m_pCarrier; }
	CCharacter *GetLastCarrier() const { return m_pLastCarrier; }
	void SetCarrier(class CCharacter *pNewCarrier) { m_pCarrier = pNewCarrier; }
	void SetLastCarrier(class CCharacter *pLastCarrier) { m_pLastCarrier = pLastCarrier; }
	bool IsAtStand() const { return m_AtStand; }
	void SetAtStand(bool AtStand) { m_AtStand = AtStand; }
	vec2 GetVel() { return m_Vel; }
	void SetVel(vec2 Vel) { m_Vel = Vel; }

	int GetDropTick() { return m_DropTick; };
	int GetTeam() { return m_Team; };

	void Grab(class CCharacter *pChr);
	void Drop(int Dir = 0);
	void HandleDropped();
	bool IsGrounded(bool SetVel = false);

	CFlag(CGameWorld *pGameWorld, int Team, vec2 Pos);

	virtual void Reset(bool Init = false);
	virtual void TickPaused();
	virtual void TickDefered();
	virtual void Snap(int SnappingClient);
	virtual void Tick();
};

#endif
