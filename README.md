GHost++
=======

GHost++ is a Warcraft III game hosting bot. As the original project on Google Code is no longer available, https://github.com/uakfdotb/ghostpp contains the most "official" version of GHost++.

* Github: https://github.com/uakfdotb/ghostpp
* Website: https://www.ghostpp.com/

GHost++ is no longer actively maintained. Nevertheless, GHost++ remains widely used for hosting Warcraft III games, and there are no major known bugs.

Here are alternatives that are actively being developed as of 31 August 2018:

* [Aura](https://github.com/Josko/aura-bot/): simple bot to host games from a server, with greatly modernized core but with many features (MySQL support, autohosting) stripped out
* [maxemann96/ghostpp](https://github.com/maxemann96/ghostpp): a few additional features like votestart, commands from terminal

What's New
----------

Discord Bot: An alternative of admin LAN game lobby.

See [DISCORD.md](DISCORD.md) for more detail.

1.30 Note
---------

For 1.30, put `Warcraft III.exe` in the `bot_war3path`. (You should name it `Warcraft III.exe` to avoid Battle.net connection issues, but you may also name it `warcraft.exe`.)

For automatic extraction of `common.j` and `blizzard.j` Also put the entire `Data` Subdirectory and the file `.build.info` in `bot_war3path`. (Actually, it is recommended that you extract common.j and blizzard.j yourself and put them in `bot_mapcfgpath`, since the new CASC file is very large.)

`War3Patch.mpq`, `War3x.mpq`, `war3.exe`, `game.dll`, and `storm.dll` are no longer needed.

Bot hanging on authenticating? The bncsutil in this repository has recently been updated, and fixes a bug in the authentication step that occasionally causes hanging. So try recompiling bncsutil.

1.29 Note
---------

For 1.29, put `Warcraft III.exe` in the `bot_war3path`. You may name it `Warcraft III.exe` or `warcraft.exe`.

`war3.exe`, `game.dll`, and `storm.dll` are no longer needed.

Also, use `War3x.mpq` instead of `War3Patch.mpq`. (Actually, it is recommended that you extract common.j and blizzard.j yourself and put them in `bot_mapcfgpath`, since the new MPQ file is very large. Make sure to also exclude War3x.mpq from `bot_war3path` so that the host bot does not attempt to read the archive.)

Docker
------

Dockerfile and docker-compose.yml are provided in this repository.

Docker bridge network is unsupported because broadcasts are bounded inside the bridge network.

To build and run GHost++ in a container:

    # Docker will mount ghost.cfg as a directory if it doesn't exist
    cp default.cfg ghost.cfg
    mkdir maps
    mkdir replays
    mkdir savegames
    mkdir war3
    sudo docker compose up

Compilation
-----------

This fork of GHost++ depends on cmake, libboost, libgmp, zlib, libbz2, libmysqlclient and libdpp. These steps should suffice to compile GHost++ on Ubuntu 22.04:

	sudo apt-get install -y cmake git wget libboost-all-dev build-essential libgmp-dev zlib1g-dev libbz2-dev libmysql++-dev

	wget -O libdpp.deb https://dl.dpp.dev/
    sudo apt-get install -y ./libdpp.deb
    rm ./libdpp.deb

	git clone https://github.com/slyh/ghostpp
	cd ghostpp

    cd bncsutil
    mkdir build
    cmake -G "Unix Makefiles" -B./build -H./
    cd build && make && sudo make install

    cd ../../StormLib/
    mkdir build
    cmake -G "Unix Makefiles" -B./build -H./
    cd build && make && sudo make install

    cd ../../CascLib/
    mkdir build
    cmake -G "Unix Makefiles" -B./build -H./
    cd build && make && sudo make install

	cd ../../ghost/
	make

See MANUAL or [the ghostpp.com wiki](https://www.ghostpp.com/wiki/index.php?title=Main_Page) for more in-depth but possibly outdated guides on other platforms.

Configuration
-------------

Generally, it is recommended to copy `default.cfg` to `ghost.cfg`, and update options there. GHost++ will read `default.cfg` first, and then overwrite the configuration with any options that appear in `ghost.cfg`.

Once configured, start GHost++:

	./ghost++

You can pass a command-line argument to use a different secondary configuration filename, instead of `ghost.cfg`:

	./ghost++ /opt/myconfig.cfg

Usage
-----

See MANUAL or [the ghostpp.com wiki](https://www.ghostpp.com/wiki/index.php?title=Main_Page) for instructions on using GHost++.
