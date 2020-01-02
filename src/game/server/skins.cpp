// made by fokkonaut

#include "skins.h"

const char *pSkinName[NUM_SKINS] = { "beaver", "bluekitty", "bluestripe", "brownbear", "bumbler", "cammo", "cammostripes", "cavebat", "default", "force", "fox", "greycoon",
							"greyfox", "hippo", "koala", "limedog", "limekitty", "monkey", "paintgre", "pandabear", "panther", "pento", "piggy", "pinky", "raccoon",
							"redbopp", "redstripe", "saddo", "setisu", "snowti", "spiky", "swardy", "tiger", "tooxy", "toptri", "twinbop", "twintri", "warmouse", "warpaint",
							"spooky_ghost", "greensward", "dummy", };

int pSkinCustomColor[NUM_SKINS][NUM_SKINPARTS] = { { 1, 1, 0, 1, 1, 1 }, { 1, 1, 0, 1, 1, 0 }, { 1, 0, 0, 1, 1, 0 }, { 1, 1, 0, 1, 1, 0 }, { 1, 1, 0, 1, 1, 1 },
		{ 1, 1, 0, 1, 1, 0 }, { 1, 1, 0, 1, 1, 0 }, { 1, 1, 0, 1, 1, 1 }, { 1, 0, 0, 1, 1, 0 }, { 1, 1, 0, 1, 1, 1 }, { 1, 1, 0, 1, 1, 1 }, { 1, 1, 0, 1, 1, 1 },
		{ 1, 1, 0, 1, 1, 1 }, { 1, 1, 0, 1, 1, 1 }, { 1, 1, 0, 1, 1, 0 }, { 1, 1, 0, 1, 1, 1 }, { 1, 1, 0, 1, 1, 0 }, { 1, 1, 1, 1, 1, 0 }, { 1, 1, 0, 1, 1, 0 },
		{ 1, 1, 0, 1, 1, 1 }, { 1, 1, 0, 1, 1, 1 }, { 1, 1, 1, 1, 1, 0 }, { 1, 1, 0, 1, 1, 1 }, { 1, 1, 0, 1, 1, 0 }, { 1, 1, 0, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1 },
		{ 1, 0, 0, 1, 1, 0 }, { 1, 1, 0, 1, 1, 0 }, { 1, 1, 1, 1, 1, 0 }, { 1, 1, 0, 1, 1, 1 }, { 1, 1, 0, 1, 1, 1 }, { 1, 1, 0, 1, 1, 1 }, { 1, 1, 0, 1, 1, 1 },
		{ 1, 1, 1, 1, 1, 0 }, { 1, 0, 0, 1, 1, 0 }, { 1, 1, 1, 1, 1, 0 }, { 1, 1, 0, 1, 1, 0 }, { 1, 1, 0, 1, 1, 1 }, { 1, 0, 0, 1, 1, 0 }, { 1, 1, 1, 1, 1, 1 },
		{ 1, 1, 0, 0, 0, 0 }, { 1, 1, 1, 1, 1, 1 },
};

const char* pSkinPartNames[NUM_SKINS][NUM_SKINPARTS] = {
		{ "beaver", "twinbelly", "", "standard", "standard", "colorable" },
		{ "kitty", "whisker", "", "standard", "standard", "standard" },
		{ "standard", "stripes", "", "standard", "standard", "standard" },
		{ "bear", "bear", "hair", "standard", "standard", "standard" },
		{ "raccoon", "setisu", "", "standard", "standard", "colorable" },
		{ "standard", "cammo2", "", "standard", "standard", "standard" },
		{ "standard", "cammostripes", "", "standard", "standard", "standard" },
		{ "bat", "belly2", "", "standard", "standard", "colorable" },
		{ "standard", "", "", "standard", "standard", "standard" },
		{ "force", "wildpaint", "", "standard", "standard", "standard" },
		{ "fox", "fox", "", "standard", "standard", "colorable" },
		{ "raccoon", "coonfluff", "", "standard", "standard", "standard" },
		{ "fox", "cammostripes", "", "standard", "standard", "colorable" },
		{ "hippo", "hipbel", "hair", "standard", "standard", "standard" },
		{ "koala", "twinbelly", "", "standard", "standard", "standard" },
		{ "dog", "whisker", "", "standard", "standard", "negative" },
		{ "kitty", "whisker", "", "standard", "standard", "standard" },
		{ "monkey", "monkey", "hair", "standard", "standard", "standard" },
		{ "standard", "lowpaint", "", "standard", "standard", "standard" },
		{ "bear", "panda1", "hair", "standard", "standard", "standard" },
		{ "kitty", "wildpaint", "", "standard", "standard", "negative" },
		{ "standard", "", "unipento", "standard", "standard", "standard" },
		{ "piglet", "hipbel", "", "standard", "standard", "colorable" },
		{ "standard", "whisker", "", "standard", "standard", "standard" },
		{ "raccoon", "coonfluff", "", "standard", "standard", "standard" },
		{ "standard", "donny", "unibop", "standard", "standard", "standard" },
		{ "standard", "stripe", "", "standard", "standard", "standard" },
		{ "standard", "saddo", "", "standard", "standard", "standard" },
		{ "standard", "setisu", "hair", "standard", "standard", "standard" },
		{ "kitty", "tiger2", "", "standard", "standard", "colorable" },
		{ "spiky", "warstripes", "", "standard", "standard", "colorable" },
		{ "spiky", "duodonny", "", "standard", "standard", "standard" },
		{ "kitty", "tiger1", "", "standard", "standard", "colorable" },
		{ "standard", "wildpaint", "unimelo", "standard", "standard", "standard" },
		{ "standard", "toptri", "", "standard", "standard", "standard" },
		{ "standard", "duodonny", "twinbopp", "standard", "standard", "standard" },
		{ "standard", "twintri", "", "standard", "standard", "standard" },
		{ "mouse", "mice", "", "standard", "standard", "negative" },
		{ "standard", "warpaint", "", "standard", "standard", "standard" },
		{ "spiky", "tricircular", "", "standard", "standard", "colorable" },
		{ "greensward", "duodonny", "", "standard", "standard", "standard" },
		{ "greensward", "duodonny", "", "standard", "standard", "standard" },
};

int pSkinPartColor[NUM_SKINS][NUM_SKINPARTS] = {
		{ 1272149, -970966063, 65408, 1082745, 1147174, 1559085 },
		{ 8681144, -8229413, 65408, 7885547, 7885547, 65408 },
		{ 10187898, -16711808, 65408, 750848, 1944919, 65408 },
		{ 1082745, -15634776, 65408, 1082745, 1147174, 65408 },
		{ 9834525, 1612888028, 65408, 1804958, 1861519, 1862454 },
		{ 5334342, -11771603, 65408, 750848, 1944919, 65408 },
		{ 5334342, -14840320, 65408, 750848, 1944919, 65408 },
		{ 1091513, -1744165075, 65408, 666952, 949148, 553007 },
		{ 1798004, -16711808, 65408, 1799582, 1869630, 65408 },
		{ 1577780, 907949571, 65408, 1769535, 1835008, 0 },
		{ 1102443, -485425166, 65408, 1094755, 1102450, 1441632 },
		{ 917651, -15269783, 65408, 1769643, 1245336, 1085234 },
		{ 1051203, 538383807, 65408, 10230803, 2100280, 1858113 },
		{ 11448503, -1296105217, 65408, 12404896, 13709860, 13026349 },
		{ 184, -15397662, 65408, 184, 9765959, 65408 },
		{ 2406825, -16737793, 65408, 832136, 970096, 1310489 },
		{ 4612803, -12229920, 65408, 3827951, 3827951, 65408 },
		{ 1421252, -15301582, 2352795, 1659536, 1274287, 65408 },
		{ 2398826, -13303809, 65408, 2599819, 2131003, 65408 },
		{ 9834574, -6411543, 65408, 1769630, 1835070, 41215 },
		{ 10813440, 721485823, 65408, 1769488, 1835062, 2162491 },
		{ 10400379, -788463617, 10400379, 184, 9996701, 65408 },
		{ 16506036, -1425840784, 65408, 16366995, 16561295, 65408 },
		{ 15911355, -801066, 65408, 15043034, 15043034, 65408 },
		{ 1082745, -15634890, 65408, 1082745, 1147174, 1557549 },
		{ 16177260, -16590390, 16177260, 16177260, 7624169, 65408 },
		{ 16307835, -16711808, 65408, 184, 9765959, 65408 },
		{ 7171455, -9685436, 65408, 3640746, 5792119, 65408 },
		{ 851764, -1306329146, 1656361, 2003775, 1677900, 65408 },
		{ 1507583, -870552832, 65408, 1441965, 1441984, 1872682 },
		{ 1835263, -16777216, 65408, 1769727, 1869823, 28 },
		{ 4959008, 1616196797, 65408, 5599232, 5592988, 1880670 },
		{ 1495659, -602669093, 65408, 1487971, 1495666, 1900288 },
		{ 4773499, -633274240, 15990924, 184, 9765959, 65408 },
		{ 6119331, -16711808, 65408, 3640746, 5792119, 65408 },
		{ 15310519, -1600806, 15310519, 15310519, 37600, 65408 },
		{ 3447932, -14098717, 65408, 185, 9634888, 65408 },
		{ 1835221, -16776961, 65408, 1769727, 1869823, 51228 },
		{ 1944919, -16711808, 65408, 750337, 1944919, 65408 },
		{ 255, -16777016, 255, 184, 9765959, 255 },
		{ 5635840, -11141356, 65408, 65408, 65408, 65408 },
		{ 50, -16777146, 50, 50, 50, 50 },
};

CSkins::CSkins()
{
	for (int i = 0; i < NUM_SKINS; i++)
	{
		for (int p = 0; p < NUM_SKINPARTS; p++)
		{
			str_copy(m_Skins[i].m_aSkinName, pSkinName[i], 24);
			str_copy(m_Skins[i].m_aaSkinPartNames[p], pSkinPartNames[i][p], 24);
			m_Skins[i].m_aUseCustomColors[p] = pSkinCustomColor[i][p];
			m_Skins[i].m_aSkinPartColors[p] = pSkinPartColor[i][p];
		}
	}
}

int CSkins::GetSkinID(const char *pSkin)
{
	for (int i = 0; i < NUM_SKINS; i++)
		if (!str_comp_nocase(pSkin, m_Skins[i].m_aSkinName))
			return i;
}
