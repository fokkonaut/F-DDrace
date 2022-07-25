/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_PICKUP_H
#define GAME_SERVER_ENTITIES_PICKUP_H

#include <game/server/entity.h>

const int PickupPhysSize = 14;

class CPickup : public CEntity
{
public:
	CPickup(CGameWorld* pGameWorld, vec2 Pos, int Type, int SubType = 0, int Layer = 0, int Number = 0, int Owner = -1, bool Collision = true);

	virtual ~CPickup();

	void Reset(bool Destroy);
	virtual void Reset() { Reset(false); };
	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);

	int GetType() { return m_Type; }
	int GetSubtype() { return m_Subtype; }
	int GetOwner() { return m_Owner; }

private:
	int m_Type;
	int m_Subtype;
	int m_SpawnTick;

	// F-DDrace
	int m_Owner;
	void Move();
	vec2 m_Core;
	int m_ID2;
	int64 m_aLastRespawnMsg[MAX_CLIENTS];

	struct
	{
		vec2 m_Pos;
		float m_Time;
		float m_LastTime;
	} m_Snap;

	void SetRespawnTime(bool Init = false);
};

#endif
