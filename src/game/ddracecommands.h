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
CONSOLE_COMMAND("portalrifle", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConPortalRifle, this, "Gives a portal rifle to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("unportalrifle", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnPortalRifle, this, "Takes the portal rifle from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("projectilerifle", "?v[id] ?i[spread]", CFGFLAG_SERVER|CMDFLAG_TEST, ConProjectileRifle, this, "Gives a projectile rifle to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("unprojectilerifle", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnProjectileRifle, this, "Takes the projectile rifle from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("ballgrenade", "?v[id] ?i[spread]", CFGFLAG_SERVER|CMDFLAG_TEST, ConBallGrenade, this, "Gives a ball grenade to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("unballgrenade", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnBallGrenade, this, "Takes the ball grenade from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("telerifle", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConTeleRifle, this, "Gives a tele rifle to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("untelerifle", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnTeleRifle, this, "Takes the tele rifle from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("taser", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConTaser, this, "Gives a taser to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("untaser", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnTaser, this, "Takes the taser from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("lightninglaser", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConLightningLaser, this, "Gives a lightning laser to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("unlightninglaser", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnLightningLaser, this, "Takes the lightning laser from player v", AUTHED_ADMIN)

CONSOLE_COMMAND("hammer", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConHammer, this, "Gives a hammer to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("gun", "?v[id] ?i[spread]", CFGFLAG_SERVER|CMDFLAG_TEST, ConGun, this, "Gives a gun to player v, or spread gun", AUTHED_ADMIN)
CONSOLE_COMMAND("unhammer", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnHammer, this, "Takes the hammer from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("ungun", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnGun, this, "Takes the gun from player v", AUTHED_ADMIN)

CONSOLE_COMMAND("draweditor", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConDrawEditor, this, "Gives the editor to player v (Laser walls need switch layer in map)", AUTHED_ADMIN)
CONSOLE_COMMAND("undraweditor", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConUnDrawEditor, this, "Takes the editor from player v", AUTHED_ADMIN)

CONSOLE_COMMAND("scrollninja", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConScrollNinja, this, "Gives a ninja to player v", AUTHED_ADMIN)

//dummy
CONSOLE_COMMAND("connectdummy", "?i[amount] ?i[dummymode]", CFGFLAG_SERVER, ConConnectDummy, this, "Connects i dummies", AUTHED_ADMIN)
CONSOLE_COMMAND("disconnectdummy", "v[id]", CFGFLAG_SERVER, ConDisconnectDummy, this, "Disconnects dummy v", AUTHED_ADMIN)
CONSOLE_COMMAND("dummymode", "?v[id] ?i[dummymode]", CFGFLAG_SERVER, ConDummymode, this, "Sets or shows the dummymode of dummy v", AUTHED_ADMIN)
CONSOLE_COMMAND("connectdefaultdummies", "", CFGFLAG_SERVER, ConConnectDefaultDummies, this, "Connects default dummies", AUTHED_ADMIN)

//tune lock player
CONSOLE_COMMAND("tune_lock_pl", "v[id] s[tuning] i[value]", CFGFLAG_SERVER, ConTuneLockPlayer, this, "Tune for lock a variable to value for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("tune_lock_pl_reset", "v[id] ?s[tuning]", CFGFLAG_SERVER, ConTuneLockPlayerReset, this, "Reset all locked tuning variables to defaults for player v (specific or all)", AUTHED_ADMIN)
CONSOLE_COMMAND("tune_lock_pl_dump", "v[id]", CFGFLAG_SERVER, ConTuneLockPlayerDump, this, "Dump lock tuning for player v", AUTHED_ADMIN)

//power
CONSOLE_COMMAND("forceflagowner", "i[flag] ?i[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConForceFlagOwner, this, "Gives flag i to player i (0 = red, 1 = blue) (to return flag, set id = -1)", AUTHED_ADMIN)
CONSOLE_COMMAND("say_by", "v[id] r[text]", CFGFLAG_SERVER, ConSayBy, this, "Says a chat message as player v", AUTHED_ADMIN)
CONSOLE_COMMAND("teecontrol", "?v[id] ?i[forcedid]", CFGFLAG_SERVER, ConTeeControl, this, "Control another tee", AUTHED_ADMIN)
CONSOLE_COMMAND("set_minigame", "v[id] s[minigame]", CFGFLAG_SERVER, ConSetMinigame, this, "Sets player v to minigame s", AUTHED_ADMIN)
CONSOLE_COMMAND("set_no_bonus_area", "?v[id]", CFGFLAG_SERVER, ConSetNoBonusArea, this, "Sets player v no-bonus area state", AUTHED_ADMIN)
CONSOLE_COMMAND("unset_no_bonus_area", "?v[id]", CFGFLAG_SERVER, ConUnsetNoBonusArea, this, "Unsets player v no-bonus area state", AUTHED_ADMIN)
CONSOLE_COMMAND("redirect_port", "i[port] ?v[id]", CFGFLAG_SERVER, ConRedirectPort, this, "Redirects player v to port i", AUTHED_ADMIN)

// hide from view counter
CONSOLE_COMMAND("hide_from_spec_count", "?i['0'|'1']", CFGFLAG_SERVER, ConHideFromSpecCount, this, "Hides yourself from spec counter of others", AUTHED_HELPER)

// savedtees
CONSOLE_COMMAND("save_drop", "?v[id] ?f[hours] ?r[text]", CFGFLAG_SERVER, ConSaveDrop, this, "Saves stats of player v for i hours and kicks him", AUTHED_ADMIN)
CONSOLE_COMMAND("list_saved_tees", "", CFGFLAG_SERVER, ConListSavedTees, this, "List all saved players with basic info", AUTHED_MOD)

// 1vs1 tourna
CONSOLE_COMMAND("1vs1_global_create", "?i[scorelimit] ?i[killborder]", CFGFLAG_SERVER, Con1VS1GlobalCreate, this, "Creates a 1vs1 arena to let other people fight there", AUTHED_ADMIN)
CONSOLE_COMMAND("1vs1_global_start", "i[id] i[id]", CFGFLAG_SERVER, Con1VS1GlobalStart, this, "Puts two players in the previously created 1vs1 arena to fight", AUTHED_ADMIN)

// jail
CONSOLE_COMMAND("jail_arrest", "v[id] i[seconds]", CFGFLAG_SERVER, ConJailArrest, this, "Arrests player v for i minutes", AUTHED_ADMIN)
CONSOLE_COMMAND("jail_release", "v[id]", CFGFLAG_SERVER, ConJailRelease, this, "Releases player v from jail", AUTHED_ADMIN)

// view cursor
CONSOLE_COMMAND("view_cursor", "?i[id]", CFGFLAG_SERVER, ConViewCursor, this, "View cursor of player i (-2 = off, -1 = everyone)", AUTHED_ADMIN)
CONSOLE_COMMAND("view_cursor_zoomed", "?i[id]", CFGFLAG_SERVER, ConViewCursorZoomed, this, "View zoomed cursor of player i (-2 = off, -1 = everyone)", AUTHED_ADMIN)

// whois
CONSOLE_COMMAND("whois", "i[mode] i[cutoff] r[name]", CFGFLAG_SERVER, ConWhoIs, this, "Mode 0=ip, 1=name, cutoff 0=direct, 1=/24, 2=/16", AUTHED_ADMIN)
CONSOLE_COMMAND("whoisid", "i[mode] i[cutoff] v[id]", CFGFLAG_SERVER, ConWhoIsID, this, "Mode 0=ip, 1=name, cutoff 0=direct, 1=/24, 2=/16", AUTHED_ADMIN)

// white list in case iphub.info falsely flagged someone or to whitelist gameserver ips
CONSOLE_COMMAND("whitelist_add", "s[ip] ?s[reason]", CFGFLAG_SERVER, ConWhitelistAdd, this, "Adds address s to whitelist", AUTHED_ADMIN)
CONSOLE_COMMAND("whitelist_remove", "s[ip/index]", CFGFLAG_SERVER, ConWhitelistRemove, this, "Removes address s from whitelist", AUTHED_ADMIN)
CONSOLE_COMMAND("whitelist", "", CFGFLAG_SERVER, ConWhitelist, this, "Shows whitelist for DNSBL/PGSC", AUTHED_ADMIN)

// bot lookup
CONSOLE_COMMAND("bot_lookup", "", CFGFLAG_SERVER, ConBotLookup, this, "Bot lookup list", AUTHED_ADMIN)

// account system ban
CONSOLE_COMMAND("acc_sys_unban", "i[index]", CFGFLAG_SERVER, ConAccSysUnban, this, "Unbans index i from account system bans", AUTHED_ADMIN)
CONSOLE_COMMAND("acc_sys_bans", "", CFGFLAG_SERVER, ConAccSysBans, this, "Shows list for account system bans", AUTHED_ADMIN)

//plots
CONSOLE_COMMAND("toteleplot", "i[plotid] ?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConToTelePlot, this, "Teleports player v to plot i", AUTHED_ADMIN)
CONSOLE_COMMAND("clearplot", "i[plotid]", CFGFLAG_SERVER, ConClearPlot, this, "Clears plot with id i", AUTHED_ADMIN)
CONSOLE_COMMAND("plot_owner", "s[username] i[plotid]", CFGFLAG_SERVER, ConPlotOwner, this, "Sets owner of plot i to account with username s", AUTHED_ADMIN)
CONSOLE_COMMAND("plot_info", "i[plotid]", CFGFLAG_SERVER, ConPlotInfo, this, "Shows information about plot i", AUTHED_ADMIN)
CONSOLE_COMMAND("preset_list", "", CFGFLAG_SERVER, ConPresetList, this, "Shows a list with presets to load from the draw editor", AUTHED_ADMIN)

//designs
CONSOLE_COMMAND("reload_designs", "", CFGFLAG_SERVER, ConReloadDesigns, this, "Reloads map designs", AUTHED_ADMIN)
CONSOLE_COMMAND("reload_languages", "", CFGFLAG_SERVER, ConReloadLanguages, this, "Reloads languages", AUTHED_ADMIN)
CONSOLE_COMMAND("list_loaded_languages", "", CFGFLAG_SERVER, ConListLoadedLanguages, this, "Prints a list of all currently loaded languages and the user amount", AUTHED_ADMIN)

//grog
CONSOLE_COMMAND("add_grog", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConAddGrog, this, "Gives a grog to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("set_permille", "v[id] f[permille]", CFGFLAG_SERVER|CMDFLAG_TEST, ConSetPermille, this, "Sets permille for player v", AUTHED_ADMIN)

//fun
CONSOLE_COMMAND("sound", "i[sound]", CFGFLAG_SERVER, ConSound, this, "Plays the sound with id i", AUTHED_ADMIN)
CONSOLE_COMMAND("lasertext", "v[id] r[text]", CFGFLAG_SERVER, ConLaserText, this, "Sends a laser text", AUTHED_ADMIN)
CONSOLE_COMMAND("sendmotd", "v[id] i[footer] r[text]", CFGFLAG_SERVER, ConSendMotd, this, "Sends a motd containing text r to player v (i=1 with footer)", AUTHED_ADMIN)
CONSOLE_COMMAND("helicopter", "?v[id] ?i[turrettype] ?f[scale]", CFGFLAG_SERVER|CMDFLAG_TEST, ConHelicopter, this, "Spawns helicopter at player v, turret type (i=0, i=1 minigun, i=2 missile), scale (0.8-5.0)", AUTHED_ADMIN)
CONSOLE_COMMAND("remove_helicopters", "", CFGFLAG_SERVER, ConRemoveHelicopters, this, "Removes all helicopters", AUTHED_ADMIN)
CONSOLE_COMMAND("snake", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConSnake, this, "Toggles snake for player v", AUTHED_ADMIN)

//zombie
CONSOLE_COMMAND("force_transform_zombie", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConForceTransformZombie, this, "Forces transformation to zombie for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("force_transform_human", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConForceTransformHuman, this, "Forces transformation to human for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("set_double_xp_lifes", "v[id] i[amount]", CFGFLAG_SERVER|CMDFLAG_TEST, ConSetDoubleXpLifes, this, "Sets double xp lifes for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("set_taser_shield", "v[id] i[percentage]", CFGFLAG_SERVER|CMDFLAG_TEST, ConSetTaserShield, this, "Sets taser shield percentage for player v", AUTHED_ADMIN)

//client information
CONSOLE_COMMAND("player_name", "v[id] ?r[name]", CFGFLAG_SERVER, ConPlayerName, this, "Sets name of player v", AUTHED_ADMIN)
CONSOLE_COMMAND("player_clan", "v[id] ?r[clan]", CFGFLAG_SERVER, ConPlayerClan, this, "Sets clan of player v", AUTHED_ADMIN)
CONSOLE_COMMAND("player_skin", "v[id] ?r[skin]", CFGFLAG_SERVER, ConPlayerSkin, this, "Sets skin of player v", AUTHED_ADMIN)

//info
CONSOLE_COMMAND("playerinfo", "v[id]", CFGFLAG_SERVER, ConPlayerInfo, this, "Shows information about the player with client id v", AUTHED_ADMIN)
CONSOLE_COMMAND("list", "?s[filter]", CFGFLAG_SERVER, ConListRcon, this, "List connected players with optional case-insensitive substring matching filter", AUTHED_HELPER)

//extras
CONSOLE_COMMAND("item", "v[id] i[item]", CFGFLAG_SERVER, ConItem, this, "Gives player v item i (-3=none, -2=heart, -1=armor, 0 and up=weapon id)", AUTHED_ADMIN)
CONSOLE_COMMAND("invisible", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConInvisible, this, "Toggles invisibility for player v", AUTHED_ADMIN)

CONSOLE_COMMAND("hookpower", "?s[power] ?v[id]", CFGFLAG_SERVER, ConHookPower, this, "Sets hook power for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("freezehammer", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConFreezeHammer, this, "Toggles freeze hammer for player v", AUTHED_ADMIN)

CONSOLE_COMMAND("set_jumps", "?v[id] ?i[jumps]", CFGFLAG_SERVER|CMDFLAG_TEST, ConSetJumps, this, "Sets amount of jumps for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("infinitejumps", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConInfiniteJumps, this, "Toggles infinite jumps for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("endlesshook", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConEndlessHook, this, "Toggles endlesshook for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("jetpack", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConJetpack, this, "Toggles jetpack for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("rainbowspeed", "?v[id] ?i[speed]", CFGFLAG_SERVER, ConRainbowSpeed, this, "Sets the rainbow speed i for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("rainbow", "?v[id]", CFGFLAG_SERVER, ConRainbow, this, "Toggles rainbow for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("infrainbow", "?v[id]", CFGFLAG_SERVER, ConInfRainbow, this, "Toggles infinite rainbow for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("atom", "?v[id]", CFGFLAG_SERVER, ConAtom, this, "Toggles atom for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("trail", "?v[id]", CFGFLAG_SERVER, ConTrail, this, "Toggles trail for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("spookyghost", "?v[id]", CFGFLAG_SERVER, ConSpookyGhost, this, "Toggles spooky ghost for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("addmeteor", "?v[id]", CFGFLAG_SERVER, ConAddMeteor, this, "Adds a meteors to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("addinfmeteor", "?v[id]", CFGFLAG_SERVER, ConAddInfMeteor, this, "Adds an infinite meteors to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("removemeteors", "?v[id]", CFGFLAG_SERVER, ConRemoveMeteors, this, "Removes all meteors from player v", AUTHED_ADMIN)
CONSOLE_COMMAND("passive", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConPassive, this, "Toggles passive mode for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("vanillamode", "?v[id]", CFGFLAG_SERVER, ConVanillaMode, this, "Activates vanilla mode for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("ddracemode", "?v[id]", CFGFLAG_SERVER, ConDDraceMode, this, "Deactivates vanilla mode for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("bloody", "?v[id]", CFGFLAG_SERVER, ConBloody, this, "Toggles bloody for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("strongbloody", "?v[id]", CFGFLAG_SERVER, ConStrongBloody, this, "Toggles strong bloody for player v", AUTHED_ADMIN)

CONSOLE_COMMAND("alwaysteleweapon", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConAlwaysTeleWeapon, this, "Lets player v always use tele weapons", AUTHED_ADMIN)
CONSOLE_COMMAND("telegun", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConTeleGun, this, "Gives a tele gun to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("telegrenade", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConTeleGrenade, this, "Gives a tele grenade to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("telelaser", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConTeleLaser, this, "Gives a tele laser to player v", AUTHED_ADMIN)
CONSOLE_COMMAND("doorhammer", "?v[id]", CFGFLAG_SERVER|CMDFLAG_TEST, ConDoorHammer, this, "Gives a door hammer to player v", AUTHED_ADMIN)

CONSOLE_COMMAND("lovely", "?v[id]", CFGFLAG_SERVER, ConLovely, this, "Toggles lovely for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("rotatingball", "?v[id]", CFGFLAG_SERVER, ConRotatingBall, this, "Toggles rotating ball for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("epiccircle", "?v[id]", CFGFLAG_SERVER, ConEpicCircle, this, "Toggles epic circle for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("staffind", "?v[id]", CFGFLAG_SERVER, ConStaffInd, this, "Toggles staff indicator for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("rainbowname", "?v[id]", CFGFLAG_SERVER, ConRainbowName, this, "Toggles rainbow name for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("confetti", "?v[id]", CFGFLAG_SERVER, ConConfetti, this, "Toggles confetti for player v", AUTHED_ADMIN)
CONSOLE_COMMAND("sparkle", "?v[id]", CFGFLAG_SERVER, ConSparkle, this, "Toggles sparkle for player v", AUTHED_ADMIN)

//account
CONSOLE_COMMAND("acc_logout_port", "i[port]", CFGFLAG_SERVER, ConAccLogoutPort, this, "Logs out all accounts with last port i", AUTHED_ADMIN)
CONSOLE_COMMAND("acc_logout", "s[username]", CFGFLAG_SERVER, ConAccLogout, this, "Logs out account s", AUTHED_ADMIN)
CONSOLE_COMMAND("acc_disable", "s[username]", CFGFLAG_SERVER, ConAccDisable, this, "Enables or disables account s", AUTHED_ADMIN)
CONSOLE_COMMAND("acc_info", "s[username]", CFGFLAG_SERVER, ConAccInfo, this, "Shows information about account s", AUTHED_ADMIN)
CONSOLE_COMMAND("acc_add_euros", "s[username] f[amount]", CFGFLAG_SERVER, ConAccAddEuros, this, "Adds i euros to account s", AUTHED_ADMIN)
CONSOLE_COMMAND("acc_edit", "s[username] s[variable] ?r[value]", CFGFLAG_SERVER, ConAccEdit, this, "Prints or changes the value of account variable", AUTHED_ADMIN)
CONSOLE_COMMAND("acc_level_needed_xp", "i[level]", CFGFLAG_SERVER, ConAccLevelNeededXP, this, "Shows how much XP is required to reach level i", AUTHED_ADMIN)
#undef CONSOLE_COMMAND
