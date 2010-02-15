/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
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
// r_misc.c

#include "quakedef.h"

//johnfitz -- new cvars
extern cvar_t r_stereo;
extern cvar_t r_stereodepth;
extern cvar_t r_clearcolor;
extern cvar_t r_drawflat;
extern cvar_t r_flatlightstyles;
extern cvar_t gl_fullbrights;
extern cvar_t gl_farclip;
extern cvar_t gl_overbright;
extern cvar_t gl_overbright_models;
extern cvar_t r_waterquality;
extern cvar_t r_oldwater;
extern cvar_t r_waterwarp;
extern cvar_t r_oldskyleaf;
extern cvar_t r_drawworld;
extern cvar_t r_showtris;
extern cvar_t r_showbboxes;
extern cvar_t r_lerpmodels;
extern cvar_t r_lerpmove;
extern cvar_t r_nolerp_list;
//johnfitz

extern float load_subdivide_size; //johnfitz -- remember what subdivide_size value was when this map was loaded

extern cvar_t gl_subdivide_size; //johnfitz -- moved here from gl_model.c

extern gltexture_t *playertextures[MAX_SCOREBOARD]; //johnfitz

void R_NoLerpList_f (void); //johnfitz

/*
====================
GL_Overbright_f -- johnfitz
====================
*/
void GL_Overbright_f (void)
{
	R_RebuildAllLightmaps ();
}

/*
====================
GL_Fullbrights_f -- johnfitz
====================
*/
void GL_Fullbrights_f (void)
{
	TexMgr_ReloadNobrightImages ();
}

/*
====================
R_SetClearColor_f -- johnfitz
====================
*/
void R_SetClearColor_f (void)
{
	byte	*rgb;
	int		s;

	s = (int)r_clearcolor.value & 0xFF;
	rgb = (byte*)(d_8to24table + s);
	glClearColor (rgb[0]/255.0,rgb[1]/255.0,rgb[2]/255.0,0);
}

/*
====================
R_Novis_f -- johnfitz
====================
*/
void R_Novis_f (void)
{
	extern int vis_changed;
	vis_changed = 1;
}

/*
====================
R_OldSkyLeaf_f -- johnfitz
====================
*/
void R_OldSkyLeaf_f (void)
{
	extern int vis_changed;
	vis_changed = 1;
}

/*
===============
R_Envmap_f

Grab six views for environment mapping tests
===============
*/
void R_Envmap_f (void)
{
	byte	buffer[256*256*4];
	// char	name[1024]; unused -- kristian

	glDrawBuffer  (GL_FRONT);
	glReadBuffer  (GL_FRONT);
	envmap = true;

	r_refdef.vrect.x = 0;
	r_refdef.vrect.y = 0;
	r_refdef.vrect.width = 256;
	r_refdef.vrect.height = 256;

	r_refdef.viewangles[0] = 0;
	r_refdef.viewangles[1] = 0;
	r_refdef.viewangles[2] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env0.rgb", buffer, sizeof(buffer));

	r_refdef.viewangles[1] = 90;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env1.rgb", buffer, sizeof(buffer));

	r_refdef.viewangles[1] = 180;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env2.rgb", buffer, sizeof(buffer));

	r_refdef.viewangles[1] = 270;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env3.rgb", buffer, sizeof(buffer));

	r_refdef.viewangles[0] = -90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env4.rgb", buffer, sizeof(buffer));

	r_refdef.viewangles[0] = 90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env5.rgb", buffer, sizeof(buffer));

	envmap = false;
	glDrawBuffer  (GL_BACK);
	glReadBuffer  (GL_BACK);
	GL_EndRendering ();
}

/*
===============
R_Init
===============
*/
void R_Init (void)
{
	// extern byte *hunk_base; unused -- kristian
	extern cvar_t gl_finish;

	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);
	Cmd_AddCommand ("envmap", R_Envmap_f);
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f);

	Cvar_RegisterVariable (&r_norefresh, NULL);
	Cvar_RegisterVariable (&r_lightmap, NULL);
	Cvar_RegisterVariable (&r_fullbright, NULL);
	Cvar_RegisterVariable (&r_drawentities, NULL);
	Cvar_RegisterVariable (&r_drawviewmodel, NULL);
	Cvar_RegisterVariable (&r_shadows, NULL);
	Cvar_RegisterVariable (&r_wateralpha, NULL);
	Cvar_RegisterVariable (&r_dynamic, NULL);
	Cvar_RegisterVariable (&r_novis, R_Novis_f);
	Cvar_RegisterVariable (&r_speeds, NULL);

	Cvar_RegisterVariable (&gl_finish, NULL);
	Cvar_RegisterVariable (&gl_clear, NULL);
	Cvar_RegisterVariable (&gl_cull, NULL);
	Cvar_RegisterVariable (&gl_smoothmodels, NULL);
	Cvar_RegisterVariable (&gl_affinemodels, NULL);
	Cvar_RegisterVariable (&gl_polyblend, NULL);
	Cvar_RegisterVariable (&gl_flashblend, NULL);
	Cvar_RegisterVariable (&gl_playermip, NULL);
	Cvar_RegisterVariable (&gl_nocolors, NULL);

	//johnfitz -- new cvars
	Cvar_RegisterVariable (&r_stereo, NULL);
	Cvar_RegisterVariable (&r_stereodepth, NULL);
	Cvar_RegisterVariable (&r_clearcolor, R_SetClearColor_f);
	Cvar_RegisterVariable (&r_waterquality, NULL);
	Cvar_RegisterVariable (&r_oldwater, NULL);
	Cvar_RegisterVariable (&r_waterwarp, NULL);
	Cvar_RegisterVariable (&r_drawflat, NULL);
	Cvar_RegisterVariable (&r_flatlightstyles, NULL);
	Cvar_RegisterVariable (&r_oldskyleaf, R_OldSkyLeaf_f);
	Cvar_RegisterVariable (&r_drawworld, NULL);
	Cvar_RegisterVariable (&r_showtris, NULL);
	Cvar_RegisterVariable (&r_showbboxes, NULL);
	Cvar_RegisterVariable (&gl_farclip, NULL);
	Cvar_RegisterVariable (&gl_fullbrights, GL_Fullbrights_f);
	Cvar_RegisterVariable (&gl_overbright, GL_Overbright_f);
	Cvar_RegisterVariable (&gl_overbright_models, NULL);
	Cvar_RegisterVariable (&r_lerpmodels, NULL);
	Cvar_RegisterVariable (&r_lerpmove, NULL);
	Cvar_RegisterVariable (&r_nolerp_list, R_NoLerpList_f);
	//johnfitz

	Cvar_RegisterVariable (&gl_subdivide_size, NULL); //johnfitz -- moved here from gl_model.c

	R_InitParticles ();
	R_SetClearColor_f (); //johnfitz

	Sky_Init (); //johnfitz
	Fog_Init (); //johnfitz

#ifdef GLTEST
	Test_Init ();
#endif
}

/*
===============
R_NoLerpList_f -- johnfitz -- called when r_nolerp_list cvar changes
===============
*/
void R_NoLerpList_f (void)
{
	int i;

	for (i=0; i < MAX_MODELS; i++)
		Mod_SetExtraFlags (cl.model_precache[i]);
}

/*
===============
R_TranslatePlayerSkin -- johnfitz -- rewritten.  also, only handles new colors, not new skins
===============
*/
void R_TranslatePlayerSkin (int playernum)
{
	int			top, bottom;

	top = (cl.scores[playernum].colors & 0xf0)>>4;
	bottom = cl.scores[playernum].colors &15;

	//FIXME: if gl_nocolors is on, then turned off, the textures may be out of sync with the scoreboard colors.
	if (!gl_nocolors.value)
		if (playertextures[playernum])
			TexMgr_ReloadImage (playertextures[playernum], top, bottom);
}

/*
===============
R_TranslateNewPlayerSkin -- johnfitz -- split off of TranslatePlayerSkin -- this is called when
the skin or model actually changes, instead of just new colors
added bug fix from bengt jardup
===============
*/
void R_TranslateNewPlayerSkin (int playernum)
{
	char		name[64];
	byte		*pixels;
	aliashdr_t	*paliashdr;
	int		skinnum;

//get correct texture pixels
	currententity = &cl_entities[1+playernum];

	if (!currententity->model || currententity->model->type != mod_alias)
		return;

	paliashdr = (aliashdr_t *)Mod_Extradata (currententity->model);

	skinnum = currententity->skinnum;

	//TODO: move these tests to the place where skinnum gets received from the server
	if (skinnum < 0 || skinnum >= paliashdr->numskins)
	{
		Con_DPrintf("(%d): Invalid player skin #%d\n", playernum, skinnum);
		skinnum = 0;
	}

	pixels = (byte *)paliashdr + paliashdr->texels[skinnum]; // This is not a persistent place!

//upload new image
	sprintf(name, "player_%i", playernum);
	playertextures[playernum] = TexMgr_LoadImage (currententity->model, name, paliashdr->skinwidth, paliashdr->skinheight,
		SRC_INDEXED, pixels, paliashdr->gltextures[skinnum][0]->source_file, paliashdr->gltextures[skinnum][0]->source_offset, TEXPREF_PAD | TEXPREF_OVERWRITE);

//now recolor it
	R_TranslatePlayerSkin (playernum);
}

/*
===============
R_NewGame -- johnfitz -- handle a game switch
===============
*/
void R_NewGame (void)
{
	int i;

	//clear playertexture pointers (the textures themselves were freed by texmgr_newgame)
	for (i=0; i<MAX_SCOREBOARD; i++)
		playertextures[i] = NULL;
}

/*
===============
R_NewMap
===============
*/
void R_NewMap (void)
{
	int		i;

	for (i=0 ; i<256 ; i++)
		d_lightstylevalue[i] = 264;		// normal light value

// clear out efrags in case the level hasn't been reloaded
// FIXME: is this one short?
	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
		cl.worldmodel->leafs[i].efrags = NULL;

	r_viewleaf = NULL;
	R_ClearParticles ();

	GL_BuildLightmaps ();

	r_framecount = 0; //johnfitz -- paranoid?
	r_visframecount = 0; //johnfitz -- paranoid?

	Sky_NewMap (); //johnfitz -- skybox in worldspawn
	Fog_NewMap (); //johnfitz -- global fog in worldspawn

	load_subdivide_size = gl_subdivide_size.value; //johnfitz -- is this the right place to set this?
}

/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
void R_TimeRefresh_f (void)
{
	int			i;
	float		start, stop, time;
    /* unused -- kristian
	int			startangle;
	vrect_t		vr;
    */

	glDrawBuffer  (GL_FRONT);
	glFinish ();

	start = Sys_FloatTime ();
	for (i=0 ; i<128 ; i++)
	{
		r_refdef.viewangles[1] = i/128.0*360.0;
		R_RenderView ();
	}

	glFinish ();
	stop = Sys_FloatTime ();
	time = stop-start;
	Con_Printf ("%f seconds (%f fps)\n", time, 128/time);

	glDrawBuffer (GL_BACK);
	GL_EndRendering ();
}

void D_FlushCaches (void)
{
}
