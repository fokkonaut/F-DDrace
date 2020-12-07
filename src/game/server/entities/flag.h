/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#ifndef GAME_SERVER_ENTITIES_FLAG_H
#define GAME_SERVER_ENTITIES_FLAG_H

#include "advanced_entity.h"

class CFlag : public CAdvancedEntity
{
private:
	static const int ms_PhysSize = 14;

	int m_Team;
	bool m_AtStand;
	vec2 m_StandPos;
	int m_DropTick;
	int m_GrabTick;

	int m_Carrier;
	int m_LastCarrier;

	void PlaySound(int Sound);
	int m_SoundTick;
	bool m_CanPlaySound;

	void UpdateSpectators(int SpectatorID);

public:

	CCharacter *GetCarrier();
	CCharacter *GetLastCarrier();
	void SetCarrier(int NewCarrier) { m_Carrier = NewCarrier; }
	void SetLastCarrier(int LastCarrier) { m_LastCarrier = LastCarrier; }
	bool IsAtStand() const { return m_AtStand; }
	void SetAtStand(bool AtStand);
	vec2 GetVel() { return m_Vel; }
	void SetVel(vec2 Vel) { m_Vel = Vel; }
	void SetPrevPos(vec2 Pos) { m_PrevPos = Pos; }

	int GetDropTick() { return m_DropTick; }
	int GetTeam() { return m_Team; }

	void Grab(int NewCarrier);
	void Drop(int Dir = 0);

	CFlag(CGameWorld *pGameWorld, int Team, vec2 Pos);

	void Reset(bool Init);
	virtual void Reset() { Reset(false); };
	virtual void TickPaused();
	virtual void TickDefered();
	virtual void Snap(int SnappingClient);
	virtual void Tick();
};

#endif
