#include "teeinfo.h"
#define NO_TRANSLATE "<{no-translate}>"

struct Skin
{
	char m_SkinName[24];
	char m_apSkinPartNames[NUM_SKINPARTS][24];
	int m_aUseCustomColors[NUM_SKINPARTS];
	int m_aSkinPartColors[NUM_SKINPARTS];

	// if the m_SkinName differs from the 0.6 skin file name it is corrected here
	char m_SkinNameSevendown[24];
	const char *SkinNameSevendown() { return m_SkinNameSevendown[0] ? m_SkinNameSevendown : m_SkinName; }
};

static Skin s_Skins[NUM_SKINS] = {
/* standard 0.6 + 0.7 */
{"bluekitty",{"kitty","whisker","","standard","standard","standard"},{1,1,0,1,1,0},{8681144,-8229413,65408,7885547,7885547,65408},""},
{"bluestripe",{"standard","stripes","","standard","standard","standard"},{1,0,0,1,1,0},{10187898,-16711808,65408,750848,1944919,65408},""},
{"brownbear",{"bear","bear","hair","standard","standard","standard"},{1,1,0,1,1,0},{1082745,-15634776,65408,1082745,1147174,65408},""},
{"cammo",{"standard","cammo2","","standard","standard","standard"},{1,1,0,1,1,0},{5334342,-11771603,65408,750848,1944919,65408},""},
{"cammostripes",{"standard","cammostripes","","standard","standard","standard"},{1,1,0,1,1,0},{5334342,-14840320,65408,750848,1944919,65408},""},
{"koala",{"koala","twinbelly","","standard","standard","standard"},{1,1,0,1,1,0},{184,-15397662,65408,184,9765959,65408},""},
{"default",{"standard","","","standard","standard","standard"},{1,0,0,1,1,0},{1798004,-16711808,65408,1799582,1869630,65408},""},
{"limekitty",{"kitty","whisker","","standard","standard","standard"},{1,1,0,1,1,0},{4612803,-12229920,65408,3827951,3827951,65408},""},
{"pinky",{"standard","whisker","","standard","standard","standard"},{1,1,0,1,1,0},{15911355,-801066,65408,15043034,15043034,65408},""},
{"redbopp",{"standard","donny","unibop","standard","standard","standard"},{1,1,1,1,1,1},{16177260,-16590390,16177260,16177260,7624169,65408},""},
{"redstripe",{"standard","stripe","","standard","standard","standard"},{1,0,0,1,1,0},{16307835,-16711808,65408,184,9765959,65408},""},
{"saddo",{"standard","saddo","","standard","standard","standard"},{1,1,0,1,1,0},{7171455,-9685436,65408,3640746,5792119,65408},""},
{"toptri",{"standard","toptri","","standard","standard","standard"},{1,0,0,1,1,0},{6119331,-16711808,65408,3640746,5792119,65408},""},
{"twinbop",{"standard","duodonny","twinbopp","standard","standard","standard"},{1,1,1,1,1,0},{15310519,-1600806,15310519,15310519,37600,65408},""},
{"twintri",{"standard","twintri","","standard","standard","standard"},{1,1,0,1,1,0},{3447932,-14098717,65408,185,9634888,65408},""},
{"warpaint",{"standard","warpaint","","standard","standard","standard"},{1,0,0,1,1,0},{1944919,-16711808,65408,750337,1944919,65408},""},
/* standard 0.7 */
{"beaver",{"beaver","twinbelly","","standard","standard","colorable"},{1,1,0,1,1,1},{1272149,-970966063,65408,1082745,1147174,1559085},""},
{"bumbler",{"raccoon","setisu","","standard","standard","colorable"},{1,1,0,1,1,1},{9834525,1612888028,65408,1804958,1861519,1862454},""},
{"cavebat",{"bat","belly2","","standard","standard","colorable"},{1,1,0,1,1,1},{1091513,-1744165075,65408,666952,949148,553007},"Bat"},
{"force",{"force","wildpaint","","standard","standard","standard"},{1,1,0,1,1,1},{1577780,907949571,65408,1769535,1835008,0},""},
{"fox",{"fox","fox","","standard","standard","colorable"},{1,1,0,1,1,1},{1102443,-485425166,65408,1094755,1102450,1441632},"foxi"},
{"greycoon",{"raccoon","coonfluff","","standard","standard","standard"},{1,1,0,1,1,1},{917651,-15269783,65408,1769643,1245336,1085234},""},
{"greyfox",{"fox","cammostripes","","standard","standard","colorable"},{1,1,0,1,1,1},{1051203,538383807,65408,10230803,2100280,1858113},NO_TRANSLATE},
{"hippo",{"hippo","hipbel","hair","standard","standard","standard"},{1,1,0,1,1,1},{11448503,-1296105217,65408,12404896,13709860,13026349},""},
{"limedog",{"dog","whisker","","standard","standard","negative"},{1,1,0,1,1,1},{2406825,-16737793,65408,832136,970096,1310489},""},
{"monkey",{"monkey","monkey","hair","standard","standard","standard"},{1,1,1,1,1,0},{1421252,-15301582,2352795,1659536,1274287,65408},""},
{"paintgre",{"standard","lowpaint","","standard","standard","standard"},{1,1,0,1,1,0},{2398826,-13303809,65408,2599819,2131003,65408},""},
{"pandabear",{"bear","panda1","hair","standard","standard","standard"},{1,1,0,1,1,1},{9834574,-6411543,65408,1769630,1835070,41215},""},
{"panther",{"kitty","wildpaint","","standard","standard","negative"},{1,1,0,1,1,1},{10813440,721485823,65408,1769488,1835062,2162491},""},
{"pento",{"standard","","unipento","standard","standard","standard"},{1,1,1,1,1,0},{10400379,-788463617,10400379,184,9996701,65408},""},
{"piggy",{"piglet","hipbel","","standard","standard","colorable"},{1,1,0,1,1,1},{16506036,-1425840784,65408,16366995,16561295,65408},""},
{"raccoon",{"raccoon","coonfluff","","standard","standard","standard"},{1,1,0,1,1,1},{1082745,-15634890,65408,1082745,1147174,1557549},""},
{"setisu",{"standard","setisu","hair","standard","standard","standard"},{1,1,1,1,1,0},{851764,-1306329146,1656361,2003775,1677900,65408},""},
{"snowti",{"kitty","tiger2","","standard","standard","colorable"},{1,1,0,1,1,1},{1507583,-870552832,65408,1441965,1441984,1872682},""},
{"spiky",{"spiky","warstripes","","standard","standard","colorable"},{1,1,0,1,1,1},{1835263,-16777216,65408,1769727,1869823,28},""},
{"swardy",{"spiky","duodonny","","standard","standard","standard"},{1,1,0,1,1,1},{4959008,1616196797,65408,5599232,5592988,1880670},""},
{"tiger",{"kitty","tiger1","","standard","standard","colorable"},{1,1,0,1,1,1},{1495659,-602669093,65408,1487971,1495666,1900288},""},
{"tooxy",{"standard","wildpaint","unimelo","standard","standard","standard"},{1,1,1,1,1,0},{4773499,-633274240,15990924,184,9765959,65408},""},
{"warmouse",{"mouse","mice","","standard","standard","negative"},{1,1,0,1,1,1},{1835221,-16776961,65408,1769727,1869823,51228},"mouse"},
/* custom */
{"greensward",{"greensward","duodonny","","standard","standard","standard"},{1,1,0,0,0,0},{5635840,-11141356,65408,65408,65408,65408},""},
{"greyfox2",{"greyfox_body","greyfox_marking","hair","standard","standard","standard"},{1,1,1,1,1,1},{1245401,1224736916,52,7864575,32,0},"greyfox"},
{"penguin",{"standard","mice","","standard","standard","standard"},{1,0,0,1,1,0},{0,-1,0,0,1415987,0},""},
/* spooky ghost cant be set by a 0.6 client, it will only translate if the player has it forced by the server */
{"spooky_ghost",{"spiky","tricircular","","standard","standard","colorable"},{1,1,1,1,1,1},{255,-16777016,255,184,9765959,255},"ghost"},
{"dummy",{"greensward","duodonny","","standard","standard","standard"},{1,1,1,1,1,1},{50,-16777146,50,50,50,50},""},
{"ninja",{"kitty","uppy","","standard","standard","negative"},{1,1,0,0,0,0},{0,-16777152,65408,0,0,0},"x_ninja"},
};

CTeeInfo::CTeeInfo(const char *pSkinName, int UseCustomColor, int ColorBody, int ColorFeet)
{
	str_copy(m_Sevendown.m_SkinName, pSkinName, sizeof(m_Sevendown.m_SkinName));
	m_Sevendown.m_UseCustomColor = UseCustomColor;
	m_Sevendown.m_ColorBody = ColorBody;
	m_Sevendown.m_ColorFeet = ColorFeet;
}

CTeeInfo::CTeeInfo(const char *pSkinPartNames[6], int *pUseCustomColors, int *pSkinPartColors)
{
	for(int i = 0; i < 6; i++)
	{
		str_copy(m_aaSkinPartNames[i], pSkinPartNames[i], sizeof(m_aaSkinPartNames[i]));
		m_aUseCustomColors[i] = pUseCustomColors[i];
		m_aSkinPartColors[i] = pSkinPartColors[i];
	}
}

CTeeInfo::CTeeInfo(int SkinID)
{
	for(int i = 0; i < 6; i++)
	{
		str_copy(m_aaSkinPartNames[i], s_Skins[SkinID].m_apSkinPartNames[i], sizeof(m_aaSkinPartNames[i]));
		m_aUseCustomColors[i] = s_Skins[SkinID].m_aUseCustomColors[i];
		m_aSkinPartColors[i] = s_Skins[SkinID].m_aSkinPartColors[i];
	}

	m_SkinID = SkinID;
	ToSevendown();
}

CTeeInfo::CTeeInfo(const char *pSkin)
{
	for (int i = 0; i < NUM_SKINS; i++)
	{
		if (!str_comp_nocase(pSkin, s_Skins[i].m_SkinName))
		{
			*this = CTeeInfo(i);
			return;
		}
	}
}

void CTeeInfo::Translate(bool Sevendown)
{
	if (Sevendown)
		FromSevendown();
	else
		ToSevendown();
}

void CTeeInfo::FromSevendown()
{
	// reset to default skin
	int Match = SKIN_DEFAULT;
	for(int p = 0; p < NUM_SKINPARTS; p++)
	{
		str_copy(m_aaSkinPartNames[p], s_Skins[SKIN_DEFAULT].m_apSkinPartNames[p], 24);
		m_aUseCustomColors[p] = s_Skins[SKIN_DEFAULT].m_aUseCustomColors[p];
		m_aSkinPartColors[p] = s_Skins[SKIN_DEFAULT].m_aSkinPartColors[p];
	}

	// check for sevendown skin
	for(int i = 0; i < NUM_SKINS; i++)
	{
		// skip spooky ghost skin for comparison
		if (i == SKIN_SPOOKY_GHOST)
			continue;

		if(!str_comp(m_Sevendown.m_SkinName, s_Skins[i].SkinNameSevendown()))
		{
			for(int p = 0; p < NUM_SKINPARTS; p++)
			{
				str_copy(m_aaSkinPartNames[p], s_Skins[i].m_apSkinPartNames[p], 24);
				m_aUseCustomColors[p] = s_Skins[i].m_aUseCustomColors[p];
				m_aSkinPartColors[p] = s_Skins[i].m_aSkinPartColors[p];
			}
			Match = i;
			break;
		}
	}

	int CustomColors = m_Sevendown.m_UseCustomColor;
	if(CustomColors)
	{
		m_aSkinPartColors[SKINPART_BODY] = m_Sevendown.m_ColorBody;
		m_aSkinPartColors[SKINPART_MARKING] = 0x22FFFFFF;
		m_aSkinPartColors[SKINPART_DECORATION] = m_Sevendown.m_ColorBody;
		m_aSkinPartColors[SKINPART_HANDS] = m_Sevendown.m_ColorBody;
		m_aSkinPartColors[SKINPART_FEET] = m_Sevendown.m_ColorFeet;
	}
	else
	{
		for (int p = 0; p < NUM_SKINPARTS; p++)
			m_aSkinPartColors[p] = s_Skins[Match].m_aSkinPartColors[p];
	}

	for (int p = 0; p < NUM_SKINPARTS; p++)
		m_aUseCustomColors[p] = s_Skins[Match].m_aUseCustomColors[p];
}

void CTeeInfo::ToSevendown()
{
	// reset to default skin
	str_copy(m_Sevendown.m_SkinName, s_Skins[SKIN_DEFAULT].SkinNameSevendown(), sizeof(m_Sevendown.m_SkinName));
	m_Sevendown.m_UseCustomColor = 0;
	m_Sevendown.m_ColorBody = 0;
	m_Sevendown.m_ColorFeet = 0;

	// find closest match
	int best_skin = 0;
	int best_matches = -1;
	for(int s = 0; s < NUM_SKINS; s++)
	{
		// skip spooky ghost skin for comparison if its manually set by player and not forced
		if (s == SKIN_SPOOKY_GHOST && m_SkinID != SKIN_SPOOKY_GHOST)
			continue;

		int matches = 0;
		for(int p = 0; p < NUM_SKINPARTS; p++)
			if(str_comp(GetSkinPartName(p), s_Skins[s].m_apSkinPartNames[p]) == 0)
				matches++;

		if(matches > best_matches)
		{
			best_matches = matches;
			best_skin = s;
		}
	}

	str_copy(m_Sevendown.m_SkinName, s_Skins[best_skin].SkinNameSevendown(), sizeof(m_Sevendown.m_SkinName));
	m_Sevendown.m_ColorBody = m_aUseCustomColors[SKINPART_BODY] ? m_aSkinPartColors[SKINPART_BODY] : 255;
	m_Sevendown.m_ColorFeet = m_aUseCustomColors[SKINPART_FEET] ? m_aSkinPartColors[SKINPART_FEET] : 255;

	int CustomColors = m_aUseCustomColors[SKINPART_BODY];
	if (m_aSkinPartColors[SKINPART_BODY] == s_Skins[best_skin].m_aSkinPartColors[SKINPART_BODY])
		CustomColors = false;
	m_Sevendown.m_UseCustomColor = CustomColors;
}
