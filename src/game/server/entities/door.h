/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_ENTITIES_DOOR_H
#define GAME_SERVER_ENTITIES_DOOR_H

#include <game/server/entity.h>

class CTrigger;

class CDoor: public CEntity
{
	vec2 m_To;
	int m_EvalTick;
	void ResetCollision(bool Remove = false);
	int m_Length;
	vec2 m_Direction;

	// F-DDrace
	float m_Rotation;
	vec2 m_PrevPos;
	int m_Thickness;
	void Update();

public:
	void Open(int Tick, bool ActivatedTeam[]);
	void Open(int Team);
	void Close(int Team);
	CDoor(CGameWorld *pGameWorld, vec2 Pos, float Rotation, int Length,
			int Number, bool Collision = true, int Thickness = 4);

	void SetDirection(float Rotation);
	void SetLength(int Length);
	void SetThickness(int Thickness) { m_Thickness = Thickness; }
	int GetLength() { return m_Length; }
	float GetRotation() { return m_Rotation; }
	int GetThickness() { return m_Thickness; }
	virtual ~CDoor();

	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);
};

#endif // GAME_SERVER_ENTITIES_DOOR_H
