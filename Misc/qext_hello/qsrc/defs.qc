/*
 * Example Quake Extension - definitions
 *
 * Copyright (C) 1996-1997 Id Software, Inc.
 * Copyright (C) 2025 Tóth János <gomba007@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

 /*
==============================================================================

			SOURCE FOR GLOBALVARS_T C STRUCTURE

==============================================================================
*/

//
// system globals
//
entity self;
entity other;
entity world;

float time;
float frametime;
float force_retouch;                // force all entities to touch triggers

string mapname;

float deathmatch;
float coop;
float teamplay;
float serverflags;                  // propagated from level to level, used to
float total_secrets;
float total_monsters;
float found_secrets;                // number of secrets found
float killed_monsters;              // number of monsters killed
float parm1, parm2, parm3, parm4, parm5, parm6, parm7, parm8, parm9, parm10, parm11, parm12, parm13, parm14, parm15, parm16;

//
// global variables set by built in functions
//
vector v_forward, v_up, v_right;    // set by makevectors()

float trace_allsolid;
float trace_startsolid;
float trace_fraction;
vector trace_endpos;
vector trace_plane_normal;
float trace_plane_dist;
entity trace_ent;
float trace_inopen;
float trace_inwater;

entity msg_entity;                  // destination of single entity writes

//
// required prog functions
//
void() main;                        // only for testing
void() StartFrame;
void() PlayerPreThink;
void() PlayerPostThink;
void() ClientKill;
void() ClientConnect;
void() PutClientInServer;           // call after setting the parm1... parms
void() ClientDisconnect;
void() SetNewParms;                 // called when a client first connects to
void() SetChangeParms;              // call to set parms for self so they can
void end_sys_globals;               // flag for structure dumping

/*
==============================================================================

			SOURCE FOR ENTVARS_T C STRUCTURE

==============================================================================
*/

.float modelindex;                  // model index in the precached list
.vector absmin, absmax;             // origin + mins / maxs
.float ltime;                       // local time for entity
.float movetype;
.float solid;
.vector origin;
.vector oldorigin;
.vector velocity;
.vector angles;
.vector avelocity;
.vector punchangle;                 // temp angle adjust from damage or recoil
.string classname;                  // spawn function
.string model;
.float frame;
.float skin;
.float effects;
.vector mins, maxs;                 // bounding box extents reletive to origin
.vector size;                       // maxs - mins
.void() touch;
.void() use;
.void() think;
.void() blocked;                    // for doors or plats, called when can't push other
.float nextthink;
.entity groundentity;
.float health;
.float frags;
.float weapon;                      // one of the IT_SHOTGUN, etc flags
.string weaponmodel;
.float weaponframe;
.float currentammo;
.float ammo_shells, ammo_nails, ammo_rockets, ammo_cells;
.float items;                       // bit flags
.float takedamage;
.entity chain;
.float deadflag;
.vector view_ofs;                   // add to origin to get eye point
.float button0;                     // fire
.float button1;                     // use
.float button2;                     // jump
.float impulse;                     // weapon changes
.float fixangle;
.vector v_angle;                    // view / targeting angle for players
.float idealpitch;                  // calculated pitch angle for lookup up slopes
.string netname;
.entity enemy;
.float flags;
.float colormap;
.float team;
.float max_health;                  // players maximum health is stored here
.float teleport_time;               // don't back up
.float armortype;                   // save this fraction of incoming damage
.float armorvalue;
.float waterlevel;                  // 0 = not in, 1 = feet, 2 = wast, 3 = eyes
.float watertype;                   // a contents value
.float ideal_yaw;
.float yaw_speed;
.entity aiment;
.entity goalentity;                 // a movetarget or an enemy
.float spawnflags;
.string target;
.string targetname;
.float dmg_take;
.float dmg_save;
.entity dmg_inflictor;
.entity owner;                      // who launched a missile
.vector movedir;                    // mostly for doors, but also used for waterjump
.string message;                    // trigger messages
.float sounds;                      // either a cd track number or sound number
.string noise, noise1, noise2, noise3;  // contains names of wavs to play

void end_sys_fields;     // flag for structure dumping
// End. Lines below this MAY be altered, to some extent
//============================================================================

//
// builtin functions
//
void(vector ang) makevectors                                            = #1; // sets v_forward, etc globals
void(entity e, vector o) setorigin                                      = #2;
void(entity e, string m) setmodel                                       = #3; // set movetype and solid first
void(entity e, vector min, vector max) setsize                          = #4;
void() break                                                            = #6;
float() random                                                          = #7; // returns 0 - 1
void(entity e, float chan, string samp, float vol, float atten) sound   = #8;
vector(vector v) normalize                                              = #9;
void(string e) error                                                    = #10;
void(string e) objerror                                                 = #11;
float(vector v) vlen                                                    = #12;
float(vector v) vectoyaw                                                = #13;
entity() spawn                                                          = #14;
void(entity e) remove                                                   = #15;
void(vector v1, vector v2, float nomonsters, entity forent) traceline   = #16;
entity() checkclient                                                    = #17; // returns a client to look for
entity(entity start, .string fld, string match) find                    = #18;
string(string s) precache_sound                                         = #19;
string(string s) precache_model                                         = #20;
void(entity client, string s)stuffcmd                                   = #21;
entity(vector org, float rad) findradius                                = #22;
void(string s) bprint                                                   = #23;
void(entity client, string s) sprint                                    = #24;
void(string s) dprint                                                   = #25;
string(float f) ftos                                                    = #26;
string(vector v) vtos                                                   = #27;
void() coredump                                                         = #28; // prints all edicts
void() traceon                                                          = #29; // turns statment trace on
void() traceoff                                                         = #30;
void(entity e) eprint                                                   = #31; // prints an entire edict
float(float yaw, float dist) walkmove                                   = #32; // returns TRUE or FALSE
float(float yaw, float dist) droptofloor                                = #34; // TRUE if landed on floor
void(float style, string value) lightstyle                              = #35;
float(float v) rint                                                     = #36; // round to nearest int
float(float v) floor                                                    = #37; // largest integer <= v
float(float v) ceil                                                     = #38; // smallest integer >= v
float(entity e) checkbottom                                             = #40; // true if self is on ground
float(vector v) pointcontents                                           = #41; // returns a CONTENT_*
float(float f) fabs                                                     = #43;
vector(entity e, float speed) aim                                       = #44; // returns the shooting vector
float(string s) cvar                                                    = #45; // return cvar.value
void(string s) localcmd                                                 = #46; // put string into local que
entity(entity e) nextent                                                = #47; // for looping through all ents
void(vector o, vector d, float color, float count) particle             = #48;// start a particle effect
void() ChangeYaw                                                        = #49; // turn towards self.ideal_yaw
vector(vector v) vectoangles                                            = #51;
void(float to, float f) WriteByte                                       = #52;
void(float to, float f) WriteChar                                       = #53;
void(float to, float f) WriteShort                                      = #54;
void(float to, float f) WriteLong                                       = #55;
void(float to, float f) WriteCoord                                      = #56;
void(float to, float f) WriteAngle                                      = #57;
void(float to, string s) WriteString                                    = #58;
void(float to, entity s) WriteEntity                                    = #59;
void(float step) movetogoal                                             = #67;
string(string s) precache_file                                          = #68; // no effect except for -copy
void(entity e) makestatic                                               = #69;
void(string s) changelevel                                              = #70;
void(string var, string val) cvar_set                                   = #72; // sets cvar.value
void(entity client, string s) centerprint                               = #73; // sprint, but in middle
void(entity client, string s, string s) centerprint2                    = #73;
void(entity client, string s, string s, string s) centerprint3          = #73;
void(entity client, string s, string s, string s, string s) centerprint4                                = #73;
void(entity client, string s, string s, string s, string s, string s) centerprint5                      = #73;
void(entity client, string s, string s, string s, string s, string s, string s) centerprint6            = #73;
void(entity client, string s, string s, string s, string s, string s, string s, string s) centerprint7  = #73;
void(vector pos, string samp, float vol, float atten) ambientsound      = #74;
string(string s) precache_model2                                        = #75; // registered version only
string(string s) precache_sound2                                        = #76; // registered version only
string(string s) precache_file2                                         = #77; // registered version only
void(entity e) setspawnparms                                            = #78; // set parm1... to the

float(string path) OpenExtension = #93;
string(float id, string cmd, string arg) CallExtensionString = #94;
float(float id, string cmd, float arg) CallExtensionNumber = #95;
vector(float id, string cmd, vector arg) CallExtensionVector = #96;
float(float id, string cmd, entity arg) CallExtensionEntity = #97;

/*
==============================================================================

				VARS NOT REFERENCED BY C CODE

==============================================================================
*/

//
// constants
//

float FALSE = 0;
float TRUE  = 1;

// edict.flags
float FL_FLY            = 1;
float FL_SWIM           = 2;
float FL_CLIENT         = 8; // set for all client edicts
float FL_INWATER        = 16; // for enter / leave water splash
float FL_MONSTER        = 32;
float FL_GODMODE        = 64; // player cheat
float FL_NOTARGET       = 128; // player cheat
float FL_ITEM           = 256; // extra wide size for bonus items
float FL_ONGROUND       = 512; // standing on something
float FL_PARTIALGROUND  = 1024; // not all corners are valid
float FL_WATERJUMP      = 2048; // player jumping out of water
float FL_JUMPRELEASED   = 4096; // for jump debouncing

// edict.movetype values
float MOVETYPE_NONE             = 0; // never moves
float MOVETYPE_ANGLENOCLIP      = 1;
float MOVETYPE_ANGLECLIP        = 2;
float MOVETYPE_WALK             = 3; // players only
float MOVETYPE_STEP             = 4; // discrete, not real time unless fall
float MOVETYPE_FLY              = 5;
float MOVETYPE_TOSS             = 6; // gravity
float MOVETYPE_PUSH             = 7; // no clip to world, push and crush
float MOVETYPE_NOCLIP           = 8;
float MOVETYPE_FLYMISSILE       = 9; // fly with extra size against monsters
float MOVETYPE_BOUNCE           = 10;
float MOVETYPE_BOUNCEMISSILE    = 11; // bounce with extra size

// edict.solid values
float SOLID_NOT         = 0; // no interaction with other objects
float SOLID_TRIGGER     = 1; // touch on edge, but not blocking
float SOLID_BBOX        = 2; // touch on edge, block
float SOLID_SLIDEBOX    = 3; // touch on edge, but not an onground
float SOLID_BSP         = 4; // bsp clip, touch on edge, block

// range values
float RANGE_MELEE   = 0;
float RANGE_NEAR    = 1;
float RANGE_MID     = 2;
float RANGE_FAR     = 3;

// deadflag values
float DEAD_NO           = 0;
float DEAD_DYING        = 1;
float DEAD_DEAD         = 2;
float DEAD_RESPAWNABLE  = 3;

// takedamage values
float DAMAGE_NO  = 0;
float DAMAGE_YES  = 1;
float DAMAGE_AIM  = 2;

.void() th_stand;
.void() th_walk;
.void() th_run;
.void(entity attacker, float damage) th_pain;
.void() th_die;
.void() th_missile;
.void() th_melee;

// point content values
float CONTENT_EMPTY = -1;
float CONTENT_SOLID = -2;
float CONTENT_WATER = -3;
float CONTENT_SLIME = -4;
float CONTENT_LAVA  = -5;
float CONTENT_SKY   = -6;

float STATE_RAISED  = 0;
float STATE_LOWERED = 1;
float STATE_UP      = 2;
float STATE_DOWN    = 3;

vector VEC_ORIGIN   = '0 0 0';
vector VEC_HULL_MIN = '-16 -16 -24';
vector VEC_HULL_MAX = '16 16 32';

vector VEC_HULL2_MIN    = '-32 -32 -24';
vector VEC_HULL2_MAX    = '32 32 64';

// protocol bytes
float SVC_BAD               = 0;
float SVC_NOP               = 1;
float SVC_DISCONNECT        = 2;
float SVC_UPDATESTAT        = 3;
float SVC_VERSION           = 4;
float SVC_SETVIEW           = 5;
float SVC_SOUND             = 6;
float SVC_TIME              = 7;
float SVC_PRINT             = 8;
float SVC_STUFFTEXT         = 9;
float SVC_SETANGLE          = 10;
float SVC_SERVERINFO        = 11;
float SVC_LIGHTSTYLE        = 12;
float SVC_UPDATENAME        = 13;
float SVC_UPDATEFRAGS       = 14;
float SVC_CLIENTDATA        = 15;
float SVC_STOPSOUND         = 16;
float SVC_UPDATECOLORS      = 17;
float SVC_PARTICLE          = 18;
float SVC_DAMAGE            = 19;
float SVC_SPAWNSTATIC       = 20;
float SVC_SPAWNBINARY       = 21;
float SVC_SPAWNBASELINE     = 22;
float SVC_TEMPENTITY        = 23;
float SVC_SETPAUSE          = 24;
float SVC_SIGNONNUM         = 25;
float SVC_CENTERPRINT       = 26;
float SVC_KILLEDMONSTER     = 27;
float SVC_FOUNDSECRET       = 28;
float SVC_SPAWNSTATICSOUND  = 29; // 1998-08-08 Complete SVC list by Zhenga
float SVC_INTERMISSION      = 30;
float SVC_FINALE            = 31;
float SVC_CDTRACK           = 32;
float SVC_SELLSCREEN        = 33;
float SVC_CUTSCENE          = 34; // 1998-08-08 Complete SVC list by Zhenga

float TE_SPIKE          = 0;
float TE_SUPERSPIKE     = 1;
float TE_GUNSHOT        = 2;
float TE_EXPLOSION      = 3;
float TE_TAREXPLOSION   = 4;
float TE_LIGHTNING1     = 5;
float TE_LIGHTNING2     = 6;
float TE_WIZSPIKE       = 7;
float TE_KNIGHTSPIKE    = 8;
float TE_LIGHTNING3     = 9;
float TE_LAVASPLASH     = 10;
float TE_TELEPORT       = 11;

// sound channels
// channel 0 never willingly overrides
// other channels (1-7) allways override a playing sound on that channel
float CHAN_AUTO     = 0;
float CHAN_WEAPON   = 1;
float CHAN_VOICE    = 2;
float CHAN_ITEM     = 3;
float CHAN_BODY     = 4;

float ATTN_NONE     = 0;
float ATTN_NORM     = 1;
float ATTN_IDLE     = 2;
float ATTN_STATIC   = 3;

// update types

float UPDATE_GENERAL    = 0;
float UPDATE_STATIC     = 1;
float UPDATE_BINARY     = 2;
float UPDATE_TEMP       = 3;

// entity effects

float EF_BRIGHTFIELD    = 1;
float EF_MUZZLEFLASH    = 2;
float EF_BRIGHTLIGHT    = 4;
float EF_DIMLIGHT       = 8;


// messages
float MSG_BROADCAST = 0; // unreliable to all
float MSG_ONE       = 1; // reliable to one (msg_entity)
float MSG_ALL       = 2; // reliable to all
float MSG_INIT      = 3; // write to the init string

float AS_STRAIGHT   = 1;
float AS_SLIDING    = 2;
float AS_MELEE      = 3;
float AS_MISSILE    = 4;

void() SUB_Null = {};
void() SUB_Null2 = {};

// Quake assumes these are defined.
entity activator;
string string_null; // null string, nothing should be held here

.string wad, map;
.float worldtype, delay, wait, lip, light_lev, speed, style, skill;
.string killtarget;
.vector pos1, pos2, mangle;

void() SUB_Remove = {
    remove(self);
};

//============================================================================
