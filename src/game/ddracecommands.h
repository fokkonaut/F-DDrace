/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */

// This file can be included several times.

#ifndef CONSOLE_COMMAND
#define CONSOLE_COMMAND(name, params, flags, callback, userdata, help)
#endif

CONSOLE_COMMAND("kill_pl", "v[id]", CFGFLAG_SERVER, ConKillPlayer, this, "Kills player v and announces the kill")
CONSOLE_COMMAND("totele", "i[number]", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConToTeleporter, this, "Teleports you to teleporter v")
CONSOLE_COMMAND("totelecp", "i[number]", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConToCheckTeleporter, this, "Teleports you to checkpoint teleporter v")
CONSOLE_COMMAND("tele", "?i[id] ?i[id]", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConTeleport, this, "Teleports player i (or you) to player i (or you to where you look at)")
CONSOLE_COMMAND("addweapon", "i[weapon-id]", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConAddWeapon, this, "Gives weapon with id i to you (all = -1, hammer = 0, gun = 1, shotgun = 2, grenade = 3, rifle = 4, ninja = 5)")
CONSOLE_COMMAND("removeweapon", "i[weapon-id]", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConRemoveWeapon, this, "removes weapon with id i from you (all = -1, hammer = 0, gun = 1, shotgun = 2, grenade = 3, rifle = 4, ninja = 5)")
CONSOLE_COMMAND("shotgun", "", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConShotgun, this, "Gives a shotgun to you")
CONSOLE_COMMAND("grenade", "", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConGrenade, this, "Gives a grenade launcher to you")
CONSOLE_COMMAND("rifle", "", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConRifle, this, "Gives a rifle to you")
CONSOLE_COMMAND("jetpack","", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConJetpack, this, "Gives jetpack to you")
CONSOLE_COMMAND("weapons", "", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConWeapons, this, "Gives all weapons to you")
CONSOLE_COMMAND("unshotgun", "", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConUnShotgun, this, "Takes the shotgun from you")
CONSOLE_COMMAND("ungrenade", "", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConUnGrenade, this, "Takes the grenade launcher you")
CONSOLE_COMMAND("unrifle", "", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConUnRifle, this, "Takes the rifle from you")
CONSOLE_COMMAND("unjetpack", "", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConUnJetpack, this, "Takes the jetpack from you")
CONSOLE_COMMAND("unweapons", "", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConUnWeapons, this, "Takes all weapons from you")
CONSOLE_COMMAND("ninja", "", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConNinja, this, "Makes you a ninja")
CONSOLE_COMMAND("super", "", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConSuper, this, "Makes you super")
CONSOLE_COMMAND("unsuper", "", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConUnSuper, this, "Removes super from you")
CONSOLE_COMMAND("unsolo", "", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConUnSolo, this, "Puts you out of solo part")
CONSOLE_COMMAND("undeep", "", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConUnDeep, this, "Puts you out of deep freeze")
CONSOLE_COMMAND("left", "", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConGoLeft, this, "Makes you move 1 tile left")
CONSOLE_COMMAND("right", "", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConGoRight, this, "Makes you move 1 tile right")
CONSOLE_COMMAND("up", "", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConGoUp, this, "Makes you move 1 tile up")
CONSOLE_COMMAND("down", "", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConGoDown, this, "Makes you move 1 tile down")

CONSOLE_COMMAND("move", "i[x] i[y]", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConMove, this, "Moves to the tile with x/y-number ii")
CONSOLE_COMMAND("move_raw", "i[x] i[y]", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConMoveRaw, this, "Moves to the point with x/y-coordinates ii")
CONSOLE_COMMAND("force_pause", "v[id] i[seconds]", CFGFLAG_SERVER, ConForcePause, this, "Force i to pause for i seconds")
CONSOLE_COMMAND("force_unpause", "v[id]", CFGFLAG_SERVER, ConForcePause, this, "Set force-pause timer of i to 0.")

CONSOLE_COMMAND("list", "?s[filter]", CFGFLAG_CHAT, ConList, this, "List connected players with optional case-insensitive substring matching filter")
CONSOLE_COMMAND("set_team_ddr", "v[id] ?i[team]", CFGFLAG_SERVER, ConSetDDRTeam, this, "Set ddrace team of a player")
CONSOLE_COMMAND("uninvite", "v[id] ?i[team]", CFGFLAG_SERVER, ConUninvite, this, "Uninvite player from team")

CONSOLE_COMMAND("mute", "", CFGFLAG_SERVER, ConMute, this, "")
CONSOLE_COMMAND("muteid", "v[id] i[seconds]", CFGFLAG_SERVER, ConMuteID, this, "")
CONSOLE_COMMAND("muteip", "s[ip] i[seconds]", CFGFLAG_SERVER, ConMuteIP, this, "")
CONSOLE_COMMAND("unmute", "v[id]", CFGFLAG_SERVER, ConUnmute, this, "")
CONSOLE_COMMAND("mutes", "", CFGFLAG_SERVER, ConMutes, this, "")

CONSOLE_COMMAND("freezehammer", "v[id]", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConFreezeHammer, this, "Gives a player Freeze Hammer")
CONSOLE_COMMAND("unfreezehammer", "v[id]", CFGFLAG_SERVER|CMDFLAG_CHEAT, ConUnFreezeHammer, this, "Removes Freeze Hammer from a player")
#undef CONSOLE_COMMAND
