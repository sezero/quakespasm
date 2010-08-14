  QuakeSpasm

  ____________________________________________________________

  Table of Contents


  1. About
  2. Hints
  3. Downloads
  4. Compiling
  5. Changes
     5.1 0.85.3
     5.2 0.85.2
     5.3 0.85.1

  6. Todo
  7. Links


  ______________________________________________________________________


  QuakeSpasm 0.85.3 (unreleased) (18 September 2010)


  1.  About


  QuakeSpasm is a Quake 1 engine based on the SDL port of FitzQuake.  It
  includes 64bit CPU cupport, a new sound driver, several networking
  fixes, and a few graphical niceities.

  <http://quakespasm.sourceforge.net>


  2.  Hints


  o  To disable some changes, use "quakespasm -fitz"

  o  For different sound drivers use "SDL_AUDIODRIVER=DRIVER
     ./quakespasm" , where DRIVER may be alsa, dsp, pulse, esd ...

  o  Shift+Escape draws the Console.

  o  From the console, use UP to browse the command line history, and
     TAB to autocomplete command and map names.

  o  Quakespasm allows loading new games (mods) on the fly with "game
     GAMENAME"


  3.  Downloads


  Source
  <http://prdownloads.sourceforge.net/quakespasm/quakespasm-0.85.3.tgz/download>
  Linux binary
  <http://prdownloads.sourceforge.net/quakespasm/quakespasm-0.85.3_linux.tgz/download>
  Windows
  <http://prdownloads.sourceforge.net/quakespasm/quakespasm-0.85.3_windows.zip/download>
  4.  Compiling

  Just extract the source tarball, then

  ______________________________________________________________________
  cd quakespasm-0.85.3
  make
  cp quakespasm /usr/local/games/quake (for eg)
  ______________________________________________________________________


  Use make DEBUG=1 for debugging.
  Optionally, HOME directory support can be enabled via the Misc/home-
  dir_0.patch diff.
  If for any reason this doesn't work, the project can also be built
  with Codeblocks.  This is a large, free, integrated development envi-
  ronment that requires wxWidgets and cmake to install.  The process is
  not for the faint hearted.


  5.  Changes


  5.1.  0.85.3


  o  Fix the "-dedicated" option (thanks Oz) and add platform specific
     networking code (default) rather than SDL_net

  o  Much needed OSX framework stuff from Kristian

  o  Add a persistent history feature (thanks Baker)

  o  Add a slider for scr_sbaralpha, which now defaults to 0.95
     (slightly transparent, allowing for nicer status bar)

  o  Allow for player messages longer than 32 chars

  o  Sockaddr fix for FreeBSD/OSX/etc networking

  o  Connect status bar size to the scale slider

  o  Include an ISNAN (is not-a-number) fix to catch the occassional
     quake C bug giving traceline problems

  o  Enumerate options menus

  o  Add a "prev weapon" menu item (from Sander)

  o  Small fix to Sound Block/Unblock on win32

  o  Lots of code fixes (some from uhexen2)

  o  Shift+Escape opens console

  o  Sys_Error calls Host_Shutdown


  5.2.  0.85.2


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


  5.3.  0.85.1


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


  o  Ogg/Mp3 music file support

  o  Tested HOME directory support

  o  Fix Centerview (V_StartPitchDrift)

  o  Fix compiler warnings

  o  Tie down the "-window/-fullscreen" options / behaviour ?


  7.  Links


  QuakeSpasm Homepage <http://quakespasm.sourceforge.net>
  QuakeSpasm Project page <http://sourceforge.net/projects/quakespasm>
  FitzQuake Homepage <http://www.celephais.net/fitzquake>
  Sleepwalkr's Original SDL Port
  <http://www.kristianduske.com/fitzquake>
  Baker's 0.85 Source Code
  <http://quakeone.com/proquake/src_other/fitzquake_sdl_20090510_src_beta_1.zip>
  Func SDL Fitzquake forum
  <http://www.celephais.net/board/view_thread.php?id=60172>
  Ozkan's email <mailto:gmail - dot - com - username - sezeroz>
  Stevenaaus email <mailto:yahoo - dot - com - username - stevenaaus>
  Kristian's email <mailto:gmail - dot - com - username - inveigle>


