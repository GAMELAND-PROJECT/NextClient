#include "host.h"

double oldrealtime;

qboolean Host_IsSinglePlayerGame()
{
	if (g_psv->active)
		return g_psvs->maxclients == 1;

	return cl->maxclients == 1;
}

qboolean Host_IsServerActive()
{
    return g_psv->active;
}

int Host_GetMaxClients()
{
    if (g_psv->active)
        return g_psvs->maxclients;

    return cl->maxclients;
}

qboolean Host_FilterTime(float time)
{
	float fps;

	if (host_framerate->value > 0.0f)
	{
		if (Host_IsSinglePlayerGame() || cls->demoplayback)
		{
			*host_frametime = sys_timescale->value * host_framerate->value;
			*realtime += *host_frametime;
			return true;
		}
	}

	*realtime += sys_timescale->value * time;
	if (g_bIsDedicatedServer)
	{
		// The validated, process-lifetime profile owns the dedicated scheduler.
		// Do not let a stale shortcut or command line silently override it.
		fps = sys_ticrate->value;

		if (fps > 0.0f)
		{
			if (1.0f / (fps + 1.0f) > *realtime - oldrealtime)
				return false;
		}
	}
	else
	{
		fps = 100.0f;
		if (g_psv->active || cls->state == ca_disconnected || cls->state == ca_active)
		{
			fps = 0.5f;
			if (fps_max->value >= 0.5f)
				fps = fps_max->value;
		}
		if (!fps_override->value)
		{
			if (fps > 100.0f)
				fps = 100.0f;
		}
		if (cl->maxclients > 1)
		{
			if (fps < 20.0f)
				fps = 20.0f;
		}
		if (gl_vsync->value)
		{
			if (!fps_override->value)
				fps = 100.f;
		}
		if (!cls->timedemo)
		{
			if (sys_timescale->value / (fps + 0.5f) > *realtime - oldrealtime)
				return false;
		}
	}

	*host_frametime = *realtime - oldrealtime;
	oldrealtime = *realtime;

	if (*host_frametime > 0.25f)
		*host_frametime = 0.25f;

	return true;
}

void Host_WriteConfiguration()
{
	eng()->Host_WriteConfiguration.InvokeChained();
}

void Host_ShutdownServer(qboolean crash)
{
    eng()->Host_ShutdownServer.InvokeChained(crash);    
}
