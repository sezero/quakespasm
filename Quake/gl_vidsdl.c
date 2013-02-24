/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010 Ozkan Sezer and Steven Atkinson

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// gl_vidsdl.c -- SDL GL vid component

#include "quakedef.h"
#include "cfgfile.h"
#include "bgmusic.h"
#include "resource.h"
#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#include <SDL/SDL.h>
#else
#include "SDL.h"
#endif

#define MAX_MODE_LIST	600 //johnfitz -- was 30
#define MAX_BPPS_LIST	5
#define WARP_WIDTH		320
#define WARP_HEIGHT		200
#define MAXWIDTH		10000
#define MAXHEIGHT		10000

#define SDL_DEFAULT_FLAGS	SDL_OPENGL

typedef struct {
	int			width;
	int			height;
	int			bpp;
} vmode_t;

static const char *gl_vendor;
static const char *gl_renderer;
static const char *gl_version;
static const char *gl_extensions;
static char * gl_extensions_nice;

static vmode_t	modelist[MAX_MODE_LIST];
static int		nummodes;

static qboolean	vid_initialized = false;

static SDL_Surface	*draw_context;

static qboolean	vid_locked = false; //johnfitz
static qboolean	vid_changed = false;

static qboolean	fullsbardraw = false;

static void VID_Menu_Init (void); //johnfitz
static void VID_Menu_f (void); //johnfitz
static void VID_MenuDraw (void);
static void VID_MenuKey (int key);

static void ClearAllStates (void);
static void GL_Init (void);
static void GL_SetupState (void); //johnfitz

viddef_t	vid;				// global video state
modestate_t	modestate = MS_UNINIT;
qboolean	scr_skipupdate;

qboolean isIntelVideo = false; //johnfitz -- intel video workarounds from Baker
qboolean gl_mtexable = false;
qboolean gl_texture_env_combine = false; //johnfitz
qboolean gl_texture_env_add = false; //johnfitz
qboolean gl_swap_control = false; //johnfitz
qboolean gl_anisotropy_able = false; //johnfitz
float gl_max_anisotropy; //johnfitz

PFNGLMULTITEXCOORD2FARBPROC GL_MTexCoord2fFunc = NULL; //johnfitz
PFNGLACTIVETEXTUREARBPROC GL_SelectTextureFunc = NULL; //johnfitz

//====================================

//johnfitz -- new cvars
static cvar_t	vid_fullscreen = {"vid_fullscreen", "0", CVAR_ARCHIVE};	// QuakeSpasm, was "1"
static cvar_t	vid_width = {"vid_width", "800", CVAR_ARCHIVE};		// QuakeSpasm, was 640
static cvar_t	vid_height = {"vid_height", "600", CVAR_ARCHIVE};	// QuakeSpasm, was 480
static cvar_t	vid_bpp = {"vid_bpp", "16", CVAR_ARCHIVE};
static cvar_t	vid_vsync = {"vid_vsync", "0", CVAR_ARCHIVE};
//johnfitz

cvar_t		vid_gamma = {"gamma", "1", CVAR_ARCHIVE}; //johnfitz -- moved here from view.c

//==========================================================================
//
//  HARDWARE GAMMA -- johnfitz
//
//==========================================================================

static unsigned short vid_gamma_red[256];
static unsigned short vid_gamma_green[256];
static unsigned short vid_gamma_blue[256];

static unsigned short vid_sysgamma_red[256];
static unsigned short vid_sysgamma_green[256];
static unsigned short vid_sysgamma_blue[256];

static int vid_gammaworks;

/*
================
VID_Gamma_SetGamma -- apply gamma correction
================
*/
static void VID_Gamma_SetGamma (void)
{
	if (draw_context && vid_gammaworks)
	{
		if (SDL_SetGammaRamp(&vid_gamma_red[0], &vid_gamma_green[0], &vid_gamma_blue[0]) == -1)
			Con_Printf ("VID_Gamma_SetGamma: failed on SDL_SetGammaRamp\n");
	}
}

/*
================
VID_Gamma_Restore -- restore system gamma
================
*/
static void VID_Gamma_Restore (void)
{
	if (draw_context && vid_gammaworks)
	{
		if (SDL_SetGammaRamp(&vid_sysgamma_red[0], &vid_sysgamma_green[0], &vid_sysgamma_blue[0]) == -1)
			Con_Printf ("VID_Gamma_Restore: failed on SDL_SetGammaRamp\n");
	}
}

/*
================
VID_Gamma_Shutdown -- called on exit
================
*/
static void VID_Gamma_Shutdown (void)
{
	VID_Gamma_Restore ();
}

/*
================
VID_Gamma_f -- callback when the cvar changes
================
*/
static void VID_Gamma_f (cvar_t *var)
{
	int i;

	for (i = 0; i < 256; i++)
	{
		vid_gamma_red[i] =
			CLAMP(0, (int) (255 * pow((i + 0.5)/255.5, var->value) + 0.5), 255) << 8;
		vid_gamma_green[i] = vid_gamma_red[i];
		vid_gamma_blue[i] = vid_gamma_red[i];
	}

	VID_Gamma_SetGamma ();
}

/*
================
VID_Gamma_Init -- call on init
================
*/
static void VID_Gamma_Init (void)
{
	vid_gammaworks = false;

	if (SDL_GetGammaRamp (&vid_sysgamma_red[0], &vid_sysgamma_green[0], &vid_sysgamma_blue[0]) != -1)
		vid_gammaworks = true;

	Cvar_RegisterVariable (&vid_gamma);
	Cvar_SetCallback (&vid_gamma, VID_Gamma_f);
}

/*
================
VID_ValidMode
================
*/
static qboolean VID_ValidMode (int width, int height, int bpp, qboolean fullscreen)
{
	Uint32 flags = SDL_DEFAULT_FLAGS;

	if (width < 320)
		return false;

	if (height < 200)
		return false;

	switch (bpp)
	{
	case 16:
	case 24:
	case 32:
		break;
	default:
		return false;
	}

	if (fullscreen)
		flags |= SDL_FULLSCREEN;

	if (!SDL_VideoModeOK(width, height, bpp, flags))
		return false;

	return true;
}

/*
================
VID_SetMode
================
*/
static int VID_SetMode (int width, int height, int bpp, qboolean fullscreen)
{
	int		temp;
	Uint32	flags = SDL_DEFAULT_FLAGS;
	char		caption[50];

	if (fullscreen)
		flags |= SDL_FULLSCREEN;

	// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause ();
	BGM_Pause ();

	//
	// swap control (the "before SDL_SetVideoMode" part)
	//
	gl_swap_control = true;
	if (SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, (vid_vsync.value) ? 1 : 0) == -1)
		gl_swap_control = false;

	draw_context = SDL_SetVideoMode(width, height, bpp, flags);
	if (!draw_context)
		Sys_Error ("Couldn't set video mode");

	q_snprintf(caption, sizeof(caption), "QuakeSpasm %1.2f.%d", (float)FITZQUAKE_VERSION, QUAKESPASM_VER_PATCH);
	SDL_WM_SetCaption(caption, caption);

	vid.width = draw_context->w;
	vid.height = draw_context->h;
	vid.conwidth = vid.width & 0xFFFFFFF8;
	vid.conheight = vid.conwidth * vid.height / vid.width;
	vid.numpages = 2;

	modestate = draw_context->flags & SDL_FULLSCREEN ? MS_FULLSCREEN : MS_WINDOWED;

	CDAudio_Resume ();
	BGM_Resume ();
	scr_disabled_for_loading = temp;

// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	Con_SafePrintf ("Video mode %dx%dx%d initialized\n",
				draw_context->w,
				draw_context->h,
				draw_context->format->BitsPerPixel);

	vid.recalc_refdef = 1;

// no pending changes
	vid_changed = false;

	return true;
}

/*
===================
VID_Changed_f -- kristian -- notify us that a value has changed that requires a vid_restart
===================
*/
static void VID_Changed_f (cvar_t *var)
{
	vid_changed = true;
}

/*
===================
VID_Restart -- johnfitz -- change video modes on the fly
===================
*/
static void VID_Restart (void)
{
	int width, height, bpp;
	qboolean fullscreen;

	if (vid_locked || !vid_changed)
		return;

	width = (int)vid_width.value;
	height = (int)vid_height.value;
	bpp = (int)vid_bpp.value;
	fullscreen = vid_fullscreen.value ? true : false;

//
// validate new mode
//
	if (!VID_ValidMode (width, height, bpp, fullscreen))
	{
		Con_Printf ("%dx%dx%d %s is not a valid mode\n",
				width, height, bpp, fullscreen? "fullscreen" : "windowed");
		return;
	}

//
// set new mode
//
	VID_SetMode (width, height, bpp, fullscreen);

	GL_Init ();
	TexMgr_ReloadImages ();
	GL_SetupState ();

	//warpimages needs to be recalculated
	TexMgr_RecalcWarpImageSize ();

	//conwidth and conheight need to be recalculated
	vid.conwidth = (scr_conwidth.value > 0) ? (int)scr_conwidth.value : (scr_conscale.value > 0) ? (int)(vid.width/scr_conscale.value) : vid.width;
	vid.conwidth = CLAMP (320, vid.conwidth, vid.width);
	vid.conwidth &= 0xFFFFFFF8;
	vid.conheight = vid.conwidth * vid.height / vid.width;
//
// keep cvars in line with actual mode
//
	VID_SyncCvars();
//
// update mouse grab
//
	if (key_dest == key_console || key_dest == key_menu)
	{
		if (modestate == MS_WINDOWED)
			IN_Deactivate(true);
		else if (modestate == MS_FULLSCREEN)
			IN_Activate();
	}
}

/*
================
VID_Test -- johnfitz -- like vid_restart, but asks for confirmation after switching modes
================
*/
static void VID_Test (void)
{
	int old_width, old_height, old_bpp, old_fullscreen;

	if (vid_locked || !vid_changed)
		return;
//
// now try the switch
//
	old_width = draw_context->w;
	old_height = draw_context->h;
	old_bpp = draw_context->format->BitsPerPixel;
	old_fullscreen = draw_context->flags & SDL_FULLSCREEN ? true : false;

	VID_Restart ();
	SCR_UpdateScreen ();

	//pop up confirmation dialoge
	if (!SCR_ModalMessage("Would you like to keep this\nvideo mode? (y/n)\n", 5.0f))
	{
		//revert cvars and mode
		Cvar_SetValueQuick (&vid_width, old_width);
		Cvar_SetValueQuick (&vid_height, old_height);
		Cvar_SetValueQuick (&vid_bpp, old_bpp);
		Cvar_SetQuick (&vid_fullscreen, old_fullscreen ? "1" : "0");
		VID_Restart ();
	}
}

/*
================
VID_Unlock -- johnfitz
================
*/
static void VID_Unlock (void)
{
	vid_locked = false;
	VID_SyncCvars();
}

//==============================================================================
//
//	OPENGL STUFF
//
//==============================================================================

/*
===============
GL_MakeNiceExtensionsList -- johnfitz
===============
*/
static char *GL_MakeNiceExtensionsList (const char *in)
{
	char *copy, *token, *out;
	int i, count;

	if (!in) return Z_Strdup("(none)");

	//each space will be replaced by 4 chars, so count the spaces before we malloc
	for (i = 0, count = 1; i < (int) strlen(in); i++)
	{
		if (in[i] == ' ')
			count++;
	}

	out = (char *) Z_Malloc (strlen(in) + count*3 + 1); //usually about 1-2k
	out[0] = 0;

	copy = (char *) Z_Strdup(in);
	for (token = strtok(copy, " "); token; token = strtok(NULL, " "))
	{
		strcat(out, "\n   ");
		strcat(out, token);
	}

	Z_Free (copy);
	return out;
}

/*
===============
GL_Info_f -- johnfitz
===============
*/
static void GL_Info_f (void)
{
	Con_SafePrintf ("GL_VENDOR: %s\n", gl_vendor);
	Con_SafePrintf ("GL_RENDERER: %s\n", gl_renderer);
	Con_SafePrintf ("GL_VERSION: %s\n", gl_version);
	Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions_nice);
}

/*
===============
GL_CheckExtensions
===============
*/
static qboolean GL_ParseExtensionList (const char *list, const char *name)
{
	const char	*start;
	const char	*where, *terminator;

	if (!list || !name || !*name)
		return false;
	if (strchr(name, ' ') != NULL)
		return false;	// extension names must not have spaces

	start = list;
	while (1) {
		where = strstr (start, name);
		if (!where)
			break;
		terminator = where + strlen (name);
		if (where == start || where[-1] == ' ')
			if (*terminator == ' ' || *terminator == '\0')
				return true;
		start = terminator;
	}
	return false;
}

static void GL_CheckExtensions (void)
{
	int swap_control;

	//
	// multitexture
	//
	if (COM_CheckParm("-nomtex"))
		Con_Warning ("Mutitexture disabled at command line\n");
	else if (GL_ParseExtensionList(gl_extensions, "GL_ARB_multitexture"))
	{
		GL_MTexCoord2fFunc = (PFNGLMULTITEXCOORD2FARBPROC) SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
		GL_SelectTextureFunc = (PFNGLACTIVETEXTUREARBPROC) SDL_GL_GetProcAddress("glActiveTextureARB");
		if (GL_MTexCoord2fFunc && GL_SelectTextureFunc)
		{
			Con_Printf("FOUND: ARB_multitexture\n");
			TEXTURE0 = GL_TEXTURE0_ARB;
			TEXTURE1 = GL_TEXTURE1_ARB;
			gl_mtexable = true;
		}
		else
		{
			Con_Warning ("Couldn't link to multitexture functions\n");
		}
	}
	else if (GL_ParseExtensionList(gl_extensions, "GL_SGIS_multitexture"))
	{
		GL_MTexCoord2fFunc = (PFNGLMULTITEXCOORD2FARBPROC) SDL_GL_GetProcAddress("glMTexCoord2fSGIS");
		GL_SelectTextureFunc = (PFNGLACTIVETEXTUREARBPROC) SDL_GL_GetProcAddress("glSelectTextureSGIS");
		if (GL_MTexCoord2fFunc && GL_SelectTextureFunc)
		{
			Con_Printf("FOUND: SGIS_multitexture\n");
			TEXTURE0 = TEXTURE0_SGIS;
			TEXTURE1 = TEXTURE1_SGIS;
			gl_mtexable = true;
		}
		else
		{
			Con_Warning ("Couldn't link to multitexture functions\n");
		}
	}
	else
	{
		Con_Warning ("multitexture not supported (extension not found)\n");
	}

	//
	// texture_env_combine
	//
	if (COM_CheckParm("-nocombine"))
		Con_Warning ("texture_env_combine disabled at command line\n");
	else if (GL_ParseExtensionList(gl_extensions, "GL_ARB_texture_env_combine"))
	{
		Con_Printf("FOUND: ARB_texture_env_combine\n");
		gl_texture_env_combine = true;
	}
	else if (GL_ParseExtensionList(gl_extensions, "GL_EXT_texture_env_combine"))
	{
		Con_Printf("FOUND: EXT_texture_env_combine\n");
		gl_texture_env_combine = true;
	}
	else
	{
		Con_Warning ("texture_env_combine not supported\n");
	}

	//
	// texture_env_add
	//
	if (COM_CheckParm("-noadd"))
		Con_Warning ("texture_env_add disabled at command line\n");
	else if (GL_ParseExtensionList(gl_extensions, "GL_ARB_texture_env_add"))
	{
		Con_Printf("FOUND: ARB_texture_env_add\n");
		gl_texture_env_add = true;
	}
	else if (GL_ParseExtensionList(gl_extensions, "GL_EXT_texture_env_add"))
	{
		Con_Printf("FOUND: EXT_texture_env_add\n");
		gl_texture_env_add = true;
	}
	else
	{
		Con_Warning ("texture_env_add not supported\n");
	}

	//
	// swap control (the "after SDL_SetVideoMode" part)
	//
	if (!gl_swap_control)
	{
		Con_Warning ("vertical sync not supported (SDL_GL_SetAttribute failed)\n");
	}
	else if (SDL_GL_GetAttribute(SDL_GL_SWAP_CONTROL, &swap_control) == -1)
	{
		gl_swap_control = false;
		Con_Warning ("vertical sync not supported (SDL_GL_GetAttribute failed)\n");
	}
	else if ((vid_vsync.value && swap_control != 1) || (!vid_vsync.value && swap_control != 0))
	{
		gl_swap_control = false;
		Con_Warning ("vertical sync not supported (swap_control doesn't match vid_vsync)\n");
	}
	else
	{
		Con_Printf("FOUND: SDL_GL_SWAP_CONTROL\n");
	}

	//
	// anisotropic filtering
	//
	if (GL_ParseExtensionList(gl_extensions, "GL_EXT_texture_filter_anisotropic"))
	{
		float test1,test2;
		GLuint tex;

		// test to make sure we really have control over it
		// 1.0 and 2.0 should always be legal values
		glGenTextures(1, &tex);
		glBindTexture (GL_TEXTURE_2D, tex);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
		glGetTexParameterfv (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &test1);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 2.0f);
		glGetTexParameterfv (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &test2);
		glDeleteTextures(1, &tex);

		if (test1 == 1 && test2 == 2)
		{
			Con_Printf("FOUND: EXT_texture_filter_anisotropic\n");
			gl_anisotropy_able = true;
		}
		else
		{
			Con_Warning ("anisotropic filtering locked by driver. Current driver setting is %f\n", test1);
		}

		//get max value either way, so the menu and stuff know it
		glGetFloatv (GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_max_anisotropy);
		if (gl_max_anisotropy < 2)
		{
			gl_anisotropy_able = false;
			gl_max_anisotropy = 1;
			Con_Warning ("anisotropic filtering broken: disabled\n");
		}
	}
	else
	{
		gl_max_anisotropy = 1;
		Con_Warning ("texture_filter_anisotropic not supported\n");
	}
}

/*
===============
GL_SetupState -- johnfitz

does all the stuff from GL_Init that needs to be done every time a new GL render context is created
===============
*/
static void GL_SetupState (void)
{
	glClearColor (0.15,0.15,0.15,0); //johnfitz -- originally 1,0,0,0
	glCullFace(GL_BACK); //johnfitz -- glquake used CCW with backwards culling -- let's do it right
	glFrontFace(GL_CW); //johnfitz -- glquake used CCW with backwards culling -- let's do it right
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.666);
	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel (GL_FLAT);
	glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST); //johnfitz
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glDepthRange (0, 1); //johnfitz -- moved here becuase gl_ztrick is gone.
	glDepthFunc (GL_LEQUAL); //johnfitz -- moved here becuase gl_ztrick is gone.
}

/*
===============
GL_Init
===============
*/
static void GL_Init (void)
{
	gl_vendor = (const char *) glGetString (GL_VENDOR);
	gl_renderer = (const char *) glGetString (GL_RENDERER);
	gl_version = (const char *) glGetString (GL_VERSION);
	gl_extensions = (const char *) glGetString (GL_EXTENSIONS);

	if (gl_extensions_nice != NULL)
		Z_Free (gl_extensions_nice);
	gl_extensions_nice = GL_MakeNiceExtensionsList (gl_extensions);

	GL_CheckExtensions (); //johnfitz

	if (SDL_strncasecmp(gl_renderer,"PowerVR",7)==0)
		fullsbardraw = true;
	//johnfitz -- intel video workarounds from Baker
	if (!strcmp(gl_vendor, "Intel"))
	{
		Con_Printf ("Intel Display Adapter detected\n");
		isIntelVideo = true;
	}
	//johnfitz
}

/*
=================
GL_BeginRendering -- sets values of glx, gly, glwidth, glheight
=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = vid.width;
	*height = vid.height;
}

/*
=================
GL_EndRendering
=================
*/
void GL_EndRendering (void)
{
	if (!scr_skipupdate)
		SDL_GL_SwapBuffers();

	if (fullsbardraw)
		Sbar_Changed();
}


void	VID_Shutdown (void)
{
	if (vid_initialized)
	{
		VID_Gamma_Shutdown (); //johnfitz

		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		draw_context = NULL;

		PL_VID_Shutdown();
	}
}

//==========================================================================

/*
===================================================================

MAIN WINDOW

===================================================================
*/

/*
================
ClearAllStates
================
*/
static void ClearAllStates (void)
{
	Key_ClearStates ();
	IN_ClearStates ();
}



//==========================================================================
//
//  COMMANDS
//
//==========================================================================

/*
=================
VID_DescribeCurrentMode_f
=================
*/
static void VID_DescribeCurrentMode_f (void)
{
	if (draw_context)
		Con_Printf("%dx%dx%d %s\n",
			draw_context->w,
			draw_context->h,
			draw_context->format->BitsPerPixel,
			draw_context->flags & SDL_FULLSCREEN ? "fullscreen" : "windowed");
}

/*
=================
VID_DescribeModes_f -- johnfitz -- changed formatting, and added refresh rates after each mode.
=================
*/
static void VID_DescribeModes_f (void)
{
	int	i;
	int	lastwidth, lastheight, lastbpp, count;

	lastwidth = lastheight = lastbpp = count = 0;

	for (i = 0; i < nummodes; i++)
	{
		if (lastwidth != modelist[i].width || lastheight != modelist[i].height || lastbpp != modelist[i].bpp)
		{
			if (count > 0)
				Con_SafePrintf ("\n");
			Con_SafePrintf ("   %4i x %4i x %i", modelist[i].width, modelist[i].height, modelist[i].bpp);
			lastwidth = modelist[i].width;
			lastheight = modelist[i].height;
			lastbpp = modelist[i].bpp;
			count++;
		}
	}
	Con_Printf ("\n%i modes\n", count);
}

//==========================================================================
//
//  INIT
//
//==========================================================================

/*
=================
VID_InitModelist
=================
*/
static void VID_InitModelist (void)
{
	SDL_PixelFormat	format;
	SDL_Rect	**modes;
	Uint32		flags;
	int		i, j, k, originalnummodes, existingmode;
	int		bpps[] = {16, 24, 32}; // enumerate >8 bpp modes

	originalnummodes = nummodes = 0;
	format.palette = NULL;

	// enumerate fullscreen modes
	flags = SDL_DEFAULT_FLAGS | SDL_FULLSCREEN;
	for (i = 0; i < (int)(sizeof(bpps)/sizeof(bpps[0])); i++)
	{
		if (nummodes >= MAX_MODE_LIST)
			break;

		format.BitsPerPixel = bpps[i];
		modes = SDL_ListModes(&format, flags);

		if (modes == (SDL_Rect **)0 || modes == (SDL_Rect **)-1)
			continue;

		for (j = 0; modes[j]; j++)
		{
			if (modes[j]->w > MAXWIDTH || modes[j]->h > MAXHEIGHT || nummodes >= MAX_MODE_LIST)
				continue;

			modelist[nummodes].width = modes[j]->w;
			modelist[nummodes].height = modes[j]->h;
			modelist[nummodes].bpp = bpps[i];

			for (k=originalnummodes, existingmode = 0 ; k < nummodes ; k++)
			{
				if ((modelist[nummodes].width == modelist[k].width)   &&
				    (modelist[nummodes].height == modelist[k].height) &&
				    (modelist[nummodes].bpp == modelist[k].bpp))
				{
					existingmode = 1;
					break;
				}
			}

			if (!existingmode)
			{
				nummodes++;
			}
		}
	}

	if (nummodes == originalnummodes)
		Con_SafePrintf ("No fullscreen DIB modes found\n");
}

/*
===================
VID_Init
===================
*/
void	VID_Init (void)
{
	static char vid_center[] = "SDL_VIDEO_CENTERED=center";
	const SDL_VideoInfo *info = SDL_GetVideoInfo();
	int		width, height, bpp;
	qboolean	fullscreen;
	const char	*read_vars[] = { "vid_fullscreen",
					 "vid_width",
					 "vid_height",
					 "vid_bpp",
					 "vid_vsync" };
#define num_readvars	( sizeof(read_vars)/sizeof(read_vars[0]) )

	Cvar_RegisterVariable (&vid_fullscreen); //johnfitz
	Cvar_RegisterVariable (&vid_width); //johnfitz
	Cvar_RegisterVariable (&vid_height); //johnfitz
	Cvar_RegisterVariable (&vid_bpp); //johnfitz
	Cvar_RegisterVariable (&vid_vsync); //johnfitz
	Cvar_SetCallback (&vid_fullscreen, VID_Changed_f);
	Cvar_SetCallback (&vid_width, VID_Changed_f);
	Cvar_SetCallback (&vid_height, VID_Changed_f);
	Cvar_SetCallback (&vid_bpp, VID_Changed_f);
	Cvar_SetCallback (&vid_vsync, VID_Changed_f);

	Cmd_AddCommand ("vid_unlock", VID_Unlock); //johnfitz
	Cmd_AddCommand ("vid_restart", VID_Restart); //johnfitz
	Cmd_AddCommand ("vid_test", VID_Test); //johnfitz
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) == -1)
		Sys_Error("Could not initialize SDL Video");

	putenv (vid_center);	/* SDL_putenv is problematic in versions <= 1.2.9 */

	if (CFG_OpenConfig("config.cfg") == 0)
	{
		CFG_ReadCvars(read_vars, num_readvars);
		CFG_CloseConfig();
	}
	CFG_ReadCvarOverrides(read_vars, num_readvars);

	VID_InitModelist();

	width = (int)vid_width.value;
	height = (int)vid_height.value;
	bpp = (int)vid_bpp.value;
	fullscreen = (int)vid_fullscreen.value;

	if (COM_CheckParm("-current"))
	{
		width = info->current_w;
		height = info->current_h;
		bpp = info->vfmt->BitsPerPixel;
		fullscreen = true;
	}
	else
	{
		int p;

		p = COM_CheckParm("-width");
		if (p && p < com_argc-1)
		{
			width = Q_atoi(com_argv[p+1]);

			if(!COM_CheckParm("-height"))
				height = width * 3 / 4;
		}

		p = COM_CheckParm("-height");
		if (p && p < com_argc-1)
		{
			height = Q_atoi(com_argv[p+1]);

			if(!COM_CheckParm("-width"))
				width = height * 4 / 3;
		}

		p = COM_CheckParm("-bpp");
		if (p && p < com_argc-1)
			bpp = Q_atoi(com_argv[p+1]);

		if (COM_CheckParm("-window") || COM_CheckParm("-w"))
			fullscreen = false;
		else if (COM_CheckParm("-fullscreen") || COM_CheckParm("-f"))
			fullscreen = true;
	}

	if (!VID_ValidMode(width, height, bpp, fullscreen))
	{
		width = (int)vid_width.value;
		height = (int)vid_height.value;
		bpp = (int)vid_bpp.value;
		fullscreen = (int)vid_fullscreen.value;
	}

	if (!VID_ValidMode(width, height, bpp, fullscreen))
	{
		width = 640;
		height = 480;
		bpp = info->vfmt->BitsPerPixel;
		fullscreen = false;
	}

	vid_initialized = true;

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	// set window icon
	PL_SetWindowIcon();

	VID_SetMode (width, height, bpp, fullscreen);

	GL_Init ();
	GL_SetupState ();
	Cmd_AddCommand ("gl_info", GL_Info_f); //johnfitz

	//johnfitz -- removed code creating "glquake" subdirectory

	vid_menucmdfn = VID_Menu_f; //johnfitz
	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	if (COM_CheckParm("-fullsbar"))
		fullsbardraw = true;

	VID_Gamma_Init(); //johnfitz
	VID_Menu_Init(); //johnfitz

	//QuakeSpasm: current vid settings should override config file settings.
	//so we have to lock the vid mode from now until after all config files are read.
	vid_locked = true;
}

// new proc by S.A., called by alt-return key binding.
void	VID_Toggle (void)
{
	static qboolean vid_toggle_works = true;

	S_ClearBuffer ();

	if (!vid_toggle_works)
		goto vrestart;
	if (SDL_WM_ToggleFullScreen(draw_context) == 1)
	{
		Sbar_Changed ();	// Sbar seems to need refreshing

		modestate = draw_context->flags & SDL_FULLSCREEN ? MS_FULLSCREEN : MS_WINDOWED;

		VID_SyncCvars();

		// update mouse grab
		if (key_dest == key_console || key_dest == key_menu)
		{
			if (modestate == MS_WINDOWED)
				IN_Deactivate(true);
			else if (modestate == MS_FULLSCREEN)
				IN_Activate();
		}
	}
	else
	{
		vid_toggle_works = false;
		Con_DPrintf ("SDL_WM_ToggleFullScreen failed, attempting VID_Restart\n");
	vrestart:
		Cvar_SetQuick (&vid_fullscreen, vid_fullscreen.value ? "0" : "1");
		Cbuf_AddText ("vid_restart\n");
	}
}

/*
================
VID_SyncCvars -- johnfitz -- set vid cvars to match current video mode
================
*/
void VID_SyncCvars (void)
{
	int swap_control;

	if (draw_context)
	{
		Cvar_SetValueQuick (&vid_width, draw_context->w);
		Cvar_SetValueQuick (&vid_height, draw_context->h);
		Cvar_SetValueQuick (&vid_bpp, draw_context->format->BitsPerPixel);
		Cvar_SetQuick (&vid_fullscreen, draw_context->flags & SDL_FULLSCREEN ? "1" : "0");

		if (SDL_GL_GetAttribute(SDL_GL_SWAP_CONTROL, &swap_control) == 0)
			Cvar_SetQuick (&vid_vsync, (swap_control > 0)? "1" : "0");
	}

	vid_changed = false;
}

//==========================================================================
//
//  NEW VIDEO MENU -- johnfitz
//
//==========================================================================

#define VIDEO_OPTIONS_ITEMS 6
static int	video_cursor_table[] = {48, 56, 64, 72, 88, 96};
static int	video_options_cursor = 0;

typedef struct {
	int width,height;
} vid_menu_mode;

//TODO: replace these fixed-length arrays with hunk_allocated buffers
static vid_menu_mode vid_menu_modes[MAX_MODE_LIST];
static int vid_menu_nummodes = 0;

static int vid_menu_bpps[MAX_BPPS_LIST];
static int vid_menu_numbpps = 0;

/*
================
VID_Menu_Init
================
*/
static void VID_Menu_Init (void)
{
	int i, j, h, w;

	for (i = 0; i < nummodes; i++)
	{
		w = modelist[i].width;
		h = modelist[i].height;

		for (j = 0; j < vid_menu_nummodes; j++)
		{
			if (vid_menu_modes[j].width == w &&
				vid_menu_modes[j].height == h)
				break;
		}

		if (j == vid_menu_nummodes)
		{
			vid_menu_modes[j].width = w;
			vid_menu_modes[j].height = h;
			vid_menu_nummodes++;
		}
	}
}

/*
================
VID_Menu_RebuildBppList

regenerates bpp list based on current vid_width and vid_height
================
*/
static void VID_Menu_RebuildBppList (void)
{
	int i, j, b;

	vid_menu_numbpps = 0;

	for (i = 0; i < nummodes; i++)
	{
		if (vid_menu_numbpps >= MAX_BPPS_LIST)
			break;

		//bpp list is limited to bpps available with current width/height
		if (modelist[i].width != vid_width.value ||
			modelist[i].height != vid_height.value)
			continue;

		b = modelist[i].bpp;

		for (j = 0; j < vid_menu_numbpps; j++)
		{
			if (vid_menu_bpps[j] == b)
				break;
		}

		if (j == vid_menu_numbpps)
		{
			vid_menu_bpps[j] = b;
			vid_menu_numbpps++;
		}
	}

	//if there are no valid fullscreen bpps for this width/height, just pick one
	if (vid_menu_numbpps == 0)
	{
		Cvar_SetValueQuick (&vid_bpp, (float)modelist[0].bpp);
		return;
	}

	//if vid_bpp is not in the new list, change vid_bpp
	for (i = 0; i < vid_menu_numbpps; i++)
		if (vid_menu_bpps[i] == (int)(vid_bpp.value))
			break;

	if (i == vid_menu_numbpps)
		Cvar_SetValueQuick (&vid_bpp, (float)vid_menu_bpps[0]);
}

/*
================
VID_Menu_ChooseNextMode

chooses next resolution in order, then updates vid_width and
vid_height cvars, then updates bpp and refreshrate lists
================
*/
static void VID_Menu_ChooseNextMode (int dir)
{
	int i;

	if (vid_menu_nummodes)
	{
		for (i = 0; i < vid_menu_nummodes; i++)
		{
			if (vid_menu_modes[i].width == vid_width.value &&
				vid_menu_modes[i].height == vid_height.value)
				break;
		}

		if (i == vid_menu_nummodes) //can't find it in list, so it must be a custom windowed res
		{
			i = 0;
		}
		else
		{
			i += dir;
			if (i >= vid_menu_nummodes)
				i = 0;
			else if (i < 0)
				i = vid_menu_nummodes-1;
		}

		Cvar_SetValueQuick (&vid_width, (float)vid_menu_modes[i].width);
		Cvar_SetValueQuick (&vid_height, (float)vid_menu_modes[i].height);
		VID_Menu_RebuildBppList ();
	}
}

/*
================
VID_Menu_ChooseNextBpp

chooses next bpp in order, then updates vid_bpp cvar, then updates refreshrate list
================
*/
static void VID_Menu_ChooseNextBpp (int dir)
{
	int i;

	if (vid_menu_numbpps)
	{
		for (i = 0; i < vid_menu_numbpps; i++)
		{
			if (vid_menu_bpps[i] == vid_bpp.value)
				break;
		}

		if (i == vid_menu_numbpps) //can't find it in list
		{
			i = 0;
		}
		else
		{
			i += dir;
			if (i >= vid_menu_numbpps)
				i = 0;
			else if (i < 0)
				i = vid_menu_numbpps-1;
		}

		Cvar_SetValueQuick (&vid_bpp, (float)vid_menu_bpps[i]);
	}
}

/*
================
VID_MenuKey
================
*/
static void VID_MenuKey (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		VID_SyncCvars (); //sync cvars before leaving menu. FIXME: there are other ways to leave menu
		S_LocalSound ("misc/menu1.wav");
		M_Menu_Options_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		video_options_cursor--;
		if (video_options_cursor < 0)
			video_options_cursor = VIDEO_OPTIONS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		video_options_cursor++;
		if (video_options_cursor >= VIDEO_OPTIONS_ITEMS)
			video_options_cursor = 0;
		break;

	case K_LEFTARROW:
		S_LocalSound ("misc/menu3.wav");
		switch (video_options_cursor)
		{
		case 0:
			VID_Menu_ChooseNextMode (1);
			break;
		case 1:
			VID_Menu_ChooseNextBpp (1);
			break;
		case 2:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case 3:
			Cbuf_AddText ("toggle vid_vsync\n"); // kristian
			break;
		default:
			break;
		}
		break;

	case K_RIGHTARROW:
		S_LocalSound ("misc/menu3.wav");
		switch (video_options_cursor)
		{
		case 0:
			VID_Menu_ChooseNextMode (-1);
			break;
		case 1:
			VID_Menu_ChooseNextBpp (-1);
			break;
		case 2:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case 3:
			Cbuf_AddText ("toggle vid_vsync\n");
			break;
		default:
			break;
		}
		break;

	case K_ENTER:
		m_entersound = true;
		switch (video_options_cursor)
		{
		case 0:
			VID_Menu_ChooseNextMode (1);
			break;
		case 1:
			VID_Menu_ChooseNextBpp (1);
			break;
		case 2:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case 3:
			Cbuf_AddText ("toggle vid_vsync\n");
			break;
		case 4:
			Cbuf_AddText ("vid_test\n");
			break;
		case 5:
			Cbuf_AddText ("vid_restart\n");
			key_dest = key_game;
			m_state = m_none;
			IN_Activate();
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}
}

/*
================
VID_MenuDraw
================
*/
static void VID_MenuDraw (void)
{
	int i = 0;
	qpic_t *p;
	const char *title;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp"));

	//p = Draw_CachePic ("gfx/vidmodes.lmp");
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	// title
	title = "Video Options";
	M_PrintWhite ((320-8*strlen(title))/2, 32, title);

	// options
	M_Print (16, video_cursor_table[i], "        Video mode");
	M_Print (184, video_cursor_table[i], va("%ix%i", (int)vid_width.value, (int)vid_height.value));
	i++;

	M_Print (16, video_cursor_table[i], "       Color depth");
	M_Print (184, video_cursor_table[i], va("%i", (int)vid_bpp.value));
	i++;

	M_Print (16, video_cursor_table[i], "        Fullscreen");
	M_DrawCheckbox (184, video_cursor_table[i], (int)vid_fullscreen.value);
	i++;

	// added vsync to the video menu -- kristian
	M_Print (16, video_cursor_table[i], "     Vertical Sync");
	if (gl_swap_control)
		M_DrawCheckbox (184, video_cursor_table[i], (int)vid_vsync.value);
	else
		M_Print (184, video_cursor_table[i], "N/A");

	i++;

	M_Print (16, video_cursor_table[i], "      Test changes");
	i++;

	M_Print (16, video_cursor_table[i], "     Apply changes");

	// cursor
	M_DrawCharacter (168, video_cursor_table[video_options_cursor], 12+((int)(realtime*4)&1));

	// notes          "345678901234567890123456789012345678"
//	M_Print (16, 172, "Windowed modes always use the desk- ");
//	M_Print (16, 180, "top color depth, and can never be   ");
//	M_Print (16, 188, "larger than the desktop resolution. ");
}

/*
================
VID_Menu_f
================
*/
static void VID_Menu_f (void)
{
	IN_Deactivate(modestate == MS_WINDOWED);
	key_dest = key_menu;
	m_state = m_video;
	m_entersound = true;

	//set all the cvars to match the current mode when entering the menu
	VID_SyncCvars ();

	//set up bpp and rate lists based on current cvars
	VID_Menu_RebuildBppList ();
}

