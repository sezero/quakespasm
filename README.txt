  QuakeSpasm

  ____________________________________________________________

  Table of Contents


  1. About
  2. Downloads
  3. Hints
     3.1 Music Playback

  4. Compiling
  5. Changes
     5.1 0.85.4
     5.2 0.85.3
     5.3 0.85.2
     5.4 0.85.1

  6. Todo
  7. Links


  ______________________________________________________________________


  QuakeSpasm 0.85.4 (19 February 2011)


  1.  About


  QuakeSpasm is a Quake 1 engine based on the SDL port of FitzQuake.  It
  includes 64bit CPU cupport, a new sound driver, several networking
  fixes, and a few graphical niceities.

  <http://quakespasm.sourceforge.net>


  2.  Downloads


  Source
  <http://sourceforge.net/projects/quakespasm/files/quakespasm-0.85.4.tgz/download>
  Linux binary
  <http://sourceforge.net/projects/quakespasm/files/quakespasm-0.85.4_linux.tgz/download>
  Windows
  <http://sourceforge.net/projects/quakespasm/files/quakespasm-0.85.4_windows.zip/download>
  OSX universal binary
  <http://sourceforge.net/projects/quakespasm/files/quakespasm-0.85.4_osx.zip/download>


  3.  Hints


   Visit the FitzQuake Homepage <http://www.celephais.net/fitzquake> for
  a full run-down of the engine's commands and variables.


  o  To disable some changes, use "quakespasm -fitz"

  o  For different sound drivers use "SDL_AUDIODRIVER=DRIVER
     ./quakespasm" , where DRIVER may be alsa, dsp, pulse, esd ...
  o  Shift+Escape draws the Console.

  o  From the console, use UP to browse the command line history, and
     TAB to autocomplete command and map names.

  o  There is currently no CD Music volume support. cd_sdl.c needs
     replacing with cd_linux.c, cd_bsd.c etc..

  o  In windows, alternative CD drives are accessible by "quakespasm
     -cddev F" (for example)

  o  Quakespasm allows loading new games (mods) on the fly with "game
     GAMENAME"

  3.1.  Music Playback

  Since version 0.85.4, Quakespasm can play back external MP3, OGG and
  Wave music files.

  o  Tracks should be named like "track02.ogg", "track03.ogg" ... (there
     is no track01) and placed into "Quake/id1/music".

  o  Unix users may need some extra libraries installed: libmad for MP3
     and libogg and libvorbis for OGG.

  o  Use the "-nomusic" option to disable this feature.


  4.  Compiling

  After extracting the source tarball, browse the Makefile and edit the
  music streaming options, then

  ______________________________________________________________________
  make
  cp quakespasm /usr/local/games/quake (for example)
  ______________________________________________________________________


  Compile time options include

  o  make DEBUG=1 for debugging

  o  make SDLNET=1 to enable SDL_net (Otherwise the socket api will be
     used directly)

  o  make SDL_CONFIG=/PATH/TO/SDL-CONFIG for unusual SDL installs

  Streaming music playback requires "libmad" or "libmpg123" for MP3, and
  "libogg" and "libvorbis" for OGG files.

  HOME directory support can be enabled via the Misc/homedir_0.patch

  The project can also be built with Codeblocks (project files included).


  5.  Changes


  5.1.  0.85.4


  o  Implement music (ogg, mp3, wav) playback

  o  A better fix for the infamous SV_TouchLinks problem, no more hard
     lockups with maps such as "whiteroom"

  o  Add support for mouse buttons 4 and 5

  o  Fix the "unalias" console command

  o  Restore the "screen size" menu item

  o  Fixed an erroneous protocol check in the server code

  o  Revised lit file loading, the lit file must be from the same game
     directory as the map itself

  o  Fixed rest of the compiler warnings

  o  Other minor sound and cdaudio updates


  5.2.  0.85.3


  o  Fix the "-dedicated" option (thanks Oz) and add platform specific
     networking code (default) rather than SDL_net

  o  Much needed OSX framework stuff from Kristian

  o  Add a persistent history feature (thanks Baker)

  o  Add a slider for scr_sbaralpha, which now defaults to 0.95
     (slightly transparent, allowing for a nicer status bar)

  o  Allow player messages longer than 32 chars

  o  Sockaddr fix for FreeBSD/OSX/etc networking

  o  Connect status bar size to the scale slider

  o  Include an ISNAN (is not-a-number) fix to catch the occassional
     quake C bug giving traceline problems

  o  Enumerate options menus

  o  Add a "prev weapon" menu item (from Sander)

  o  Small fix to Sound Block/Unblock on win32

  o  Lots of code fixes (some from uhexen2)

  o  Sys_Error calls Host_Shutdown

  o  Added MS Visual Studio support

  o  Add a "-cd" option to let the CD Player work in dedicated mode, and
     some other CD tweaks.


  5.3.  0.85.2


  o  Replace the old "Screen size" slider with a "Scale" slider

  o  Don't constantly open and close condebug log

  o  Heap of C clean-ups

  o  Fix mapname sorting

  o  Alias the "mods" command to "games"

  o  Block/Unblock sound upon focus loss/gain

  o  NAT fix (networking protocol fix)

  o  SDLNet_ResolveHost bug-fix allowing connection to ports other than
     26000

  o  sv_main.c (localmodels) Bumped array size from 5 to 6 in order for
     it to operate correctly with the raised limits of fitzquake-0.85

  o  Accept commandline options like "+connect ip:port"

  o  Add OSX Makefile (tested?)


  5.4.  0.85.1


  o  64 bit CPU support

  o  Restructured SDL sound driver

  o  Custom conback

  o  Tweaked the command line completion , and added a map/changelevel
     autocompletion function

  o  Alt+Enter toggles fullscreen

  o  Disable Draw_BeginDisc which causes core dumps when called
     excessively

  o  Show helpful info on start-up

  o  Include real map name (sv.name) and skill in the status bar

  o  Remove confirm quit dialog

  o  Don't spam the console with PackFile seek requests

  o  Default to window mode

  o  Withdraw console when playing demos

  o  Don't play demos on program init

  o  Default Heapsize is 64meg

  o  Changes to default console alpha, speed

  o  Changes to cvar persistence gl_flashblend(default 0), r_shadow,
     r_wateralpha, r_dynamic, r_novis


  6.  Todo


  o  Add HoT's first person camera (and menu item)

  o  Native CD audio support (if desired). cd_sdl.c doesn't have proper
     volume controls

  o  Test usb keyboards. Do the keypads work ? Make the OSX apple key
     work.

  o  Tested HOME directory support

  o  Finalize OSX automatic updating feature

  o  Fix Centerview (V_StartPitchDrift)


  o  There is still an unecessary screen render on program init under
     some conditions, when using the "-window/-fullscreen" options.


  7.  Links


  QuakeSpasm Homepage <http://quakespasm.sourceforge.net>
  QuakeSpasm Project page <http://sourceforge.net/projects/quakespasm>
  FitzQuake Homepage <http://www.celephais.net/fitzquake>
  Sleepwalkr's Original SDL Port
  <http://www.kristianduske.com/fitzquake>
  Baker's 0.85 Source Code
  <http://quakeone.com/proquake/src_other/fitzquake_sdl_20090510_src_beta_1.zip>
  Func Quakespasm forum
  <http://www.celephais.net/board/view_thread.php?id=60452>
  Func SDL Fitzquake forum
  <http://www.celephais.net/board/view_thread.php?id=60172>
  Ozkan's email <mailto:gmail - dot - com - username - sezeroz>
  Stevenaaus email <mailto:yahoo - dot - com - username - stevenaaus>
  Kristian's email <mailto:gmail - dot - com - username - inveigle>


