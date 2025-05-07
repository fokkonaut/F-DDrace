F-DDrace [![](https://github.com/fokkonaut/F-DDrace/workflows/Build/badge.svg)](https://github.com/fokkonaut/F-DDrace/actions?query=workflow%3ABuild+event%3Apush+branch%3AF-DDrace)
=========

F-DDrace is a modification of Teeworlds, developed by fokkonaut. <br>
Discord: fokkonaut (old: #5556) or https://discord.gg/qccRrtb
	
<details>
<summary>History</summary>
<br>

[]()
My Teeworlds coding history started in the beginning of 2018. ChillerDragon has helped me a lot and introduced me into the world of Teeworlds coding and coding in general. <br>
In the beginning I started with some smaller pull requests on [DDNetPP](https://github.com/DDNetPP/DDNetPP). My first commit in DDNetPP was on January 19, 2018. <br>
This being said, I implemented my first features in DDNetPP, which is also the reason why DDNetPP and F-DDrace have many similarities. I liked the idea of the mod and played it myself for several years. At some point ChillerDragon motivated me to create my own modification where I could really express myself and do things the way I want them to be. Thats when [BlockDDrace](https://github.com/fokkonaut/BlockDDrace) was born with it's first commit on March 15, 2018. <br>
After 975 commits in BlockDDrace, I wanted to create a new modification based on Vanilla 0.7, because around that time the Teeworlds GitHub was active again and 0.7 got released. <br>
That's when F-DDrace was born with it's first commit on August 14, 2019. I manually ported various parts of the DDNet code to the Vanilla 0.7 code base and on top of that came BlockDDrace. From then on, F-DDrace has been developed more and more and became what it is today: My own personal playground to learn coding, create fun things and to provide something unique for the players. That's also the reason my server is called `fokkonaut's playground`. <br>
At the time I am writing this right now (February 18, 2023), I made more than 2935 commits on F-DDrace.
</details>

Base concept
============

The code base is a mixture of Vanilla 0.7 and DDNet. In order to achieve that, I ported various parts of the DDNet code base to Vanilla 0.7 and created my mod on top of that.

Originally, this mod was only accessable by a 0.7 client. At some point I added support for DDNet clients via a translation layer between both versions.

That being said, F-DDrace "basically" is a modification of DDNet with extra steps.

The modification itself is designed to be an "open world" game type. You can do anything you want: Chat with others, hang out, farm money on chairs, play minigames, block others, play the race of the map, collect upgrades, buy things from shop, annoy others, form groups and clans, raid specific areas and much more.

The idea was to not have a specific aim to go for, rather play and enjoy the time doing exactly what you want.

Core features
=============

This modification has a lot of features, smaller and bigger ones, and each of these features are designed to work pretty flawlessly with each other.

The core features of this modification are:

* **Multi version support** <br>
As said earlier, F-DDrace is a 0.7 modification. DDNet client support has been implemented perfectly with 100% feature support.

* **128 player support** <br>
F-DDrace is the first modification to fully support more than 64 players at once. There may have been other modifications before, but F-DDrace implemented the support for more clients perfectly and flawlessly, covering all edge cases as good as possible. <br>
Also it is the first server to actually reach such high player counts at once, during the very active "corona phase", and therefore testing and improving the performance with live tests made it an unique experience.

* **Account system** <br>
Accounts are used to save your stats, such as your level, experience, money, kills, deaths and much more. <br>
You can create an account using `/register`, use `/login` to log in and use `/stats` and `/account` to see some more details about your account. <br>
In order to protect your account, you can set a security pin using `/pin` and a contact method using `/contact`.

* **Shop system** <br>
Using money you have previously farmed you can buy things from the shop, such as cosmetics, upgrades and much more. The shop is integrated into the map.

* **Custom votes menu** <br>
The call vote menu has been adjusted to hold 3 categories: Votes, Account, Miscellaneous. You can switch between them effortlessly and interact with toggle options, value options via reason aswell as collapse elements.<br>
It's also possible to combine those elements, or add other prefixes such as bullet points.

* **Plots** <br>
One of the biggest features is probably the plot editor and the plots themselves. You can rent a plot ingame and modify it as you like using the self-made and innovative ingame plot editor. <br>
It allows you to place and do several things: <br>
	* **Pickups** <br>
	Hearts, shields, weapons.
	* **Laser walls** <br>
	Modifiable in color, length, angle, thickness and collision on/off.
	* **Laser doors** <br>
	Doors with matching toggle buttons.
	* **Speedups** <br>
	Modifiable angle, force speed and max speed.
	* **Teleporters** <br>
	Modes: In/Out, From, To, Weapon From, Hook From. Evil mode on/off possible (red and blue teleporters)
	* **Transformation** <br>
	Select a whole area and move, copy, erase or save it at once. Saved drawings can be loaded later on again.

* **Tavern & Grog** <br>
Yes, that's right. You can sit down in a nice tavern and buy some grog for you and your friends. Cheers!

* **Minigames** <br>
You can find a list of minigames using `/minigames`. When joining a minigame, your complete tee status including position, weapons, extras, etc. will be saved and can be loaded later when leaving the minigame again using `/leave`. Nothing will be missed and you can continue playing as before.

* **1vs1 minigame** <br>
The reason why the 1vs1 minigame deserves it's own entry in this list is simple. It's innovative and unique. You can create a 1vs1 lobby using `/1vs1 <name>` and what's so special about it is, that you can take ANY place from the whole map to play a 1vs1 there. You can enlarge the area using zoom and adjust weapons to be used during the round.<br>

* **Durák card game** <br>
You can play Durák by mouse or via keyboarc controls (+left, +right, +jump, +showhookcoll) <br>
	* **Requirements** <br>
	`cl_show_direction 0; cl_nameplates_size 50; cl_nameplates_offset 30; cl_show_others_alpha 40;`
	* **Durák controls/rules** <br>
	Round start: Initial attacker name will be displayed at top left of table.<br>
	Attacker: You start, drag and drop a card of your choice on the middle table area. Or select a card using A/D, submit using SPACE or go back using HOOK-COLLISION. When you selected the card using SPACE, you can choose what you want to do with the card using A/D again and submit using SPACE again.
	If you don't want to place any offense cards anymore, you can click End move (click on the card stack or trump card) in order to speed up the process. Otherwise wait 1 min. <br>
	Defender: You can either defend by dragging a card onto an offense card, or you can place a card with the same rank next to it in the table area to push the attack to the next person. <br>
	If you can't defend or do not want to, you can click in the table area to take the cards in order to speed up the process. Otherwise wait 1min.

* **Server-side translation** <br>
F-DDrace has a server-side translation feature using `/language`. <br>
You can specify a default language using `sv_default_language` or turn on `sv_language_suggestion` to receive a vote question on join. <br>
Language files are stored in `datasrc/languages` and are moved to `data/languages` upon compiling, like the rest of `datasrc`.

* **Flags and weapon drops** <br>
You can drop flags using `F3` key (vote yes key) and weapons using `F4` key (vote no key). <br>
They interact with explosions, shotgun, speedups, teleporters, doors and portals, and can be collected by other players again. <br>
The flag has a special ability: It can be hooked by players once it's dropped. The flag hooking has been made as smooth as possible and shouldn't look buggy that much. <br>
You can easily spectate the flags using the spectate menu (`bind x +spectate`).

* **Persistent gameplay after server restart or reload** <br>
Things such as money drops and plots are saved and loaded upon server start so you don't lose important things. `sv_shutdown_save_tees 1` allows you to also save and load all players automatically, even upon restart everything is stored in files and later loaded and matched when you join.

* **Extra weapons** <br>
The inventory has been extended to support more than the regular weapons. If not placed in the map, most of the extra weapons are admin-only.

* **Many settings & commands** <br>
[Configuration/Settings](https://github.com/fokkonaut/F-DDrace?tab=readme-ov-file#configurationsettings)<br>
[Commands](https://github.com/fokkonaut/F-DDrace?tab=readme-ov-file#commands)<br>
[Chat commands](https://github.com/fokkonaut/F-DDrace?tab=readme-ov-file#chat-commands)<br>

<br><br>
# Configuration/Settings
<br>


## Account
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_accounts` | 0 | 1 | 1 | Whether accounts are activated or deactivated |
| `sv_acc_file_path` | - | - | "data/accounts" | The path where the server searches the account files (relative to binary) |
| `sv_data_save_interval` | 5 | 60 | 30 | Interval in minutes between data saves |
| `sv_donation_file_path` | - | - | "data" | The path where the server searches the for the donation file (relative to binary) |
| `sv_plot_file_path` | - | - | "data/plots" | The path where the server searches the plot files (relative to binary) |
| `sv_kill_logout` | 0 | 60 | 3 | Time in seconds a tee can kill after trying to logout (0 = disabled) |
| `sv_euro_mode` | 0 | 1 | 0 | Whether euro mode is enabled |
| `sv_euro_discount_percentage` | 0 | 100 | 0 | Euro discount percentage for shop (requires reload) |
| `sv_police_farm_limit` | 0 | 1 | 0 | Whether police farm tiles are limited to a dynamic number of players (disables silentfarm on police) |

## Experience Broadcast
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_exp_msg_color_text` | - | - | "999" | Text color for the experience broadcast |
| `sv_exp_msg_color_symbol` | - | - | "999" | Symbol color for the experience broadcast |
| `sv_exp_msg_color_value` | - | - | "595" | Value color for the experience broadcast |

## Money
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_money_drops_file_path` | - | - | "data/money_drops" | The path where the server searches the money drops file (relative to binary) |
| `sv_money_history_file_path` | - | - | "money_history" | The path to money history files (relative to dumps dir) |
| `sv_money_bank_mode` | 0 | 2 | 0 | Bank mode (0=no bank; bank=wallet, 1=normal bank, 2=instant deposit from farm) |
| `sv_money_farm_team` | 0 | 1 | 1 | Whether a player can farm money on a money tile while being in a ddrace team |

## Account System Ban
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_acc_sys_ban_registrations` | 0 | 10 | 3 | Max registrations per IP within 6 hours |
| `sv_acc_sys_ban_pw_fails` | 0 | 10 | 5 | Max password fails per IP within 6 hours |
| `sv_acc_sys_ban_pin_fails` | 0 | 10 | 3 | Max PIN fails per IP within 6 hours |

## Saved Tees
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_shutdown_save_tees` | 0 | 1 | 1 | Whether to save characters before shutdown/reload to load them again |
| `sv_saved_tees_file_path` | - | - | "savedtees" | The path to saved tees files (relative to dumps dir) |
| `sv_shutdown_save_tee_expire` | 1 | 168 | 1 | How many hours until a shutdown save expires |
| `sv_jail_save_tee_expire` | 1 | 168 | 24 | How many hours until a jail save expires |

## Shutdown Auto Reconnect
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_shutdown_auto_reconnect` | 0 | 1 | 0 | Whether shutdown will send a map change to time out client, so it'll reconnect (DDNet only) |

## Flags
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_flag_sounds` | 0 | 2 | 2 | Flag sounds on drop/pickup/respawn (0=off, 1=public sounds, 2=respawn public rest local) |
| `sv_flag_hooking` | 0 | 1 | 1 | Whether flags are hookable |
| `sv_flag_respawn_dropped` | 0 | 9999 | 90 | Time in seconds a dropped flag resets |

## Dummy
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_hide_dummies` | 0 | 1 | 0 | Whether to hide server-side dummies from scoreboard |
| `sv_default_dummies` | 0 | 1 | 1 | Whether to create default dummies for specific maps when the server starts |
| `sv_fake_dummy_ping` | 0 | 1 | 0 | Whether ping of server-side dummies are more natural or 0 |
| `sv_v3_offset_x` | 0 | 9999 | 0 | Offset X for the blmapV3 dummy |
| `sv_v3_offset_y` | 0 | 9999 | 0 | Offset Y for the blmapV3 dummy |
| `sv_dummy_bot_skin` | 0 | 1 | 0 | Whether dummies should have the bot skin applied (0.7 only) |
| `sv_dummy_blocking` | 0 | 1 | 0 | Whether blocking dummies increases killstreak and gives block points |
| `sv_hide_dummies_status` | 0 | 1 | 1 | Whether to hide server-side dummies from status command |

## Weapon Indicator
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_weapon_indicator_default` | 0 | 1 | 1 | Whether the weapon names are displayed in the broadcast |

## Drops
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_allow_empty_inventory` | 0 | 1 | 1 | Whether to allow dropping all your weapons, ending up with an empty inventory |
| `sv_drop_weapons` | 0 | 1 | 1 | Whether to allow dropping weapons with f4 |
| `sv_drops_on_death` | 0 | 1 | 1 | Whether there is a chance of dropping weapons on death (health and armor in survival, after 5min in no minigames) |
| `sv_destroy_drops_on_leave` | 0 | 1 | 0 | Destroy dropped weapons (hearts, shields) when their owner disconnects |
| `sv_max_weapon_drops` | 0 | 10 | 5 | Maximum amount of dropped weapons per player |
| `sv_max_pickup_drops` | 0 | 600 | 500 | Maximum amount of dropped hearts and shields |
| `sv_interactive_drops` | 0 | 1 | 1 | Whether dropped weapons, flags, money interact with shotgun and explosions |

## Vanilla
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_vanilla_mode_start` | 0 | 1 | 0 | Whether to set the players mode to vanilla on spawn or ddrace |

## Survival
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_survival_min_players` | 2 | MAX_CLIENTS | 4 | Minimum players to start a survival round |
| `sv_survival_lobby_countdown` | 5 | 120 | 15 | Number in seconds until the survival round starts |
| `sv_survival_round_time` | 1 | 20 | 2 | Time in minutes until deathmatch starts |
| `sv_survival_deathmatch_time` | 1 | 5 | 2 | Length of the deathmatch in minutes |

## Portal Rifle
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_portal_rifle_delay` | 0 | 60 | 10 | The minimum time in seconds between linking two portals with portal rifle |
| `sv_portal_radius` | 0 | 1024 | 46 | The radius of a portal for portal rifles |
| `sv_portal_detonation_linked` | 0 | 60 | 5 | Time in seconds linked portals detonate |
| `sv_portal_detonation` | 0 | 60 | 10 | Time in seconds unlinked portals detonate |
| `sv_portal_max_distance` | 50 | 1000 | 750 | Maximum distance to place a portal |
| `sv_portal_rifle_shop` | 0 | 1 | 1 | Whether portal rifle is in shop |
| `sv_portal_rifle_ammo` | 0 | 1 | 1 | Whether portal rifle entity respawns after x minutes and portal requires ammo |
| `sv_portal_rifle_respawn_time` | 1 | 999 | 15 | Time in minutes a portal rifle respawns after pickup |
| `sv_portal_through_door` | 0 | 1 | 0 | Whether portal rilfe can be used through a closed door (outside of plot only) |

## Portal Blocker
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_portal_blocker_detonation` | 0 | 999 | 30 | Time in seconds a portal blocker detonates |
| `sv_portal_blocker_max_length` | 0 | 999 | 15 | Maximum portal blocker length in blocks (0 = no limit) |

## Draw Editor
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_max_objects_plot_small` | 0 | 150 | 50 | Maximum amount of objects that can be placed within a small plot |
| `sv_max_objects_plot_big` | 0 | 500 | 150 | Maximum amount of objects that can be placed within a big plot |
| `sv_max_objects_free_draw` | 0 | 5000 | 5000 | Maximum amount of objects that can be placed in free draw |
| `sv_light_speedups` | 0 | 1 | 1 | Whether draw editor speedups use light mode (heavy mode not recommended) |
| `sv_light_teleporters` | 0 | 1 | 1 | Whether draw editor teleporters use light mode (heavy mode not recommended) |
| `sv_editor_preset_level` | AUTHED_NO | NUM_AUTHEDS-1 | AUTHED_ADMIN | Required auth level to use the draw editor preset save/load feature |
| `sv_clear_free_draw_level` | AUTHED_NO | NUM_AUTHEDS-1 | AUTHED_ADMIN | Required auth level to clear free draw area (clearplot 0) |
| `sv_editor_max_distance` | 0 | 99999 | 750 | Maximum distance to place something with draw editor |

## Taser Officers
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_police_taser_plot_raid` | 0 | 1 | 1 | Whether police taser can destroy plots of wanted players |
| `sv_plot_door_health` | 0 | 1000 | 30 | Initial plot door health to withstand tasers |

## Taser Battery
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_taser_battery_respawn_time` | 1 | 60 | 10 | Time in minutes a taser battery respawns after pickup |
| `sv_taser_strength_default` | 0 | 10 | 0 | Default taser strength for when a player is not logged in |

## Spawn Weapons
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_spawn_weapons` | 0 | 1 | 1 | Whether account spawn weapons will be given on spawn |
| `sv_slash_spawn` | 0 | 1 | 1 | Whether /spawn is activated (WARNING: can be abused to dodge specific tiles) |

## Snake
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_snake_auto_move` | 0 | 1 | 1 | Whether snake keeps last input or can stand still if no inputs applied |
| `sv_snake_speed` | 1 | 50 | 6 | Snake blocks per second speed |
| `sv_snake_diagonal` | 0 | 1 | 0 | Whether snake can move diagonally |
| `sv_snake_smooth` | 0 | 1 | 1 | Whether snake moves smoothly |

## Chat
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_ateveryone_level` | AUTHED_NO | NUM_AUTHEDS-1 | AUTHED_MOD | Required auth level to use @everyone in chat |
| `sv_chat_admin_ping_level` | AUTHED_NO | NUM_AUTHEDS-1 | AUTHED_NO | Required auth level to ping authed players in chat |
| `sv_lol_filter` | 0 | 1 | 1 | I like turtles. |
| `sv_local_chat` | 0 | 1 | 1 | Whether local chat is enabled (deactivates sv_authed_highlighted) |
| `sv_whisper_log` | 0 | 1 | 0 | Whether whisper messages get logged as well |
| `sv_join_msg_delay` | 0 | 60 | 0 | The time in seconds the player join message gets delayed (and sv_chat_initial_delay is imitated) |

## Status Recent
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_recently_left_save_time` | 5 | 300 | 120 | The time in seconds a recently left player is stored for moderating |

## Admin Highlight
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_authed_highlighted` | 0 | 1 | 1 | Whether authed players are highlighted in the scoreboard (deactivated by sv_local_chat) |

## Spawn Block
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_spawn_block_protection` | 0 | 1 | 1 | Whether spawnblocking in a given area will add escape time |
| `sv_spawnarea_low_x` | 0 | 9999 | 0 | Low X tile position of area for spawnblock protection |
| `sv_spawnarea_low_y` | 0 | 9999 | 0 | Low Y tile position of area for spawnblock protection |
| `sv_spawnarea_high_x` | 0 | 9999 | 0 | High X tile position of area for spawnblock protection |
| `sv_spawnarea_high_y` | 0 | 9999 | 0 | High Y tile position of area for spawnblock protection |

## Sevendown DDNet Clients
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_allow_sevendown` | 0 | 1 | 1 | Allows sevendown connections |
| `sv_map_window` | 0 | 100 | 15 | Map downloading send-ahead window |
| `sv_drop_old_clients` | 0 | 1 | 1 | Whether old and not fully supported clients are getting dropped |
| `sv_browser_score_fix` | 0 | 2 | 0 | Whether server tries to make clients display score correctly in browser (2=red color) |
| `sv_https_map_download_url` | - | - | "" | URL path to the maps folder |

## Antibot
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_antibot_treshold` | 0 | 16 | 0 | Treshold for antibot autoban (0=off) |
| `sv_antibot_ban_minutes` | 0 | 99999 | 10000 | Time in minutes a player gets banned for by antibot |
| `sv_antibot_reports_level` | AUTHED_NO | NUM_AUTHEDS-1 | AUTHED_ADMIN | Required auth level to see antibot reports |
| `sv_antibot_reports_filter` | 0 | 1 | 1 | Whether antibot reports are filtered if they are legit |

## Whois
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_whois_ip_entries` | 0 | 1999999 | 120000 | WhoIs IP entries |
| `sv_whois` | 0 | 1 | 0 | Whether WhoIs is enabled |
| `sv_whois_file` | - | - | "data" | WhoIs file |

## Bugs
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_weak_hook` | 0 | 1 | 0 | Whether everybody has the same hook strength and bounce or weak is also there |
| `sv_stoppers_passthrough` | 0 | 1 | 0 | Whether tees can pass through stoppers with enough speed |
| `sv_shotgun_bug` | 0 | 1 | 0 | Whether firing shotgun while standing in another tee gives an insane boost |

## Score
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_default_score_mode` | 0 | NUM_SCORE_MODES-1 | SCORE_LEVEL | Default score (0=time, 1=level, 2=blockpoints, 3=bonus) |
| `sv_allow_bonus_score_mode` | 0 | 1 | 0 | Whether bonus score can be seen using '/score' |

## No-Bonus Area
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_no_bonus_max_jumps` | 2 | 9999 | 5 | Maximum number of jumps in no-bonus area (treshold=0: set this amount, else: start score increase at this) |
| `sv_no_bonus_score_treshold` | 0 | 100 | 10 | Treshold value for bonus score in no-bonus area (0=bonus removal) |
| `sv_no_bonus_score_decrease` | 0 | 60 | 10 | Time in seconds between bonus score decrease |

## Grog
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_grog_price` | 1 | 50000 | 500 | Price per grog |
| `sv_grog_hold_limit` | 1 | 10 | 3 | Amount of grogs a player can carry |
| `sv_grog_min_permille_limit` | 0 | 39 | 6 | Divided by 10: Minimum legal permille limit, if exceeded=wanted |
| `sv_grog_force_hammer` | 0 | 1 | 0 | Whether holding grog forces to hold hammer, or can have no weapon (new DDNet can render tee without weapon) |

## Other
| Command | Min | Max | Default | Description |
|---------|-----|-----|---------|-------------|
| `sv_hide_minigame_players` | 0 | 1 | 1 | Whether players in different minigames are shown in the scoreboard |
| `sv_rainbow_speed_default` | 1 | 50 | 5 | Default speed for rainbow |
| `sv_old_jetpack_sound` | 0 | 1 | 0 | Whether to use the default gun sound for jetpack or another sound |
| `sv_block_points_delay` | 0 | 600 | 20 | Seconds a tee has to be alive in order to give block points to the killer |
| `sv_always_tele_weapon` | 0 | 1 | 1 | Whether tele weapons can be used on any block or only on marked ones |
| `sv_clan_protection` | 0 | 1 | 1 | Whether players have to use greensward skin for Chilli.* clantag |
| `sv_freeze_prediction` | 0 | 1 | 1 | Whether your tee bounces while moving in freeze |
| `sv_map_update_rate` | 1 | 100 | 15 | Player map update rate |
| `sv_helper_victim_me` | 0 | 1 | 0 | Victim for commands is always yourself when executing as helper |
| `sv_wallet_kill_protection` | 0 | 100000 | 10000 | Minimum wallet amount to trigger the kill protection (0 = disabled) |
| `sv_touched_kills` | 0 | 1 | 0 | Whether touching a tee without hooking or hammering can count as kill |
| `sv_tele_rifle_allow_blocks` | 0 | 1 | 0 | Whether you can teleport inside of blocks using tele rifle |
| `sv_allow_dummy` | 0 | 1 | 1 | Whether clients can connect their dummy to the server |
| `sv_minigame_afk_auto_leave` | 0 | 600 | 120 | Minigame auto leave when afk for x seconds (0=off) |


<br><br>
# Commands
<br>


## Weapons
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `allweapons` | "?v[id] ?i[spread]" | Gives all weapons and extra weapons to player v, or spread weapons | AUTHED_ADMIN |
| `unallweapons` | "?v[id]" | Takes all weapons and extra weapons from player v | AUTHED_ADMIN |
| `extraweapons` | "?v[id] ?i[spread]" | Gives all extra weapons to player v, or spread extra weapons | AUTHED_ADMIN |
| `unextraweapons` | "?v[id]" | Takes all extra weapons from player v | AUTHED_ADMIN |
| `plasmarifle` | "?v[id] ?i[spread]" | Gives a plasma rifle to player v, or spread plasma rifle | AUTHED_ADMIN |
| `unplasmarifle` | "?v[id]" | Takes the plasma rifle from player v | AUTHED_ADMIN |
| `heartgun` | "?v[id] ?i[spread]" | Gives a heart gun to player v, or spread heart gun | AUTHED_ADMIN |
| `unheartgun` | "?v[id]" | Takes the heart gun from player v | AUTHED_ADMIN |
| `straightgrenade` | "?v[id] ?i[spread]" | Gives a straight grenade to player v, or spread straight grenade | AUTHED_ADMIN |
| `unstraightgrenade` | "?v[id]" | Takes the straight grenade from player v | AUTHED_ADMIN |
| `telekinesis` | "?v[id]" | Gives telekinesis power to player v | AUTHED_ADMIN |
| `untelekinesis` | "?v[id]" | Takes telekinesis power from player v | AUTHED_ADMIN |
| `lightsaber` | "?v[id]" | Gives a lightsaber to player v | AUTHED_ADMIN |
| `unlightsaber` | "?v[id]" | Takes the lightsaber from player v | AUTHED_ADMIN |
| `portalrifle` | "?v[id]" | Gives a portal rifle to player v | AUTHED_ADMIN |
| `unportalrifle` | "?v[id]" | Takes the portal rifle from player v | AUTHED_ADMIN |
| `projectilerifle` | "?v[id] ?i[spread]" | Gives a projectile rifle to player v, or spread projectile rifle | AUTHED_ADMIN |
| `unprojectilerifle` | "?v[id]" | Takes the projectile rifle from player v | AUTHED_ADMIN |
| `ballgrenade` | "?v[id] ?i[spread]" | Gives a ball grenade to player v, or spread ball grenade | AUTHED_ADMIN |
| `unballgrenade` | "?v[id]" | Takes the ball grenade from player v | AUTHED_ADMIN |
| `telerifle` | "?v[id]" | Gives a tele rifle to player v | AUTHED_ADMIN |
| `untelerifle` | "?v[id]" | Takes the tele rifle from player v | AUTHED_ADMIN |
| `taser` | "?v[id]" | Gives a taser to player v | AUTHED_ADMIN |
| `untaser` | "?v[id]" | Takes the taser from player v | AUTHED_ADMIN |
| `lightninglaser` | "?v[id]" | Gives a lightning laser to player v | AUTHED_ADMIN |
| `unlightninglaser` | "?v[id]" | Takes the lightning laser from player v | AUTHED_ADMIN |
| `hammer` | "?v[id]" | Gives a hammer to player v | AUTHED_ADMIN |
| `gun` | "?v[id] ?i[spread]" | Gives a gun to player v, or spread gun | AUTHED_ADMIN |
| `unhammer` | "?v[id]" | Takes the hammer from player v | AUTHED_ADMIN |
| `ungun` | "?v[id]" | Takes the gun from player v | AUTHED_ADMIN |

## Draw Editor
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `draweditor` | "?v[id]" | Gives the editor to player v (Laser walls need switch layer in map) | AUTHED_ADMIN |
| `undraweditor` | "?v[id]" | Takes the editor from player v | AUTHED_ADMIN |

## Dummy
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `connectdummy` | "?i[amount] ?i[dummymode]" | Connects i dummies | AUTHED_ADMIN |
| `disconnectdummy` | "v[id]" | Disconnects dummy v | AUTHED_ADMIN |
| `dummymode` | "?v[id] ?i[dummymode]" | Sets or shows the dummymode of dummy v | AUTHED_ADMIN |
| `connectdefaultdummies` | "" | Connects default dummies | AUTHED_ADMIN |

## Tune Lock Player
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `tune_lock_pl` | "v[id] s[tuning] i[value]" | Tune for lock a variable to value for player v | AUTHED_ADMIN |
| `tune_lock_pl_reset` | "v[id] ?s[tuning]" | Reset all locked tuning variables to defaults for player v (specific or all) | AUTHED_ADMIN |
| `tune_lock_pl_dump` | "v[id]" | Dump lock tuning for player v | AUTHED_ADMIN |

## Power
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `forceflagowner` | "i[flag] ?i[id]" | Gives flag i to player i (0 = red, 1 = blue) (to return flag, set id = -1) | AUTHED_ADMIN |
| `say_by` | "v[id] r[text]" | Says a chat message as player v | AUTHED_ADMIN |
| `teecontrol` | "?v[id] ?i[forcedid]" | Control another tee | AUTHED_ADMIN |
| `set_minigame` | "v[id] s[minigame]" | Sets player v to minigame s | AUTHED_ADMIN |
| `set_no_bonus_area` | "?v[id]" | Sets player v no-bonus area state | AUTHED_ADMIN |
| `unset_no_bonus_area` | "?v[id]" | Unsets player v no-bonus area state | AUTHED_ADMIN |
| `redirect_port` | "i[port] ?v[id]" | Redirects player v to port i | AUTHED_ADMIN |

## Hide From Spec Counter
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `hide_from_spec_count` | "?i['0'|'1']" | Hides yourself from spec counter of others | AUTHED_HELPER |

## Saved Tees
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `save_drop` | "?v[id] ?f[hours] ?r[text]" | Saves stats of player v for i hours and kicks him | AUTHED_ADMIN |
| `list_saved_tees` | "" | List all saved players with basic info | AUTHED_MOD |

## 1vs1 Tournament
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `1vs1_global_create` | "?i[scorelimit] ?i[killborder]" | Creates a 1vs1 arena to let other people fight there | AUTHED_ADMIN |
| `1vs1_global_start` | "i[id] i[id]" | Puts two players in the previously created 1vs1 arena to fight | AUTHED_ADMIN |

## Jail
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `jail_arrest` | "v[id] i[seconds]" | Arrests player v for i minutes | AUTHED_ADMIN |
| `jail_release` | "v[id]" | Releases player v from jail | AUTHED_ADMIN |

## View Cursor
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `view_cursor` | "?i[id]" | View cursor of player i (-2 = off, -1 = everyone) | AUTHED_ADMIN |
| `view_cursor_zoomed` | "?i[id]" | View zoomed cursor of player i (-2 = off, -1 = everyone) | AUTHED_ADMIN |

## Whois
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `whois` | "i[mode] i[cutoff] r[name]" | Mode 0=ip, 1=name, cutoff 0=direct, 1=/24, 2=/16 | AUTHED_ADMIN |
| `whoisid` | "i[mode] i[cutoff] v[id]" | Mode 0=ip, 1=name, cutoff 0=direct, 1=/24, 2=/16 | AUTHED_ADMIN |

## Whitelist
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `whitelist_add` | "s[ip] ?s[reason]" | Adds address s to whitelist | AUTHED_ADMIN |
| `whitelist_remove` | "s[ip/index]" | Removes address s from whitelist | AUTHED_ADMIN |
| `whitelist` | "" | Shows whitelist for DNSBL/PGSC | AUTHED_ADMIN |

## Bot Lookup
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `bot_lookup` | "" | Bot lookup list | AUTHED_ADMIN |

## Account System Ban
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `acc_sys_unban` | "i[index]" | Unbans index i from account system bans | AUTHED_ADMIN |
| `acc_sys_bans` | "" | Shows list for account system bans | AUTHED_ADMIN |

## Plots
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `toteleplot` | "i[plotid] ?v[id]" | Teleports player v to plot i | AUTHED_ADMIN |
| `clearplot` | "i[plotid]" | Clears plot with id i | AUTHED_ADMIN |
| `plot_owner` | "s[username] i[plotid]" | Sets owner of plot i to account with username s | AUTHED_ADMIN |
| `plot_info` | "i[plotid]" | Shows information about plot i | AUTHED_ADMIN |
| `preset_list` | "" | Shows a list with presets to load from the draw editor | AUTHED_ADMIN |

## Designs
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `reload_designs` | "" | Reloads map designs | AUTHED_ADMIN |

## Grog
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `add_grog` | "?v[id]" | Gives a grog to player v | AUTHED_ADMIN |
| `set_permille` | "v[id] f[permille]" | Sets permille for player v | AUTHED_ADMIN |

## Fun
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `sound` | "i[sound]" | Plays the sound with id i | AUTHED_ADMIN |
| `lasertext` | "v[id] r[text]" | Sends a laser text | AUTHED_ADMIN |
| `sendmotd` | "v[id] i[footer] r[text]" | Sends a motd containing text r to player v (i=1 with footer) | AUTHED_ADMIN |
| `helicopter` | "?v[id]" | Spawns a helicopter at position of player v | AUTHED_ADMIN |
| `remove_helicopters` | "" | Removes all helicopters | AUTHED_ADMIN |
| `snake` | "?v[id]" | Toggles snake for player v | AUTHED_ADMIN |

## Zombie
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `force_transform_zombie` | "?v[id]" | Forces transformation to zombie for player v | AUTHED_ADMIN |
| `force_transform_human` | "?v[id]" | Forces transformation to human for player v | AUTHED_ADMIN |
| `set_double_xp_lifes` | "v[id] i[amount]" | Sets double xp lifes for player v | AUTHED_ADMIN |
| `set_taser_shield` | "v[id] i[percentage]" | Sets taser shield percentage for player v | AUTHED_ADMIN |

## Client Information
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `player_name` | "v[id] ?r[name]" | Sets name of player v | AUTHED_ADMIN |
| `player_clan` | "v[id] ?r[clan]" | Sets clan of player v | AUTHED_ADMIN |
| `player_skin` | "v[id] ?r[skin]" | Sets skin of player v | AUTHED_ADMIN |

## Info
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `playerinfo` | "v[id]" | Shows information about the player with client id v | AUTHED_ADMIN |
| `list` | "?s[filter]" | List connected players with optional case-insensitive substring matching filter | AUTHED_HELPER |

## Extras
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `item` | "v[id] i[item]" | Gives player v item i (-3=none, -2=heart, -1=armor, 0 and up=weapon id) | AUTHED_ADMIN |
| `invisible` | "?v[id]" | Toggles invisibility for player v | AUTHED_ADMIN |
| `hookpower` | "?s[power] ?v[id]" | Sets hook power for player v | AUTHED_ADMIN |
| `freezehammer` | "?v[id]" | Toggles freeze hammer for player v | AUTHED_ADMIN |
| `set_jumps` | "?v[id] ?i[jumps]" | Sets amount of jumps for player v | AUTHED_ADMIN |
| `infinitejumps` | "?v[id]" | Toggles infinite jumps for player v | AUTHED_ADMIN |
| `endlesshook` | "?v[id]" | Toggles endlesshook for player v | AUTHED_ADMIN |
| `jetpack` | "?v[id]" | Toggles jetpack for player v | AUTHED_ADMIN |
| `rainbowspeed` | "?v[id] ?i[speed]" | Sets the rainbow speed i for player v | AUTHED_ADMIN |
| `rainbow` | "?v[id]" | Toggles rainbow for player v | AUTHED_ADMIN |
| `infrainbow` | "?v[id]" | Toggles infinite rainbow for player v | AUTHED_ADMIN |
| `atom` | "?v[id]" | Toggles atom for player v | AUTHED_ADMIN |
| `trail` | "?v[id]" | Toggles trail for player v | AUTHED_ADMIN |
| `spookyghost` | "?v[id]" | Toggles spooky ghost for player v | AUTHED_ADMIN |
| `addmeteor` | "?v[id]" | Adds a meteors to player v | AUTHED_ADMIN |
| `addinfmeteor` | "?v[id]" | Adds an infinite meteors to player v | AUTHED_ADMIN |
| `removemeteors` | "?v[id]" | Removes all meteors from player v | AUTHED_ADMIN |
| `passive` | "?v[id]" | Toggles passive mode for player v | AUTHED_ADMIN |
| `vanillamode` | "?v[id]" | Activates vanilla mode for player v | AUTHED_ADMIN |
| `ddracemode` | "?v[id]" | Deactivates vanilla mode for player v | AUTHED_ADMIN |
| `bloody` | "?v[id]" | Toggles bloody for player v | AUTHED_ADMIN |
| `strongbloody` | "?v[id]" | Toggles strong bloody for player v | AUTHED_ADMIN |
| `alwaysteleweapon` | "?v[id]" | Lets player v always use tele weapons | AUTHED_ADMIN |
| `telegun` | "?v[id]" | Gives a tele gun to player v | AUTHED_ADMIN |
| `telegrenade` | "?v[id]" | Gives a tele grenade to player v | AUTHED_ADMIN |
| `telelaser` | "?v[id]" | Gives a tele laser to player v | AUTHED_ADMIN |
| `doorhammer` | "?v[id]" | Gives a door hammer to player v | AUTHED_ADMIN |
| `lovely` | "?v[id]" | Toggles lovely for player v | AUTHED_ADMIN |
| `rotatingball` | "?v[id]" | Toggles rotating ball for player v | AUTHED_ADMIN |
| `epiccircle` | "?v[id]" | Toggles epic circle for player v | AUTHED_ADMIN |
| `staffind` | "?v[id]" | Toggles staff indicator for player v | AUTHED_ADMIN |
| `rainbowname` | "?v[id]" | Toggles rainbow name for player v | AUTHED_ADMIN |
| `confetti` | "?v[id]" | Toggles confetti for player v | AUTHED_ADMIN |
| `sparkle` | "?v[id]" | Toggles sparkle for player v | AUTHED_ADMIN |

## Account
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `acc_logout_port` | "i[port]" | Logs out all accounts with last port i | AUTHED_ADMIN |
| `acc_logout` | "s[username]" | Logs out account s | AUTHED_ADMIN |
| `acc_disable` | "s[username]" | Enables or disables account s | AUTHED_ADMIN |
| `acc_info` | "s[username]" | Shows information about account s | AUTHED_ADMIN |
| `acc_add_euros` | "s[username] f[amount]" | Adds i euros to account s | AUTHED_ADMIN |
| `acc_edit` | "s[username] s[variable] ?r[value]" | Prints or changes the value of account variable | AUTHED_ADMIN |
| `acc_level_needed_xp` | "i[level]" | Shows how much XP is required to reach level i | AUTHED_ADMIN |


<br><br>
# Chat commands
Note: It might be helpful to simply write a chat command ingame without any parameter to see what it requests you to do, then follow those steps.
<br><br>


## Score
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `score` | "?s[mode]" | Changes the displayed score in scoreboard | AUTHED_NO |

## Stats
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `stats` | "?r[player name]" | Shows stats of player r | AUTHED_NO |
| `account` | "" | Shows information about your account | AUTHED_NO |

## Info
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `helptoggle` | "" | Shows help to enable spookyghost or portal blocker | AUTHED_NO |
| `police` | "?i[page]" | Shows information about police | AUTHED_NO |
| `vip` | "" | Shows information about VIP and VIP+ | AUTHED_NO |
| `spawnweapons` | "" | Shows information about spawn weapons | AUTHED_NO |

## Account
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `register` | "s[name] s[password] s[password]" | Register an account | AUTHED_NO |
| `login` | "s[name] s[password]" | Log into an account | AUTHED_NO |
| `logout` | "" | Log out of an account | AUTHED_NO |
| `changepassword` | "s[old-pw] s[new-pw] s[new-pw]" | Changes account password | AUTHED_NO |
| `contact` | "?r[option]" | Sets contact information for your account | AUTHED_NO |
| `pin` | "?s[pin]" | Sets a new security pin for your account | AUTHED_NO |
| `pay` | "i[amount] r[name]" | Pays i money to player r | AUTHED_NO |
| `money` | "?s['drop'] ?i[amount]" | Shows your current balance and last transactions, or drops i amount | AUTHED_NO |
| `portal` | "?s['drop'] ?i[amount]" | Shows your current portal battery amount, or drops i amount | AUTHED_NO |
| `taser` | "?s['drop'] ?i[amount]" | Shows your current taser stats, or drops i amount | AUTHED_NO |

## Room
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `room` | "s['invite'|'kick'] r[name]" | Invite or kick player r from the room | AUTHED_NO |
| `spawn` | "" | Teleport to spawn (-50.000 money) | AUTHED_NO |
| `silentfarm` | "?i['0'|'1']" | Mute sounds while on moneytile | AUTHED_NO |

## Plots
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `plot` | "?s[command] ?s[price|helpcmd|swapname] ?r[playername]" | Plot command | AUTHED_NO |
| `hidedrawings` | "?i['0'|'1']" | Whether drawings are hidden | AUTHED_NO |

## Extras
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `weaponindicator` | "?i['0'|'1']" | Tells you which weapon you are holding under the heart and armor bar | AUTHED_NO |
| `zoomcursor` | "?i['0'|'1']" | Whether to zoom the cursor as well, do not use with dynamic camera | AUTHED_NO |

## Other
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `resumemoved` | "?i['0'|'1']" | Whether to resume from pause when someone moved your tee (off by default), optional i = 0 for off else for on | AUTHED_NO |
| `mute` | "r[playername]" | Mutes player r until disconnect | AUTHED_NO |
| `design` | "?s[name]" | Changes map design or shows a list of available designs | AUTHED_NO |
| `language` | "?s[language]" | Changes language or shows a list of available languages | AUTHED_NO |
| `discord` | "" | Sends Discord invite link | AUTHED_NO |
| `shrug` | "" | ¯\\_(ツ)_/¯ | AUTHED_NO |
| `hidebroadcasts` | "?i['0'|'1']" | Whether to hide money, jail, escape broadcasts and show them in vote menu instead | AUTHED_NO |

## Minigames
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `minigames` | "" | Shows a list of all available minigames | AUTHED_NO |
| `leave` | "" | Leaves the current minigame | AUTHED_NO |
| `block` | "" | Joins the block minigame | AUTHED_NO |
| `survival` | "" | Joins the survival minigame | AUTHED_NO |
| `boomfng` | "" | Joins the boom fng minigame | AUTHED_NO |
| `fng` | "" | Joins the fng minigame | AUTHED_NO |
| `1vs1` | "?s[playername] ?i[scorelimit] ?i[killborder]" | Joins the 1vs1 minigame or accepts/creates a fight/new arena | AUTHED_NO |

## Account Top5s
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `top5level` | "?i[rank to start with]" | Shows top5 accounts sorted by level | AUTHED_NO |
| `top5points` | "?i[rank to start with]" | Shows top5 accounts sorted by block points | AUTHED_NO |
| `top5money` | "?i[rank to start with]" | Shows top5 accounts sorted by money | AUTHED_NO |
| `top5spree` | "?i[rank to start with]" | Shows top5 accounts sorted by killing spree | AUTHED_NO |
| `top5portal` | "?i[rank to start with]" | Shows top5 accounts sorted by portal batteries | AUTHED_NO |

## Police
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `policehelper` | "s[add|remove] r[name]" | Adds/removes player r to/from policehelpers | AUTHED_NO |
| `wanted` | "" | Shows a list of all players being wanted by police | AUTHED_NO |

## VIP
| Command | Params | Description | Access Level |
|---------|--------|-------------|--------------|
| `rainbow` | "" | Toggles rainbow for yourself | AUTHED_NO |
| `bloody` | "" | Toggles bloody for yourself | AUTHED_NO |
| `atom` | "" | Toggles atom for yourself | AUTHED_NO |
| `trail` | "" | Toggles trail for yourself | AUTHED_NO |
| `spreadgun` | "" | Toggles spread gun for yourself | AUTHED_NO |
| `rotatingball` | "" | Toggles rotating ball for yourself | AUTHED_NO |
| `epiccircle` | "" | Toggles epic circle for yourself | AUTHED_NO |
| `lovely` | "" | Toggles lovely for yourself | AUTHED_NO |
| `rainbowhook` | "" | Toggles rainbow hook for yourself | AUTHED_NO |
| `rainbowname` | "" | Toggles rainbow name for yourself | AUTHED_NO |
| `rainbowspeed` | "?i[speed]" | Sets rainbow speed for yourself | AUTHED_NO |
| `sparkle` | "" | Toggles sparkle for yourself | AUTHED_NO |


The list of [settings](https://github.com/fokkonaut/F-DDrace/blob/F-DDrace/src/game/variables.h) and [commands](https://github.com/fokkonaut/F-DDrace/blob/F-DDrace/src/game/ddracecommands.h) can be found in the source files when scrolling down a little bit.
