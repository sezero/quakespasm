/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2005 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske

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
// gl_vidnt.c -- NT GL vid component

#include "quakedef.h"
#include "resource.h"

#define MAX_MODE_LIST	600 //johnfitz -- was 30
#define VID_ROW_SIZE	3
#define WARP_WIDTH		320
#define WARP_HEIGHT		200
#define MAXWIDTH		10000
#define MAXHEIGHT		10000
#define BASEWIDTH		320
#define BASEHEIGHT		200

#define SDL_DEFAULT_FLAGS	SDL_OPENGL

typedef struct {
	modestate_t	type;
	int			width;
	int			height;
	int			modenum;
	int			dib;
	int			fullscreen;
	int			bpp;
	int			halfscreen;
	char		modedesc[17];
} vmode_t;

typedef struct {
	int			width;
	int			height;
} lmode_t;

lmode_t	lowresmodes[] = {
	{320, 200},
	{320, 240},
	{400, 300},
	{512, 384},
};

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

qboolean		DDActive;
qboolean		scr_skipupdate;

static vmode_t	modelist[MAX_MODE_LIST];
static int		nummodes;
static vmode_t	badmode;

static qboolean	vid_initialized = false;
static qboolean	windowed, leavecurrentmode;
static qboolean vid_canalttab = false;
static qboolean vid_toggle_works = true;
extern qboolean	mouseactive;  // from in_win.c

SDL_Surface	*draw_context;

int			vid_modenum = NO_MODE;
int			vid_realmode;
int			vid_default = MODE_WINDOWED;
unsigned char	vid_curpal[256*3];
static qboolean fullsbardraw = false;

glvert_t glv;
viddef_t	vid;				// global video state

//unsigned short	d_8to16table[256]; //johnfitz -- never used
//unsigned char		d_15to8table[65536]; //johnfitz -- never used


PFNGLARRAYELEMENTEXTPROC glArrayElementEXT = NULL;
PFNGLCOLORPOINTEREXTPROC glColorPointerEXT = NULL;
PFNGLTEXCOORDPOINTEREXTPROC glTexCoordPointerEXT = NULL;
PFNGLVERTEXPOINTEREXTPROC glVertexPointerEXT = NULL;

modestate_t	modestate = MODE_WINDOWED;

void VID_Menu_Init (void); //johnfitz
void VID_Menu_f (void); //johnfitz
void VID_MenuDraw (void);
void VID_MenuKey (int key);

char *VID_GetModeDescription (int mode);
void ClearAllStates (void);
void VID_UpdateWindowStatus (void);
void GL_Init (void);

PFNGLMULTITEXCOORD2FARBPROC GL_MTexCoord2fFunc = NULL; //johnfitz
PFNGLACTIVETEXTUREARBPROC GL_SelectTextureFunc = NULL; //johnfitz

qboolean isPermedia = false;
qboolean isIntelVideo = false; //johnfitz -- intel video workarounds from Baker
qboolean gl_mtexable = false;
qboolean gl_texture_env_combine = false; //johnfitz
qboolean gl_texture_env_add = false; //johnfitz
qboolean gl_swap_control = false; //johnfitz
qboolean gl_anisotropy_able = false; //johnfitz
float gl_max_anisotropy; //johnfitz

int			gl_stencilbits; //johnfitz

qboolean vid_locked = false; //johnfitz
qboolean vid_changed = false;

void GL_SetupState (void); //johnfitz

//====================================

//johnfitz -- new cvars
cvar_t		vid_fullscreen = {"vid_fullscreen", "0", true};
cvar_t		vid_width = {"vid_width", "800", true};
cvar_t		vid_height = {"vid_height", "600", true};
cvar_t		vid_bpp = {"vid_bpp", "16", true};
cvar_t		vid_refreshrate = {"vid_refreshrate", "60", true};
cvar_t		vid_vsync = {"vid_vsync", "0", true};
//johnfitz

cvar_t		_windowed_mouse = {"_windowed_mouse","1", true};
cvar_t		vid_gamma = {"gamma", "1", true}; //johnfitz -- moved here from view.c

//==========================================================================
//
//  HARDWARE GAMMA -- johnfitz
//
//==========================================================================

unsigned short vid_gamma_red[256];
unsigned short vid_gamma_green[256];
unsigned short vid_gamma_blue[256];

unsigned short vid_sysgamma_red[256];
unsigned short vid_sysgamma_green[256];
unsigned short vid_sysgamma_blue[256];

int vid_gammaworks;

/*
================
VID_Gamma_SetGamma -- apply gamma correction
================
*/
void VID_Gamma_SetGamma (void)
{
	if (draw_context && vid_gammaworks)
		if (SDL_SetGammaRamp(&vid_gamma_red[0], &vid_gamma_green[0], &vid_gamma_blue[0]) == -1)
			Con_Printf ("VID_Gamma_SetGamma: failed on SDL_SetGammaRamp\n");
}

/*
================
VID_Gamma_Restore -- restore system gamma
================
*/
void VID_Gamma_Restore (void)
{
	if (draw_context && vid_gammaworks)
		if (SDL_SetGammaRamp(&vid_sysgamma_red[0], &vid_sysgamma_green[0], &vid_sysgamma_blue[0]) == -1)
			Con_Printf ("VID_Gamma_Restore: failed on SDL_SetGammaRamp\n");
}

/*
================
VID_Gamma_Shutdown -- called on exit
================
*/
void VID_Gamma_Shutdown (void)
{
	VID_Gamma_Restore ();
}

/*
================
VID_Gamma_f -- callback when the cvar changes
================
*/
void VID_Gamma_f (void)
{
	static float oldgamma;
	int i;

	if (vid_gamma.value == oldgamma)
		return;

	oldgamma = vid_gamma.value;

	for (i=0; i<256; i++)
	{
		vid_gamma_red[i] = CLAMP(0, (int) (255 * pow ((i+0.5)/255.5, vid_gamma.value) + 0.5), 255) << 8;
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
void VID_Gamma_Init (void)
{
	vid_gammaworks = false;

	if (SDL_GetGammaRamp (&vid_sysgamma_red[0], &vid_sysgamma_green[0], &vid_sysgamma_blue[0]) != -1)
		vid_gammaworks = true;

	Cvar_RegisterVariable (&vid_gamma, VID_Gamma_f);
}

/*
================
VID_SetMode
================
*/
int VID_SetMode (int modenum)
{
	int temp;
	qboolean stat;
	Uint32 flags = SDL_DEFAULT_FLAGS;
	char caption[50];

// TODO: check if video mode is supported using SDL_VideoModeOk
	if ((windowed && (modenum != 0)) ||
		(!windowed && (modenum < 1)) ||
		(!windowed && (modenum >= nummodes)))
	{
		Sys_Error ("Bad video mode\n");
	}

// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause ();

	// set vertical sync
	if (gl_swap_control)
	{
		if (vid_vsync.value)
		{
			if (SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1) == -1)
				Con_Printf ("VID_Vsync_f: failed on SDL_GL_SetAttribute\n");
		}
		else
		{
			if (SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 0) == -1)
				Con_Printf ("VID_Vsync_f: failed on SDL_GL_SetAttribute\n");
		}
	}

	if (modelist[modenum].type == MODE_WINDOWED)
	{
		if (_windowed_mouse.value && key_dest == key_game)
		{
			draw_context = SDL_SetVideoMode(modelist[modenum].width,
							modelist[modenum].height,
							modelist[modenum].bpp, flags);
			stat = true;
		}
		else
		{
			draw_context = SDL_SetVideoMode(modelist[modenum].width,
							modelist[modenum].height,
							modelist[modenum].bpp, flags);
			stat = true;
		}
		modestate = MODE_WINDOWED;
		// TODO set icon and title
	}
	else if (modelist[modenum].type == MODE_FULLSCREEN_DEFAULT)
	{
		flags |= SDL_FULLSCREEN;
		draw_context = SDL_SetVideoMode(modelist[modenum].width,
						modelist[modenum].height,
						modelist[modenum].bpp, flags);
		stat = true;
		modestate = MODE_FULLSCREEN_DEFAULT;
	}
	else
	{
		Sys_Error ("VID_SetMode: Bad mode type in modelist");
	}

	//kristian -- set window caption
	sprintf(caption, "QuakeSpasm %1.2f.%d", (float)FITZQUAKE_VERSION, QUAKESPASM_VER_PATCH);
	SDL_WM_SetCaption((const char* )&caption, (const char*)&caption);

	vid.width = modelist[modenum].width;
	vid.height = modelist[modenum].height;
	vid.conwidth = vid.width & 0xFFFFFFF8;
	vid.conheight = vid.conwidth * vid.height / vid.width;
	vid.numpages = 2;
	vid.type = modelist[modenum].type;

	VID_UpdateWindowStatus ();

	CDAudio_Resume ();
	scr_disabled_for_loading = temp;

	if (!stat)
	{
		Sys_Error ("Couldn't set video mode");
	}

	vid_modenum = modenum;

// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	if (!msg_suppress_1)
		Con_SafePrintf ("Video mode %s initialized\n", VID_GetModeDescription (vid_modenum));

	vid.recalc_refdef = 1;

// with SDL, this needs to be done every time the render context is recreated, so I moved it here
	TexMgr_ReloadImages ();
	GL_SetupState ();

// no pending changes
	vid_changed = false;

	return true;
}

/*
===================
VID_Changed_f -- kristian -- notify us that a value has changed that requires a vid_restart
===================
*/
void VID_Changed_f (void)
{
	vid_changed = true;
}

/*
===================
VID_Restart -- johnfitz -- change video modes on the fly
===================
*/
void VID_Restart (void)
{
	int			i;
	vmode_t		oldmode;

	if (vid_locked || !vid_changed)
		return;

//
// decide which mode to set
//
	oldmode = modelist[vid_default];

	if (vid_fullscreen.value)
	{
		for (i=1; i<nummodes; i++)
		{
			if (modelist[i].width == (int)vid_width.value &&
			    modelist[i].height == (int)vid_height.value &&
			    modelist[i].bpp == (int)vid_bpp.value)
			{
				break;
			}
		}

		if (i == nummodes)
		{
			Con_Printf ("%dx%dx%d %dHz is not a valid fullscreen mode\n",
						(int)vid_width.value,
						(int)vid_height.value,
						(int)vid_bpp.value,
						(int)vid_refreshrate.value);
			return;
		}

		windowed = false;
		vid_default = i;
	}
	else //not fullscreen
	{
		if (vid_width.value < 320)
		{
			Con_Printf ("Window width can't be less than 320\n");
			return;
		}

		if (vid_height.value < 200)
		{
			Con_Printf ("Window height can't be less than 200\n");
			return;
		}

		modelist[0].width = (int)vid_width.value;
		modelist[0].height = (int)vid_height.value;
		sprintf (modelist[0].modedesc, "%dx%dx%d",
				 modelist[0].width,
				 modelist[0].height,
				 modelist[0].bpp);

		windowed = true;
		vid_default = 0;
	}
//
// set new mode
//
	VID_SetMode (vid_default);

	vid_canalttab = true;

	//warpimages needs to be recalculated
	TexMgr_RecalcWarpImageSize ();

	//conwidth and conheight need to be recalculated
	vid.conwidth = (scr_conwidth.value > 0) ? (int)scr_conwidth.value : vid.width;
	vid.conwidth = CLAMP (320, vid.conwidth, vid.width);
	vid.conwidth &= 0xFFFFFFF8;
	vid.conheight = vid.conwidth * vid.height / vid.width;
//
// keep cvars in line with actual mode
//
	Cvar_Set ("vid_width", va("%i", modelist[vid_default].width));
	Cvar_Set ("vid_height", va("%i", modelist[vid_default].height));
	Cvar_Set ("vid_bpp", va("%i", modelist[vid_default].bpp));
	Cvar_Set ("vid_fullscreen", (windowed) ? "0" : "1");
}

/*
================
VID_Test -- johnfitz -- like vid_restart, but asks for confirmation after switching modes
================
*/
void VID_Test (void)
{
	vmode_t oldmode;

	if (vid_locked || !vid_changed)
		return;
//
// now try the switch
//
	oldmode = modelist[vid_default];

	VID_Restart ();

	//pop up confirmation dialoge
	if (!SCR_ModalMessage("Would you like to keep this\nvideo mode? (y/n)\n", 5.0f))
	{
		//revert cvars and mode
		Cvar_Set ("vid_width", va("%i", oldmode.width));
		Cvar_Set ("vid_height", va("%i", oldmode.height));
		Cvar_Set ("vid_bpp", va("%i", oldmode.bpp));
		Cvar_Set ("vid_fullscreen", (oldmode.type == MODE_WINDOWED) ? "0" : "1");
		VID_Restart ();
	}
}

/*
================
VID_Unlock -- johnfitz
================
*/
void VID_Unlock (void)
{
	vid_locked = false;

	//sync up cvars in case they were changed during the lock
	Cvar_Set ("vid_width", va("%i", modelist[vid_default].width));
	Cvar_Set ("vid_height", va("%i", modelist[vid_default].height));
	Cvar_Set ("vid_bpp", va("%i", modelist[vid_default].bpp));
	Cvar_Set ("vid_fullscreen", (windowed) ? "0" : "1");
}

/*
================
VID_UpdateWindowStatus
================
*/
void VID_UpdateWindowStatus (void)
{
	//IN_UpdateClipCursor ();
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
char *GL_MakeNiceExtensionsList (const char *in)
{
	char *copy, *token, *out;
	int i, count;

	//each space will be replaced by 4 chars, so count the spaces before we malloc
	for (i = 0, count = 1; i < strlen(in); i++)
		if (in[i] == ' ')
			count++;
	out = Z_Malloc (strlen(in) + count*3 + 1); //usually about 1-2k
	out[0] = 0;

	copy = Z_Malloc(strlen(in) + 1);
	strcpy(copy, in);

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
void GL_Info_f (void)
{
	static char *gl_extensions_nice = NULL;

	if (!gl_extensions_nice)
		gl_extensions_nice = GL_MakeNiceExtensionsList (gl_extensions);

	Con_SafePrintf ("GL_VENDOR: %s\n", gl_vendor);
	Con_SafePrintf ("GL_RENDERER: %s\n", gl_renderer);
	Con_SafePrintf ("GL_VERSION: %s\n", gl_version);
	Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions_nice);
}

/*
===============
CheckArrayExtensions
===============
*/
void CheckArrayExtensions (void)
{
	unsigned char *tmp;

	/* check for texture extension */
	tmp = (GLubyte *)glGetString(GL_EXTENSIONS);
	while (*tmp)
	{
		if (strncmp((const char*)tmp, "GL_EXT_vertex_array", strlen("GL_EXT_vertex_array")) == 0)
		{
			if (((glArrayElementEXT = SDL_GL_GetProcAddress("glArrayElementEXT")) == NULL) ||
			    ((glColorPointerEXT = SDL_GL_GetProcAddress("glColorPointerEXT")) == NULL) ||
			    ((glTexCoordPointerEXT = SDL_GL_GetProcAddress("glTexCoordPointerEXT")) == NULL) ||
			    ((glVertexPointerEXT = SDL_GL_GetProcAddress("glVertexPointerEXT")) == NULL) )
			{
				Sys_Error ("GetProcAddress for vertex extension failed");
				return;
			}
			return;
		}
		tmp++;
	}

	Sys_Error ("Vertex array extension not present");
}

/*
===============
GL_CheckExtensions -- johnfitz
===============
*/
void GL_CheckExtensions (void)
{
	int swap_control;

	//
	// multitexture
	//
	if (COM_CheckParm("-nomtex"))
		Con_Warning ("Mutitexture disabled at command line\n");
	else
		if (strstr(gl_extensions, "GL_ARB_multitexture"))
		{
			GL_MTexCoord2fFunc = SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
			GL_SelectTextureFunc = SDL_GL_GetProcAddress("glActiveTextureARB");
			if (GL_MTexCoord2fFunc && GL_SelectTextureFunc)
			{
				Con_Printf("FOUND: ARB_multitexture\n");
				TEXTURE0 = GL_TEXTURE0_ARB;
				TEXTURE1 = GL_TEXTURE1_ARB;
				gl_mtexable = true;
			}
			else
				Con_Warning ("multitexture not supported (wglGetProcAddress failed)\n");
		}
		else
			if (strstr(gl_extensions, "GL_SGIS_multitexture"))
			{
				GL_MTexCoord2fFunc = SDL_GL_GetProcAddress("glMTexCoord2fSGIS");
				GL_SelectTextureFunc = SDL_GL_GetProcAddress("glSelectTextureSGIS");
				if (GL_MTexCoord2fFunc && GL_SelectTextureFunc)
				{
					Con_Printf("FOUND: SGIS_multitexture\n");
					TEXTURE0 = TEXTURE0_SGIS;
					TEXTURE1 = TEXTURE1_SGIS;
					gl_mtexable = true;
				}
				else
					Con_Warning ("multitexture not supported (SDL_GL_GetProcAddress failed)\n");

			}
			else
				Con_Warning ("multitexture not supported (extension not found)\n");
	//
	// texture_env_combine
	//
	if (COM_CheckParm("-nocombine"))
		Con_Warning ("texture_env_combine disabled at command line\n");
	else
		if (strstr(gl_extensions, "GL_ARB_texture_env_combine"))
		{
			Con_Printf("FOUND: ARB_texture_env_combine\n");
			gl_texture_env_combine = true;
		}
		else
			if (strstr(gl_extensions, "GL_EXT_texture_env_combine"))
			{
				Con_Printf("FOUND: EXT_texture_env_combine\n");
				gl_texture_env_combine = true;
			}
			else
				Con_Warning ("texture_env_combine not supported\n");
	//
	// texture_env_add
	//
	if (COM_CheckParm("-noadd"))
		Con_Warning ("texture_env_add disabled at command line\n");
	else
		if (strstr(gl_extensions, "GL_ARB_texture_env_add"))
		{
			Con_Printf("FOUND: ARB_texture_env_add\n");
			gl_texture_env_add = true;
		}
		else
			if (strstr(gl_extensions, "GL_EXT_texture_env_add"))
			{
				Con_Printf("FOUND: EXT_texture_env_add\n");
				gl_texture_env_add = true;
			}
			else
				Con_Warning ("texture_env_add not supported\n");

	//
	// swap control
	//
	if (strstr(gl_extensions, "GL_EXT_swap_control"))
	{
		if (SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 0) == -1)
		{
			Con_Warning ("vertical sync not supported (SDL_GL_SetAttribute failed)\n");
		}
		else
		{
			if (SDL_GL_GetAttribute(SDL_GL_SWAP_CONTROL, &swap_control) == -1)
			{
				Con_Warning ("vertical sync not supported (SDL_GL_GetAttribute failed). Make sure you don't have vertical sync disabled in your driver settings.\n");
			}
			else if (swap_control == -1)
			{
			// TODO: check if this is correct - I don't know what SDL returns if vertical sync is disabled
				Con_Warning ("vertical sync not supported (swap interval is -1.) Make sure you don't have vertical sync disabled in your driver settings.\n");
			}
			else
			{
				Con_Printf("FOUND: WGL_EXT_swap_control\n");
				gl_swap_control = true;
			}
		}
	}
	else
		Con_Warning ("vertical sync not supported (extension not found)\n");

	//
	// anisotropic filtering
	//
	if (strstr(gl_extensions, "GL_EXT_texture_filter_anisotropic"))
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
			Con_Warning ("anisotropic filtering locked by driver. Current driver setting is %f\n", test1);

		//get max value either way, so the menu and stuff know it
		glGetFloatv (GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_max_anisotropy);
	}
	else
		Con_Warning ("texture_filter_anisotropic not supported\n");
}

/*
===============
GL_SetupState -- johnfitz

does all the stuff from GL_Init that needs to be done every time a new GL render context is created
GL_Init will still do the stuff that only needs to be done once
===============
*/
void GL_SetupState (void)
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
void GL_Init (void)
{
	gl_vendor = (const char *) glGetString (GL_VENDOR);
	gl_renderer = (const char *) glGetString (GL_RENDERER);
	gl_version = (const char *) glGetString (GL_VERSION);
	gl_extensions = (const char *) glGetString (GL_EXTENSIONS);

	GL_CheckExtensions (); //johnfitz

	Cmd_AddCommand ("gl_info", GL_Info_f); //johnfitz

	Cvar_RegisterVariable (&vid_vsync, VID_Changed_f); //johnfitz

	if (SDL_strncasecmp(gl_renderer,"PowerVR",7)==0)
		fullsbardraw = true;
	if (SDL_strncasecmp(gl_renderer,"Permedia",8)==0)
		isPermedia = true;
#if 1
	//johnfitz -- intel video workarounds from Baker
	if (!strcmp(gl_vendor, "Intel"))
	{
		Con_Printf ("Intel Display Adapter detected\n");
		isIntelVideo = true;
	}
	//johnfitz
#endif

#if 0
	//johnfitz -- confirm presence of stencil buffer
	glGetIntegerv(GL_STENCIL_BITS, &gl_stencilbits);
	if(!gl_stencilbits)
		Con_Warning ("Could not create stencil buffer\n");
	else
		Con_Printf ("%i bit stencil buffer\n", gl_stencilbits);
#endif

	GL_SetupState (); //johnfitz
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
	if (!scr_skipupdate || block_drawing)
		SDL_GL_SwapBuffers();

	if (fullsbardraw)
		Sbar_Changed();
}

void VID_SetDefaultMode (void)
{
}


void	VID_Shutdown (void)
{
	if (vid_initialized)
	{
		vid_canalttab = false;
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
void ClearAllStates (void)
{
	int		i;

// send an up event for each key, to make sure the server clears them all
	for (i=0 ; i<256 ; i++)
	{
		Key_Event (i, false);
	}

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
VID_NumModes
=================
*/
int VID_NumModes (void)
{
	return nummodes;
}


/*
=================
VID_GetModePtr
=================
*/
vmode_t *VID_GetModePtr (int modenum)
{

	if ((modenum >= 0) && (modenum < nummodes))
		return &modelist[modenum];
	else
		return &badmode;
}


/*
=================
VID_GetModeDescription
=================
*/
char *VID_GetModeDescription (int mode)
{
	char		*pinfo;
	vmode_t		*pv;
	static char	temp[100];

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	if (!leavecurrentmode)
	{
		pv = VID_GetModePtr (mode);
		pinfo = pv->modedesc;
	}
	else
	{
		sprintf (temp, "Desktop resolution (%ix%ix%i)", //johnfitz -- added bpp
				 modelist[MODE_FULLSCREEN_DEFAULT].width,
				 modelist[MODE_FULLSCREEN_DEFAULT].height,
				 modelist[MODE_FULLSCREEN_DEFAULT].bpp); //johnfitz -- added bpp
		pinfo = temp;
	}

	return pinfo;
}

// KJB: Added this to return the mode driver name in description for console
/*
=================
VID_GetExtModeDescription
=================
*/
char *VID_GetExtModeDescription (int mode)
{
	static char	pinfo[40];
	vmode_t		*pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);
	if (modelist[mode].type == MODE_FULLSCREEN_DEFAULT)
	{
		if (!leavecurrentmode)
		{
			sprintf(pinfo,"%s fullscreen", pv->modedesc);
		}
		else
		{
			sprintf (pinfo, "Desktop resolution (%ix%ix%i)", //johnfitz -- added bpp
					 modelist[MODE_FULLSCREEN_DEFAULT].width,
					 modelist[MODE_FULLSCREEN_DEFAULT].height,
					 modelist[MODE_FULLSCREEN_DEFAULT].bpp); //johnfitz -- added bpp
		}
	}
	else
	{
		if (modestate == MODE_WINDOWED)
			sprintf(pinfo, "%s windowed", pv->modedesc);
		else
			sprintf(pinfo, "windowed");
	}

	return pinfo;
}

/*
=================
VID_DescribeCurrentMode_f
=================
*/
void VID_DescribeCurrentMode_f (void)
{
	Con_Printf ("%s\n", VID_GetExtModeDescription (vid_modenum));
}

/*
=================
VID_DescribeModes_f -- johnfitz -- changed formatting, and added refresh rates after each mode.
=================
*/
void VID_DescribeModes_f (void)
{
	int		i, lnummodes, t;
	vmode_t		*pv;
	int		lastwidth=0, lastheight=0, lastbpp=0, count=0;

	lnummodes = VID_NumModes ();

	t = leavecurrentmode;
	leavecurrentmode = 0;

	for (i=1 ; i<lnummodes ; i++)
	{
		pv = VID_GetModePtr (i);
		if (lastwidth != pv->width || lastheight != pv->height || lastbpp != pv->bpp)
		{
			if (count>0)
				Con_SafePrintf ("\n");
			Con_SafePrintf ("   %4i x %4i x %i", pv->width, pv->height, pv->bpp);
			lastwidth = pv->width;
			lastheight = pv->height;
			lastbpp = pv->bpp;
			count++;
		}
	}
	Con_Printf ("\n%i modes\n", count);

	leavecurrentmode = t;
}

//==========================================================================
//
//  INIT
//
//==========================================================================

/*
=================
VID_InitDIB
=================
*/
void VID_InitDIB (void)
{
	const SDL_VideoInfo *info;

	modelist[0].type = MODE_WINDOWED;

	if (COM_CheckParm("-width"))
		modelist[0].width = Q_atoi(com_argv[COM_CheckParm("-width")+1]);
	else
		modelist[0].width = 800;

	if (modelist[0].width < 320)
		modelist[0].width = 320;

	if (COM_CheckParm("-height"))
		modelist[0].height= Q_atoi(com_argv[COM_CheckParm("-height")+1]);
	else
		modelist[0].height = modelist[0].width * 240/320;

	if (modelist[0].height < 200) //johnfitz -- was 240
		modelist[0].height = 200; //johnfitz -- was 240

	info = SDL_GetVideoInfo();
	modelist[0].bpp = info->vfmt->BitsPerPixel;

	sprintf (modelist[0].modedesc, "%dx%dx%d", //johnfitz -- added bpp
			 modelist[0].width,
			 modelist[0].height,
			 modelist[0].bpp); //johnfitz -- added bpp

	modelist[0].modenum = MODE_WINDOWED;
	modelist[0].dib = 1;
	modelist[0].fullscreen = 0;
	modelist[0].halfscreen = 0;

	nummodes = 1;
}

/*
=================
VID_InitFullDIB
=================
*/
void VID_InitFullDIB (void)
{
	SDL_PixelFormat	format;
	SDL_Rect	**modes;
	Uint32		flags;
	int		i, j, k, modenum, originalnummodes, existingmode;
	int		bpps[3] = {16, 24, 32}; // enumerate >8 bpp modes

	originalnummodes = nummodes;
	modenum = 0;
	format.palette = NULL;

	// enumerate fullscreen modes
	flags = SDL_DEFAULT_FLAGS | SDL_FULLSCREEN;
	for (i = 0; i < 3; i++)
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

			modelist[nummodes].type = MODE_FULLSCREEN_DEFAULT;
			modelist[nummodes].width = modes[j]->w;
			modelist[nummodes].height = modes[j]->h;
			modelist[nummodes].modenum = 0;
			modelist[nummodes].halfscreen = 0;
			modelist[nummodes].dib = 1;
			modelist[nummodes].fullscreen = 1;
			modelist[nummodes].bpp = bpps[i];

			sprintf (modelist[nummodes].modedesc, "%dx%dx%d",
					modelist[nummodes].width,
					modelist[nummodes].height,
					modelist[nummodes].bpp); //johnfitz -- refreshrate

			// if the width is more than twice the height, reduce it by half because this
			// is probably a dual-screen monitor
			if (!COM_CheckParm("-noadjustaspect"))
			{
				if (modelist[nummodes].width > (modelist[nummodes].height << 1))
				{
					modelist[nummodes].width >>= 1;
					modelist[nummodes].halfscreen = 1;
					sprintf (modelist[nummodes].modedesc, "%dx%dx%d",
							modelist[nummodes].width,
							modelist[nummodes].height,
							modelist[nummodes].bpp);
				}
			}

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
		modenum++;
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
	const SDL_VideoInfo *info;
	int		i, existingmode;
	int		basenummodes, width, height, bpp, findbpp, done;

	//johnfitz -- clean up init readouts
	//Con_Printf("------------- Init Video -------------\n");
	//Con_Printf("%cVideo Init\n", 2);
	//johnfitz

	Cvar_RegisterVariable (&vid_fullscreen, VID_Changed_f); //johnfitz
	Cvar_RegisterVariable (&vid_width, VID_Changed_f); //johnfitz
	Cvar_RegisterVariable (&vid_height, VID_Changed_f); //johnfitz
	Cvar_RegisterVariable (&vid_bpp, VID_Changed_f); //johnfitz
	//Cvar_RegisterVariable (&vid_refreshrate, NULL); //johnfitz
	Cvar_RegisterVariable (&_windowed_mouse, NULL);

	Cmd_AddCommand ("vid_unlock", VID_Unlock); //johnfitz
	Cmd_AddCommand ("vid_restart", VID_Restart); //johnfitz
	Cmd_AddCommand ("vid_test", VID_Test); //johnfitz
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) == -1)
		Sys_Error("Could not initialize SDL Video");

	SDL_putenv("SDL_VIDEO_CENTERED=center");

	VID_InitDIB();
	basenummodes = nummodes = 1;
	VID_InitFullDIB();

	// Config file is not read yet, so we don't know vid_fullscreen.value
	// Changed this to default to -window as otherwise it occasionally forces
	// two switches of video mode (window->fullscreen->window) which is bad S.A
	// It's still not perfect but, hell, this ancient code can be a pain
	if (!COM_CheckParm("-fullscreen") && !COM_CheckParm ("-f"))
	{
		windowed = true;
		vid_default = MODE_WINDOWED;
	}
	else
	{
		Cvar_Set ("vid_fullscreen", "1");
		if (nummodes == 1)
			Sys_Error ("No RGB fullscreen modes available");

		windowed = false;

		if (COM_CheckParm("-mode"))
		{
			vid_default = Q_atoi(com_argv[COM_CheckParm("-mode")+1]);
		}
		else
		{
			if (COM_CheckParm("-current"))
			{
				info = SDL_GetVideoInfo();
				modelist[MODE_FULLSCREEN_DEFAULT].width = info->current_w;
				modelist[MODE_FULLSCREEN_DEFAULT].height = info->current_h;
				vid_default = MODE_FULLSCREEN_DEFAULT;
				leavecurrentmode = 1;
			}
			else
			{
				if (COM_CheckParm("-width"))
				{
					width = Q_atoi(com_argv[COM_CheckParm("-width")+1]);
				}
				else
				{
					width = 640;
				}

				if (COM_CheckParm("-bpp"))
				{
					bpp = Q_atoi(com_argv[COM_CheckParm("-bpp")+1]);
					findbpp = 0;
				}
				else
				{
					bpp = 15;
					findbpp = 1;
				}

				if (COM_CheckParm("-height"))
					height = Q_atoi(com_argv[COM_CheckParm("-height")+1]);

			// if they want to force it, add the specified mode to the list
				if (COM_CheckParm("-force") && (nummodes < MAX_MODE_LIST))
				{
					modelist[nummodes].type = MODE_FULLSCREEN_DEFAULT;
					modelist[nummodes].width = width;
					modelist[nummodes].height = height;
					modelist[nummodes].modenum = 0;
					modelist[nummodes].halfscreen = 0;
					modelist[nummodes].dib = 1;
					modelist[nummodes].fullscreen = 1;
					modelist[nummodes].bpp = bpp;
					sprintf (modelist[nummodes].modedesc, "%dx%dx%d",
							 modelist[nummodes].width,
							 modelist[nummodes].height,
							 modelist[nummodes].bpp);

					for (i=nummodes, existingmode = 0 ; i<nummodes ; i++)
					{
						if ((modelist[nummodes].width == modelist[i].width)   &&
							(modelist[nummodes].height == modelist[i].height) &&
							(modelist[nummodes].bpp == modelist[i].bpp))
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

				done = 0;

				do
				{
					if (COM_CheckParm("-height"))
					{
						height = Q_atoi(com_argv[COM_CheckParm("-height")+1]);

						for (i=1, vid_default=0 ; i<nummodes ; i++)
						{
							if ((modelist[i].width == width) &&
								(modelist[i].height == height) &&
								(modelist[i].bpp == bpp))
							{
								vid_default = i;
								done = 1;
								break;
							}
						}
					}
					else
					{
						for (i=1, vid_default=0 ; i<nummodes ; i++)
						{
							if ((modelist[i].width == width) && (modelist[i].bpp == bpp))
							{
								vid_default = i;
								done = 1;
								break;
							}
						}
					}

					if (!done)
					{
						if (findbpp)
						{
							switch (bpp)
							{
							case 15:
								bpp = 16;
								break;
							case 16:
								bpp = 32;
								break;
							case 32:
								bpp = 24;
								break;
							case 24:
								done = 1;
								break;
							}
						}
						else
						{
							done = 1;
						}
					}
				} while (!done);

				if (!vid_default)
				{
					Sys_Error ("Specified video mode not available");
				}
			}
		}
	}

	vid_initialized = true;

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	// set window icon
	PL_SetWindowIcon();

	VID_SetMode (vid_default);
	GL_Init ();

	vid_realmode = vid_modenum;

	vid_menucmdfn = VID_Menu_f; //johnfitz
	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	strcpy (badmode.modedesc, "Bad mode");
	vid_canalttab = true;

	if (COM_CheckParm("-fullsbar"))
		fullsbardraw = true;

	VID_Gamma_Init(); //johnfitz
	VID_Menu_Init(); //johnfitz

	//johnfitz -- command line vid settings should override config file settings.
	//so we have to lock the vid mode from now until after all config files are read.
	if (COM_CheckParm("-width") || COM_CheckParm("-height") ||
		COM_CheckParm("-bpp")				||
		COM_CheckParm("-window") || COM_CheckParm("-w") ||
		COM_CheckParm("-fullscreen") || COM_CheckParm("-f"))
	{
		vid_locked = true;
	}
	//johnfitz

	// The problem here is (say) previous video mode is 1024x768 windowed
	// And we call "fitzquake -w". This disables setting of 1024x768, and when video lock
	// is removed, we get 800x600.
}

// new proc by S.A., called by alt-return key binding.
void	VID_Toggle (void)
{
	S_ClearBuffer ();

	if (!vid_toggle_works)
		goto vrestart;
	if (SDL_WM_ToggleFullScreen(draw_context) == 1)
	{
		Sbar_Changed ();	// Sbar seems to need refreshing
		windowed = !windowed;
		Cvar_SetValue ("vid_fullscreen", ! (int)vid_fullscreen.value);
	}
	else
	{
		vid_toggle_works = false;
		Con_Printf ("ToggleFullScreen failed, attempting VID_Restart\n");
vrestart:
		Cvar_SetValue ("vid_fullscreen", ! (int)vid_fullscreen.value);
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

	Cvar_Set ("vid_width", va("%i", modelist[vid_default].width));
	Cvar_Set ("vid_height", va("%i", modelist[vid_default].height));
	Cvar_Set ("vid_bpp", va("%i", modelist[vid_default].bpp));
	Cvar_Set ("vid_fullscreen", (windowed) ? "0" : "1");

	if (SDL_GL_GetAttribute(SDL_GL_SWAP_CONTROL, &swap_control) == 0)
		Cvar_Set ("vid_vsync", (swap_control) ? "1" : "0");
}

//==========================================================================
//
//  NEW VIDEO MENU -- johnfitz
//
//==========================================================================

extern void M_Menu_Options_f (void);
extern void M_Print (int cx, int cy, char *str);
extern void M_PrintWhite (int cx, int cy, char *str);
extern void M_DrawCharacter (int cx, int line, int num);
extern void M_DrawTransPic (int x, int y, qpic_t *pic);
extern void M_DrawPic (int x, int y, qpic_t *pic);
extern void M_DrawCheckbox (int x, int y, int on);

extern qboolean	m_entersound;

#define VIDEO_OPTIONS_ITEMS 7
int		video_cursor_table[] = {48, 56, 64, 72, 80, 96, 104};
int		video_options_cursor = 0;

typedef struct {int width,height;} vid_menu_mode;

//TODO: replace these fixed-length arrays with hunk_allocated buffers

vid_menu_mode vid_menu_modes[MAX_MODE_LIST];
int vid_menu_nummodes=0;

int vid_menu_bpps[4];
int vid_menu_numbpps=0;

int vid_menu_rates[20];
int vid_menu_numrates=0;

/*
================
VID_Menu_Init
================
*/
void VID_Menu_Init (void)
{
	int i,j,h,w;

	for (i=1;i<nummodes;i++) //start i at mode 1 because 0 is windowed mode
	{
		w = modelist[i].width;
		h = modelist[i].height;

		for (j=0;j<vid_menu_nummodes;j++)
		{
			if (vid_menu_modes[j].width == w &&
				vid_menu_modes[j].height == h)
				break;
		}

		if (j==vid_menu_nummodes)
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
void VID_Menu_RebuildBppList (void)
{
	int i,j,b;

	vid_menu_numbpps=0;

	for (i=1;i<nummodes;i++) //start i at mode 1 because 0 is windowed mode
	{
		//bpp list is limited to bpps available with current width/height
		if (modelist[i].width != vid_width.value ||
			modelist[i].height != vid_height.value)
			continue;

		b = modelist[i].bpp;

		for (j=0;j<vid_menu_numbpps;j++)
		{
			if (vid_menu_bpps[j] == b)
				break;
		}

		if (j==vid_menu_numbpps)
		{
			vid_menu_bpps[j] = b;
			vid_menu_numbpps++;
		}
	}

	//if there are no valid fullscreen bpps for this width/height, just pick one
	if (vid_menu_numbpps == 0)
	{
		Cvar_SetValue ("vid_bpp",(float)modelist[0].bpp);
		return;
	}

	//if vid_bpp is not in the new list, change vid_bpp
	for (i=0;i<vid_menu_numbpps;i++)
		if (vid_menu_bpps[i] == (int)(vid_bpp.value))
			break;

	if (i==vid_menu_numbpps)
		Cvar_SetValue ("vid_bpp",(float)vid_menu_bpps[0]);
}

/*
================
VID_Menu_ChooseNextMode

chooses next resolution in order, then updates vid_width and
vid_height cvars, then updates bpp and refreshrate lists
================
*/
void VID_Menu_ChooseNextMode (int dir)
{
	int i;

	for (i=0;i<vid_menu_nummodes;i++)
	{
		if (vid_menu_modes[i].width == vid_width.value &&
			vid_menu_modes[i].height == vid_height.value)
			break;
	}

	if (i==vid_menu_nummodes) //can't find it in list, so it must be a custom windowed res
	{
		i = 0;
	}
	else
	{
		i+=dir;
		if (i>=vid_menu_nummodes)
			i = 0;
		else if (i<0)
			i = vid_menu_nummodes-1;
	}

	Cvar_SetValue ("vid_width",(float)vid_menu_modes[i].width);
	Cvar_SetValue ("vid_height",(float)vid_menu_modes[i].height);
	VID_Menu_RebuildBppList ();
}

/*
================
VID_Menu_ChooseNextBpp

chooses next bpp in order, then updates vid_bpp cvar, then updates refreshrate list
================
*/
void VID_Menu_ChooseNextBpp (int dir)
{
	int i;

	for (i=0;i<vid_menu_numbpps;i++)
	{
		if (vid_menu_bpps[i] == vid_bpp.value)
			break;
	}

	if (i==vid_menu_numbpps) //can't find it in list
	{
		i = 0;
	}
	else
	{
		i+=dir;
		if (i>=vid_menu_numbpps)
			i = 0;
		else if (i<0)
			i = vid_menu_numbpps-1;
	}

	Cvar_SetValue ("vid_bpp",(float)vid_menu_bpps[i]);
}

/*
================
VID_Menu_ChooseNextRate

chooses next refresh rate in order, then updates vid_refreshrate cvar
================
*/
void VID_Menu_ChooseNextRate (int dir)
{
	int i;

	for (i=0;i<vid_menu_numrates;i++)
	{
		if (vid_menu_rates[i] == vid_refreshrate.value)
			break;
	}

	if (i==vid_menu_numrates) //can't find it in list
	{
		i = 0;
	}
	else
	{
		i+=dir;
		if (i>=vid_menu_numrates)
			i = 0;
		else if (i<0)
			i = vid_menu_numrates-1;
	}

	Cvar_SetValue ("vid_refreshrate",(float)vid_menu_rates[i]);
}

/*
================
VID_MenuKey
================
*/
void VID_MenuKey (int key)
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
			VID_Menu_ChooseNextMode (-1);
			break;
		case 1:
			VID_Menu_ChooseNextBpp (-1);
			break;
		case 2:
			VID_Menu_ChooseNextRate (-1);
			break;
		case 3:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case 4:
			Cbuf_AddText ("toggle vid_vsync\n"); // kristian
			break;
		case 5:
		case 6:
		default:
			break;
		}
		break;

	case K_RIGHTARROW:
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
			VID_Menu_ChooseNextRate (1);
			break;
		case 3:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case 4:
			Cbuf_AddText ("toggle vid_vsync\n");
			break;
		case 5:
		case 6:
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
			VID_Menu_ChooseNextRate (1);
			break;
		case 3:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case 4:
			Cbuf_AddText ("toggle vid_vsync\n");
			break;
		case 5:
			Cbuf_AddText ("vid_test\n");
			break;
		case 6:
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
void VID_MenuDraw (void)
{
	int i = 0;
	qpic_t *p;
	char *title;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp"));

	//p = Draw_CachePic ("gfx/vidmodes.lmp");
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	// title
	title = "Video Options";
	M_PrintWhite ((320-8*strlen(title))/2, 32, title);

	// options
	M_Print (16, video_cursor_table[i], "            Video mode");
	M_Print (216, video_cursor_table[i], va("%ix%i", (int)vid_width.value, (int)vid_height.value));
	i++;

	M_Print (16, video_cursor_table[i], "           Color depth");
	M_Print (216, video_cursor_table[i], va("%i", (int)vid_bpp.value));
	i++;

	M_Print (16, video_cursor_table[i], "          Refresh rate");
//	M_Print (216, video_cursor_table[i], va("%i Hz", (int)vid_refreshrate.value)); refresh rates are disabled for now -- kristian
	M_Print (216, video_cursor_table[i], "N/A");
	i++;

	M_Print (16, video_cursor_table[i], "            Fullscreen");
	M_DrawCheckbox (216, video_cursor_table[i], (int)vid_fullscreen.value);
	i++;

	// added vsync to the video menu -- kristian
	M_Print (16, video_cursor_table[i], "         Vertical Sync");
	if (gl_swap_control)
		M_DrawCheckbox (216, video_cursor_table[i], (int)vid_vsync.value);
	else
		M_Print (216, video_cursor_table[i], "N/A");

	i++;

	M_Print (16, video_cursor_table[i], "          Test changes");
	i++;

	M_Print (16, video_cursor_table[i], "         Apply changes");

	// cursor
	M_DrawCharacter (200, video_cursor_table[video_options_cursor], 12+((int)(realtime*4)&1));

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
void VID_Menu_f (void)
{
	key_dest = key_menu;
	m_state = m_video;
	m_entersound = true;

	//set all the cvars to match the current mode when entering the menu
	VID_SyncCvars ();

	//set up bpp and rate lists based on current cvars
	VID_Menu_RebuildBppList ();
}
