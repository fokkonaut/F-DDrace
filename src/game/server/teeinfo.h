#ifndef GAME_SERVER_TEEINFO_H
#define GAME_SERVER_TEEINFO_H

#include <engine/shared/protocol.h>
#include <generated/protocol.h>

enum Skins
{
	SKIN_NONE = -1,

	// standard 0.6 + 0.7
	SKIN_BLUEKITTY,
	SKIN_BLUESTRIPE,
	SKIN_BROWNBEAR,
	SKIN_CAMMO,
	SKIN_CAMMOSTRIPES,
	SKIN_KOALA,
	SKIN_DEFAULT,
	SKIN_LIMEKITTY,
	SKIN_PINKY,
	SKIN_REDBOPP,
	SKIN_REDSTRIPE,
	SKIN_SADDO,
	SKIN_TOPTRI,
	SKIN_TWINBOP,
	SKIN_TWINTRI,
	SKIN_WARPAINT,

	// standard 0.7
	SKIN_BEAVER,
	SKIN_BUMBLER,
	SKIN_CAVEBAT,
	SKIN_FORCE,
	SKIN_FOX,
	SKIN_GREYCOON,
	SKIN_GREYFOX,
	SKIN_HIPPO,
	SKIN_LIMEDOG,
	SKIN_MONKEY,
	SKIN_PAINTGRE,
	SKIN_PANDABEAR,
	SKIN_PANTHER,
	SKIN_PENTO,
	SKIN_PIGGY,
	SKIN_RACCOON,	
	SKIN_SETISU,
	SKIN_SNOWTI,
	SKIN_SPIKY,
	SKIN_SWARDY,
	SKIN_TIGER,
	SKIN_TOOXY,
	SKIN_WARMOUSE,

	// custom
	SKIN_GREENSWARD,
	SKIN_GREYFOX2,
	SKIN_PENGUIN,
	SKIN_SPOOKY_GHOST,
	SKIN_DUMMY,
	SKIN_NINJA,

	NUM_SKINS
};

class CTeeInfo
{
public:
	char m_aaSkinPartNames[NUM_SKINPARTS][MAX_SKIN_ARRAY_SIZE] = {"", "", "", "", "", ""};
	int m_aUseCustomColors[NUM_SKINPARTS] = {false, false, false, false, false, false};
	int m_aSkinPartColors[NUM_SKINPARTS] = {0, 0, 0, 0, 0, 0};

	// 0.6 info
	struct Sevendown
	{
		char m_SkinName[MAX_SKIN_LENGTH] = { '\0' };
		int m_UseCustomColor = 0;
		int m_ColorBody = 0;
		int m_ColorFeet = 0;

	} m_Sevendown;

	// skin id for when skin got set by rcon
	int m_SkinID = SKIN_NONE;

	CTeeInfo() = default;

	// This constructor will assume all arrays are of length 6
	CTeeInfo(const char *pSkinPartNames[6], int *pUseCustomColors, int *pSkinPartColors);
	CTeeInfo(const char *pSkinName, int UseCustomColor, int ColorBody, int ColorFeet);
	CTeeInfo(int SkinID);
	CTeeInfo(const char *pSkin);

	void Translate(bool Sevendown);
	void FromSevendown();
	void ToSevendown();

	const char *GetSkinPartName(int Part)
	{
		// filter out F-Client strings from client recognition
		if (str_startswith(m_aaSkinPartNames[Part], "fclient!"))
		{
			if (Part == SKINPART_MARKING || Part == SKINPART_DECORATION)
				return "";
			return "standard";
		}
		return m_aaSkinPartNames[Part];
	}
};
#endif //GAME_SERVER_TEEINFO_H
