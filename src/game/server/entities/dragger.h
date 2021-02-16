/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_ENTITIES_DRAGGER_H
#define GAME_SERVER_ENTITIES_DRAGGER_H

#include <game/server/entity.h>
class CCharacter;

class CDragger: public CEntity
{
	vec2 m_Core;
	float m_Strength;
	int m_EvalTick;
	void Move(int Team);
	void Drag(int Team);
	bool m_NW;

	CCharacter *m_apTarget[MAX_CLIENTS];
	CCharacter *m_aapSoloEnts[MAX_CLIENTS][MAX_CLIENTS];

public:

	CDragger(CGameWorld *pGameWorld, vec2 Pos, float Strength, bool NW, int Layer = 0, int Number = 0);

	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);
};

#endif // GAME_SERVER_ENTITIES_DRAGGER_H
