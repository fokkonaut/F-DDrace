# F-DDrace [![](https://github.com/fokkonaut/F-DDrace/workflows/Build/badge.svg)](https://github.com/fokkonaut/F-DDrace/actions?query=workflow%3ABuild+event%3Apush+branch%3AF-DDrace)

F-DDrace is a modification of Teeworlds, developed by fokkonaut.

## About

F-DDrace is an "open world" game type where you can do whatever you want - chat, hang out, farm money, play minigames, block people, race the map, collect stuff, buy things, annoy others, form clans, and more. The idea was to not have a specific goal, just play and enjoy doing exactly what you want.

## Core Features

**Architecture:** The code base mixes Vanilla 0.7 and DDNet. F-DDrace manually ports various DDNet parts to Vanilla 0.7 and builds on top of that. Originally only 0.7 clients could connect, but DDNet client support was added via a translation layer. So F-DDrace is basically a DDNet modification with extra steps.

**Multi version support:** Works with both 0.7 and DDNet clients perfectly with 100% feature support.

**128 player support:** F-DDrace was the first mod to fully support more than 64 players, implementing it perfectly and covering all edge cases. It was also the first server to actually reach such high player counts during the COVID pandemic, testing and improving performance with live tests.

**Account system:** Save your stats like level, experience, money, kills, deaths and much more. Use `/register`, `/login`, `/stats`, `/account`. Protect your account with `/pin` for security and `/contact` for recovery.

**Shop system:** Buy cosmetics, upgrades and more with farmed money. Shop is integrated into the map.

**Custom votes menu:** The callvote menu has 4 categories: Votes, Account, Miscellaneous, Languages. Switch between them effortlessly and interact with toggle options, value options via reason, and collapse elements. You can combine elements or add prefixes like bullet points.

**Plot system & innovative editor:** One of F-DDrace's biggest features. Rent your own plot and design it with the self-made ingame plot editor. The editor lets you place pickups (hearts, shields, weapons), create laser walls (fully customizable color, length, angle, thickness, collision), build laser doors with matching toggle buttons, add speedups (configurable angle, force, max speed), and set up teleporters (multiple modes: In/Out, From, To, Weapon From, Hook From, with evil mode for red/blue teleporters). Advanced transformation tools let you select whole areas to move, copy, erase, or save for later loading.

**Tavern & Grog:** Sit in a tavern and buy grog. Yes, really.

**Minigames:** List with `/minigames`. When joining, your complete tee status (position, weapons, extras, etc.) gets saved and restored when you `/leave`. Nothing gets missed.

**1vs1 system:** Create a 1vs1 lobby with `/1vs1 <name>` and fight anywhere on the map. What's special is you can take ANY place from the whole map to play a 1vs1 there. Enlarge the area using zoom and adjust weapons for the round.

**Durák card game:** Play by mouse or keyboard (`+left`, `+right`, `+jump`, `+showhookcoll`). Requires: `cl_show_direction 0; cl_nameplates_size 50; cl_nameplates_offset 30; cl_show_others_alpha 40;` Keyboard controls: select cards using A/D, submit using SPACE, go back using HOOK-COLLISION. When you select a card with SPACE, you can choose what to do with it using A/D again and submit with SPACE again. Attacker drags cards to table, defender can defend by dragging cards onto offense cards or place same rank to push attack to next player.

**Server-side translation:** Use `/language` to switch languages. Set default with `sv_default_language` or enable suggestions with `sv_language_suggestion`. Language files are in [`datasrc/languages`](https://github.com/fokkonaut/F-DDrace/tree/F-DDrace/datasrc/languages) and moved to `data/languages` upon compiling.

**Flags and weapon drops:** Drop flags with `F3`, weapons with `F4`. They interact with explosions, shotgun, speedups, teleporters, doors, portals. Flags can be hooked smoothly by players and you can spectate them using the spectate menu (`bind x +spectate`).

**Persistent data:** Money drops and plots survive server restarts. Use `sv_shutdown_save_tees 1` to save and load all players automatically - everything is stored in files and matched when you rejoin.

**Extra weapons:** Extended inventory beyond regular weapons. Most are admin-only if not placed in map.

## Building

F-DDrace uses the same build system as DDNet. For detailed building instructions, dependencies, and platform-specific setup, see the [DDNet README](https://github.com/ddnet/ddnet/blob/master/README.md):

- **Dependencies:** [Linux/macOS](https://github.com/ddnet/ddnet/blob/master/README.md#dependencies-on-linux--macos)
- **Building on Linux/macOS:** [Instructions](https://github.com/ddnet/ddnet/blob/master/README.md#building-on-linux-and-macos)  
- **Building on Windows:** [Instructions](https://github.com/ddnet/ddnet/blob/master/README.md#building-on-windows-with-the-visual-studio-ide)

## Common Chat Commands

Most commands show help when used without parameters.

- `/register name pw pw` - Create account
- `/login name pw` - Log in
- `/stats [player]` - View stats
- `/money` - Check balance
- `/pay amount name` - Send money
- `/plot` - Manage your plot
- `/minigames` - List available minigames
- `/1vs1` - Join/create 1vs1
- `/leave` - Leave current minigame
- `/language [lang]` - Change language
- `/design [name]` - Change map design

## Development, Configuration & Commands

F-DDrace has tons of settings and commands. Instead of maintaining duplicate documentation here, check the source files:

- **All settings:** [`src/game/variables.h`](https://github.com/fokkonaut/F-DDrace/blob/F-DDrace/src/game/variables.h)
- **Admin commands:** [`src/game/ddracecommands.h`](https://github.com/fokkonaut/F-DDrace/blob/F-DDrace/src/game/ddracecommands.h)  
- **Chat commands:** [`src/game/ddracechat.h`](https://github.com/fokkonaut/F-DDrace/blob/F-DDrace/src/game/server/ddracechat.h)

These files contain the complete and up-to-date documentation for everything.

Source code is in [`src/`](https://github.com/fokkonaut/F-DDrace/tree/F-DDrace/src) and [`scripts/`](https://github.com/fokkonaut/F-DDrace/tree/F-DDrace/scripts).
Pull requests and suggestions welcome!

## Tile Documentation & Entities

- **Editor entities:** [all layers separated](https://github.com/fokkonaut/F-DDrace/tree/F-DDrace/F-DDrace_Entities/editor-all-layers) (best mapping experience)
- **Editor entities:** [legacy combined game layer](https://github.com/fokkonaut/F-DDrace/blob/F-DDrace/F-DDrace_Entities/F-DDrace_editor.png)
- **Ingame overlay entities:** [f-ddrace.png](https://github.com/fokkonaut/F-DDrace/blob/F-DDrace/F-DDrace_Entities/f-ddrace.png)
- **All map tile names:** [`src/game/mapitems.h`](https://github.com/fokkonaut/F-DDrace/blob/F-DDrace/src/game/mapitems.h)

Most custom tiles can be used and tested directly from Game and Front layer (or Switch layer for some tiles, like weapons). However there are some tiles and combinations that require more attention and are explained below (use `CTRL+I` in editor for indices):

| Index | Tile | Layer | Description |
| ----- | ---- | ----- | ----------- |
| 69, 70 | Tune Lock, Tune Lock Reset | Tune | Lock a tune parameter per player, stackable with tune zones |
| 100, 101, 102, 103, 130, 145, 146, 147, 148, 151, 154 | Maskable toggle tiles | Game, Front | These tiles (e.g. rainbow or passive) can be placed as toggle tiles but are maskable |
| 166, 167 | Toggle mask ON and OFF | Game, Front | Place a toggle mask tile on any of the above toggle tiles to make them work as ON/OFF tile only |
| 134+114 | Force human+gift | Game+Front | Place force human transformation tile on Game layer and money-xp bomb tile on Front layer, gifts 5000 xp on successful transformation |
| 131 | Hookpower | Switch | Delay=Hookpower: 0=off, 1=rainbow, 2=atom, 3=trail, 4=bloody |
| 180, 181 | Durák table and seats | Switch | ID=matching (e.g. 1 for all), Seat Delay=1-6 (for each seat) |
| 192 | Plot inside | Switch | ID=Plot number, Delay=0 |
| 193 | Plot door | Switch | ID=Plot number, Delay=0: Works like index 240 (Door) and has to be combined with length tile, Delay>0: Laser length, works like index 210-212 |
| 194 | Plot spawn and option | Switch | ID=Plot number, Delay=Plot size: 0=small, 1=big |
| 195, 196 | Redirect FROM and TO teleport | Switch | FROM and TO teleporters but for redirect, ID to port mapping: `sv_redirect_server_tile_ports 1:8304,2:8303`, TO: Delay=1: Playercounter display |
| 199, 200, 202, 241, 243, 244, 245, 249, 250, 252 | Most weapons | Switch | ID=0, Delay=1: Give spread weapon variant |
| 201 | Scroll Ninja | Switch | ID=0, Delay=1: Give scroll ninja |
| 242 | Door Hammer | Switch | ID=0, Delay=1: Give door hammmer, ability to open doors by hitting them |
| 219 | Helicopter | Switch | Delay=Variant: 0=standard, 1=minigun, 2=missile, spawning can be delayed by disabling switch with same ID |

## Map Settings

Following config options can be used from "Server settings" field in the editor. To check their values, see the linked code files above or try them ingame. If you want to add other options to your map, place them in autoexec.cfg file or create an issue or pull request explaining your usecase.

| Config |
| ------ |
| `tune_lock` and `tune_lock_enter` |
| `sv_exp_msg_color_text` (0.7) |
| `sv_exp_msg_color_symbol` (0.7) |
| `sv_exp_msg_color_value` (0.7) |
| `sv_kill_logout` |
| `sv_police_farm_limit` |
| `sv_money_bank_mode` |
| `sv_money_farm_team` |
| `sv_money_drop_delay` |
| `sv_flag_sounds` |
| `sv_flag_hooking` |
| `sv_flag_respawn_dropped` |
| `sv_immunity_flag` |
| `sv_immunity_flag_tele` |
| `sv_hide_dummies` |
| `sv_default_dummies` |
| `sv_v3_offset_x` |
| `sv_v3_offset_y` |
| `sv_dummy_bot_skin` (0.7) |
| `sv_dummy_blocking` |
| `sv_weapon_indicator_default` |
| `sv_allow_empty_inventory` |
| `sv_drop_weapons` |
| `sv_drops_on_death` |
| `sv_destroy_drops_on_leave` |
| `sv_max_weapon_drops` |
| `sv_max_pickup_drops` |
| `sv_interactive_drops` |
| `sv_drops_pickup_delay` |
| `sv_heli_respawn_time` |
| `sv_heli_tile_type` |
| `sv_vanilla_mode_start` |
| `sv_survival_min_players` |
| `sv_survival_lobby_countdown` |
| `sv_survival_round_time` |
| `sv_survival_deathmatch_time` |
| `sv_portal_rifle_delay` |
| `sv_portal_radius` |
| `sv_portal_detonation_linked` |
| `sv_portal_detonation` |
| `sv_portal_max_distance` |
| `sv_portal_rifle_ammo` |
| `sv_portal_rifle_respawn_time` |
| `sv_portal_through_door` |
| `sv_portal_blocker_detonation` |
| `sv_portal_blocker_max_length` |
| `sv_max_objects_plot_small` |
| `sv_max_objects_plot_big` |
| `sv_max_objects_free_draw` |
| `sv_police_taser_plot_raid` |
| `sv_plot_door_health` |
| `sv_taser_battery_respawn_time` |
| `sv_spawn_weapons` |
| `sv_slash_spawn` |
| `sv_spawn_block_protection` |
| `sv_spawnarea_low_x` |
| `sv_spawnarea_low_y` |
| `sv_spawnarea_high_x` |
| `sv_spawnarea_high_y` |
| `sv_redirect_server_tile_ports` |
| `sv_weak_hook` |
| `sv_stoppers_passthrough` |
| `sv_shotgun_bug` |
| `sv_default_score_mode` |
| `sv_allow_bonus_score_mode` |
| `sv_no_bonus_max_jumps` |
| `sv_bonus_score_threshold` |
| `sv_bonus_score_decrease` |
| `sv_spawn_as_zombie` |
| `sv_increase_human_cap_bots` |
| `sv_human_limit_percent` |
| `sv_hide_minigame_players` |
| `sv_rainbow_speed_default` |
| `sv_old_jetpack_sound` |
| `sv_block_points_delay` |
| `sv_always_tele_weapon` |
| `sv_touched_kills` |
| `sv_tele_rifle_allow_blocks` |
| `sv_minigame_afk_auto_leave` |

## Credits

- **Lead Developer:** [fokkonaut](https://github.com/fokkonaut)
- **Major Contributors:** [ChillerDragon](https://github.com/ChillerDragon), [Matq](https://github.com/Matqyou), and the Teeworlds/DDNet community
- **Based on:** [Teeworlds](https://github.com/teeworlds/teeworlds), [DDNet](https://github.com/ddnet/ddnet)

## License

See `license.txt` in the repository root.

---

For help or to contribute, open an issue or make a pull request.
