/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */

// This file can be included several times.

#ifndef CONSOLE_COMMAND
#define CONSOLE_COMMAND(name, params, flags, callback, userdata, help, accesslevel)
#endif

CONSOLE_COMMAND("kill_pl", "v[id]", CFGFLAG_SERVER, ConKillPlayer, this, "Kills player v and announces the kill", AUTHED_ADMIN)
CONSOLE_COMMAND("totele", "i[number]", CFGFLAG_SERVER|CMDFLAG_TEST, ConToTeleporter, this, "Teleports you to teleporter v", AUTHED_ADMIN)
CONSOLE_COMMAND("totelecp", "i[number]", CFGFLAG_SERVER|CMDFLAG_TEST, ConToCheckTeleporter, this, "Teleports you to checkpoint teleporter v", AUTHED_ADMIN)
CONSOLE_COMMAND("tele", "?v[id] ?i[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConTeleport, this, "Teleports player v to player i", AUTHED_ADMIN)
CONSOLE_COMMAND("addweapon", "i[weapon-id] ?v[id] ?i[spread]", CFGFLAG_SERVER|CMDFLAG_TEST, ConAddWeapon, this, "Gives weapon i to player v, or spread weapon", AUTHED_ADMIN)
CONSOLE_COMMAND("removeweapon", "i[weapon-id] ?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConRemoveWeapon, this, "Removes weapon i from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("shotgun", "?v[id] ?i[spread]", CFGFLAG_SERVER|CMDFLAG_TEST, ConShotgun, this, "Gives a shotgun to player v, or spread shotgun", AUTHED_ADMIN)
CONSOLE_COMMAND("grenade", "?v[id] ?i[spread]", CFGFLAG_SERVER|CMDFLAG_TEST, ConGrenade, this, "Gives a grenade launcher to player v, or spread grenade", AUTHED_ADMIN)
CONSOLE_COMMAND("rifle", "?v[id] ?i[spread]", CFGFLAG_SERVER|CMDFLAG_TEST, ConRifle, this, "Gives a rifle to player v, or spread rifle", AUTHED_ADMIN)
CONSOLE_COMMAND("weapons", "?v[id] ?i[spread]", CFGFLAG_SERVER|CMDFLAG_TEST, ConWeapons, this, "Gives all weapons to player v, or spread weapons", AUTHED_ADMIN)
CONSOLE_COMMAND("unshotgun", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnShotgun, this, "Takes the shotgun from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("ungrenade", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnGrenade, this, "Takes the grenade launcher player v", AUTHED_ADMIN)
CONSOLE_COMMAND("unrifle", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnRifle, this, "Takes the rifle from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("unweapons", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnWeapons, this, "Takes all weapons from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("ninja", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConNinja, this, "Makes player v a ninja", AUTHED_ADMIN)
CONSOLE_COMMAND("super", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConSuper, this, "Makes player v super", AUTHED_ADMIN)
CONSOLE_COMMAND("unsuper", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnSuper, this, "Removes super from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("unsolo", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnSolo, this, "Puts player v out of solo part", AUTHED_ADMIN)
CONSOLE_COMMAND("undeep", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnDeep, this, "Puts player v out of deep freeze", AUTHED_ADMIN)
CONSOLE_COMMAND("left", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConGoLeft, this, "Makes you move 1 tile left", AUTHED_ADMIN)
CONSOLE_COMMAND("right", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConGoRight, this, "Makes you move 1 tile right", AUTHED_ADMIN)
CONSOLE_COMMAND("up", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConGoUp, this, "Makes you move 1 tile up", AUTHED_ADMIN)
CONSOLE_COMMAND("down", "", CFGFLAG_SERVER|CMDFLAG_TEST, ConGoDown, this, "Makes you move 1 tile down", AUTHED_ADMIN)

CONSOLE_COMMAND("move", "i[x] i[y]", CFGFLAG_SERVER|CMDFLAG_TEST, ConMove, this, "Moves to the tile with x/y-number ii", AUTHED_ADMIN)
CONSOLE_COMMAND("move_raw", "i[x] i[y]", CFGFLAG_SERVER|CMDFLAG_TEST, ConMoveRaw, this, "Moves to the point with x/y-coordinates ii", AUTHED_ADMIN)
CONSOLE_COMMAND("force_pause", "v[id] i[seconds]", CFGFLAG_SERVER, ConForcePause, this, "Force i to pause for i seconds", AUTHED_ADMIN)
CONSOLE_COMMAND("force_unpause", "v[id]", CFGFLAG_SERVER, ConForcePause, this, "Set force-pause timer of i to 0.", AUTHED_ADMIN)

CONSOLE_COMMAND("set_team_ddr", "v[id] ?i[team]", CFGFLAG_SERVER, ConSetDDRTeam, this, "Set ddrace team of a player", AUTHED_ADMIN)
CONSOLE_COMMAND("uninvite", "v[id] ?i[team]", CFGFLAG_SERVER, ConUninvite, this, "Uninvite player from team", AUTHED_ADMIN)

CONSOLE_COMMAND("vote_mute", "v[id] i[seconds]", CFGFLAG_SERVER, ConVoteMute, this, "Remove v's right to vote for i seconds", AUTHED_ADMIN)
CONSOLE_COMMAND("vote_unmute", "v[id]", CFGFLAG_SERVER, ConVoteUnmute, this, "Give back v's right to vote.", AUTHED_ADMIN)
CONSOLE_COMMAND("vote_mutes", "", CFGFLAG_SERVER, ConVoteMutes, this, "List the current active vote mutes.", AUTHED_ADMIN)
CONSOLE_COMMAND("mute", "", CFGFLAG_SERVER, ConMute, this, "", AUTHED_MOD)
CONSOLE_COMMAND("muteid", "v[id] i[seconds] ?r[reason]", CFGFLAG_SERVER, ConMuteID, this, "", AUTHED_MOD)
CONSOLE_COMMAND("muteip", "s[ip] i[seconds] ?r[reason]", CFGFLAG_SERVER, ConMuteIP, this, "", AUTHED_MOD)
CONSOLE_COMMAND("unmute", "v[id]", CFGFLAG_SERVER, ConUnmute, this, "", AUTHED_MOD)
CONSOLE_COMMAND("mutes", "", CFGFLAG_SERVER, ConMutes, this, "", AUTHED_HELPER)

// F-DDrace
//weapons
CONSOLE_COMMAND("allweapons", "?v[id] ?i[spread]", CFGFLAG_SERVER|CMDFLAG_TEST, ConAllWeapons, this, "Gives all weapons and extra weapons to player v, or spread weapons", AUTHED_ADMIN)
CONSOLE_COMMAND("unallweapons", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnAllWeapons, this, "Takes all weapons and extra weapons from player v", AUTHED_ADMIN)

CONSOLE_COMMAND("extraweapons", "?v[id] ?i[spread]", CFGFLAG_SERVER|CMDFLAG_TEST, ConExtraWeapons, this, "Gives all extra weapons to player v, or spread extra weapons", AUTHED_ADMIN)
CONSOLE_COMMAND("unextraweapons", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnExtraWeapons, this, "Takes all extra weapons from player v", AUTHED_ADMIN)

CONSOLE_COMMAND("plasmarifle", "?v[id] ?i[spread]", CFGFLAG_SERVER|CMDFLAG_TEST, ConPlasmaRifle, this, "Gives a plasma rifle to player v, or spread plasma rifle", AUTHED_ADMIN)
CONSOLE_COMMAND("unplasmarifle", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnPlasmaRifle, this, "Takes the plasma rifle from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("heartgun", "?v[id] ?i[spread]", CFGFLAG_SERVER|CMDFLAG_TEST, ConHeartGun, this, "Gives a heart gun to player v, or spread heart gun", AUTHED_ADMIN)
CONSOLE_COMMAND("unheartgun", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnHeartGun, this, "Takes the heart gun from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("straightgrenade", "?v[id] ?i[spread]", CFGFLAG_SERVER|CMDFLAG_TEST, ConStraightGrenade, this, "Gives a straight grenade to player v, or spread straight grenade", AUTHED_ADMIN)
CONSOLE_COMMAND("unstraightgrenade", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnStraightGrenade, this, "Takes the straight grenade from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("telekinesis", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConTelekinesis, this, "Gives telekinses power to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("untelekinesis", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnTelekinesis, this, "Takes telekinses power from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("lightsaber", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConLightsaber, this, "Gives a lightsaber to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("unlightsaber", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnLightsaber, this, "Takes the lightsaber from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("telerifle", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConTeleRifle, this, "Gives a tele rifle to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("untelerifle", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnTeleRifle, this, "Takes the tele rifle from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("projectilerifle", "?v[id] ?i[spread]", CFGFLAG_SERVER|CMDFLAG_TEST, ConProjectileRifle, this, "Gives a projectile rifle to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("unprojectilerifle", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnProjectileRifle, this, "Takes the projectile rifle from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("ballgrenade", "?v[id] ?i[spread]", CFGFLAG_SERVER|CMDFLAG_TEST, ConBallGrenade, this, "Gives a ball grenade to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("unballgrenade", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnBallGrenade, this, "Takes the ball grenade from player v", AUTHED_ADMIN)

CONSOLE_COMMAND("hammer", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConHammer, this, "Gives a hammer to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("gun", "?v[id] ?i[spread]", CFGFLAG_SERVER|CMDFLAG_TEST, ConGun, this, "Gives a gun to player v, or spread gun", AUTHED_ADMIN)
CONSOLE_COMMAND("unhammer", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnHammer, this, "Takes the hammer from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("ungun", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnGun, this, "Takes the gun from player v", AUTHED_ADMIN)

CONSOLE_COMMAND("scrollninja", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConScrollNinja, this, "Gives a ninja to player v", AUTHED_ADMIN)

//dummy
CONSOLE_COMMAND("connectdummy", "?i[amount] ?i[dummymode]", CFGFLAG_SERVER, ConConnectDummy, this, "Connects i dummies", AUTHED_ADMIN)
CONSOLE_COMMAND("disconnectdummy", "v[id]", CFGFLAG_SERVER, ConDisconnectDummy, this, "Disconnects dummy v", AUTHED_ADMIN)
CONSOLE_COMMAND("dummymode", "?v[id] ?i[dummymode]", CFGFLAG_SERVER, ConDummymode, this, "Sets or shows the dummymode of dummy v", AUTHED_ADMIN)
CONSOLE_COMMAND("connectdefaultdummies", "", CFGFLAG_SERVER, ConConnectDefaultDummies, this, "Connects default dummies", AUTHED_ADMIN)

//power
CONSOLE_COMMAND("forceflagowner", "i[flag] ?i[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConForceFlagOwner, this, "Gives flag i to player i (0 = red, 1 = blue) (to return flag, set id = -1)", AUTHED_ADMIN)
CONSOLE_COMMAND("say_by", "v[id] r[text]", CFGFLAG_SERVER, ConSayBy, this, "Says a chat message as player v", AUTHED_ADMIN)
CONSOLE_COMMAND("teecontrol", "?v[id] ?i[forcedid]", CFGFLAG_SERVER, ConTeeControl, this, "Control another tee", AUTHED_ADMIN)

//fun
CONSOLE_COMMAND("sound", "i[sound]", CFGFLAG_SERVER, ConSound, this, "Plays the sound with id i", AUTHED_ADMIN)

//client information
CONSOLE_COMMAND("player_name", "v[id] ?r[name]", CFGFLAG_SERVER, ConPlayerName, this, "Sets name of player v", AUTHED_ADMIN)
CONSOLE_COMMAND("player_clan", "v[id] ?r[clan]", CFGFLAG_SERVER, ConPlayerClan, this, "Sets clan of player v", AUTHED_ADMIN)
CONSOLE_COMMAND("player_skin", "v[id] ?r[skin]", CFGFLAG_SERVER, ConPlayerSkin, this, "Sets skin of player v", AUTHED_ADMIN)

//info
CONSOLE_COMMAND("playerinfo", "i[id]", CFGFLAG_SERVER, ConPlayerInfo, this, "Shows information about the player with client id i", AUTHED_ADMIN)
CONSOLE_COMMAND("lasertext", "v[id] r[text]", CFGFLAG_SERVER, ConLaserText, this, "Sends a laser text", AUTHED_ADMIN)

//extras
CONSOLE_COMMAND("item", "v[id] i[item]", CFGFLAG_SERVER, ConItem, this, "Gives player v item i (-3=none, -2=heart, -1=armor, 0 and up=weapon id)", AUTHED_ADMIN)
CONSOLE_COMMAND("invisible", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConInvisible, this, "Toggles invisibility for player v", AUTHED_ADMIN)

CONSOLE_COMMAND("hookpower", "?s[power] ?v[id]", CFGFLAG_SERVER, ConHookPower, this, "Sets hook power for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("freezehammer", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConFreezeHammer, this, "Toggles freeze hammer for player v", AUTHED_ADMIN)

CONSOLE_COMMAND("infinitejumps", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConInfiniteJumps, this, "Toggles infinite jumps for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("endlesshook", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConEndlessHook, this, "Toggles endlesshook for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("jetpack", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConJetpack, this, "Toggles jetpack for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("rainbowspeed", "?v[id] ?i[speed]", CFGFLAG_SERVER, ConRainbowSpeed, this, "Sets the rainbow speed i for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("rainbow", "?v[id]", CFGFLAG_SERVER, ConRainbow, this, "Toggles rainbow for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("infrainbow", "?v[id]", CFGFLAG_SERVER, ConInfRainbow, this, "Toggles infinite rainbow for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("atom", "?v[id]", CFGFLAG_SERVER, ConAtom, this, "Toggles atom for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("trail", "?v[id]", CFGFLAG_SERVER, ConTrail, this, "Toggles trail for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("spookyghost", "?v[id]", CFGFLAG_SERVER, ConSpookyGhost, this, "Toggles spooky ghost for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("spooky", "?v[id]", CFGFLAG_SERVER, ConSpooky, this, "Toggles spooky mode for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("addmeteor", "?v[id]", CFGFLAG_SERVER, ConAddMeteor, this, "Adds a meteors to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("addinfmeteor", "?v[id]", CFGFLAG_SERVER, ConAddInfMeteor, this, "Adds an infinite meteors to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("removemeteors", "?v[id]", CFGFLAG_SERVER, ConRemoveMeteors, this, "Removes all meteors from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("passive", "?v[id]", CFGFLAG_SERVER, ConPassive, this, "Toggles passive mode for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("vanillamode", "?v[id]", CFGFLAG_SERVER, ConVanillaMode, this, "Activates vanilla mode for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("ddracemode", "?v[id]", CFGFLAG_SERVER, ConDDraceMode, this, "Deactivates vanilla mode for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("bloody", "?v[id]", CFGFLAG_SERVER, ConBloody, this, "Toggles bloody for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("strongbloody", "?v[id]", CFGFLAG_SERVER, ConStrongBloody, this, "Toggles strong bloody for player v", AUTHED_ADMIN)

CONSOLE_COMMAND("alwaysteleweapon", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConAlwaysTeleWeapon, this, "Lets player v always use tele weapons", AUTHED_ADMIN)
CONSOLE_COMMAND("telegun", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConTeleGun, this, "Gives a tele gun to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("telegrenade", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConTeleGrenade, this, "Gives a tele grenade to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("telelaser", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConTeleLaser, this, "Gives a tele laser to player v", AUTHED_ADMIN)

CONSOLE_COMMAND("doorhammer", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConDoorHammer, this, "Gives a door hammer to player v", AUTHED_ADMIN)

CONSOLE_COMMAND("spinbot", "?v[id]", CFGFLAG_SERVER, ConSpinBot, this, "Gives spinbot to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("spinbotspeed", "?v[id] ?i[speed]", CFGFLAG_SERVER, ConSpinBotSpeed, this, "Sets the spinbot speed i for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("aimclosest", "?v[id]", CFGFLAG_SERVER, ConAimClosest, this, "Makes player v aim to the closest tee", AUTHED_ADMIN)

//account
CONSOLE_COMMAND("acc_logout_port", "i[port]", CFGFLAG_SERVER, ConAccLogoutPort, this, "Logs out all accounts with last port i", AUTHED_ADMIN)
CONSOLE_COMMAND("acc_logout", "s[username]", CFGFLAG_SERVER, ConAccLogout, this, "Logs out account s", AUTHED_ADMIN)
CONSOLE_COMMAND("acc_disable", "s[username]", CFGFLAG_SERVER, ConAccDisable, this, "Enables or disables account s", AUTHED_ADMIN)
CONSOLE_COMMAND("acc_vip", "s[username]", CFGFLAG_SERVER, ConAccVIP, this, "Enables or disables vip for account s", AUTHED_ADMIN)
CONSOLE_COMMAND("acc_info", "s[username]", CFGFLAG_SERVER, ConAccInfo, this, "Shows information about account s", AUTHED_ADMIN)
#undef CONSOLE_COMMAND
