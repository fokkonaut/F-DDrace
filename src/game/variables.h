/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_VARIABLES_H
#define GAME_VARIABLES_H
#undef GAME_VARIABLES_H // this file will be included several times


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
MACRO_CONFIG_INT(SvAccounts, sv_accounts, 1, 0, 1, CFGFLAG_SERVER, "Whether accounts are activated or deactivated", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvAccFilePath, sv_acc_file_path, 128, "data/accounts", CFGFLAG_SERVER, "The path where the server searches the account files (relative to binary)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDataSaveInterval, sv_data_save_interval, 60, 5, 1440, CFGFLAG_SERVER, "Intervall in minutes between data saves", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvDonationFilePath, sv_donation_file_path, 128, "data", CFGFLAG_SERVER, "The path where the server searches the for the donation file (relative to binary)", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvTopAccountsFilePath, sv_top_accounts_file_path, 128, "data", CFGFLAG_SERVER, "The path where the server searches the for the top accounts file (relative to binary)", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvPlotFilePath, sv_plot_file_path, 128, "data/plots", CFGFLAG_SERVER, "The path where the server searches the plot files (relative to binary)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvKillLogout, sv_kill_logout, 3, 0, 60, CFGFLAG_SERVER|CFGFLAG_GAME, "Time in seconds a tee can kill after trying to logout (0 = disabled)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvEuroMode, sv_euro_mode, 0, 0, 1, CFGFLAG_SERVER, "Whether euro mode is enabled", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvEuroDiscountPercentage, sv_euro_discount_percentage, 0, 0, 100, CFGFLAG_SERVER, "Euro discount percentage for shop (requires reload)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPoliceFarmLimit, sv_police_farm_limit, 0, -1, MAX_CLIENTS, CFGFLAG_SERVER|CFGFLAG_GAME, "Police farm tiles limited (0=off, -1=dynamic number of players, >0=fixed playercount) (disables silentfarm on police)", AUTHED_ADMIN)

MACRO_CONFIG_STR(SvExpMsgColorText, sv_exp_msg_color_text, 4, "999", CFGFLAG_SERVER|CFGFLAG_GAME, "Text color for the experience broadcast", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvExpMsgColorSymbol, sv_exp_msg_color_symbol, 4, "999", CFGFLAG_SERVER|CFGFLAG_GAME, "Symbol color for the experience broadcast", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvExpMsgColorValue, sv_exp_msg_color_value, 4, "595", CFGFLAG_SERVER|CFGFLAG_GAME, "Value color for the experience broadcast", AUTHED_ADMIN)

MACRO_CONFIG_STR(SvMoneyDropsFilePath, sv_money_drops_file_path, 128, "data/money_drops", CFGFLAG_SERVER, "The path where the server searches the money drops file (relative to binary)", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvMoneyHistoryFilePath, sv_money_history_file_path, 128, "money_history", CFGFLAG_SAVE|CFGFLAG_SERVER, "The path to money history files (relative to dumps dir)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvMoneyBankMode, sv_money_bank_mode, 1, 0, 2, CFGFLAG_SERVER|CFGFLAG_GAME, "Bank mode (0=no bank; bank=wallet, 1=normal bank, 2=instant deposit from farm)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvMoneyFarmTeam, sv_money_farm_team, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether a player can farm money on a money tile while being in a ddrace team", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvMoneyDropDelay, sv_money_drop_delay, 1, 0, 9999, CFGFLAG_SERVER|CFGFLAG_GAME, "Time in seconds a player has to wait to drop money again", AUTHED_ADMIN)

// account system ban
MACRO_CONFIG_INT(SvAccSysBanRegistrations, sv_acc_sys_ban_registrations, 3, 0, 10, CFGFLAG_SERVER, "Max registrations per IP within 6 hours", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvAccSysBanPwFails, sv_acc_sys_ban_pw_fails, 5, 0, 10, CFGFLAG_SERVER, "Max passwords fails per IP within 6 hours", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvAccSysBanPinFails, sv_acc_sys_ban_pin_fails, 3, 0, 10, CFGFLAG_SERVER, "Max passwords fails per IP within 6 hours", AUTHED_ADMIN)

// saved tees
MACRO_CONFIG_INT(SvShutdownSaveTees, sv_shutdown_save_tees, 1, 0, 1, CFGFLAG_SERVER, "Whether to save characters before shutdown/reload to load them again", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvSavedTeesFilePath, sv_saved_tees_file_path, 128, "savedtees", CFGFLAG_SAVE|CFGFLAG_SERVER, "The path to saved tees files (relative to dumps dir)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvShutdownSaveTeeExpire, sv_shutdown_save_tee_expire, 8, 1, 24*7, CFGFLAG_SERVER, "How many hours until a shutdown save expires", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSaveTeeForceAccMatch, sv_save_tee_force_acc_match, 1, 0, 1, CFGFLAG_SERVER, "Whether login always tries to load a matching save regardless of previously loaded saves", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvJailSaveTeeExpire, sv_jail_save_tee_expire, 24, 1, 24*7, CFGFLAG_SERVER, "How many hours until a jail save expires", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDisconnectSaveTees, sv_disconnect_save_tees, 0, 0, 1, CFGFLAG_SERVER, "Whether to save characters before disconnect to load them again (0=disabled, 1=allow manual activation)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDisconnectSaveTeeExpire, sv_disconnect_save_tee_expire, 2, 1, 24, CFGFLAG_SERVER, "How many hours until a disconnect save expires", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDisconnectSaveTeeFreeze, sv_disconnect_save_tee_freeze, 5, 0, 60, CFGFLAG_SERVER, "Time in seconds a player loading a disconnect save tee gets frozen", AUTHED_ADMIN)

// shutdown auto reconnect
MACRO_CONFIG_INT(SvShutdownAutoReconnect, sv_shutdown_auto_reconnect, 0, 0, 2, CFGFLAG_SERVER, "Whether shutdown will send a map change to time out client, so it'll reconnect (1=Map timeout, 2=Message)", AUTHED_ADMIN)

// flags
MACRO_CONFIG_INT(SvFlagSounds, sv_flag_sounds, 2, 0, 2, CFGFLAG_SERVER|CFGFLAG_GAME, "Flag sounds on drop/pickup/respawn (0=off, 1=public sounds, 2=respawn public rest local)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvFlagHooking, sv_flag_hooking, 2, 0, 2, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether flags are hookable (1=allow hook, 2=allow, but disallow hooking from solo tees)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvFlagRespawnDropped, sv_flag_respawn_dropped, 90, 0, 9999, CFGFLAG_SERVER|CFGFLAG_GAME, "Time in seconds a dropped flag resets", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvImmunityFlag, sv_immunity_flag, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether the blue flag makes you immune to zombiefication", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvImmunityFlagTele, sv_immunity_flag_tele, 0, 0, 256, CFGFLAG_SERVER|CFGFLAG_GAME, "Teleporter number to tele to when grabbing blue flag from stand", AUTHED_ADMIN)

// dummy
MACRO_CONFIG_INT(SvHideDummies, sv_hide_dummies, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether to hide server-side dummies from scoreboard", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDefaultDummies, sv_default_dummies, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether to create default dummies for specific maps when the server starts", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvFakeDummyPing, sv_fake_dummy_ping, 0, 0, 1, CFGFLAG_SERVER, "Whether ping of server-side dummies are more natural or 0", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvV3OffsetX, sv_v3_offset_x, 0, 0, 9999, CFGFLAG_SERVER|CFGFLAG_GAME, "Offset X for the blmapV3 dummy", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvV3OffsetY, sv_v3_offset_y, 0, 0, 9999, CFGFLAG_SERVER|CFGFLAG_GAME, "Offset Y for the blmapV3 dummy", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDummyBotSkin, sv_dummy_bot_skin, 0, 0, 1, CFGFLAG_SERVER, "Whether dummies should have the bot skin applied (0.7 only)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDummyBlocking, sv_dummy_blocking, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether blocking dummies increases killstreak and gives block points", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvHideBotsStatus, sv_hide_dummies_status, 1, 0, 1, CFGFLAG_SERVER, "Whether to hide server-side dummies from status command", AUTHED_ADMIN)

// weapon indicator
MACRO_CONFIG_INT(SvWeaponIndicatorDefault, sv_weapon_indicator_default, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether the weapon names are displayed in the broadcast", AUTHED_ADMIN)

// drops
MACRO_CONFIG_INT(SvAllowEmptyInventory, sv_allow_empty_inventory, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether to allow dropping all your weapons, ending up with an empty inventory", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDropWeapons, sv_drop_weapons, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether to allow dropping weapons with f4", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDropsOnDeath, sv_drops_on_death, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether there is a chance of dropping weapons on death (health and armor in survival, after 5min in no minigame)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDestroyDropsOnLeave, sv_destroy_drops_on_leave, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Destroy dropped weapons (hearts, shields) when their owner disconnects", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvMaxWeaponDrops, sv_max_weapon_drops, 5, 0, 10, CFGFLAG_SERVER|CFGFLAG_GAME, "Maximum amount of dropped weapons per player", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvMaxPickupDrops, sv_max_pickup_drops, 500, 0, 600, CFGFLAG_SERVER|CFGFLAG_GAME, "Maximum amount of dropped hearts and shields", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvInteractiveDrops, sv_interactive_drops, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether dropped weapons, flags, money interact with shotgun and explosions", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDropsPickupDelay, sv_drops_pickup_delay, 2000, 100, 60000, CFGFLAG_SERVER|CFGFLAG_GAME, "Pickup delay in ms for weapon, grog, money drops", AUTHED_ADMIN)

// helicopter
MACRO_CONFIG_INT(SvHeliRespawnTime, sv_heli_respawn_time, 60, 0, 9999, CFGFLAG_SERVER|CFGFLAG_GAME, "Delay in seconds for a tile-placed helicopter to (re)spawn", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvHeliTileType, sv_heli_tile_type, 3, 0, 3/*NUM_HELICOPTER_TYPES, HELICOPTER_DEFAULT, NUM_HELICOPTER_TYPES*/, CFGFLAG_SERVER|CFGFLAG_GAME, "Helicopter tile type (0=default, 1=attack, 2=chinook, 3=random)", AUTHED_ADMIN)

// vanilla
MACRO_CONFIG_INT(SvVanillaModeStart, sv_vanilla_mode_start, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether to set the players mode to vanilla on spawn or ddrace", AUTHED_ADMIN)

// survival
MACRO_CONFIG_INT(SvSurvivalMinPlayers, sv_survival_min_players, 4, 2, MAX_CLIENTS, CFGFLAG_SERVER|CFGFLAG_GAME, "Minimum players to start a survival round", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSurvivalLobbyCountdown, sv_survival_lobby_countdown, 15, 5, 120, CFGFLAG_SERVER|CFGFLAG_GAME, "Number in seconds until the survival round starts", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSurvivalRoundTime, sv_survival_round_time, 2, 1, 20, CFGFLAG_SERVER|CFGFLAG_GAME, "Time in minutes until deathmatch starts", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSurvivalDeathmatchTime, sv_survival_deathmatch_time, 2, 1, 5, CFGFLAG_SERVER|CFGFLAG_GAME, "Length of the deathmatch in minutes", AUTHED_ADMIN)

// portal rifle
MACRO_CONFIG_INT(SvPortalRifleDelay, sv_portal_rifle_delay, 10, 0, 60, CFGFLAG_SERVER|CFGFLAG_GAME, "The minimum time in seconds between linking two portals with portal rifle", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPortalRadius, sv_portal_radius, 46, 0, 1024, CFGFLAG_SERVER|CFGFLAG_GAME, "The radius of a portal for portal rifles", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPortalDetonationLinked, sv_portal_detonation_linked, 5, 0, 60, CFGFLAG_SERVER|CFGFLAG_GAME, "Time in seconds linked portals detonate", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPortalDetonation, sv_portal_detonation, 10, 0, 60, CFGFLAG_SERVER|CFGFLAG_GAME, "Time in seconds unlinked portals detonate", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPortalMaxDistance, sv_portal_max_distance, 750, 50, 1000, CFGFLAG_SERVER|CFGFLAG_GAME, "Maximum distance to place a portal", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPortalRifleShop, sv_portal_rifle_shop, 1, 0, 1, CFGFLAG_SERVER, "Whether portal rifle is in shop", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPortalRifleAmmo, sv_portal_rifle_ammo, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether portal rifle entity respawns after x minutes and portal requires ammo", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPortalRifleRespawnTime, sv_portal_rifle_respawn_time, 15, 1, 999, CFGFLAG_SERVER|CFGFLAG_GAME, "Time in minutes a portal rifle respawns after pickup", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPortalThroughDoor, sv_portal_through_door, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether portal rilfe can be used through a closed door (outside of plot only)", AUTHED_ADMIN)

// portal blocker
MACRO_CONFIG_INT(SvPortalBlockerDetonation, sv_portal_blocker_detonation, 30, 0, 999, CFGFLAG_SERVER|CFGFLAG_GAME, "Time in seconds a portal blocker detonates", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPortalBlockerMaxLength, sv_portal_blocker_max_length, 15, 0, 999, CFGFLAG_SERVER|CFGFLAG_GAME, "Maximum portal blocker length in blocks (0 = no limit)", AUTHED_ADMIN)

// draw editor
MACRO_CONFIG_INT(SvMaxObjectsPlotSmall, sv_max_objects_plot_small, 50, 0, 150, CFGFLAG_SERVER|CFGFLAG_GAME, "Maximum amount of objects that can be placed within a small plot", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvMaxObjectsPlotBig, sv_max_objects_plot_big, 150, 0, 500, CFGFLAG_SERVER|CFGFLAG_GAME, "Maximum amount of objects that can be placed within a big plot", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvMaxObjectsFreeDraw, sv_max_objects_free_draw, 5000, 0, 5000, CFGFLAG_SERVER|CFGFLAG_GAME, "Maximum amount of objects that can be placed in free draw", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvLightSpeedups, sv_light_speedups, 1, 0, 1, CFGFLAG_SERVER, "Whether draw editor speedups use light mode (heavy mode not recommended)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvLightTeleporters, sv_light_teleporters, 1, 0, 1, CFGFLAG_SERVER, "Whether draw editor teleporters use light mode (heavy mode not recommended)", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvPlotEditorCategories, sv_plot_editor_categories, 128, "pickups,walls,doors,speedups,teleporters,transform", CFGFLAG_SERVER, "Comma separated list (pickups,walls,doors,speedups,teleporters,transform,tile)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvEditorPresetLevel, sv_editor_preset_level, AUTHED_ADMIN, AUTHED_NO, NUM_AUTHEDS-1, CFGFLAG_SERVER, "Required auth level to use the draw editor preset save/load feature", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvClearFreeDrawLevel, sv_clear_free_draw_level, AUTHED_ADMIN, AUTHED_NO, NUM_AUTHEDS-1, CFGFLAG_SERVER, "Required auth level to clear free draw area (clearplot 0)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvEditorMaxDistance, sv_editor_max_distance, 750, 0, 99999, CFGFLAG_SERVER, "Maximum distance to place something with draw editor", AUTHED_ADMIN)

MACRO_CONFIG_INT(SvHideTileWarnings, sv_hide_tile_warnings, 0, 0, 1, CFGFLAG_SERVER, "Whether tile warnings are shown (invalid toggle mask, wrongly placed switch tiles...)", AUTHED_ADMIN)

// taser officers can shoot doors of wanted players' plots
MACRO_CONFIG_INT(SvPoliceTaserPlotRaid, sv_police_taser_plot_raid, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether police taser can destroy plots of wanted players", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPlotDoorHealth, sv_plot_door_health, 30, 0, 1000, CFGFLAG_SERVER|CFGFLAG_GAME, "Initial plot door health to withstand tasers", AUTHED_ADMIN)

// taser battery
MACRO_CONFIG_INT(SvTaserBatteryRespawnTime, sv_taser_battery_respawn_time, 10, 1, 60, CFGFLAG_SERVER|CFGFLAG_GAME, "Time in minutes a taser battery respawns after pickup", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvTaserStrengthDefault, sv_taser_strength_default, 1, 0, 10, CFGFLAG_SERVER, "Default taser strength for when a player is not logged in", AUTHED_ADMIN)

// spawn weapons
MACRO_CONFIG_INT(SvSpawnWeapons, sv_spawn_weapons, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether account spawn weapons will be given on spawn", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSlashSpawn, sv_slash_spawn, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether /spawn is activated (WARNING: can be abused to dodge specific tiles)", AUTHED_ADMIN)

// snake
MACRO_CONFIG_INT(SvSnakeAutoMove, sv_snake_auto_move, 1, 0, 1, CFGFLAG_SERVER, "Whether snake keeps last input or can stand still if no inputs applied", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSnakeSpeed, sv_snake_speed, 6, 1, 50, CFGFLAG_SERVER, "Snake blocks per second speed", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSnakeDiagonal, sv_snake_diagonal, 0, 0, 1, CFGFLAG_SERVER, "Whether snake can move diagonally", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvSnakeSmooth, sv_snake_smooth, 1, 0, 1, CFGFLAG_SERVER, "Whether snake moves smoothly", AUTHED_ADMIN)

// chat
MACRO_CONFIG_INT(SvAtEveryoneLevel, sv_ateveryone_level, AUTHED_MOD, AUTHED_NO, NUM_AUTHEDS-1, CFGFLAG_SERVER, "Required auth level to use @everyone in chat", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvChatAdminPingLevel, sv_chat_admin_ping_level, AUTHED_NO, AUTHED_NO, NUM_AUTHEDS-1, CFGFLAG_SERVER, "Required auth level to ping authed players in chat", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvLolFilter, sv_lol_filter, 1, 0, 1, CFGFLAG_SERVER, "I like turtles.", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvLocalChat, sv_local_chat, 1, 0, 1, CFGFLAG_SERVER, "Whether local chat is enabled (deactivates sv_authed_highlighted)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvWhisperLog, sv_whisper_log, 0, 0, 1, CFGFLAG_SERVER, "Whether whisper messages get logged aswell", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvJoinMsgDelay, sv_join_msg_delay, 0, 0, 60, CFGFLAG_SERVER, "The time in seconds the player join message get's delayed (and sv_chat_initial_delay is imitated)", AUTHED_MOD)

// status recent
MACRO_CONFIG_INT(SvRecentlyLeftSaveTime, sv_recently_left_save_time, 120, 5, 300, CFGFLAG_SERVER, "The time in seconds a recently left player is stored for moderating", AUTHED_HELPER)

// admin highlight
MACRO_CONFIG_INT(SvAuthedHighlighted, sv_authed_highlighted, 1, 0, 1, CFGFLAG_SERVER, "Whether authed players are highlighted in the scoreboard (deactivated by sv_local_chat)", AUTHED_ADMIN)

// spawn block
MACRO_CONFIG_INT(SvSpawnBlockProtection, sv_spawn_block_protection, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether spawnblocking in a given area will add escape time", AUTHED_ADMIN)
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
MACRO_CONFIG_INT(SvWebhookChatSkinAvatars, sv_webhook_chat_skin_avatars, 1, 0, 1, CFGFLAG_SERVER, "Whether chat bridge webhook shows skins as avatars", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvWebhookChatSkinRenderer, sv_webhook_chat_skin_renderer, 0, 0, 1, CFGFLAG_SERVER, "Webhook chat bridge skin renderer (0=skins.tw, 1=KoG)", AUTHED_ADMIN)

MACRO_CONFIG_STR(SvWebhook1vs1URL, sv_webhook_1vs1_url, 128, "", CFGFLAG_SERVER, "Webhook URL for 1vs1", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvWebhook1vs1Name, sv_webhook_1vs1_name, 128, "F-DDrace 1vs1", CFGFLAG_SERVER, "Webhook name for 1vs1", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvWebhook1vs1AvatarURL, sv_webhook_1vs1_avatar_url, 128, "", CFGFLAG_SERVER, "Webhook URL for 1vs1 bridge avatar", AUTHED_ADMIN)

// vpn/proxy detection
MACRO_CONFIG_STR(SvIPHubXKey, sv_iphub_x_key, 128, "", CFGFLAG_SERVER, "IPHub.info X-Key", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvWhitelistFile, sv_whitelist_file, 128, "whitelist.cfg", CFGFLAG_SERVER, "Whitelist file for DNSBL/PGSC/Antibot", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvPgsc, sv_pgsc, 0, 0, 1, CFGFLAG_SERVER, "Whether to ban IPs of players that also broadcast a server", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvPgscString, sv_pgsc_string, 128, "", CFGFLAG_SERVER, "String that has to be in a server name to ban players with that IP (empty for direct ban)", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvBotLookupURL, sv_bot_lookup_url, 128, "", CFGFLAG_SERVER, "Bot lookup URL", AUTHED_ADMIN)

// translate
MACRO_CONFIG_STR(SvLibreTranslateURL, sv_libretranslate_url, 128, "https://translate.argosopentech.com/translate", CFGFLAG_SERVER, "LibreTranslate URL for chat messages", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvLibreTranslateKey, sv_libretranslate_key, 128, "", CFGFLAG_SERVER, "LibreTranslate API Key", AUTHED_ADMIN)

// sockets
MACRO_CONFIG_INT(SvPortTwo, sv_port_two, 0, 0, 0, CFGFLAG_SAVE|CFGFLAG_SERVER, "Port to use for the second serverinfo in 0.7 server browser (0=disabled) (may cause delay)", AUTHED_ADMIN)

// redirect server tiles
MACRO_CONFIG_STR(SvRedirectServerTilePorts, sv_redirect_server_tile_ports, 128, "", CFGFLAG_SERVER|CFGFLAG_GAME, "Comma separated list of switch number to port mapping (e.g. 1:8305,2:8303)", AUTHED_ADMIN)

#if defined(CONF_FAMILY_UNIX)
MACRO_CONFIG_STR(SvConnLoggingServer, sv_conn_logging_server, 128, "", CFGFLAG_SERVER, "Unix socket server for IP address logging (Unix only)", AUTHED_ADMIN)
#endif

// discord
MACRO_CONFIG_STR(SvDiscordURL, sv_discord_url, 128, "", CFGFLAG_SERVER, "Discord server URL", AUTHED_ADMIN)

// antibot
MACRO_CONFIG_INT(SvAntibotThreshold, sv_antibot_threshold, 0, 0, 64, CFGFLAG_SERVER, "Threshold for antibot autoban (0=off)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvAntibotAutoAction, sv_antibot_auto_action, 1, 0, 2, CFGFLAG_SERVER, "Automatic antibot action (0=off, 1=jail, 2=ban)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvAntibotAutoActionTime, sv_antibot_auto_action_time, 900, 0, 99999, CFGFLAG_SERVER, "Time for sv_antibot_auto_action (if 1: jail seconds, if 2: ban minutes)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvAntibotReportsLevel, sv_antibot_reports_level, AUTHED_ADMIN, AUTHED_NO, NUM_AUTHEDS-1, CFGFLAG_SERVER, "Required auth level to see antibot reports", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvAntibotLogPending, sv_antibot_log_pending, 1, 0, 1, CFGFLAG_SERVER, "Whether pending antibot reports are logged", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvAntibotSkipKinds, sv_antibot_skip_kinds, 256, "", CFGFLAG_SERVER, "Antibot skip kinds list", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvAntibotSkipDummyHammer, sv_antibot_skip_dummy_hammer, 0, 0, 1, CFGFLAG_SERVER, "Whether antibot tries to ignore dummy hammerfly", AUTHED_ADMIN)

// DNSBL
MACRO_CONFIG_INT(SvDnsbl, sv_dnsbl, 0, 0, 1, CFGFLAG_SERVER, "Enable DNSBL (DNS-based Blackhole List)", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvDnsblHost, sv_dnsbl_host, 128, "", CFGFLAG_SERVER, "Hostname of DNSBL provider to use for IP Verification", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvDnsblKey, sv_dnsbl_key, 128, "", CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, "Optional Authentication Key for the specified DNSBL provider", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDnsblVote, sv_dnsbl_vote, 0, 0, 1, CFGFLAG_SERVER, "Block votes by blacklisted addresses", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDnsblBan, sv_dnsbl_ban, 0, 0, 1, CFGFLAG_SERVER, "Automatically ban blacklisted addresses", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvDnsblBanReason, sv_dnsbl_ban_reason, 128, "VPN detected, try connecting without. Contact admin if mistaken", CFGFLAG_SERVER, "Ban/jail reason for 'sv_dnsbl_ban/sv_dnsbl_jail'", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDnsblChat, sv_dnsbl_chat, 0, 0, 1, CFGFLAG_SERVER, "Don't allow chat from blacklisted addresses", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDnsblJail, sv_dnsbl_jail, 0, 0, 1, CFGFLAG_SERVER, "Automatically jail blacklisted addresses", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDnsblCache, sv_dnsbl_cache, 0, 0, 1, CFGFLAG_SERVER, "Whether black and whitelisted address results are cached for 36 hours", AUTHED_ADMIN)

// whois
MACRO_CONFIG_INT(SvWhoIsIPEntries, sv_whois_ip_entries, 120000, 0, 1999999, CFGFLAG_SERVER, "WhoIs IP entries", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvWhoIs, sv_whois, 0, 0, 1, CFGFLAG_SERVER, "Whether WhoIs is enabled", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvWhoIsFile, sv_whois_file, 128, "data", CFGFLAG_SERVER, "WhoIs file", AUTHED_ADMIN)

// bugs
MACRO_CONFIG_INT(SvWeakHook, sv_weak_hook, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether everybody has the same hook strength and bounce or weak is also there", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvStoppersPassthrough, sv_stoppers_passthrough, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether tees can pass through stoppers with enough speed", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvShotgunBug, sv_shotgun_bug, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether firing shotgun while standing in another tee gives an insane boost", AUTHED_ADMIN)

// score
MACRO_CONFIG_INT(SvDefaultScoreMode, sv_default_score_mode, SCORE_LEVEL, 0, NUM_SCORE_MODES-1, CFGFLAG_SERVER|CFGFLAG_GAME, "Default score (0=time, 1=level, 2=blockpoints, 3=bonus)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvAllowBonusScoreMode, sv_allow_bonus_score_mode, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether bonus score can be seen using '/score'", AUTHED_ADMIN)
// no-bonus area
MACRO_CONFIG_INT(SvNoBonusMaxJumps, sv_no_bonus_max_jumps, 5, 2, 9999, CFGFLAG_SERVER|CFGFLAG_GAME, "Maximum number of jumps in no-bonus area (threshold=0: set this amount, else: start score increase at this)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvNoBonusScoreThreshold, sv_bonus_score_threshold, 10, 0, 100, CFGFLAG_SERVER|CFGFLAG_GAME, "Threshold value for bonus score in no-bonus area (0=bonus removal)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvNoBonusScoreDecrease, sv_bonus_score_decrease, 10, 0, 60, CFGFLAG_SERVER|CFGFLAG_GAME, "Time in seconds between bonus score decrease", AUTHED_ADMIN)

// grog
MACRO_CONFIG_INT(SvGrogPrice, sv_grog_price, 500, 1, 50000, CFGFLAG_SERVER, "Price per grog", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvGrogHoldLimit, sv_grog_hold_limit, 3, 1, 10, CFGFLAG_SERVER, "Amount of grogs a player can carry", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvGrogMinPermilleLimit, sv_grog_min_permille_limit, 12, 0, 39, CFGFLAG_SERVER, "Divided by 10: Minimum legal permille limit, if exceeded=wanted", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvGrogPermillePassiveLimit, sv_grog_permille_passive_limit, 6, 0, 39, CFGFLAG_SERVER, "Divided by 10: Permille limit, if exceeded=passive off", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvGrogForceHammer, sv_grog_force_hammer, 0, 0, 1, CFGFLAG_SERVER, "Whether holding grog forces to hold hammer, or can have no weapon (new DDNet can render tee without weapon)", AUTHED_ADMIN)

// Durak
MACRO_CONFIG_INT(SvDurakTeamColor, sv_durak_team_colors, 1, 0, 1, CFGFLAG_SERVER, "Whether Durak minigame uses team colors for own round", AUTHED_ADMIN)

// Language
MACRO_CONFIG_STR(SvDefaultLanguage, sv_default_language, 255, "", CFGFLAG_SERVER, "Default server language filename string without extension", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvLanguagesPath, sv_languages_path, 128, "data/languages", CFGFLAG_SERVER, "The path where the server searches the for the language index.txt (relative to binary)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvLanguageSuggestion, sv_language_suggestion, 1, 0, 1, CFGFLAG_SERVER, "Whether player that entered the server will get a language suggestion vote", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvCountriesFilePath, sv_countries_file_path, 128, "data", CFGFLAG_SERVER, "The path where the server searches the for the countries file (relative to binary)", AUTHED_ADMIN)

// zombie
MACRO_CONFIG_INT(SvSpawnAsZombie, sv_spawn_as_zombie, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether player respawn as zombie by default", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvIncreaseHumanCapBots, sv_increase_human_cap_bots, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether server-side bots are counted (0=not counted, 1=counted as players, but not humans!)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvHumanLimitPercent, sv_human_limit_percent, 50, 0, 100, CFGFLAG_SERVER|CFGFLAG_GAME, "Humans limit in percent for trial tile", AUTHED_ADMIN)

// dummy and 1vs1
MACRO_CONFIG_INT(SvAllowDummy, sv_allow_dummy, 1, 0, 1, CFGFLAG_SERVER, "Whether clients can connect their dummy to the server", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvDummyCtrlCopyUpdateIdle, sv_dummy_ctrl_copy_update_idle, 1, 0, 1, CFGFLAG_SERVER, "Whether dummy control/copy moves will update idle (0=can cause bugs with minigame auto leave)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvMinigameAfkAutoLeave, sv_minigame_afk_auto_leave, 120, 0, 600, CFGFLAG_SERVER|CFGFLAG_GAME, "Minigame auto leave when afk for x seconds (0=off)", AUTHED_ADMIN)

// tele weapons
MACRO_CONFIG_INT(SvAlwaysTeleWeapon, sv_always_tele_weapon, 1, 0, 2, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether tele weapons can be used on any block or only on marked ones (1=red/evil, 2=blue/keep velocity)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvTeleWeaponThroughRoomVip, sv_tele_weapon_through_room_vip, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether tele weapons can be used through room and vip door if player can pass it", AUTHED_ADMIN)

// other
MACRO_CONFIG_INT(SvResetProjLifetimeAfterHit, sv_reset_proj_lifetime_after_hit, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Wheter projectile lifetime is reset after being hit and redirected with hammer", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvAllowXSkins, sv_allow_x_skins, 1, 0, 1, CFGFLAG_SERVER, "Whether special skins are allowed (x_ninja, x_spec, ...)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvRainbowNameSpec, sv_rainbowname_spec, 0, 0, 1, CFGFLAG_SERVER, "Whether rainbowname is processed while paused or spectating (annoying in +spectate)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvHideMinigamePlayers, sv_hide_minigame_players, 1, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether players in different minigames are shown in the scoreboard", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvRainbowSpeedDefault, sv_rainbow_speed_default, 5, 1, 20, CFGFLAG_SERVER|CFGFLAG_GAME, "Default speed for rainbow", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvOldJetpackSound, sv_old_jetpack_sound, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether to use the default gun sound for jetpack or another sound", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvBlockPointsDelay, sv_block_points_delay, 20, 0, 600, CFGFLAG_SERVER|CFGFLAG_GAME, "Seconds a tee has to be alive in order to give block points to the killer", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvClanProtection, sv_clan_protection, 1, 0, 1, CFGFLAG_SERVER, "Whether players have to use greensward skin for Chilli.* clantag", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvFreezePrediction, sv_freeze_prediction, 1, 0, 1, CFGFLAG_SERVER, "Whether your tee bounces while moving in freeze", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvMapUpdateRate, sv_mapupdaterate, 15, 1, 100, CFGFLAG_SERVER, "Player map update rate", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvHelperVictimMe, sv_helper_victim_me, 0, 0, 1, CFGFLAG_SERVER, "Victim for commands is always yourself when executing as helper", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvWalletKillProtection, sv_wallet_kill_protection, 10000, 0, 100000, CFGFLAG_SERVER, "Minimum wallet amount to trigger the kill protection (0 = disabled)", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvTouchedKills, sv_touched_kills, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether touching a tee without hooking or hammering can count as kill", AUTHED_ADMIN)
MACRO_CONFIG_STR(SvBansFile, sv_bans_file, 128, "bans.cfg", CFGFLAG_SERVER, "Ban file to load on server start", AUTHED_ADMIN)
MACRO_CONFIG_INT(SvTeleRifleAllowBlocks, sv_tele_rifle_allow_blocks, 0, 0, 1, CFGFLAG_SERVER|CFGFLAG_GAME, "Whether you can teleport inside of blocks using tele rifle", AUTHED_ADMIN)
#endif
