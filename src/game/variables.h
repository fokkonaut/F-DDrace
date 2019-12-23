/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_VARIABLES_H
#define GAME_VARIABLES_H
#undef GAME_VARIABLES_H // this file will be included several times


// client
MACRO_CONFIG_INT(ClPredict, cl_predict, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Predict client movements", AUTHED_NO)
MACRO_CONFIG_INT(ClNameplates, cl_nameplates, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show name plates", AUTHED_NO)
MACRO_CONFIG_INT(ClNameplatesAlways, cl_nameplates_always, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Always show name plates disregarding of distance", AUTHED_NO)
MACRO_CONFIG_INT(ClNameplatesTeamcolors, cl_nameplates_teamcolors, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Use team colors for name plates", AUTHED_NO)
MACRO_CONFIG_INT(ClNameplatesSize, cl_nameplates_size, 50, 0, 100, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Size of the name plates from 0 to 100%", AUTHED_NO)
MACRO_CONFIG_INT(ClAutoswitchWeapons, cl_autoswitch_weapons, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Auto switch weapon on pickup", AUTHED_NO)

MACRO_CONFIG_INT(ClShowhud, cl_showhud, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show ingame HUD", AUTHED_NO)
MACRO_CONFIG_INT(ClFilterchat, cl_filterchat, 0, 0, 2, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show chat messages from: 0=all, 1=friends only, 2=no one", AUTHED_NO)
MACRO_CONFIG_INT(ClShowsocial, cl_showsocial, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show social data like names, clans, chat etc.", AUTHED_NO)
MACRO_CONFIG_INT(ClShowfps, cl_showfps, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show ingame FPS counter", AUTHED_NO)

MACRO_CONFIG_INT(ClAirjumpindicator, cl_airjumpindicator, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show double jump indicator", AUTHED_NO)

MACRO_CONFIG_INT(ClWarningTeambalance, cl_warning_teambalance, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Warn about team balance", AUTHED_NO)

MACRO_CONFIG_INT(ClDynamicCamera, cl_dynamic_camera, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Switches camera mode. 0=static camera, 1=dynamic camera", AUTHED_NO)
MACRO_CONFIG_INT(ClMouseDeadzone, cl_mouse_deadzone, 300, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Zone that doesn't trigger the dynamic camera", AUTHED_NO)
MACRO_CONFIG_INT(ClMouseFollowfactor, cl_mouse_followfactor, 60, 0, 200, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Trigger amount for the dynamic camera", AUTHED_NO)
MACRO_CONFIG_INT(ClMouseMaxDistanceDynamic, cl_mouse_max_distance_dynamic, 1000, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Mouse max distance, in dynamic camera mode", AUTHED_NO)
MACRO_CONFIG_INT(ClMouseMaxDistanceStatic, cl_mouse_max_distance_static, 400, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Mouse max distance, in static camera mode", AUTHED_NO)

MACRO_CONFIG_INT(ClCustomizeSkin, cl_customize_skin, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Use a customized skin", AUTHED_NO)

MACRO_CONFIG_INT(ClShowUserId, cl_show_user_id, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show the ID for every user", AUTHED_NO)

MACRO_CONFIG_INT(EdZoomTarget, ed_zoom_target, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Zoom to the current mouse target", AUTHED_NO)
MACRO_CONFIG_INT(EdShowkeys, ed_showkeys, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Editor shows which keys are pressed", AUTHED_NO)
MACRO_CONFIG_INT(EdColorGridInner, ed_color_grid_inner, (int)0xFFFFFF26, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color inner grid", AUTHED_NO)
MACRO_CONFIG_INT(EdColorGridOuter, ed_color_grid_outer, (int)0xFF4C4C4C, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color outer grid", AUTHED_NO)
MACRO_CONFIG_INT(EdColorQuadPoint, ed_color_quad_point, (int)0xFF0000FF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of quad points", AUTHED_NO)
MACRO_CONFIG_INT(EdColorQuadPointHover, ed_color_quad_point_hover, (int)0xFFFFFFFF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of quad points when hovering over with the mouse cursor", AUTHED_NO)
MACRO_CONFIG_INT(EdColorQuadPointActive, ed_color_quad_point_active, (int)0xFFFFFFFF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of active quad points", AUTHED_NO)
MACRO_CONFIG_INT(EdColorQuadPivot, ed_color_quad_pivot, (int)0x00FF00FF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of the quad pivot", AUTHED_NO)
MACRO_CONFIG_INT(EdColorQuadPivotHover, ed_color_quad_pivot_hover, (int)0xFFFFFFFF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of the quad pivot when hovering over with the mouse cursor", AUTHED_NO)
MACRO_CONFIG_INT(EdColorQuadPivotActive, ed_color_quad_pivot_active, (int)0xFFFFFFFF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of the active quad pivot", AUTHED_NO)
MACRO_CONFIG_INT(EdColorSelectionQuad, ed_color_selection_quad, (int)0xFFFFFFFF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of the selection area for a quad", AUTHED_NO)
MACRO_CONFIG_INT(EdColorSelectionTile, ed_color_selection_tile, (int)0xFFFFFF66, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of the selection area for a tile", AUTHED_NO)

//MACRO_CONFIG_INT(ClFlow, cl_flow, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")

// server
MACRO_CONFIG_STR(SvMotd, sv_motd, 900, "", CFGFLAG_SAVE|CFGFLAG_SERVER, "Message of the day to display for the clients", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvScorelimit, sv_scorelimit, 0, 0, 1000, CFGFLAG_SAVE|CFGFLAG_SERVER, "Score limit (0 disables)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvTimelimit, sv_timelimit, 0, 0, 1000, CFGFLAG_SAVE|CFGFLAG_SERVER, "Time limit in minutes (0 disables)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvTournamentMode, sv_tournament_mode, 0, 0, 2, CFGFLAG_SAVE|CFGFLAG_SERVER, "Tournament mode. When enabled, players joins the server as spectator (2=additional restricted spectator chat)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPlayerReadyMode, sv_player_ready_mode, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "When enabled, players can pause/unpause the game and start the game on warmup via their ready state", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSpamprotection, sv_spamprotection, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Spam protection", AUTHED_ADMIN)

MACRO_CONFIG_INT(SvPlayerSlots, sv_player_slots, 64, 0, MAX_PLAYERS, CFGFLAG_SAVE|CFGFLAG_SERVER, "Number of slots to reserve for players", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSkillLevel, sv_skill_level, 2, SERVERINFO_LEVEL_MIN, SERVERINFO_LEVEL_MAX, CFGFLAG_SAVE|CFGFLAG_SERVER, "Supposed player skill level", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvInactiveKickTime, sv_inactivekick_time, 0, 0, 1000, CFGFLAG_SAVE|CFGFLAG_SERVER, "How many minutes to wait before taking care of inactive clients", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvInactiveKick, sv_inactivekick, 2, 1, 3, CFGFLAG_SAVE|CFGFLAG_SERVER, "How to deal with inactive clients (1=move player to spectator, 2=move to free spectator slot/kick, 3=kick)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvInactiveKickSpec, sv_inactivekick_spec, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Kick inactive spectators", AUTHED_ADMIN)

MACRO_CONFIG_INT(SvSilentSpectatorMode, sv_silent_spectator_mode, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Mute join/leave message of spectator", AUTHED_ADMIN)

MACRO_CONFIG_INT(SvStrictSpectateMode, sv_strict_spectate_mode, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Restricts information in spectator mode", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvVoteSpectate, sv_vote_spectate, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Allow voting to move players to spectators", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvVoteSpectateRejoindelay, sv_vote_spectate_rejoindelay, 3, 0, 1000, CFGFLAG_SAVE|CFGFLAG_SERVER, "How many minutes to wait before a player can rejoin after being moved to spectators by vote", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvVoteKick, sv_vote_kick, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Allow voting to kick players", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvVoteKickMin, sv_vote_kick_min, 0, 0, MAX_CLIENTS, CFGFLAG_SAVE|CFGFLAG_SERVER, "Minimum number of players required to start a kick vote", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvVoteKickBantime, sv_vote_kick_bantime, 5, 0, 1440, CFGFLAG_SAVE|CFGFLAG_SERVER, "The time to ban a player if kicked by vote. 0 makes it just use kick", AUTHED_ADMIN)

// debug
#ifdef CONF_DEBUG // this one can crash the server if not used correctly
	MACRO_CONFIG_INT(DbgDummies, dbg_dummies, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "", AUTHED_ADMIN)
#endif

MACRO_CONFIG_INT(DbgFocus, dbg_focus, 0, 0, 1, CFGFLAG_CLIENT, "", AUTHED_ADMIN)
MACRO_CONFIG_INT(DbgTuning, dbg_tuning, 0, 0, 1, CFGFLAG_CLIENT, "", AUTHED_ADMIN)

// F-DDrace

MACRO_CONFIG_INT(SvOldTeleportWeapons, sv_old_teleport_weapons, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Teleporting of all weapons (deprecated, use special entities instead)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvOldTeleportHook, sv_old_teleport_hook, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Hook through teleporter (deprecated, use special entities instead)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDeepfly, sv_deepfly, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Allow fire non auto weapons when deep", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDestroyBulletsOnDeath, sv_destroy_bullets_on_death, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Destroy bullets when their owner dies", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDestroyLasersOnDeath, sv_destroy_lasers_on_death, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Destroy lasers when their owner dies", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvTeleportHoldHook, sv_teleport_hold_hook, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Hold hook when teleported", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvTeleportLoseWeapons, sv_teleport_lose_weapons, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Lose weapons when teleported (useful for some race maps)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvRescue, sv_rescue, 0, 0, 1, CFGFLAG_SERVER, "Allow /rescue command so players can teleport themselves out of freeze", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvRescueDelay, sv_rescue_delay, 5, 0, 1000, CFGFLAG_SERVER, "Number of seconds between two rescues", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvJoinVoteDelay, sv_join_vote_delay, 300, 0, 1000, CFGFLAG_SERVER, "Add a delay before recently joined players can call any vote or participate in a kick/spec vote (in seconds)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSpectatorSlots, sv_spectator_slots, 0, 0, 64, CFGFLAG_SERVER, "Number of slots to reserve for spectators", AUTHED_ADMIN)

// F-DDrace

// account
MACRO_CONFIG_INT(SvAccounts, sv_accounts, 0, 0, 1, CFGFLAG_SERVER, "Whether accounts are activated or deactivated", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvAccFilePath, sv_acc_file_path, 128, "data/accounts", CFGFLAG_SERVER, "The path were the server searches the .acc files", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvAccSaveInterval, sv_acc_save_interval, 30, 5, 60, CFGFLAG_SERVER, "Intervall in minutes between account saves", AUTHED_ADMIN)

// flags
MACRO_CONFIG_INT(SvFlagSounds, sv_flag_sounds, 0, 0, 1, CFGFLAG_SERVER, "Whether flags create a public sound on drop/pickup/respawn", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvFlagHooking, sv_flag_hooking, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether flags are hookable", AUTHED_ADMIN)

// dummy
MACRO_CONFIG_INT(SvHideDummies, sv_hide_dummies, 1, 0, 1, CFGFLAG_SERVER, "Whether to hide server-side dummies from scoreboard", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDefaultDummies, sv_default_dummies, 0, 0, 1, CFGFLAG_SERVER, "Whether to create default dummies for specific maps when the server starts", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvFakeDummyPing, sv_fake_dummy_ping, 0, 0, 1, CFGFLAG_SERVER, "Whether ping of server-side dummies are more natural or 1000", AUTHED_ADMIN)
MACRO_CONFIG_INT(V3OffsetX, v3_offset_x, -1, 0, 9999, CFGFLAG_SERVER|CFGFLAG_GAME, "Offset X for the blmapV3 dummy", AUTHED_ADMIN)
MACRO_CONFIG_INT(V3OffsetY, v3_offset_y, -1, 0, 9999, CFGFLAG_SERVER|CFGFLAG_GAME, "Offset Y for the blmapV3 dummy", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDummyBotSkin, sv_dummy_bot_skin, 1, 0, 1, CFGFLAG_SERVER, "Whether dummies should have the bot skin applied", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDummyBlocking, sv_dummy_blocking, 0, 0, 1, CFGFLAG_SERVER, "Whether blocking dummies increases killstreak and gives block points", AUTHED_ADMIN)

// weapon indicator
MACRO_CONFIG_INT(SvWeaponIndicatorDefault, sv_weapon_indicator_default, 0, 0, 1, CFGFLAG_SERVER, "Whether the weapon names are displayed under the health and armor bars", AUTHED_ADMIN)

// drops
MACRO_CONFIG_INT(SvDropWeapons, sv_drop_weapons, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether to allow dropping weapons with f4", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDropsOnDeath, sv_drops_on_death, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether there is a chance of dropping weapons on death (health and armor in survival)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDestroyDropsOnLeave, sv_destroy_drops_on_leave, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Destroy dropped weapons (hearts, shields) when their owner disconnects", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvMaxWeaponDrops, sv_max_weapon_drops, 5, 0, 10, CFGFLAG_SERVER, "Maximum amount of dropped weapons per player", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvMaxPickupDrops, sv_max_pickup_drops, 500, 0, 600, CFGFLAG_SERVER, "Maximum amount of dropped hearts and shields", AUTHED_ADMIN)

// spread weapons
MACRO_CONFIG_INT(SvNumSpreadShots, sv_num_spread_shots, 3, 2, 9, CFGFLAG_SERVER, "Number of shots for the spread weapons", AUTHED_ADMIN)

// vanilla
MACRO_CONFIG_INT(SvVanillaModeStart, sv_vanilla_mode_start, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether to set the players mode to vanilla on spawn or ddrace", AUTHED_ADMIN)

//survival
MACRO_CONFIG_INT(SvSurvivalMinPlayers, sv_survival_min_players, 4, 2, MAX_CLIENTS, CFGFLAG_SERVER|CFGFLAG_GAME, "Minimum players to start a survival round", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSurvivalLobbyCountdown, sv_survival_lobby_countdown, 30, 5, 120, CFGFLAG_SERVER|CFGFLAG_GAME, "Number in seconds until the survival round starts", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSurvivalRoundTime, sv_survival_round_time, 5, 1, 20, CFGFLAG_SERVER|CFGFLAG_GAME, "Time in minutes until deathmatch starts", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSurvivalDeathmatchTime, sv_survival_deathmatch_time, 2, 1, 5, CFGFLAG_SERVER|CFGFLAG_GAME, "Length of the deathmatch in minutes", AUTHED_ADMIN)

// other
MACRO_CONFIG_INT(SvHideMinigamePlayers, sv_hide_minigame_players, 0, 0, 1, CFGFLAG_SERVER, "Whether players in different minigames are shown in the scoreboard", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvRainbowSpeedDefault, sv_rainbow_speed_default, 1, 1, 50, CFGFLAG_SERVER, "Whether players in different minigames are shown in the scoreboard", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDefaultScoreMode, sv_default_score_mode, 0, 0, 1, CFGFLAG_SERVER, "Default score (0 = time, 1 = level, 2 = blockpoints)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSmoothFreeze, sv_smooth_freeze, 0, 0, 1, CFGFLAG_SERVER, "Fix buggy tee when moving in freeze", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvOldJetpackSound, sv_old_jetpack_sound, 1, 0, 1, CFGFLAG_SERVER, "Whether to use the default gun sound for jetpack or another sound", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvContactDiscord, sv_contact_discord, 128, "", CFGFLAG_SERVER, "Name of the admin on Discord (Use \" at start and end, # will break otherwise))", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvBlockPointsDelay, sv_block_points_delay, 20, 0, 600, CFGFLAG_SERVER|CFGFLAG_GAME, "Seconds a tee has to be alive in order to give block points to the killer", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvAlwaysTeleWeapon, sv_always_tele_weapon, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether tele weapons can be used on any block or only on marked ones", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvTeleRifleAllowBlocks, sv_tele_rifle_allow_blocks, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether you can teleport inside of blocks using tele rifle", AUTHED_ADMIN)
#endif
