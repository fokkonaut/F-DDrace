// made by fokkonaut

#ifndef GAME_SKINS_H
#define GAME_SKINS_H

#include <engine/shared/protocol.h>
#include "player.h"

enum Skins
{
	SKIN_BEAVER,
	SKIN_BLUEKITTY,
	SKIN_BLUESTRIPE,
	SKIN_BROWNBEAR,
	SKIN_BUMBLER,
	SKIN_CAMMO,
	SKIN_CAMMOSTRIPES,
	SKIN_CAVEBAT,
	SKIN_DEFAULT,
	SKIN_FORCE,
	SKIN_FOX,
	SKIN_GREYCOON,
	SKIN_GREYFOX,
	SKIN_HIPPO,
	SKIN_KOALA,
	SKIN_LIMEDOG,
	SKIN_LIMEKITTY,
	SKIN_MONKEY,
	SKIN_PAINTGRE,
	SKIN_PANDABEAR,
	SKIN_PANTHER,
	SKIN_PENTO,
	SKIN_PIGGY,
	SKIN_PINKY,
	SKIN_RACCOON,
	SKIN_REDBOPP,
	SKIN_REDSTRIPE,
	SKIN_SADDO,
	SKIN_SETISU,
	SKIN_SNOWTI,
	SKIN_SPIKY,
	SKIN_SWARDY,
	SKIN_TIGER,
	SKIN_TOOXY,
	SKIN_TOPTRI,
	SKIN_TWINBOP,
	SKIN_TWINTRI,
	SKIN_WARMOUSE,
	SKIN_WARPAINT,

	// vanilla skins above

	SKIN_SPOOKY_GHOST,
	SKIN_GREENSWARD,
	SKIN_DUMMY,
	SKIN_PENGUIN,
	SKIN_GREYFOX2,

	NUM_SKINS
};

class CSkins
{
private:
	CPlayer::TeeInfos m_Skins[NUM_SKINS];

public:

	CSkins();

	int GetSkinID(const char *pSkin);
	CPlayer::TeeInfos GetSkin(int Skin);

	void SkinToSevendown(CPlayer::TeeInfos *pTeeInfos);
	void SkinFromSevendown(CPlayer::TeeInfos *pTeeInfos);
};

#endif
