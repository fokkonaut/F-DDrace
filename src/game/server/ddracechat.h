/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */

// This file can be included several times.

#ifndef CHAT_COMMAND
#define CHAT_COMMAND(name, params, flags, callback, userdata, help)
#endif

CHAT_COMMAND("credits", "", CFGFLAG_CHAT, ConCredits, this, "Shows the credits of the F-DDrace mod")
CHAT_COMMAND("emote", "?s[emote name] i[duration in seconds]", CFGFLAG_CHAT, ConEyeEmote, this, "Sets your tee's eye emote")
CHAT_COMMAND("settings", "?s[configname]", CFGFLAG_CHAT, ConSettings, this, "Shows gameplay information for this server")
CHAT_COMMAND("help", "?r[command]", CFGFLAG_CHAT, ConHelp, this, "Shows help to command r, general help if left blank")
CHAT_COMMAND("info", "", CFGFLAG_CHAT, ConInfo, this, "Shows info about this server")
CHAT_COMMAND("me", "r[message]", CFGFLAG_CHAT, ConMe, this, "Like the famous irc command '/me says hi' will display '<yourname> says hi'")
CHAT_COMMAND("pause", "?r[player name]", CFGFLAG_CHAT, ConTogglePause, this, "Toggles pause")
CHAT_COMMAND("spec", "?r[player name]", CFGFLAG_CHAT, ConToggleSpec, this, "Toggles spec (if not available behaves as /pause)")
CHAT_COMMAND("pausevoted", "", CFGFLAG_CHAT, ConTogglePauseVoted, this, "Toggles pause on the currently voted player")
CHAT_COMMAND("specvoted", "", CFGFLAG_CHAT, ConToggleSpecVoted, this, "Toggles spec on the currently voted player")
CHAT_COMMAND("mapinfo", "?r[map]", CFGFLAG_CHAT, ConMapInfo, this, "Show info about the map with name r gives (current map by default)")
CHAT_COMMAND("rank", "?r[player name]", CFGFLAG_CHAT, ConRank, this, "Shows the rank of player with name r (your rank by default)")
CHAT_COMMAND("top5", "?i[rank to start with]", CFGFLAG_CHAT, ConTop5, this, "Shows five ranks of the ladder beginning with rank i (1 by default)")
CHAT_COMMAND("points", "?r[player name]", CFGFLAG_CHAT, ConPoints, this, "Shows the global points of a player beginning with name r (your rank by default)")
CHAT_COMMAND("top5points", "?i[number]", CFGFLAG_CHAT, ConTopPoints, this, "Shows five points of the global point ladder beginning with rank i (1 by default)")

CHAT_COMMAND("team", "?i[id]", CFGFLAG_CHAT, ConJoinTeam, this, "Lets you join team i (shows your team if left blank)")
CHAT_COMMAND("lock", "?i['0'|'1']", CFGFLAG_CHAT, ConLockTeam, this, "Lock team so no-one else can join it")
CHAT_COMMAND("invite", "r[player name]", CFGFLAG_CHAT, ConInviteTeam, this, "Invite a person to a locked team")

CHAT_COMMAND("showothers", "?i['0'|'1']", CFGFLAG_CHAT, ConShowOthers, this, "Whether to show players from other teams or not (off by default), optional i = 0 for off else for on")
CHAT_COMMAND("showall", "?i['0'|'1']", CFGFLAG_CHAT, ConShowAll, this, "Whether to show players at any distance (off by default), optional i = 0 for off else for on")
CHAT_COMMAND("specteam", "?i['0'|'1']", CFGFLAG_CHAT, ConSpecTeam, this, "Whether to show players from other teams when spectating (on by default), optional i = 0 for off else for on")
CHAT_COMMAND("ninjajetpack", "?i['0'|'1']", CFGFLAG_CHAT, ConNinjaJetpack, this, "Whether to use ninja jetpack or not. Makes jetpack look more awesome")
CHAT_COMMAND("saytime", "?r[player name]", CFGFLAG_CHAT, ConSayTime, this, "Privately messages someone's current time in this current running race (your time by default)")
CHAT_COMMAND("saytimeall", "", CFGFLAG_CHAT, ConSayTimeAll, this, "Publicly messages everyone your current time in this current running race")
CHAT_COMMAND("time", "", CFGFLAG_CHAT, ConTime, this, "Privately shows you your current time in this current running race in the broadcast message")
CHAT_COMMAND("r", "", CFGFLAG_CHAT, ConRescue, this, "Teleport yourself out of freeze (use sv_rescue 1 to enable this feature)")
CHAT_COMMAND("rescue", "", CFGFLAG_CHAT, ConRescue, this, "Teleport yourself out of freeze (use sv_rescue 1 to enable this feature)")

CHAT_COMMAND("kill", "", CFGFLAG_CHAT, ConProtectedKill, this, "Kill yourself")

// F-DDrace

//score
CHAT_COMMAND("score", "?s", CFGFLAG_CHAT, ConScore, this, "Changes the displayed score in scoreboard")

//stats
CHAT_COMMAND("stats", "?r", CFGFLAG_CHAT, ConStats, this, "Shows stats of player r")

//info
CHAT_COMMAND("spookyghost", "", CFGFLAG_CHAT, ConSpookyGhostInfo, this, "Shows information about the spooky ghost")
CHAT_COMMAND("policeinfo", "?i", CFGFLAG_CHAT, ConPoliceInfo, this, "Shows information about police")
CHAT_COMMAND("vip", "", CFGFLAG_CHAT, ConVIPInfo, this, "Shows information about VIP")

//account
CHAT_COMMAND("register", "s[name] s[password] s[password]", CFGFLAG_CHAT, ConRegister, this, "Register an account")
CHAT_COMMAND("login", "s[name] s[password]", CFGFLAG_CHAT, ConLogin, this, "Log into an account")
CHAT_COMMAND("logout", "", CFGFLAG_CHAT, ConLogout, this, "Log out of an account")
CHAT_COMMAND("changepassword", "s[old-pw] s[new-pw] s[new-pw]", CFGFLAG_CHAT, ConChangePassword, this, "Changes account password")

CHAT_COMMAND("pay", "i[amount] r[name]", CFGFLAG_CHAT, ConPayMoney, this, "Pays i money to player r")

//extras
CHAT_COMMAND("weaponindicator", "", CFGFLAG_CHAT, ConWeaponIndicator, this, "Tells you which weapon you are holding under the heart and armor bar")

//other
CHAT_COMMAND("resumemoved", "?i['0'|'1']", CFGFLAG_CHAT, ConResumeMoved, this, "Whether to resume from pause when someone moved your tee (off by default), optional i = 0 for off else for on")

//minigames
CHAT_COMMAND("minigames", "", CFGFLAG_CHAT, ConMinigames, this, "Shows a list of all available minigames")
CHAT_COMMAND("leave", "", CFGFLAG_CHAT, ConLeaveMinigame, this, "Leaves the current minigame")
CHAT_COMMAND("block", "?s[enable/disable]", CFGFLAG_CHAT, ConJoinBlock, this, "Joins the block minigame")
CHAT_COMMAND("survival", "?s[enable/disable]", CFGFLAG_CHAT, ConJoinSurvival, this, "Joins the survival minigame")
CHAT_COMMAND("boomfng", "?s[enable/disable]", CFGFLAG_CHAT, ConJoinBoomFNG, this, "Joins the boom fng minigame")
CHAT_COMMAND("fng", "?s[enable/disable]", CFGFLAG_CHAT, ConJoinFNG, this, "Joins the fng minigame")
#undef CHAT_COMMAND
