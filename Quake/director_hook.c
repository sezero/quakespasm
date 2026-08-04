/*
The actual integration: three hook points into stock Quake (see
Director_Init/Director_NewLevel/Director_Frame call sites in host.c and
sv_main.c), everything else lives behind the quake_director.h C ABI in
rust/quake_bridge. No QuakeC/progs.dat changes — difficulty is applied
through the `skill` cvar QuakeC's stock monster-spawn logic already
reads, and telemetry is read from fields the engine already tracks
(pr_global_struct->killed_monsters/total_monsters, sv.time, player
health) — see rust/quake_bridge/src/lib.rs's quake_apply_kill_stats doc
comment for exactly how those are normalized into signals.

Decision JSON returned by quake_poll_decide_llm is parsed with
json.c/json.h/jsmn.h (JSON_Parse/JSON_Find*) — a small, self-contained
GPL parser vendored in from ironwail (a QuakeSpasm fork that happens to
ship one) rather than written from scratch, since QuakeSpasm itself
doesn't include a JSON parser.
*/
#include "quakedef.h"
#include "json.h"
#include "quake_director.h"
#include "director_hook.h"

static qboolean g_director_ready = false;
static int64_t g_pending_request_id = -1;		// -1 = no decide() in flight

static qboolean g_have_pending_difficulty = false;
static double g_pending_difficulty = 0.5;		// 0.0-1.0, from the last resolved Decision

static qboolean g_have_level_start_health = false;
static float g_level_start_health = 100.0f;

void Director_Init (void)
{
	if (quake_director_init ("http://localhost:11434", "qwen2.5:7b") == 0)
	{
		g_director_ready = true;
		Con_Printf ("Director: initialized (qwen2.5:7b @ http://localhost:11434)\n");
	}
	else
	{
		Con_Printf ("Director: init failed, gameplay will run stock: %s\n", quake_last_error ());
	}
}

/*
Called once per frame from _Host_Frame. Polls whatever start_decide_llm
call is in flight; once it resolves, stashes the difficulty for
Director_NewLevel to apply at the *next* level transition (never applied
mid-level) — never blocks, never fabricates a result on failure.
*/
void Director_Frame (void)
{
	int status;
	char *result;

	if (!g_director_ready || g_pending_request_id < 0)
		return;

	status = 0;
	result = quake_poll_decide_llm (g_pending_request_id, &status);
	if (status == 0)
		return;		// still pending; result is NULL, nothing to free

	g_pending_request_id = -1;

	if (status == 1)
	{
		json_t *json = JSON_Parse (result);
		if (json)
		{
			const jsonentry_t *attributes = JSON_Find (json->root, "attributes", JSON_OBJECT);
			const jsonentry_t *labels = JSON_Find (json->root, "labels", JSON_OBJECT);
			const double *difficulty = attributes ? JSON_FindNumber (attributes, "difficulty") : NULL;
			const char *pacing_beat = labels ? JSON_FindString (labels, "pacing_beat") : NULL;
			const char *intensity = labels ? JSON_FindString (labels, "intensity") : NULL;

			if (difficulty)
			{
				g_pending_difficulty = *difficulty;
				g_have_pending_difficulty = true;
			}
			Con_Printf ("Director: decision ready -- difficulty=%.2f intensity=%s pacing_beat=%s\n",
				difficulty ? *difficulty : 0.5,
				intensity ? intensity : "?",
				pacing_beat ? pacing_beat : "?");
			JSON_Free (json);
		}
		else
		{
			Con_Printf ("Director: decision JSON failed to parse: %s\n", result);
		}
	}
	else
	{
		Con_Printf ("Director: decide failed: %s\n", result);
	}

	quake_free_string (result);
}

/*
Called once per level transition, from the very top of SV_SpawnServer --
before anything there tears down the just-finished level's state, so
pr_global_struct/sv.time/player health still reflect it.
*/
void Director_NewLevel (const char *server)
{
	if (!g_director_ready)
		return;

	if (g_have_pending_difficulty)
	{
		int skill_value = (int)(g_pending_difficulty * 3.0 + 0.5);
		if (skill_value < 0) skill_value = 0;
		if (skill_value > 3) skill_value = 3;
		Cvar_SetValue ("skill", (float)skill_value);
		Con_Printf ("Director: applying difficulty=%.2f as skill=%d for %s\n",
			g_pending_difficulty, skill_value, server);
		g_have_pending_difficulty = false;
	}

	if (sv.active && pr_global_struct)
	{
		double killed = pr_global_struct->killed_monsters;
		double total = pr_global_struct->total_monsters;
		double elapsed = sv.time;
		double health_lost = 0.0;

		if (svs.maxclients >= 1 && svs.clients[0].active && svs.clients[0].edict && g_have_level_start_health)
		{
			float current_health = svs.clients[0].edict->v.health;
			health_lost = g_level_start_health - current_health;
			if (health_lost < 0.0) health_lost = 0.0;
			if (health_lost > 100.0) health_lost = 100.0;
		}

		if (quake_apply_kill_stats (killed, total, elapsed, health_lost) != 0)
			Con_Printf ("Director: apply_kill_stats failed: %s\n", quake_last_error ());
		else
			Con_Printf ("Director: level stats -- killed=%.0f/%.0f elapsed=%.1fs health_lost=%.0f\n",
				killed, total, elapsed, health_lost);
	}

	if (svs.maxclients >= 1 && svs.clients[0].active && svs.clients[0].edict)
	{
		g_level_start_health = svs.clients[0].edict->v.health;
		g_have_level_start_health = true;
	}

	g_pending_request_id = quake_start_decide_llm ();
	if (g_pending_request_id < 0)
		Con_Printf ("Director: start_decide_llm failed: %s\n", quake_last_error ());
}
