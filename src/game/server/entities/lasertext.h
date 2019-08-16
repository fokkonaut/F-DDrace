// made by fstd, improved by jupeyy

#ifndef GAME_SERVER_ENTITIES_LASERTEXT_H
#define GAME_SERVER_ENTITIES_LASERTEXT_H

#include <game/server/entity.h>

class CLaserChar : public CEntity {
public:
	CLaserChar(CGameWorld *pGameWorld) : CEntity(pGameWorld, CGameWorld::ENTTYPE_LASERTEXT, GetPos()) {
	}
public:
	int getID() { return GetID(); }
	vec2 m_Frompos;
	void SetPosX(float X) { m_Pos.x = X; };
	void SetPosY(float Y) { m_Pos.y = Y; };
};

class CLaserText : public CEntity
{
public:
	CLaserText(CGameWorld *pGameWorld, vec2 Pos, int Owner, int AliveTicks, const char* pText, int TextLen, float CharPointOffset = 15.f, float CharOffsetFactor = 3.5f);
	virtual ~CLaserText(){ 
		delete[] m_Text; 
		for(int i = 0; i < m_CharNum; ++i) {
			delete (CLaserChar*)m_Chars[i];
		}
		delete[] m_Chars;
	}

	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);

private:
	float m_PosOffsetCharPoints;
	float m_PosOffsetChars;

	void makeLaser(char pChar, int pCharOffset, int& charCount);

	int m_Owner;
	
	int m_AliveTicks;
	int m_CurTicks;
	int m_StartTick;
	
	char* m_Text;
	int m_TextLen;
	
	CLaserChar** m_Chars;
	int m_CharNum;
};

#endif
