/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_MAPITEMS_H
#define GAME_MAPITEMS_H

#include <vector>

// layer types
enum
{
	LAYERTYPE_INVALID=0,
	LAYERTYPE_GAME,
	LAYERTYPE_TILES,
	LAYERTYPE_QUADS,
	LAYERTYPE_FRONT,
	LAYERTYPE_TELE,
	LAYERTYPE_SPEEDUP,
	LAYERTYPE_SWITCH,
	LAYERTYPE_TUNE,

	MAPITEMTYPE_VERSION=0,
	MAPITEMTYPE_INFO,
	MAPITEMTYPE_IMAGE,
	MAPITEMTYPE_ENVELOPE,
	MAPITEMTYPE_GROUP,
	MAPITEMTYPE_LAYER,
	MAPITEMTYPE_ENVPOINTS,


	CURVETYPE_STEP=0,
	CURVETYPE_LINEAR,
	CURVETYPE_SLOW,
	CURVETYPE_FAST,
	CURVETYPE_SMOOTH,
	CURVETYPE_BEZIER,
	NUM_CURVETYPES,

	// game layer tiles
	ENTITY_OFFSET=255-16*4,
	ENTITY_SPAWN = 192,
	ENTITY_SPAWN_RED = 193,
	ENTITY_SPAWN_BLUE = 194,
	ENTITY_FLAGSTAND_RED = 195,
	ENTITY_FLAGSTAND_BLUE = 196,
	ENTITY_ARMOR_1 = 197,
	ENTITY_HEALTH_1 = 198,
	ENTITY_WEAPON_SHOTGUN = 199,
	ENTITY_WEAPON_GRENADE = 200,
	ENTITY_POWERUP_NINJA = 201,
	ENTITY_WEAPON_LASER = 202,
	//F-DDrace - Main Lasers
	ENTITY_LASER_FAST_CCW = 203,
	ENTITY_LASER_NORMAL_CCW = 204,
	ENTITY_LASER_SLOW_CCW = 205,
	ENTITY_LASER_STOP = 206,
	ENTITY_LASER_SLOW_CW = 207,
	ENTITY_LASER_NORMAL_CW = 208,
	ENTITY_LASER_FAST_CW = 209,
	//F-DDrace - Laser Modifiers
	ENTITY_LASER_SHORT = 210,
	ENTITY_LASER_MEDIUM = 211,
	ENTITY_LASER_LONG = 212,
	ENTITY_LASER_C_SLOW = 213,
	ENTITY_LASER_C_NORMAL = 214,
	ENTITY_LASER_C_FAST = 215,
	ENTITY_LASER_O_SLOW = 216,
	ENTITY_LASER_O_NORMAL = 217,
	ENTITY_LASER_O_FAST = 218,
	//F-DDrace - Plasma
	ENTITY_PLASMAE = 220,
	ENTITY_PLASMAF = 221,
	ENTITY_PLASMA = 222,
	ENTITY_PLASMAU = 223,
	//F-DDrace - Shotgun
	ENTITY_CRAZY_SHOTGUN_EX = 224,
	ENTITY_CRAZY_SHOTGUN = 225,

	// F-DDrace
	ENTITY_PICKUP_TASER_BATTERY = 226,
	ENTITY_CLOCK = 227,
	ENTITY_PICKUP_PORTAL_BATTERY = 228,

	//F-DDrace - Draggers
	ENTITY_DRAGGER_WEAK = 233,
	ENTITY_DRAGGER_NORMAL = 234,
	ENTITY_DRAGGER_STRONG = 235,
	//Draggers Behind Walls
	ENTITY_DRAGGER_WEAK_NW = 236,
	ENTITY_DRAGGER_NORMAL_NW = 237,
	ENTITY_DRAGGER_STRONG_NW = 238,
	//Doors
	ENTITY_DOOR = 240,

	ENTITY_WEAPON_LIGHTNING_LASER = 232,
	ENTITY_WEAPON_GUN = 241,
	ENTITY_WEAPON_HAMMER = 242,
	ENTITY_WEAPON_PLASMA_RIFLE = 243,
	ENTITY_WEAPON_HEART_GUN = 244,
	ENTITY_WEAPON_STRAIGHT_GRENADE = 245,
	ENTITY_WEAPON_TELEKINESIS = 246,
	ENTITY_WEAPON_LIGHTSABER = 247,
	ENTITY_WEAPON_PORTAL_RIFLE = 248,
	ENTITY_WEAPON_PROJECTILE_RIFLE = 249,
	ENTITY_WEAPON_BALL_GRENADE = 250,
	ENTITY_WEAPON_TELE_RIFLE = 251,
	ENTITY_WEAPON_TASER = 252,

	ENTITY_TAVERN_DUMMY_SPAWN = 239,
	ENTITY_BANK_DUMMY_SPAWN = 253,
	ENTITY_PLOT_SHOP_DUMMY_SPAWN = 254,
	ENTITY_SHOP_DUMMY_SPAWN = 255,
	//End Of Lower Tiles
	NUM_ENTITIES,
	//Start From Top Left
	//Tile Controllers
	TILE_AIR = 0,
	TILE_SOLID,
	TILE_DEATH,
	TILE_NOHOOK,
	TILE_NOLASER,
	TILE_THROUGH_CUT,
	TILE_THROUGH,
	TILE_JUMP,
	TILE_FREEZE = 9,
	TILE_TELEINEVIL,
	TILE_UNFREEZE,
	TILE_DFREEZE,
	TILE_DUNFREEZE,
	TILE_TELEINWEAPON,
	TILE_TELEINHOOK,
	TILE_WALLJUMP = 16,
	TILE_EHOOK_START,
	TILE_EHOOK_END,
	TILE_HIT_START,
	TILE_HIT_END,
	TILE_SOLO_START,
	TILE_SOLO_END,
	//Switches
	TILE_SWITCHTIMEDOPEN = 22,
	TILE_SWITCHTIMEDCLOSE,
	TILE_SWITCHOPEN,
	TILE_SWITCHCLOSE,
	TILE_TELEIN,
	TILE_TELEOUT,
	TILE_BOOST,
	TILE_TELECHECK,
	TILE_TELECHECKOUT,
	TILE_TELECHECKIN,
	TILE_REFILL_JUMPS = 32,
	TILE_BEGIN,
	TILE_END,
	TILE_CHECKPOINT_FIRST = 35,
	TILE_CHECKPOINT_LAST = 59,
	TILE_STOP = 60,
	TILE_STOPS,
	TILE_STOPA,
	TILE_TELECHECKINEVIL,
	TILE_CP,
	TILE_CP_F,
	TILE_THROUGH_ALL,
	TILE_THROUGH_DIR,
	TILE_TUNE,
	TILE_TUNELOCK,
	TILE_TUNELOCK_RESET,
	TILE_OLDLASER = 71,
	TILE_NPC,
	TILE_EHOOK,
	TILE_NOHIT,
	TILE_NPH,
	TILE_UNLOCK_TEAM,
	TILE_PENALTY = 79,
	TILE_NPC_END = 88,
	TILE_SUPER_END,
	TILE_JETPACK_END,
	TILE_NPH_END,
	TILE_BONUS = 95,
	TILE_TELE_GUN_ENABLE = 96,
	TILE_TELE_GUN_DISABLE = 97,
	TILE_ALLOW_TELE_GUN = 98,
	TILE_ALLOW_BLUE_TELE_GUN = 99,
	TILE_NPC_START = 104,
	TILE_SUPER_START,
	TILE_JETPACK_START,
	TILE_NPH_START,
	TILE_TELE_GRENADE_ENABLE = 112,
	TILE_TELE_GRENADE_DISABLE = 113,
	TILE_TELE_LASER_ENABLE = 128,
	TILE_TELE_LASER_DISABLE = 129,
	TILE_CREDITS_1 = 140,
	TILE_CREDITS_2 = 141,
	TILE_CREDITS_3 = 142,
	TILE_CREDITS_4 = 143,
	TILE_CREDITS_5 = 156,
	TILE_CREDITS_6 = 157,
	TILE_CREDITS_7 = 158,
	TILE_CREDITS_8 = 159,
	TILE_ENTITIES_OFF_1 = 190,
	TILE_ENTITIES_OFF_2 = 191,

	TILE_JETPACK = 144,
	TILE_RAINBOW = 145,
	TILE_ATOM = 146,
	TILE_TRAIL = 147,
	TILE_SPOOKY_GHOST = 148,
	TILE_ADD_METEOR = 149,
	TILE_REMOVE_METEORS = 150,
	TILE_PASSIVE = 151,
	TILE_VANILLA_MODE = 152,
	TILE_DDRACE_MODE = 153,
	TILE_BLOODY = 154,
	TILE_JUMP_ADD = 155,

	TILE_MONEY_XP_BOMB = 114,
	TILE_BANK = 119,
	TILE_JAIL = 121,
	TILE_JAIL_RELEASE = 122,
	TILE_MONEY = 160,
	TILE_SHOP = 161,
	TILE_ROOM = 162,
	TILE_SPECIAL_FINISH = 163,
	TILE_MONEY_POLICE = 164,
	TILE_MONEY_EXTRA = 165,
	TILE_PLOT_SHOP = 168,
	TILE_TAVERN = 169,

	TILE_NO_BONUS_AREA = 170,
	TILE_NO_BONUS_AREA_LEAVE = 171,

	TILE_VIP_PLUS_ONLY = 172,
	TILE_HELPERS_ONLY = 173,
	TILE_MODERATORS_ONLY = 174,
	TILE_ADMINS_ONLY = 175,

	TILE_MINIGAME_BLOCK = 176,
	TILE_SURVIVAL_LOBBY = 177,
	TILE_SURVIVAL_SPAWN = 178,
	TILE_SURVIVAL_DEATHMATCH = 179,

	TILE_DURAK_TABLE = 180,
	TILE_DURAK_SEAT = 181,
	TILE_DURAK_LOBBY = 182,

	TILE_1VS1_LOBBY = 132,
	TILE_TRANSFORM_ZOMBIE = 133,
	TILE_TRANSFORM_HUMAN = 134,

	TILE_BIRTHDAY_ENABLE = 138,
	TILE_BIRTHDAY_JETPACK_RECV = 139,

	TILE_FLAG_STOP = 185,
	TILE_ADD_2X_XP_TWO_LIFES = 186,
	TILE_TASER_SHIELD_PLUS = 187,
	TILE_REM_FIRST_PORTAL = 188,
	TILE_PORTAL_RIFLE_STOP = 189,
	//End of higher tiles

	// Switch layer only for plots
	TILE_SWITCH_PLOT = 192, // plot inside (able to place objects here)
	TILE_SWITCH_PLOT_DOOR = 193, // door lenght in blocks: delay
	TILE_SWITCH_PLOT_TOTELE = 194, // totele plot position, // plot size; delay=0: small, delay=1: big
	TILE_SWITCH_REDIRECT_SERVER_FROM = 195, // from tp for server redirection
	TILE_SWITCH_REDIRECT_SERVER_TO = 196,
	TILE_SWITCH_HELICOPTER_SPAWN = 219,
	TILE_SWITCHTOGGLE = 28, // for draw editor placed buttons for plot draw doors, also supported as mappable tiles
	TILE_TELE_INOUT_EVIL = 94, // also supported as mappable tiles
	TILE_TELE_INOUT = 95, // also supported as mappable tiles

	//Layers
	LAYER_GAME=0,
	LAYER_FRONT,
	LAYER_TELE,
	LAYER_SPEEDUP,
	LAYER_SWITCH,
	LAYER_TUNE,
	NUM_LAYERS,
	//Flags
	TILEFLAG_VFLIP=1,
	TILEFLAG_HFLIP=2,
	TILEFLAG_OPAQUE=4,
	TILEFLAG_ROTATE=8,
	//Rotation
	ROTATION_0 = 0,
	ROTATION_90 = TILEFLAG_ROTATE,
	ROTATION_180 = (TILEFLAG_VFLIP|TILEFLAG_HFLIP),
	ROTATION_270 = (TILEFLAG_VFLIP|TILEFLAG_HFLIP|TILEFLAG_ROTATE),

	LAYERFLAG_DETAIL=1,
	TILESLAYERFLAG_GAME=1,
	TILESLAYERFLAG_TELE=2,
	TILESLAYERFLAG_SPEEDUP=4,
	TILESLAYERFLAG_FRONT=8,
	TILESLAYERFLAG_SWITCH=16,
	TILESLAYERFLAG_TUNE=32,

	// F-DDrace
	NUM_INDICES = 256,
	MAX_PLOTS = 256-1,
};

struct CPoint
{
	int x, y; // 22.10 fixed point
};

struct CColor
{
	int r, g, b, a;
};

struct CQuad
{
	CPoint m_aPoints[5];
	CColor m_aColors[4];
	CPoint m_aTexcoords[4];

	int m_PosEnv;
	int m_PosEnvOffset;

	int m_ColorEnv;
	int m_ColorEnvOffset;
};

class CTile
{
public:
	unsigned char m_Index;
	unsigned char m_Flags;
	unsigned char m_Skip;
	unsigned char m_Reserved;
};

struct CMapItemInfo
{
	enum { CURRENT_VERSION=1 };
	int m_Version;
	int m_Author;
	int m_MapVersion;
	int m_Credits;
	int m_License;
} ;

struct CMapItemInfoSettings : CMapItemInfo
{
	int m_Settings;
};

struct CMapItemImage_v1
{
	int m_Version;
	int m_Width;
	int m_Height;
	int m_External;
	int m_ImageName;
	int m_ImageData;
} ;

struct CMapItemImage : public CMapItemImage_v1
{
	enum { CURRENT_VERSION=2 };
	int m_Format;
};

struct CMapItemGroup_v1
{
	int m_Version;
	int m_OffsetX;
	int m_OffsetY;
	int m_ParallaxX;
	int m_ParallaxY;

	int m_StartLayer;
	int m_NumLayers;
} ;


struct CMapItemGroup : public CMapItemGroup_v1
{
	enum { CURRENT_VERSION=3 };

	int m_UseClipping;
	int m_ClipX;
	int m_ClipY;
	int m_ClipW;
	int m_ClipH;

	int m_aName[3];
} ;

struct CMapItemLayer
{
	int m_Version;
	int m_Type;
	int m_Flags;
} ;

struct CMapItemLayerTilemap
{
	enum { CURRENT_VERSION=4 };

	CMapItemLayer m_Layer;
	int m_Version;

	int m_Width;
	int m_Height;
	int m_Flags;

	CColor m_Color;
	int m_ColorEnv;
	int m_ColorEnvOffset;

	int m_Image;
	int m_Data;

	int m_aName[3];

	// F-DDrace

	int m_Tele;
	int m_Speedup;
	int m_Front;
	int m_Switch;
	int m_Tune;
} ;

struct CMapItemLayerQuads
{
	enum { CURRENT_VERSION=2 };

	CMapItemLayer m_Layer;
	int m_Version;

	int m_NumQuads;
	int m_Data;
	int m_Image;

	int m_aName[3];
} ;

struct CMapItemVersion
{
	enum { CURRENT_VERSION=1 };

	int m_Version;
} ;

struct CEnvPoint_v1
{
	int m_Time; // in ms
	int m_Curvetype;
	int m_aValues[4]; // 1-4 depending on envelope (22.10 fixed point)

	bool operator<(const CEnvPoint_v1 &Other) const { return m_Time < Other.m_Time; }
} ;

struct CEnvPoint : public CEnvPoint_v1
{
	// bezier curve only
	// dx in ms and dy as 22.10 fxp
	int m_aInTangentdx[4];
	int m_aInTangentdy[4];
	int m_aOutTangentdx[4];
	int m_aOutTangentdy[4];

	bool operator<(const CEnvPoint& other) const { return m_Time < other.m_Time; }
};

struct CMapItemEnvelope_v1
{
	int m_Version;
	int m_Channels;
	int m_StartPoint;
	int m_NumPoints;
	int m_aName[8];
} ;

struct CMapItemEnvelope_v2 : public CMapItemEnvelope_v1
{
	enum { CURRENT_VERSION=2 };
	int m_Synchronized;
};

struct CMapItemEnvelope : public CMapItemEnvelope_v2
{
	// bezier curve support
	enum { CURRENT_VERSION=3 };
};

// F-DDrace

class CTeleTile
{
public:
	CTeleTile() :
		m_Number(0),
		m_Type(0)
	{
	}

	unsigned char m_Number;
	unsigned char m_Type;
};

class CSpeedupTile
{
public:
	CSpeedupTile() :
		m_Force(0),
		m_MaxSpeed(0),
		m_Type(0),
		m_Angle(0)
	{
	}

	unsigned char m_Force;
	unsigned char m_MaxSpeed;
	unsigned char m_Type;
	short m_Angle;
};

class CSwitchTile
{
public:
	CSwitchTile() :
		m_Number(0),
		m_Type(0),
		m_Flags(0),
		m_Delay(0)
	{
	}

	unsigned char m_Number;
	unsigned char m_Type;
	unsigned char m_Flags;
	unsigned char m_Delay;
};

class CDoorTile
{
public:
	struct SInfo
	{
		SInfo(int Index, int Number, int Flags = 0)
		{
			m_Index = Index;
			m_Number = Number;
			m_Flags = Flags;
			m_Usage = 0;
		}
		int m_Index;
		int m_Number;
		int m_Flags;
		int m_Usage; // draw editor putting multiple walls on top of each other so we know when to remove the collision on removal of last door
	};
	std::vector<SInfo> m_vTiles;
};

class CTuneTile
{
public:
	unsigned char m_Number;
	unsigned char m_Type;
};

#endif
