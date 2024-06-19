F-DDrace [![](https://github.com/fokkonaut/F-DDrace/workflows/Build/badge.svg)](https://github.com/fokkonaut/F-DDrace/actions?query=workflow%3ABuild+event%3Apush+branch%3AF-DDrace)
=========

F-DDrace is a modification of Teeworlds, developed by fokkonaut. <br>
Discord: fokkonaut (old: #5556) or https://discord.gg/qccRrtb
	
History
=======

My Teeworlds coding history started in the beginning of 2018. ChillerDragon has helped me a lot and introduced me into the world of Teeworlds coding and coding in general. <br>
In the beginning I started with some smaller pull requests on [DDNetPP](https://github.com/DDNetPP/DDNetPP). My first commit in DDNetPP was on January 19, 2018. <br>
This being said, I implemented my first features in DDNetPP, which is also the reason why DDNetPP and F-DDrace have many similarities. I liked the idea of the mod and played it myself for several years. At some point ChillerDragon motivated me to create my own modification where I could really express myself and do things the way I want them to be. Thats when [BlockDDrace](https://github.com/fokkonaut/BlockDDrace) was born with it's first commit on March 15, 2018. <br>
After 975 commits in BlockDDrace, I wanted to create a new modification based on Vanilla 0.7, because around that time the Teeworlds GitHub was active again and 0.7 got released. <br>
That's when F-DDrace was born with it's first commit on August 14, 2019. I manually ported various parts of the DDNet code to the Vanilla 0.7 code base and on top of that came BlockDDrace. From then on, F-DDrace has been developed more and more and became what it is today: My own personal playground to learn coding, create fun things and to provide something unique for the players. That's also the reason my server is called `fokkonaut's playground`. <br>
At the time I am writing this right now (February 18, 2023), I made more than 2935 commits on F-DDrace.

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

* **Minigames** <br>
You can find a list of minigames using `/minigames`. When joining a minigame, your complete tee status including position, weapons, extras, etc. will be saved and can be loaded later when leaving the minigame again using `/leave`. Nothing will be missed and you can continue playing as before.

* **1vs1 minigame** <br>
The reason why the 1vs1 minigame deserves it's own entry in this list is simple. It's innovative and unique. You can create a 1vs1 lobby using `/1vs1 <name>` and what's so special about it is, that you can take ANY place from the whole map to play a 1vs1 there. You can enlarge the area using zoom and adjust weapons to be used during the round.

* **Flags and weapon drops** <br>
You can drop flags using `F3` key (vote yes key) and weapons using `F4` key (vote no key). <br>
They interact with explosions, shotgun, speedups, teleporters, doors and portals, and can be collected by other players again. <br>
The flag has a special ability: It can be hooked by players once it's dropped. The flag hooking has been made as smooth as possible and shouldn't look buggy that much. <br>
You can easily spectate the flags using the spectate menu (`bind x +spectate`).

* **Extra weapons** <br>
The inventory has been extended to support more than the regular weapons. If not placed in the map, most of the extra weapons are admin-only.

* **Persistent gameplay after server restart or reload** <br>
Things such as money drops and plots are saved and loaded upon server start so you don't lose important things. `sv_shutdown_save_tees 1` allows you to also save and load all players automatically, even upon restart everything is stored in files and later loaded and matched when you join.

* **Many settings and commands** <br>
The list of [settings](https://github.com/fokkonaut/F-DDrace/blob/F-DDrace/src/game/variables.h) and [commands](https://github.com/fokkonaut/F-DDrace/blob/F-DDrace/src/game/ddracecommands.h) can be found in the source files when scrolling down a little bit.
