# UA_source
![icon image](https://github.com/Marisa-Chan/UA_source/blob/master/svg/icon.svg?raw=true)

Opensource reimplementation of **UrbanAssault** engine. You needed copy of original game files for play.

**License** GPLv2

<a href="https://scan.coverity.com/projects/marisa-chan-ua_source">
  <img alt="Coverity Scan Build Status"
       src="https://scan.coverity.com/projects/8507/badge.svg"/>
</a>


**Dependings**: SDL3, SDL3_image, SDL3_ttf, SDL3_net, OpenGL, OpenAL, vorbisfile(ogg), ffmpeg


**Building**:
- linux native: **make nix**
- windows msys compile: **make msys**

[**Wiki page with instructions**](https://github.com/Marisa-Chan/UA_source/wiki)


**Fonts** directory contains:
- Liberation Mono, Liberation Sans, Liberation Serif.  Version 2.00.1
- Xolonium
- Press Start 2P
- Textar


**Licenses of used fonts**:
- The Liberation(tm) version 2.00.0 onward are Licensed under the SIL Open Font License, Version 1.1.
- Xolonium by Severin Meyer. License: SIL Open Font License (OFL)
- Press Start 2P by Cody 'CodeMan38' Boisclair. License: SIL Open Font License (OFL)
- Textar (https://github.com/yamacraft/textar-font) License: IPA-1


**Launch options**: start the game in the game's folder; `UA_source --help` lists the options. Options are `--name`, with a value as `--name VALUE` or `--name=VALUE`. The original game's `-env DIR` works as well, so `UA_source -env Ghor_env` and `UA_source --env Taer_env` play the expansions. `UA_source --level 41` starts level 41 straight away, and `--load SAVE/<profile>/0.sgm` a saved game; the game quits after the level, and nothing is written to the profile. With `--headless` the level runs as fast as the machine can, with nothing shown or heard, which needs no display, GPU or sound device: `UA_source --headless --level 41 --time-limit 600` says on stdout how 10 minutes of game time went.


**Additional keys (nucleus.ini)**:
- gfx.blending   0(default)/1/2 - blending mode for transparent things. 0 - default multiply, 1 - additive, 2 - sharp edged
- gfx.solidfont   on/off(default) - enable(on) pointy font rasterizing without of pixel blending
- gfx.vsync 0/1(default)/2 - vertical sync type. 0 - disable, 1 - default sync, 2 - adaptive(if you've already missed the vertical retrace for a given frame, it swaps buffers immediately). Players turn vsync on and off in the settings screen; this is the default for profiles that have not set it, and 2 makes "on" adaptive
- gfx.maxfps 0(default) - maximal fps. 0 - a quarter above the refresh rate of the display, so vsync paces the game; above 1000 or below 0 - no limit. Players set it as the frame rate limit in the settings screen; this is the default for profiles that have not set it
- gfx.filter, gfx.movie_player, audio.channels, game.timeline - texture filtering, the intro and mission movies, how many sounds play at once (1-16), and how long enemy host stations wait after each decision before making another of its kind: up to the timeline in milliseconds (600000 by default), less their effort for it. Players set these in the settings screen; these are the defaults for profiles that have not set them
- gfx.color_effects 0(default)/1/2 - emulate color effects like in original game in software rendering. 0 - disable, 1 - affects all, 2 - do not affect GUI
- gfx.color_eff_pwr[1] 0-100(default) - power of red color effect
- gfx.color_eff_pwr[2] 0-100(default) - power of blue color effect
- gfx.color_eff_pwr[3] 0-100(default) - power of green color effect
- gfx.color_eff_pwr[4] 0-100(default) - power of inverse color effect
- gfx.color_eff_pwr[5] 0-100(default) - power of invdark color effect
- gfx.color_eff_pwr[6] 0-100(default) - power of sw color effect
- gfx.color_eff_pwr[7] 0-100(default) - power of invtuerk color effect
- particles.limit 5000(default) - limit amount of particles 
- gfx.vbo on(default)/off - disable modern VBO(upload vertex data into video card)



- world.ini : begin_misc : fix_weapon_radius - on/off(default) - enable(on) fix of original bug that prevent working of different weapon radius
- world.ini : begin_misc : hidden_fractions - mask for hidden fractions for whole world
- levelfile : begin_level : hidden_fractions - mask for hidden fractions for this level
- vehicle : hidden (yes/no) - hide this unit type
- vehicle : unhide_radar (0 disable) - unhide number of sectors

