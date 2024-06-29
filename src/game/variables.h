/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_VARIABLES_H
#define GAME_VARIABLES_H
#undef GAME_VARIABLES_H // this file will be included several times


// client
MACRO_CONFIG_INT(ClPredict, cl_predict, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Use prediction for objects in the game world", AUTHED_NO)
MACRO_CONFIG_INT(ClPredictPlayers, cl_predict_players, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Predict movements of other players", AUTHED_NO)
MACRO_CONFIG_INT(ClPredictProjectiles, cl_predict_projectiles, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Predict position of projectiles", AUTHED_NO)
MACRO_CONFIG_INT(ClNameplates, cl_nameplates, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show name plates", AUTHED_NO)
MACRO_CONFIG_INT(ClNameplatesAlways, cl_nameplates_always, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Always show name plates disregarding of distance", AUTHED_NO)
MACRO_CONFIG_INT(ClNameplatesTeamcolors, cl_nameplates_teamcolors, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Use team colors for name plates", AUTHED_NO)
MACRO_CONFIG_INT(ClNameplatesSize, cl_nameplates_size, 50, 0, 100, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Size of the name plates from 0 to 100%", AUTHED_NO)
MACRO_CONFIG_INT(ClAutoswitchWeapons, cl_autoswitch_weapons, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Auto switch weapon on pickup", AUTHED_NO)

MACRO_CONFIG_INT(ClShowhud, cl_showhud, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show ingame HUD", AUTHED_NO)
MACRO_CONFIG_INT(ClShowChat, cl_showchat, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show chat", AUTHED_NO)
MACRO_CONFIG_INT(ClFilterchat, cl_filterchat, 0, 0, 2, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show chat messages from: 0=all, 1=friends only, 2=no one", AUTHED_NO)
MACRO_CONFIG_INT(ClDisableWhisper, cl_disable_whisper, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Disable completely the whisper feature.", AUTHED_NO)
MACRO_CONFIG_INT(ClShowsocial, cl_showsocial, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show social data like names, clans, chat etc.", AUTHED_NO)
MACRO_CONFIG_INT(ClShowfps, cl_showfps, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show ingame FPS counter", AUTHED_NO)

MACRO_CONFIG_INT(ClAirjumpindicator, cl_airjumpindicator, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show double jump indicator", AUTHED_NO)

MACRO_CONFIG_INT(ClWarningTeambalance, cl_warning_teambalance, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Warn about team balance", AUTHED_NO)

MACRO_CONFIG_INT(ClDynamicCamera, cl_dynamic_camera, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Switches camera mode. 0=static camera, 1=dynamic camera", AUTHED_NO)
MACRO_CONFIG_INT(ClMouseDeadzone, cl_mouse_deadzone, 300, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Zone that doesn't trigger the dynamic camera", AUTHED_NO)
MACRO_CONFIG_INT(ClMouseFollowfactor, cl_mouse_followfactor, 60, 0, 200, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Trigger amount for the dynamic camera", AUTHED_NO)
MACRO_CONFIG_INT(ClMouseMaxDistanceDynamic, cl_mouse_max_distance_dynamic, 1000, 1, 2000, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Mouse max distance, in dynamic camera mode", AUTHED_NO)
MACRO_CONFIG_INT(ClMouseMaxDistanceStatic, cl_mouse_max_distance_static, 400, 1, 2000, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Mouse max distance, in static camera mode", AUTHED_NO)

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

MACRO_CONFIG_INT(ClShowWelcome, cl_show_welcome, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show initial set-up dialog", AUTHED_NO)
MACRO_CONFIG_INT(ClMotdTime, cl_motd_time, 10, 0, 100, CFGFLAG_CLIENT|CFGFLAG_SAVE, "How long to show the server message of the day", AUTHED_NO)
MACRO_CONFIG_INT(ClShowXmasHats, cl_show_xmas_hats, 1, 0, 2, CFGFLAG_CLIENT|CFGFLAG_SAVE, "0=never, 1=during christmas, 2=always", AUTHED_NO)
MACRO_CONFIG_INT(ClShowEasterEggs, cl_show_easter_eggs, 1, 0, 2, CFGFLAG_CLIENT|CFGFLAG_SAVE, "0=never, 1=during easter, 2=always", AUTHED_NO)

MACRO_CONFIG_STR(ClVersionServer, cl_version_server, 100, "version.teeworlds.com", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Server to use to check for new versions", AUTHED_NO)

MACRO_CONFIG_STR(ClFontfile, cl_fontfile, 255, "DejaVuSans.ttf", CFGFLAG_CLIENT|CFGFLAG_SAVE, "What font file to use", AUTHED_NO)
MACRO_CONFIG_STR(ClLanguagefile, cl_languagefile, 255, "", CFGFLAG_CLIENT|CFGFLAG_SAVE, "What language file to use", AUTHED_NO)

MACRO_CONFIG_INT(PlayerColorBody, player_color_body, (int)0x1B6F74, 0, (int)0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player body color", AUTHED_NO)
MACRO_CONFIG_INT(PlayerColorMarking, player_color_marking, (int)0xFF0000FF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player marking color", AUTHED_NO)
MACRO_CONFIG_INT(PlayerColorDecoration, player_color_decoration, (int)0x1B6F74, 0, (int)0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player decoration color", AUTHED_NO)
MACRO_CONFIG_INT(PlayerColorHands, player_color_hands, (int)0x1B759E, 0, (int)0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player hands color", AUTHED_NO)
MACRO_CONFIG_INT(PlayerColorFeet, player_color_feet, (int)0x1C873E, 0, (int)0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player feet color", AUTHED_NO)
MACRO_CONFIG_INT(PlayerColorEyes, player_color_eyes, (int)0x0000FF, 0, (int)0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player eyes color", AUTHED_NO)
MACRO_CONFIG_INT(PlayerUseCustomColorBody, player_use_custom_color_body, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toggles usage of custom colors for body", AUTHED_NO)
MACRO_CONFIG_INT(PlayerUseCustomColorMarking, player_use_custom_color_marking, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toggles usage of custom colors for marking", AUTHED_NO)
MACRO_CONFIG_INT(PlayerUseCustomColorDecoration, player_use_custom_color_decoration, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toggles usage of custom colors for decoration", AUTHED_NO)
MACRO_CONFIG_INT(PlayerUseCustomColorHands, player_use_custom_color_hands, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toggles usage of custom colors for hands", AUTHED_NO)
MACRO_CONFIG_INT(PlayerUseCustomColorFeet, player_use_custom_color_feet, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toggles usage of custom colors for feet", AUTHED_NO)
MACRO_CONFIG_INT(PlayerUseCustomColorEyes, player_use_custom_color_eyes, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toggles usage of custom colors for eyes", AUTHED_NO)
MACRO_CONFIG_UTF8STR(PlayerSkin, player_skin, MAX_SKIN_ARRAY_SIZE, MAX_SKIN_LENGTH, "default", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player skin", AUTHED_NO)
MACRO_CONFIG_UTF8STR(PlayerSkinBody, player_skin_body, MAX_SKIN_ARRAY_SIZE, MAX_SKIN_LENGTH, "standard", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player skin body", AUTHED_NO)
MACRO_CONFIG_UTF8STR(PlayerSkinMarking, player_skin_marking, MAX_SKIN_ARRAY_SIZE, MAX_SKIN_LENGTH, "", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player skin marking", AUTHED_NO)
MACRO_CONFIG_UTF8STR(PlayerSkinDecoration, player_skin_decoration, MAX_SKIN_ARRAY_SIZE, MAX_SKIN_LENGTH, "", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player skin decoration", AUTHED_NO)
MACRO_CONFIG_UTF8STR(PlayerSkinHands, player_skin_hands, MAX_SKIN_ARRAY_SIZE, MAX_SKIN_LENGTH, "standard", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player skin hands", AUTHED_NO)
MACRO_CONFIG_UTF8STR(PlayerSkinFeet, player_skin_feet, MAX_SKIN_ARRAY_SIZE, MAX_SKIN_LENGTH, "standard", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player skin feet", AUTHED_NO)
MACRO_CONFIG_UTF8STR(PlayerSkinEyes, player_skin_eyes, MAX_SKIN_ARRAY_SIZE, MAX_SKIN_LENGTH, "standard", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player skin eyes", AUTHED_NO)

MACRO_CONFIG_INT(UiBrowserPage, ui_browser_page, 5, 5, 8, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface serverbrowser page", AUTHED_NO)
MACRO_CONFIG_INT(UiSettingsPage, ui_settings_page, 0, 0, 5, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface settings page", AUTHED_NO)
MACRO_CONFIG_STR(UiServerAddress, ui_server_address, 64, "localhost:8303", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface server address (Internet page)", AUTHED_NO)
MACRO_CONFIG_STR(UiServerAddressLan, ui_server_address_lan, 64, "localhost:8303", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface server address (LAN page)", AUTHED_NO)
MACRO_CONFIG_INT(UiMousesens, ui_mousesens, 100, 1, 100000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Mouse sensitivity for menus/editor", AUTHED_NO)
MACRO_CONFIG_INT(UiJoystickSens, ui_joystick_sens, 100, 1, 100000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Joystick sensitivity for menus/editor", AUTHED_NO)
MACRO_CONFIG_INT(UiAutoswitchInfotab, ui_autoswitch_infotab, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Switch to the info tab when clicking on a server", AUTHED_NO)
MACRO_CONFIG_INT(UiWideview, ui_wideview, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Extended menus GUI", AUTHED_NO)

MACRO_CONFIG_INT(GfxNoclip, gfx_noclip, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Disable clipping", AUTHED_NO)

MACRO_CONFIG_STR(ClMenuMap, cl_menu_map, 64, "auto", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Background map in the menu, auto = automatic based on season", AUTHED_NO)
MACRO_CONFIG_INT(ClShowMenuMap, cl_show_menu_map, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Display background map in the menu", AUTHED_NO)
MACRO_CONFIG_INT(ClMenuAlpha, cl_menu_alpha, 25, 0, 75, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Transparency of the menu background", AUTHED_NO)
MACRO_CONFIG_INT(ClRotationRadius, cl_rotation_radius, 30, 1, 500, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Menu camera rotation radius", AUTHED_NO)
MACRO_CONFIG_INT(ClRotationSpeed, cl_rotation_speed, 40, 1, 120, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Menu camera rotations in seconds", AUTHED_NO)
MACRO_CONFIG_INT(ClCameraSpeed, cl_camera_speed, 5, 1, 10, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Menu camera speed", AUTHED_NO)

MACRO_CONFIG_INT(ClShowStartMenuImages, cl_show_start_menu_images, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show start menu images", AUTHED_NO)
MACRO_CONFIG_INT(ClSkipStartMenu, cl_skip_start_menu, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Skip the start menu", AUTHED_NO)

MACRO_CONFIG_INT(ClStatboardInfos, cl_statboard_infos, 1259, 1, 2047, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Mask of infos to display on the global statboard", AUTHED_NO)

MACRO_CONFIG_INT(ClLastVersionPlayed, cl_last_version_played, PREV_CLIENT_VERSION, 0, 0xFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Last version of the game that was played", AUTHED_NO)

// server
MACRO_CONFIG_STR(SvMotd, sv_motd, 900, "", CFGFLAG_SAVE|CFGFLAG_SERVER, "Message of the day to display for the clients", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvScorelimit, sv_scorelimit, 0, 0, 1000, CFGFLAG_SAVE|CFGFLAG_SERVER, "Score limit (0 disables)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvTimelimit, sv_timelimit, 0, 0, 1000, CFGFLAG_SAVE|CFGFLAG_SERVER, "Time limit in minutes (0 disables)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvTournamentMode, sv_tournament_mode, 0, 0, 2, CFGFLAG_SAVE|CFGFLAG_SERVER, "Tournament mode. When enabled, players joins the server as spectator (2=additional restricted spectator chat)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPlayerReadyMode, sv_player_ready_mode, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "When enabled, players can pause/unpause the game and start the game on warmup via their ready state", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSpamprotection, sv_spamprotection, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Spam protection", AUTHED_ADMIN)

MACRO_CONFIG_INT(SvPlayerSlots, sv_player_slots, 128, 0, MAX_PLAYERS, CFGFLAG_SAVE|CFGFLAG_SERVER, "Number of slots to reserve for players", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSkillLevel, sv_skill_level, 2, SERVERINFO_LEVEL_MIN, SERVERINFO_LEVEL_MAX, CFGFLAG_SAVE|CFGFLAG_SERVER, "Supposed player skill level", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvInactiveKickTime, sv_inactivekick_time, 0, 0, 1000, CFGFLAG_SAVE|CFGFLAG_SERVER, "How many minutes to wait before taking care of inactive clients", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvInactiveKick, sv_inactivekick, 2, 1, 3, CFGFLAG_SAVE|CFGFLAG_SERVER, "How to deal with inactive clients (1=move player to spectator, 2=move to free spectator slot/kick, 3=kick)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvInactiveKickSpec, sv_inactivekick_spec, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Kick inactive spectators", AUTHED_ADMIN)

MACRO_CONFIG_INT(SvSilentSpectatorMode, sv_silent_spectator_mode, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Mute join/leave message of spectator", AUTHED_ADMIN)

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

// DDrace

MACRO_CONFIG_INT(SvOldTeleportWeapons, sv_old_teleport_weapons, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Teleporting of all weapons (deprecated, use special entities instead)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvOldTeleportHook, sv_old_teleport_hook, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Hook through teleporter (deprecated, use special entities instead)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDeepfly, sv_deepfly, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Allow fire non auto weapons when deep", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDestroyBulletsOnDeath, sv_destroy_bullets_on_death, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Destroy bullets when their owner dies", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDestroyLasersOnDeath, sv_destroy_lasers_on_death, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Destroy lasers when their owner dies", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvTeleportHoldHook, sv_teleport_hold_hook, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Hold hook when teleported", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvTeleportLoseWeapons, sv_teleport_lose_weapons, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Lose weapons when teleported (useful for some race maps)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvRescue, sv_rescue, 0, 0, 1, CFGFLAG_SERVER, "Allow /rescue command so players can teleport themselves out of freeze", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvRescueDelay, sv_rescue_delay, 1, 0, 1000, CFGFLAG_SERVER, "Number of seconds between two rescues", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPractice, sv_practice, 0, 0, 1, CFGFLAG_SERVER, "Enable practice mode for teams. Means you can use /rescue, but in turn your rank doesn't count.", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvJoinVoteDelay, sv_join_vote_delay, 300, 0, 1000, CFGFLAG_SERVER, "Add a delay before recently joined players can call any vote or participate in a kick/spec vote (in seconds)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvVotesPerTick, sv_votes_per_tick, 5, 1, 15, CFGFLAG_SERVER, "Number of vote options being sent per tick", AUTHED_ADMIN)

// F-DDrace

// account
MACRO_CONFIG_INT(SvAccounts, sv_accounts, 0, 0, 1, CFGFLAG_SERVER, "Whether accounts are activated or deactivated", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvAccFilePath, sv_acc_file_path, 128, "data/accounts", CFGFLAG_SERVER, "The path where the server searches the account files (relative to binary)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDataSaveInterval, sv_data_save_interval, 30, 5, 60, CFGFLAG_SERVER, "Intervall in minutes between data saves", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvDonationFilePath, sv_donation_file_path, 128, "data", CFGFLAG_SERVER, "The path where the server searches the for the donation file (relative to binary)", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvPlotFilePath, sv_plot_file_path, 128, "data/plots", CFGFLAG_SERVER, "The path where the server searches the plot files (relative to binary)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvKillLogout, sv_kill_logout, 0, 0, 60, CFGFLAG_SERVER, "Time in seconds a tee can kill after trying to logout (0 = disabled)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvEuroMode, sv_euro_mode, 0, 0, 1, CFGFLAG_SERVER, "Whether euro mode is enabled", AUTHED_ADMIN)

MACRO_CONFIG_STR(SvExpMsgColorText, sv_exp_msg_color_text, 4, "999", CFGFLAG_SERVER|CFGFLAG_GAME, "Text color for the experience broadcast", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvExpMsgColorSymbol, sv_exp_msg_color_symbol, 4, "999", CFGFLAG_SERVER|CFGFLAG_GAME, "Symbol color for the experience broadcast", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvExpMsgColorValue, sv_exp_msg_color_value, 4, "595", CFGFLAG_SERVER|CFGFLAG_GAME, "Value color for the experience broadcast", AUTHED_ADMIN)

MACRO_CONFIG_STR(SvMoneyDropsFilePath, sv_money_drops_file_path, 128, "data/money_drops", CFGFLAG_SERVER, "The path where the server searches the money drops file (relative to binary)", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvMoneyHistoryFilePath, sv_money_history_file_path, 128, "money_history", CFGFLAG_SAVE|CFGFLAG_SERVER, "The path to money history files (relative to dumps dir)", AUTHED_ADMIN)

// account system ban
MACRO_CONFIG_INT(SvAccSysBanRegistrations, sv_acc_sys_ban_registrations, 3, 0, 10, CFGFLAG_SERVER, "Max registrations per IP within 6 hours", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvAccSysBanPwFails, sv_acc_sys_ban_pw_fails, 5, 0, 10, CFGFLAG_SERVER, "Max passwords fails per IP within 6 hours", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvAccSysBanPinFails, sv_acc_sys_ban_pin_fails, 3, 0, 10, CFGFLAG_SERVER, "Max passwords fails per IP within 6 hours", AUTHED_ADMIN)

// saved tees
MACRO_CONFIG_INT(SvShutdownSaveTees, sv_shutdown_save_tees, 0, 0, 1, CFGFLAG_SERVER, "Whether to save characters before shutdown/reload to load them again", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvSavedTeesFilePath, sv_saved_tees_file_path, 128, "savedtees", CFGFLAG_SAVE|CFGFLAG_SERVER, "The path to saved tees files (relative to dumps dir)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvShutdownSaveTeeExpire, sv_shutdown_save_tee_expire, 1, 1, 24*7, CFGFLAG_SERVER, "How many hours until a shutdown save expires", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvJailSaveTeeExpire, sv_jail_save_tee_expire, 24, 1, 24*7, CFGFLAG_SERVER, "How many hours until a jail save expires", AUTHED_ADMIN)

// flags
MACRO_CONFIG_INT(SvFlagSounds, sv_flag_sounds, 2, 0, 2, CFGFLAG_SERVER, "Flag sounds on drop/pickup/respawn (0=off, 1=public sounds, 2=respawn public rest local)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvFlagHooking, sv_flag_hooking, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether flags are hookable", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvFlagRespawnDropped, sv_flag_respawn_dropped, 90, 0, 9999, CFGFLAG_SERVER|CFGFLAG_GAME, "Time in seconds a dropped flag resets", AUTHED_ADMIN)

// dummy
MACRO_CONFIG_INT(SvHideDummies, sv_hide_dummies, 1, 0, 1, CFGFLAG_SERVER, "Whether to hide server-side dummies from scoreboard", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDefaultDummies, sv_default_dummies, 0, 0, 1, CFGFLAG_SERVER, "Whether to create default dummies for specific maps when the server starts", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvFakeDummyPing, sv_fake_dummy_ping, 0, 0, 1, CFGFLAG_SERVER, "Whether ping of server-side dummies are more natural or 0", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvV3OffsetX, sv_v3_offset_x, 0, 0, 9999, CFGFLAG_SERVER|CFGFLAG_GAME, "Offset X for the blmapV3 dummy", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvV3OffsetY, sv_v3_offset_y, 0, 0, 9999, CFGFLAG_SERVER|CFGFLAG_GAME, "Offset Y for the blmapV3 dummy", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDummyBotSkin, sv_dummy_bot_skin, 1, 0, 1, CFGFLAG_SERVER, "Whether dummies should have the bot skin applied", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDummyBlocking, sv_dummy_blocking, 0, 0, 1, CFGFLAG_SERVER, "Whether blocking dummies increases killstreak and gives block points", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvHideBotsStatus, sv_hide_dummies_status, 1, 0, 1, CFGFLAG_SERVER, "Whether to hide server-side dummies from status command", AUTHED_ADMIN)

// weapon indicator
MACRO_CONFIG_INT(SvWeaponIndicatorDefault, sv_weapon_indicator_default, 1, 0, 1, CFGFLAG_SERVER, "Whether the weapon names are displayed in the broadcast", AUTHED_ADMIN)

// redirect server tiles
MACRO_CONFIG_STR(SvRedirectServerTilePorts, sv_redirect_server_tile_ports, 128, "", CFGFLAG_SERVER, "Comma separated list of switch number to port mapping (e.g. 1:8305,2:8303)", AUTHED_ADMIN)

// drops
MACRO_CONFIG_INT(SvDropWeapons, sv_drop_weapons, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether to allow dropping weapons with f4", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDropsOnDeath, sv_drops_on_death, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether there is a chance of dropping weapons on death (health and armor in survival, after 5min in no minigame)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDestroyDropsOnLeave, sv_destroy_drops_on_leave, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Destroy dropped weapons (hearts, shields) when their owner disconnects", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvMaxWeaponDrops, sv_max_weapon_drops, 5, 0, 10, CFGFLAG_SERVER, "Maximum amount of dropped weapons per player", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvMaxPickupDrops, sv_max_pickup_drops, 500, 0, 600, CFGFLAG_SERVER, "Maximum amount of dropped hearts and shields", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvInteractiveDrops, sv_interactive_drops, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether dropped weapons, flags, money interact with shotgun and explosions", AUTHED_ADMIN)

// vanilla
MACRO_CONFIG_INT(SvVanillaModeStart, sv_vanilla_mode_start, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether to set the players mode to vanilla on spawn or ddrace", AUTHED_ADMIN)

// survival
MACRO_CONFIG_INT(SvSurvivalMinPlayers, sv_survival_min_players, 4, 2, MAX_CLIENTS, CFGFLAG_SERVER|CFGFLAG_GAME, "Minimum players to start a survival round", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSurvivalLobbyCountdown, sv_survival_lobby_countdown, 15, 5, 120, CFGFLAG_SERVER|CFGFLAG_GAME, "Number in seconds until the survival round starts", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSurvivalRoundTime, sv_survival_round_time, 2, 1, 20, CFGFLAG_SERVER|CFGFLAG_GAME, "Time in minutes until deathmatch starts", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSurvivalDeathmatchTime, sv_survival_deathmatch_time, 2, 1, 5, CFGFLAG_SERVER|CFGFLAG_GAME, "Length of the deathmatch in minutes", AUTHED_ADMIN)

// portal rifle
MACRO_CONFIG_INT(SvPortalRifleDelay, sv_portal_rifle_delay, 10, 0, 60, CFGFLAG_SERVER, "The minimum time in seconds between linking two portals with portal rifle", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPortalRadius, sv_portal_radius, 46, 0, 1024, CFGFLAG_SERVER, "The radius of a portal for portal rifles", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPortalDetonationLinked, sv_portal_detonation_linked, 5, 0, 60, CFGFLAG_SERVER, "Time in seconds linked portals detonate", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPortalDetonation, sv_portal_detonation, 10, 0, 60, CFGFLAG_SERVER, "Time in seconds unlinked portals detonate", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPortalMaxDistance, sv_portal_max_distance, 750, 50, 1000, CFGFLAG_SERVER, "Maximum distance to place a portal", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPortalRifleShop, sv_portal_rifle_shop, 1, 0, 1, CFGFLAG_SERVER, "Whether portal rifle is in shop", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPortalRifleAmmo, sv_portal_rifle_ammo, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether portal rifle entity respawns after x minutes and portal requires ammo", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPortalRifleRespawnTime, sv_portal_rifle_respawn_time, 15, 1, 999, CFGFLAG_SERVER, "Time in minutes a portal rifle respawns after pickup", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPortalThroughDoor, sv_portal_through_door, 1, 0, 1, CFGFLAG_SERVER, "Whether portal rilfe can be used through a closed door (outside of plot only)", AUTHED_ADMIN)

// portal blocker
MACRO_CONFIG_INT(SvPortalBlockerDetonation, sv_portal_blocker_detonation, 30, 0, 999, CFGFLAG_SERVER, "Time in seconds a portal blocker detonates", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPortalBlockerMaxLength, sv_portal_blocker_max_length, 15, 0, 999, CFGFLAG_SERVER, "Maximum portal blocker length in blocks (0 = no limit)", AUTHED_ADMIN)

// draw editor
MACRO_CONFIG_INT(SvMaxObjectsPlotSmall, sv_max_objects_plot_small, 50, 0, 150, CFGFLAG_SERVER|CFGFLAG_GAME, "Maximum amount of objects that can be placed within a small plot", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvMaxObjectsPlotBig, sv_max_objects_plot_big, 150, 0, 500, CFGFLAG_SERVER|CFGFLAG_GAME, "Maximum amount of objects that can be placed within a big plot", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvMaxObjectsFreeDraw, sv_max_objects_free_draw, 500, 0, 2000, CFGFLAG_SERVER|CFGFLAG_GAME, "Maximum amount of objects that can be placed in free draw", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvLightSpeedups, sv_light_speedups, 1, 0, 1, CFGFLAG_SERVER, "Whether draw editor speedups use light mode (heavy mode not recommended)", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvPlotEditorCategories, sv_plot_editor_categories, 128, "pickups,walls,doors,speedups,teleporters,transform", CFGFLAG_SERVER, "Comma separated list (pickups,walls,doors,speedups,teleporters,transform)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvEditorPresetLevel, sv_editor_preset_level, AUTHED_ADMIN, AUTHED_NO, NUM_AUTHEDS, CFGFLAG_SERVER, "Required auth level to use the draw editor preset save/load feature", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvClearFreeDrawLevel, sv_clear_free_draw_level, AUTHED_ADMIN, AUTHED_NO, NUM_AUTHEDS, CFGFLAG_SERVER, "Required auth level to clear free draw area (clearplot 0)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvEditorMaxDistance, sv_editor_max_distance, 750, 0, 99999, CFGFLAG_SERVER, "Maximum distance to place something with draw editor", AUTHED_ADMIN)

// taser battery
MACRO_CONFIG_INT(SvBatteryRespawnTime, sv_battery_respawn_time, 10, 1, 60, CFGFLAG_SERVER, "Time in minutes a taser battery respawns after pickup", AUTHED_ADMIN)

// snake
MACRO_CONFIG_INT(SvSnakeAutoMove, sv_snake_auto_move, 1, 0, 1, CFGFLAG_SERVER, "Whether snake keeps last input or can stand still if no inputs applied", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSnakeSpeed, sv_snake_speed, 4, 1, 50, CFGFLAG_SERVER, "Snake blocks per second speed", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSnakeDiagonal, sv_snake_diagonal, 0, 0, 1, CFGFLAG_SERVER, "Whether snake can move diagonally", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSnakeSmooth, sv_snake_smooth, 1, 0, 1, CFGFLAG_SERVER, "Whether snake moves smoothly", AUTHED_ADMIN)

// chat
MACRO_CONFIG_INT(SvAtEveryoneLevel, sv_ateveryone_level, AUTHED_MOD, AUTHED_NO, NUM_AUTHEDS, CFGFLAG_SERVER, "Required auth level to use @everyone in chat", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvChatAdminPingLevel, sv_chat_admin_ping_level, AUTHED_NO, AUTHED_NO, NUM_AUTHEDS, CFGFLAG_SERVER, "Required auth level to ping authed players in chat", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvLolFilter, sv_lol_filter, 1, 0, 1, CFGFLAG_SERVER, "I like turtles.", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvLocalChat, sv_local_chat, 1, 0, 1, CFGFLAG_SERVER, "Whether local chat is enabled (deactivates sv_authed_highlighted)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvWhisperLog, sv_whisper_log, 0, 0, 1, CFGFLAG_SERVER, "Whether whisper messages get logged aswell", AUTHED_ADMIN)

// admin highlight
MACRO_CONFIG_INT(SvAuthedHighlighted, sv_authed_highlighted, 1, 0, 1, CFGFLAG_SERVER, "Whether authed players are highlighted in the scoreboard (deactivated by sv_local_chat)", AUTHED_ADMIN)

// spawn block
MACRO_CONFIG_INT(SvSpawnBlockProtection, sv_spawn_block_protection, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether spawnblocking in a given area will add escape time", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSpawnAreaLowX, sv_spawnarea_low_x, 0, 0, 9999, CFGFLAG_SERVER|CFGFLAG_GAME, "Low X tile position of area for spawnblock protection", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSpawnAreaLowY, sv_spawnarea_low_y, 0, 0, 9999, CFGFLAG_SERVER|CFGFLAG_GAME, "Low Y tile position of area for spawnblock protection", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSpawnAreaHighX, sv_spawnarea_high_x, 0, 0, 9999, CFGFLAG_SERVER|CFGFLAG_GAME, "High X tile position of area for spawnblock protection", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSpawnAreaHighY, sv_spawnarea_high_y, 0, 0, 9999, CFGFLAG_SERVER|CFGFLAG_GAME, "High Y tile position of area for spawnblock protection", AUTHED_ADMIN)

// sevendown ddnet clients
MACRO_CONFIG_INT(SvAllowSevendown, sv_allow_sevendown, 1, 0, 1, CFGFLAG_SERVER, "Allows sevendown connections", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvMapWindow, sv_map_window, 15, 0, 100, CFGFLAG_SERVER, "Map downloading send-ahead window", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDropOldClients, sv_drop_old_clients, 1, 0, 1, CFGFLAG_SERVER, "Whether old and not fully supported clients are getting dropped", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvBrowserScoreFix, sv_browser_score_fix, 0, 0, 2, CFGFLAG_SERVER, "Whether server tries to make clients display score correctly in browser (2=red color)", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvHttpsMapDownloadURL, sv_https_map_download_url, 128, "", CFGFLAG_SERVER, "URL path to the maps folder", AUTHED_ADMIN)

// map crc
MACRO_CONFIG_STR(FakeMapFile, fake_map_file, 128, "data/maps/fakemap", CFGFLAG_SERVER, "Fake map file name to be loaded (relative to binary)", AUTHED_ADMIN)
MACRO_CONFIG_STR(FakeMapName, fake_map_name, 128, "", CFGFLAG_SERVER, "Fake map name", AUTHED_ADMIN)
MACRO_CONFIG_STR(FakeMapCrc, fake_map_crc, 128, "", CFGFLAG_SERVER, "Fake map crc", AUTHED_ADMIN)

// map design
MACRO_CONFIG_STR(SvMapDesignPath, sv_map_design_path, 128, "data/designs", CFGFLAG_SERVER, "The path where the server searches the map designs (relative to binary)", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvDefaultMapDesign, sv_default_map_design, 128, "", CFGFLAG_SERVER, "Default map design name", AUTHED_ADMIN)

// webhook
MACRO_CONFIG_STR(SvWebhookAntibotURL, sv_webhook_antibot_url, 128, "", CFGFLAG_SERVER, "Webhook URL for antibot reports", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvWebhookAntibotName, sv_webhook_antibot_name, 128, "", CFGFLAG_SERVER, "Webhook name for antibot reports", AUTHED_ADMIN)

MACRO_CONFIG_STR(SvWebhookModLogURL, sv_webhook_mod_log_url, 128, "", CFGFLAG_SERVER, "Webhook URL for moderator logging", AUTHED_ADMIN)

MACRO_CONFIG_STR(SvWebhookChatURL, sv_webhook_chat_url, 128, "", CFGFLAG_SERVER, "Webhook URL for chat bridge", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvWebhookChatAvatarURL, sv_webhook_chat_avatar_url, 128, "", CFGFLAG_SERVER, "Webhook URL for chat bridge avatar", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvWebhookChatSkinAvatars, sv_webhook_chat_skin_avatars, 0, 0, 1, CFGFLAG_SERVER, "Whether chat bridge webhook shows skins as avatars", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvWebhookChatSkinRenderer, sv_webhook_chat_skin_renderer, 0, 0, 1, CFGFLAG_SERVER, "Webhook chat bridge skin renderer (0=skins.tw, 1=KoG)", AUTHED_ADMIN)

MACRO_CONFIG_STR(SvWebhook1vs1URL, sv_webhook_1vs1_url, 128, "", CFGFLAG_SERVER, "Webhook URL for 1vs1", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvWebhook1vs1Name, sv_webhook_1vs1_name, 128, "F-DDrace 1vs1", CFGFLAG_SERVER, "Webhook name for 1vs1", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvWebhook1vs1AvatarURL, sv_webhook_1vs1_avatar_url, 128, "", CFGFLAG_SERVER, "Webhook URL for 1vs1 bridge avatar", AUTHED_ADMIN)

// vpn/proxy detection
MACRO_CONFIG_STR(SvIPHubXKey, sv_iphub_x_key, 128, "", CFGFLAG_SERVER, "IPHub.info X-Key", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvWhitelistFile, sv_whitelist_file, 128, "whitelist.cfg", CFGFLAG_SERVER, "Whitelist file in case IPHub.info falsely flagged someone", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPgsc, sv_pgsc, 0, 0, 1, CFGFLAG_SERVER, "Whether to ban IPs of players that also broadcast a server", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvPgscString, sv_pgsc_string, 128, "", CFGFLAG_SERVER, "String that has to be in a server name to ban players with that IP (empty for direct ban)", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvBotLookupURL, sv_bot_lookup_url, 128, "", CFGFLAG_SERVER, "Bot lookup URL", AUTHED_ADMIN)

// translate
MACRO_CONFIG_STR(SvLibreTranslateURL, sv_libretranslate_url, 128, "https://translate.argosopentech.com/translate", CFGFLAG_SERVER, "LibreTranslate URL for chat messages", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvLibreTranslateKey, sv_libretranslate_key, 128, "", CFGFLAG_SERVER, "LibreTranslate API Key", AUTHED_ADMIN)

// sockets
MACRO_CONFIG_INT(SvPortTwo, sv_port_two, 8304, 0, 0, CFGFLAG_SAVE|CFGFLAG_SERVER, "Port to use for the second serverinfo", AUTHED_ADMIN)

#if defined(CONF_FAMILY_UNIX)
MACRO_CONFIG_STR(SvConnLoggingServer, sv_conn_logging_server, 128, "", CFGFLAG_SERVER, "Unix socket server for IP address logging (Unix only)", AUTHED_ADMIN)
#endif

// discord
MACRO_CONFIG_STR(SvDiscordURL, sv_discord_url, 128, "", CFGFLAG_SERVER, "Discord server URL", AUTHED_ADMIN)

// antibot
MACRO_CONFIG_INT(SvAntibotTreshold, sv_antibot_treshold, 0, 0, 16, CFGFLAG_SERVER, "Treshold for antibot autoban (0=off)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvAntibotBanMinutes, sv_antibot_ban_minutes, 10000, 0, 99999, CFGFLAG_SERVER, "Time in minutes a player gets banned for by antibot", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvAntibotReportsLevel, sv_antibot_reports_level, AUTHED_ADMIN, AUTHED_NO, NUM_AUTHEDS, CFGFLAG_SERVER, "Required auth level to see antibot reports", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvAntibotReportsFilter, sv_antibot_reports_filter, 1, 0, 1, CFGFLAG_SERVER, "Whether antibot reports are filtered if they are legit", AUTHED_ADMIN)

// whois
MACRO_CONFIG_INT(SvWhoIsIPEntries, sv_whois_ip_entries, 120000, 0, 999999, CFGFLAG_SERVER, "WhoIs IP entries", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvWhoIs, sv_whois, 0, 0, 1, CFGFLAG_SERVER, "Whether WhoIs is enabled", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvWhoIsFile, sv_whois_file, 128, "data", CFGFLAG_SERVER, "WhoIs file", AUTHED_ADMIN)

// bugs
MACRO_CONFIG_INT(SvWeakHook, sv_weak_hook, 0, 0, 1, CFGFLAG_SERVER, "Whether everybody has the same hook strength and bounce or weak is also there", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvStoppersPassthrough, sv_stoppers_passthrough, 0, 0, 1, CFGFLAG_SERVER, "Whether tees can pass through stoppers with enough speed", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvShotgunBug, sv_shotgun_bug, 0, 0, 1, CFGFLAG_SERVER, "Whether firing shotgun while standing in another tee gives an insane boost", AUTHED_ADMIN)

// other
MACRO_CONFIG_INT(SvHideMinigamePlayers, sv_hide_minigame_players, 1, 0, 1, CFGFLAG_SERVER, "Whether players in different minigames are shown in the scoreboard", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvRainbowSpeedDefault, sv_rainbow_speed_default, 5, 1, 50, CFGFLAG_SERVER, "Default speed for rainbow", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDefaultScoreMode, sv_default_score_mode, 1, 0, 2, CFGFLAG_SERVER, "Default score (0 = time, 1 = level, 2 = blockpoints)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvOldJetpackSound, sv_old_jetpack_sound, 0, 0, 1, CFGFLAG_SERVER, "Whether to use the default gun sound for jetpack or another sound", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvBlockPointsDelay, sv_block_points_delay, 20, 0, 600, CFGFLAG_SERVER|CFGFLAG_GAME, "Seconds a tee has to be alive in order to give block points to the killer", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvAlwaysTeleWeapon, sv_always_tele_weapon, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether tele weapons can be used on any block or only on marked ones", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvClanProtection, sv_clan_protection, 1, 0, 1, CFGFLAG_SERVER, "Whether players have to use greensward skin for Chilli.* clantag", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvFreezePrediction, sv_freeze_prediction, 1, 0, 1, CFGFLAG_SERVER, "Whether your tee bounces while moving in freeze", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvMapUpdateRate, sv_mapupdaterate, 15, 1, 100, CFGFLAG_SERVER, "Player map update rate", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvHelperVictimMe, sv_helper_victim_me, 0, 0, 1, CFGFLAG_SERVER, "Victim for commands is always yourself when executing as helper", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvWalletKillProtection, sv_wallet_kill_protection, 10000, 0, 100000, CFGFLAG_SERVER, "Minimum wallet amount to trigger the kill protection (0 = disabled)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvTouchedKills, sv_touched_kills, 0, 0, 1, CFGFLAG_SERVER, "Whether touching a tee without hooking or hammering can count as kill", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvBansFile, sv_bans_file, 128, "bans.cfg", CFGFLAG_SERVER, "Ban file to load on server start", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvTeleRifleAllowBlocks, sv_tele_rifle_allow_blocks, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether you can teleport inside of blocks using tele rifle", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvAllowDummy, sv_allow_dummy, 1, 0, 1, CFGFLAG_SERVER, "Whether clients can connect their dummy to the server", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvMinigameAfkAutoLeave, sv_minigame_afk_auto_leave, 120, 0, 600, CFGFLAG_SERVER, "Minigame auto leave when afk for x seconds (0=off)", AUTHED_ADMIN)
#endif
