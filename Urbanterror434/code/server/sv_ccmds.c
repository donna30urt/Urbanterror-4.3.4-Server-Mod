/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "server.h"

/*
===============================================================================
OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/

/*
Reusable version of SV_GetPlayerByHandle() that doesn't
print any silly messages.
*/
client_t *SV_BetterGetPlayerByHandle(const char *handle) {

    client_t  *cl;
    int       i;
    char      cleanName[64];

    // make sure server is running
    if (!com_sv_running->integer) {
        return NULL;
    }

    // Check whether this is a numeric player handle
    for(i = 0; handle[i] >= '0' && handle[i] <= '9'; i++);

    if(!handle[i]) {

        int plid = atoi(handle);

        // Check for numeric playerid match
        if(plid >= 0 && plid < sv_maxclients->integer) {

            cl = &svs.clients[plid];

            if(cl->state) {
                return cl;
            }

        }
    }

    // check for a name match
    for (i = 0, cl=svs.clients; i < sv_maxclients->integer; i++,cl++) {

        if (!cl->state) {
            continue;
        }

        if (!Q_stricmp(cl->name, handle) || !Q_stricmp(cl->colourName, handle)) {
            return cl;
        }

        Q_strncpyz(cleanName, cl->name, sizeof(cleanName));
        Q_CleanStr(cleanName);
        if (!Q_stricmp(cleanName, handle)) {
            return cl;
        }

        Q_strncpyz(cleanName, cl->colourName, sizeof(cleanName));
        Q_CleanStr(cleanName);
        if (!Q_stricmp(cleanName, handle)) {
            return cl;
        }

    }

    return NULL;
}

/*
==================
SV_GetPlayerByHandle

Returns the player with player id or name from Cmd_Argv(1)
==================
*/
static client_t *SV_GetPlayerByHandle(void) {

    char        *s;
    char        name[64];
    int         count = 0;
    int         i, idnum;
    client_t    *cl;
    client_t    *matches[MAX_CLIENTS];

    // make sure server is running
    if (!com_sv_running->integer) {
        return NULL;
    }

    if (Cmd_Argc() < 2) {
        Com_Printf("No client specified\n");
        return NULL;
    }

    s = Cmd_Argv(1);

    // check whether this is a numeric player handle
    for (i = 0; s[i] >= '0' && s[i] <= '9'; i++);

    if (!s[i]) {

        // numeric player handle given as input
        idnum = atoi(s);
        if ((idnum < 0) || (idnum >= sv_maxclients->integer)) {
            Com_Printf("Bad client slot: %i\n", idnum);
            return NULL;
        }

        cl = &svs.clients[idnum];

        if (!cl->state) {
            Com_Printf("Client in slot %i is not connected\n", idnum);
            return NULL;
        }

        return cl;

    } else {

        // full/partial player name given as input
        for (i = 0; i < sv_maxclients->integer ; i++) {

            cl = &svs.clients[i];

            // client is not connected
            if (!cl->state) {
                continue;
            }

            // check for exact match
            if (!Q_stricmp(cl->name, s) || !Q_stricmp(cl->colourName, s)) {
                return cl;
            }

            strcpy(name, cl->name);
            Q_CleanStr(name);
            if (!Q_stricmp(name, s)) {
                return cl;
            }

            strcpy(name, cl->colourName);
            Q_CleanStr(name);
            if (!Q_stricmp(name, s)) {
                return cl;
            }

            // check for substring match
            if (Q_strisub(name, s)) {
                matches[count] = &svs.clients[i];
                count++;
            }

        }

        if (count == 0) {
            // no match found for the given input string
            Com_Printf("No client found matching: %s\n", s);
            return NULL;

        } else if (count > 1) {

            // multiple matches found for the given string
            Com_Printf("Multiple clients found matching '%s':\n", s);

            for (i = 0; i < count; i++) {
                cl = matches[i];
                strcpy(name, cl->name);
                Com_Printf(" %2d: [%s^7]\n", (int)(cl - svs.clients), name);
            }

            return NULL;
        }

        // found just 1 match
        return matches[0];
    }

}

/*
==================
SV_GetPlayerByNum

Returns the player with idnum from Cmd_Argv(1)
==================
*/
static client_t *SV_GetPlayerByNum(void) {

    int         i;
    int         idnum;
    char        *s;
    client_t    *cl;

    // make sure server is running
    if (!com_sv_running->integer) {
        return NULL;
    }

    if (Cmd_Argc() < 2) {
        Com_Printf("No client specified\n");
        return NULL;
    }

    s = Cmd_Argv(1);

    for (i = 0; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9') {
            Com_Printf("Bad slot number: %s\n", s);
            return NULL;
        }
    }

    idnum = atoi(s);
    if (idnum < 0 || idnum >= sv_maxclients->integer) {
        Com_Printf("Bad client slot: %i\n", idnum);
        return NULL;
    }

    cl = &svs.clients[idnum];
    if (!cl->state) {
        Com_Printf("Client in slot %i is not connected\n", idnum);
        return NULL;
    }

    return cl;

}

#ifdef USE_AUTH
/*
==================
SV_PP_Cmd_Argc_to_idnum

@Barbatos: wow this one looks horrible, FIXME!
==================
*//*
static int SV_Argc_to_idnum( int arg_num ) {
	client_t	*cl;
	int			idnum;
	int			i, k, f, g;
	int			len, nlen, slen;
	int			count;
	char		*search;
	char		*name;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		return -1;
	}

	if ( Cmd_Argc() < 1 ) {
		Com_Printf( "No player specified.\n" );
		return -1;
	}

	search = Cmd_Argv( arg_num );
	
	if(strlen( search ) < 3 )
	{
		for (i = 0; search[i]; i++) {
			if (search[i] < '0' || search[i] > '9') {
				Com_Printf( "Bad slot number: \"%s\".\n", search);
				return -1;
			}
		}
		idnum = atoi( search );
		if ( idnum < 0 || idnum >= sv_maxclients->integer ) {
			Com_Printf( "Bad client slot: %i.\n", idnum );
			return -1;
		}

		cl = &svs.clients[idnum];
		if ( !cl->state ) {
			Com_Printf( "Client %i is not active.\n", idnum );
			return -1;
		}
		return idnum;
	}
	else
	{
		f = 0; g = 0;
		count = 0;
		idnum = -1;
		slen = strlen(search);
		for (i=0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++) {
			if ( cl->state >= CS_CONNECTED ) {
				name = cl->name;
				nlen = strlen(name);
				len = nlen - slen;
				if(len>=0) {
					for (k=0; k<=len; k++, name++) {
						if( toupper(name[0])==toupper(search[0]) ) {
							for (g=1,f=1; search[f] && name[g]; g++) {
								if( Q_IsColorString( &name[g] ) ) {
									g++;
								} else {
									if( toupper(name[g])!=toupper(search[f]) ) break;
									f++;
								}
							}
							if (f==slen) { 
								count++; 
								idnum = i; 
								break; 
							}
						}
					}
				}
			}
		}
		if( count == 1 ) { 
			return idnum;
		}
		if( count > 0 ) { 
			Com_Printf( "Too many players found for \"%s\".\n", search );
			return -1;
		}
		if( count == 0 ) { 
			Com_Printf( "No player found for \"%s\".\n", search );
			return -1;
		}
	}
	
	return -1;
}*/
#endif


/////////////////////////////////////////////////////////////////////
// Name        : SV_StripExtension
// Description : Remove the extension from a given file name
// Author      : Fenix
/////////////////////////////////////////////////////////////////////
static void SV_StripExtension(const char *in, char *out) {
    while (*in && *in != '.') {
        *out++ = *in++;
    }
    *out = 0;
}


/////////////////////////////////////////////////////////////////////
// Name        : SV_SortMaps
// Description : Array sorting comparison function (for qsort)
// Author      : Fenix
/////////////////////////////////////////////////////////////////////
static int QDECL SV_SortMaps(const void *a, const void *b) {
    return strcmp (*(const char **) a, *(const char **) b);
}


/////////////////////////////////////////////////////////////////////
// Name        : SV_GetMapSoundingLike
// Description : Retrieve a full map name given a substring of it
// Author      : Fenix, p5yc0runn3r
/////////////////////////////////////////////////////////////////////
static char *SV_GetMapSoundingLike(const char *s) 
{
    int  i, mapcount;
    int  len = 0, count = 0;
    char *matches[MAX_MAPLIST_SIZE];
    char *searchmap;
    static char maplist[MAX_MAPLIST_STRING];

    // [BUGFIX]: instead of iterating through all the maps matching both full and
    // partial name, search just for the exact map name and return it if the match is found
    Com_sprintf(maplist, sizeof(maplist), "maps/%s.bsp", s);
    if (FS_ReadFile(maplist, NULL) > 0) 
    {
        Com_sprintf(maplist, sizeof(maplist), "%s", s);
        return maplist; // @p5yc0runn3r: Return static string
    }

    // We didn't found an exact name match. Keep iterating through all the
    // available maps matching partial substrings
    if (!(mapcount = FS_GetFileList("maps", ".bsp", maplist, sizeof(maplist)))) 
    {
        Com_Printf("Unable to retrieve map list\n");
        return NULL;
    }

    for (searchmap = maplist, i = 0; i < mapcount && count < MAX_MAPLIST_SIZE; i++, searchmap += len + 1) 
    {
        len = strlen(searchmap);
        SV_StripExtension(searchmap, searchmap);

        // Check for substring match
        if (Q_strisub(searchmap, s)) 
        {
            matches[count] = searchmap;
            count++;
        }
    }

    // One match = one map, found match.
    if (count == 1) return matches[0]; // @p5yc0runn3r: matches points to static string, safe to return.

    if (count > 1) 
    {
        // Multiple matches found for the given map name
        Com_Printf("Multiple maps found matching '%s':\n", s);

        // Sorting the short map list alphabetically
        qsort(matches, count, sizeof(char *), SV_SortMaps);

        for (i = 0; i < count; i++) 
        {
            // Printing a short map list so the user can retry with a more specific name
            Com_Printf(" %2d: [%s]\n", i + 1, matches[i]);
        }

        if (count >= MAX_MAPLIST_SIZE) 
        {
            // Tell the user that there are actually more
            // maps matching the given substring, although
            // we are not displaying them....
            Com_Printf("...and more\n");
        }
        return NULL;
    }

    // No match found for the given map name input
    Com_Printf("No map found matching '%s'\n", s);
    return NULL;
}

//=========================================================

/*
==================
SV_Map_f

Restart the server on a different map
==================
*/
static void SV_Map_f(void) {

    char      *cmd;
    char      *searchmap;
    char      mapname[MAX_QPATH];
    qboolean  killBots, cheat;

    if (Cmd_Argc() < 2) {
        return;
    }

    searchmap = SV_GetMapSoundingLike(Cmd_Argv(1));
    if (!searchmap) {
        return;
    }

    // force latched values to get set
    Cvar_Get ("g_gametype", "0", CVAR_SERVERINFO | CVAR_USERINFO | CVAR_LATCH);

    cmd = Cmd_Argv(0);
    if (Q_stricmpn(cmd, "sp", 2) == 0) {
        Cvar_SetValue("g_gametype", GT_SINGLE_PLAYER);
        Cvar_SetValue("g_doWarmup", 0);
        // may not set sv_maxclients directly, always set latched
        Cvar_SetLatched("sv_maxclients", "8");
        cmd += 2;
        cheat = qfalse;
        killBots = qtrue;
    }
    else {

        if (!Q_stricmp( cmd, "devmap") || !Q_stricmp(cmd, "spdevmap") ) {
            cheat = qtrue;
            killBots = qtrue;
        } else {
            cheat = qfalse;
            killBots = qfalse;
        }

        if (sv_gametype->integer == GT_SINGLE_PLAYER) {
            Cvar_SetValue("g_gametype", GT_FFA);
        }

    }

    // save the map name here cause on a map restart we reload the q3config.cfg
    // and thus nuke the arguments of the map command
    Q_strncpyz(mapname, searchmap, sizeof(mapname));

    // start up the map
    SV_SpawnServer(mapname, killBots);

    // set the cheat value
    // if the level was started with "map <levelname>", then
    // cheats will not be allowed.  If started with "devmap <levelname>"
    // then cheats will be allowed
    if (cheat) {
        Cvar_Set("sv_cheats", "1");
    } else {
        Cvar_Set("sv_cheats", "0");
    }

}

/*
================
SV_MapRestart_f

Completely restarts a level, but doesn't send a new gamestate to the clients.
This allows fair starts with variable load times.
================
*/
static void SV_MapRestart_f( void ) {
	int			i;
	client_t	*client;
	char		*denied;
	qboolean	isBot;
	int			delay;

	// make sure we aren't restarting twice in the same frame
	if ( com_frameTime == sv.serverId ) {
		return;
	}

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running\n" );
		return;
	}

	if ( sv.restartTime ) {
		return;
	}

	if (Cmd_Argc() > 1 ) {
		delay = atoi( Cmd_Argv(1) );
	}
	else {
		delay = 5;
	}
	if( delay && !Cvar_VariableValue("g_doWarmup") ) {
		sv.restartTime = sv.time + delay * 1000;
		SV_SetConfigstring( CS_WARMUP, va("%i", sv.restartTime) );
		return;
	}

	// check for changes in variables that can't just be restarted
	// check for maxclients change
	if ( sv_maxclients->modified || sv_gametype->modified ) {
		char	mapname[MAX_QPATH];

		Com_Printf( "variable change -- restarting.\n" );
		// restart the map the slow way
		Q_strncpyz( mapname, Cvar_VariableString( "mapname" ), sizeof( mapname ) );

		SV_SpawnServer( mapname, qfalse );
		return;
	}

	// toggle the server bit so clients can detect that a
	// map_restart has happened
	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

	// generate a new serverid	
	// TTimo - don't update restartedserverId there, otherwise we won't deal correctly with multiple map_restart
	sv.serverId = com_frameTime;
	Cvar_Set( "sv_serverid", va("%i", sv.serverId ) );

	// if a map_restart occurs while a client is changing maps, we need
	// to give them the correct time so that when they finish loading
	// they don't violate the backwards time check in cl_cgame.c
	for (i = 0; i < sv_maxclients->integer; i++) {
		if (svs.clients[i].state == CS_PRIMED) {
			svs.clients[i].oldServerTime = sv.restartTime;
		}
	}

	// reset all the vm data in place without changing memory allocation
	// note that we do NOT set sv.state = SS_LOADING, so configstrings that
	// had been changed from their default values will generate broadcast updates
	sv.state = SS_LOADING;
	sv.restarting = qtrue;

	SV_RestartGameProgs();

	// run a few frames to allow everything to settle
	for (i = 0; i < 3; i++)
	{
		VM_Call (gvm, GAME_RUN_FRAME, sv.time);
		sv.time += 100;
		svs.time += 100;
	}

	sv.state = SS_GAME;
	sv.restarting = qfalse;

	// connect and begin all the clients
	for (i=0 ; i<sv_maxclients->integer ; i++) {

		client = &svs.clients[i];

		// send the new gamestate to all connected clients
		if ( client->state < CS_CONNECTED) {
			continue;
		}

		if ( client->netchan.remoteAddress.type == NA_BOT ) {
			isBot = qtrue;
		} else {
			isBot = qfalse;
		}

		// add the map_restart command
		SV_AddServerCommand( client, "map_restart\n" );

		// connect the client again, without the firstTime flag
		denied = VM_ExplicitArgPtr( gvm, VM_Call( gvm, GAME_CLIENT_CONNECT, i, qfalse, isBot ) );
		if ( denied ) {
			// this generally shouldn't happen, because the client
			// was connected before the level change
			SV_DropClient( client, denied );
			Com_Printf( "SV_MapRestart_f(%d): dropped client %i - denied!\n", delay, i );
			continue;
		}

        // set stamina and walljumps back to default
        client->cm.infiniteStamina = 0;
        client->cm.infiniteWallJumps = 0;

		if(client->state == CS_ACTIVE) {
			SV_ClientEnterWorld(client, &client->lastUsercmd);
		}
		else {
			// If we don't reset client->lastUsercmd and are restarting during map load,
		 	// the client will hang because we'll use the last Usercmd from the previous map,
		 	// which is wrong obviously.
		 	SV_ClientEnterWorld(client, NULL);
		}
	}	

	// run another frame to allow things to look at all the players
	VM_Call (gvm, GAME_RUN_FRAME, sv.time);
	sv.time += 100;
	svs.time += 100;
}

//===============================================================

/*
==================
SV_Kick_f

Kick a user off of the server. FIXME: move to game
==================
*/
static void SV_Kick_f(void) {

    int         i;
    client_t    *cl;
    char        *reason = "was ^1kicked^7";

    // make sure server is running
    if (!com_sv_running->integer ) {
        Com_Printf("Server is not running\n");
        return;
    }

    if ((Cmd_Argc() < 2) || (Cmd_Argc() > 3)) {
        Com_Printf ("Usage: kick <client> <reason>\nkick all = kick everyone\nkick allbots = kick all bots\n");
        return;
    }

    if (Cmd_Argc() == 3) {
        // If the reason was specified attach it to the disconnect message
        reason = va("was ^1kicked^7: %s", Cmd_Argv(2));
    }


    if (!Q_stricmp(Cmd_Argv(1), "all")) {

        for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {

            if (!cl->state) {
                continue;
            }

            if (cl->netchan.remoteAddress.type == NA_LOOPBACK) {
                continue;
            }

            SV_DropClient(cl, reason);
            cl->lastPacketTime = svs.time;    // in case there is a funny zombie
        }

    } else if (!Q_stricmp(Cmd_Argv(1), "allbots")) {

        for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {

            if (!cl->state) {
                continue;
            }

            if (cl->netchan.remoteAddress.type != NA_BOT) {
                continue;
            }

            SV_DropClient(cl, reason);
            cl->lastPacketTime = svs.time;    // in case there is a funny zombie

        }

    } else {

        cl = SV_GetPlayerByHandle();

        if (!cl) {
            return;
        }

        if (cl->netchan.remoteAddress.type == NA_LOOPBACK) {
            SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host client\n");
            return;
        }

        SV_DropClient(cl, reason);
        cl->lastPacketTime = svs.time;    // in case there is a funny zombie

    }

}

/*
==================
SV_Ban_f

Ban a user from being able to play on this server through the auth
server
==================
*/
static void SV_Ban_f(void) {

    client_t *cl;

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf( "Server is not running\n" );
        return;
    }

    if (Cmd_Argc() != 2 ) {
        Com_Printf ("Usage: banUser <client>\n");
        return;
    }

    cl = SV_GetPlayerByHandle();

    if (!cl) {
        return;
    }

    if (cl->netchan.remoteAddress.type == NA_LOOPBACK) {
        SV_SendServerCommand(NULL, "print \"%s\"", "Cannot ban host client\n");
        return;
    }

    // look up the authorize server's IP
    if (!svs.authorizeAddress.ip[0] && svs.authorizeAddress.type != NA_BAD) {
        Com_Printf("Resolving %s\n", AUTHORIZE_SERVER_NAME);
        if (!NET_StringToAdr( AUTHORIZE_SERVER_NAME, &svs.authorizeAddress)) {
            Com_Printf("Couldn't resolve address\n");
            return;
        }
        svs.authorizeAddress.port = BigShort( PORT_AUTHORIZE );
        Com_Printf("%s resolved to %i.%i.%i.%i:%i\n", AUTHORIZE_SERVER_NAME,
            svs.authorizeAddress.ip[0], svs.authorizeAddress.ip[1],
            svs.authorizeAddress.ip[2], svs.authorizeAddress.ip[3],
            BigShort(svs.authorizeAddress.port));
    }

    // otherwise send their ip to the authorize server
    if (svs.authorizeAddress.type != NA_BAD) {
        NET_OutOfBandPrint(NS_SERVER, svs.authorizeAddress,
                           "banUser %i.%i.%i.%i", cl->netchan.remoteAddress.ip[0], cl->netchan.remoteAddress.ip[1],
                           cl->netchan.remoteAddress.ip[2], cl->netchan.remoteAddress.ip[3]);

        Com_Printf("%s ^7was banned from coming back\n", cl->name);

    }
}

/*
==================
SV_BanNum_f

Ban a user from being able to play on this server through the auth
server
==================
*/
static void SV_BanNum_f( void ) {

    client_t *cl;

    // make sure server is running
    if (!com_sv_running->integer ) {
        Com_Printf("Server is not running\n");
        return;
    }

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: banClient <client number>\n");
        return;
    }

    cl = SV_GetPlayerByNum();
    if (!cl) {
        return;
    }

    if (cl->netchan.remoteAddress.type == NA_LOOPBACK) {
        SV_SendServerCommand(NULL, "print \"%s\"", "Cannot ban host client\n");
        return;
    }

    // look up the authorize server's IP
    if (!svs.authorizeAddress.ip[0] && svs.authorizeAddress.type != NA_BAD) {
        Com_Printf( "Resolving %s\n", AUTHORIZE_SERVER_NAME);
        if (!NET_StringToAdr( AUTHORIZE_SERVER_NAME, &svs.authorizeAddress)) {
            Com_Printf("Couldn't resolve address\n");
            return;
        }
        svs.authorizeAddress.port = BigShort(PORT_AUTHORIZE);
        Com_Printf("%s resolved to %i.%i.%i.%i:%i\n", AUTHORIZE_SERVER_NAME,
            svs.authorizeAddress.ip[0], svs.authorizeAddress.ip[1],
            svs.authorizeAddress.ip[2], svs.authorizeAddress.ip[3],
            BigShort( svs.authorizeAddress.port));
    }

    // otherwise send their ip to the authorize server
    if (svs.authorizeAddress.type != NA_BAD) {
        NET_OutOfBandPrint(NS_SERVER, svs.authorizeAddress,
                           "banUser %i.%i.%i.%i", cl->netchan.remoteAddress.ip[0], cl->netchan.remoteAddress.ip[1],
                           cl->netchan.remoteAddress.ip[2], cl->netchan.remoteAddress.ip[3] );

        Com_Printf("%s ^7was banned from coming back\n", cl->name);

    }
}

/*
==================
SV_KickNum_f

Kick a user off of the server  FIXME: move to game
==================
*/
static void SV_KickNum_f(void) {

    client_t *cl;

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running\n");
        return;
    }

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: kicknum <client number>\n");
        return;
    }

    cl = SV_GetPlayerByNum();
    if (!cl) {
        return;
    }

    if (cl->netchan.remoteAddress.type == NA_LOOPBACK) {
        SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host client\n");
        return;
    }

    SV_DropClient(cl, "was ^1kicked^7");
    cl->lastPacketTime = svs.time;    // in case there is a funny zombie

}

/*
================
SV_Status_f
================
*/
static void SV_Status_f(void) {

    int            i, j, l;
    client_t       *cl;
    playerState_t  *ps;
    const char     *s;
    int            ping;

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running\n");
        return;
    }

    Com_Printf ("map: %s\n", sv_mapname->string);
    Com_Printf ("num score ping name            lastmsg address               qport rate\n");
    Com_Printf ("--- ----- ---- --------------- ------- --------------------- ----- -----\n");

    for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {

        if (!cl->state) {
            continue;
        }

        Com_Printf("%3i ", i);
        ps = SV_GameClientNum( i );
        Com_Printf("%5i ", ps->persistant[PERS_SCORE]);

        if (cl->state == CS_CONNECTED) {
            Com_Printf("CNCT ");
        } else if (cl->state == CS_ZOMBIE) {
            Com_Printf("ZMBI ");
        } else {
            ping = cl->ping < 9999 ? cl->ping : 9999;
            Com_Printf ("%4i ", ping);
        }

        Com_Printf ("%s", cl->name);
        // TTimo adding a ^7 to reset the color
        // NOTE: colored names in status breaks the padding (WONTFIX)
        Com_Printf ("^7");
        l = 16 - strlen(cl->name);
        for (j=0 ; j<l ; j++) {
            Com_Printf (" ");
        }

        Com_Printf ("%7i ", svs.time - cl->lastPacketTime );
        s = NET_AdrToString( cl->netchan.remoteAddress );
        Com_Printf ("%s", s);
        l = 22 - strlen(s);
        for (j = 0; j < l; j++) {
            Com_Printf(" ");
        }

        Com_Printf ("%5i", cl->netchan.qport);
        Com_Printf (" %5i", cl->rate);
        Com_Printf ("\n");
    }

    Com_Printf ("\n");

}

/*
==================
SV_ConSay_f
==================
*/
static void SV_ConSay_f(void) {

    char    *p;
    char    text[1024];

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running\n");
        return;
    }

    if (Cmd_Argc () < 2) {
        return;
    }

    strcpy(text, sv_sayprefix->string);
    p = Cmd_Args();

    if (*p == '"') {
        p++;
        p[strlen(p) - 1] = 0;
    }

    strcat(text, p);
    SV_SendServerCommand(NULL, "chat \"%s\"", text);
}

/*
==================
SV_ConTell_f
==================
*/
static void SV_ConTell_f(void) {

    char      text[1024];
    char      *p;
    client_t  *cl;

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running\n");
        return;
    }

    if (Cmd_Argc() < 3) {
        Com_Printf("Usage: tell <client> <message>\n");
        return;
    }

    cl = SV_GetPlayerByHandle();

    if (!cl) {
        return;
    }

    strcpy (text, sv_tellprefix->string);
    p = Cmd_ArgsFrom(2);

    if (*p == '"') {
        p++;
        p[strlen(p) - 1] = 0;
    }

    strcat(text, p);
    SV_SendServerCommand(cl, "chat \"%s\"", text);
}


/*
==================
SV_Heartbeat_f

Also called by SV_DropClient, SV_DirectConnect, and SV_SpawnServer
==================
*/
void SV_Heartbeat_f(void) {
    svs.nextHeartbeatTime = -9999999;
}


/*
===========
SV_Serverinfo_f

Examine the serverinfo string
===========
*/
static void SV_Serverinfo_f(void) {
    Com_Printf("Server info settings:\n");
    Info_Print(Cvar_InfoString(CVAR_SERVERINFO));
}


/*
===========
SV_Systeminfo_f

Examine or change the serverinfo string
===========
*/
static void SV_Systeminfo_f(void) {
    Com_Printf("System info settings:\n");
    Info_Print(Cvar_InfoString(CVAR_SYSTEMINFO));
}


/*
===========
SV_DumpUser_f

Examine all a users info strings FIXME: move to game
===========
*/
static void SV_DumpUser_f(void) {

    client_t *cl;

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running\n");
        return;
    }

    if (Cmd_Argc() != 2) {
        Com_Printf ("Usage: dumpuser <client>\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if (!cl) {
        return;
    }

    Com_Printf("userinfo\n");
    Com_Printf("--------\n");
    Info_Print(cl->userinfo);

}


/*
=================
SV_KillServer
=================
*/
static void SV_KillServer_f(void) {
    SV_Shutdown("killserver");
}

//===========================================================

/*
Start a server-side demo.

This does it all, create the file and adjust the demo-related
stuff in client_t.

This is mostly ripped from sv_client.c/SV_SendClientGameState
and cl_main.c/CL_Record_f.
*/
static void SVD_StartDemoFile(client_t *client, const char *path) {

    int             i, len;
    entityState_t   *base, nullstate;
    msg_t           msg;
    byte            buffer[MAX_MSGLEN];
    fileHandle_t    file;
#ifdef USE_DEMO_FORMAT_42
    char            *s;
    int             v, size;
#endif

    Com_DPrintf("SVD_StartDemoFile\n");
    assert(!client->demo_recording);

    // create the demo file and write the necessary header
    file = FS_FOpenFileWrite(path);
    assert(file != 0);

    /* File_write_header_demo // ADD this fx */
    /* HOLBLIN  entete demo */
    #ifdef USE_DEMO_FORMAT_42
        //@Barbatos: get the mod version from the server
        s = Cvar_VariableString("g_modversion");

        size = strlen(s);
        len = LittleLong(size);
        FS_Write(&len, 4, file);
        FS_Write(s, size, file);

        v = LittleLong(DEMO_VERSION);
        FS_Write (&v, 4, file);

        len = 0;
        len = LittleLong(len);
        FS_Write(&len, 4, file);
        FS_Write(&len, 4, file);
    #endif
    /* END HOLBLIN  entete demo */

    MSG_Init(&msg, buffer, sizeof(buffer));
    MSG_Bitstream(&msg); // XXX server code doesn't do this, client code does
    MSG_WriteLong(&msg, client->lastClientCommand); // TODO: or is it client->reliableSequence?
    MSG_WriteByte(&msg, svc_gamestate);
    MSG_WriteLong(&msg, client->reliableSequence);

    for (i = 0; i < MAX_CONFIGSTRINGS; i++) {
        if (sv.configstrings[i][0]) {
            MSG_WriteByte(&msg, svc_configstring);
            MSG_WriteShort(&msg, i);
            MSG_WriteBigString(&msg, sv.configstrings[i]);
        }
    }

    Com_Memset(&nullstate, 0, sizeof(nullstate));
    for (i = 0 ; i < MAX_GENTITIES; i++) {
        base = &sv.svEntities[i].baseline;
        if (!base->number) {
            continue;
        }
        MSG_WriteByte(&msg, svc_baseline);
        MSG_WriteDeltaEntity(&msg, &nullstate, base, qtrue);
    }

    MSG_WriteByte(&msg, svc_EOF);
    MSG_WriteLong(&msg, client - svs.clients);
    MSG_WriteLong(&msg, sv.checksumFeed);
    MSG_WriteByte(&msg, svc_EOF); // XXX server code doesn't do this, SV_Netchan_Transmit adds it!

    len = LittleLong(client->netchan.outgoingSequence - 1);
    FS_Write(&len, 4, file);

    len = LittleLong (msg.cursize);
    FS_Write(&len, 4, file);
    FS_Write(msg.data, msg.cursize, file);

    #ifdef USE_DEMO_FORMAT_42
        // add size of packet in the end for backward play /* holblin */
        FS_Write(&len, 4, file);
    #endif

    FS_Flush(file);

    // adjust client_t to reflect demo started
    client->demo_recording = qtrue;
    client->demo_file = file;
    client->demo_waiting = qtrue;
    client->demo_backoff = 1;
    client->demo_deltas = 0;
}

/*
Write a message to a server-side demo file.
*/
void SVD_WriteDemoFile(const client_t *client, const msg_t *msg) {

    int len;
    msg_t cmsg;
    byte cbuf[MAX_MSGLEN];
    fileHandle_t file = client->demo_file;

    if (*(int *)msg->data == -1) { // TODO: do we need this?
        Com_DPrintf("Ignored connectionless packet, not written to demo!\n");
        return;
    }

    // TODO: we only copy because we want to add svc_EOF; can we add it and then
    // "back off" from it, thus avoiding the copy?
    MSG_Copy(&cmsg, cbuf, sizeof(cbuf), (msg_t*) msg);
    MSG_WriteByte(&cmsg, svc_EOF); // XXX server code doesn't do this, SV_Netchan_Transmit adds it!

    // TODO: the headerbytes stuff done in the client seems unnecessary
    // here because we get the packet *before* the netchan has it's way
    // with it; just not sure that's really true :-/

    len = LittleLong(client->netchan.outgoingSequence);
    FS_Write(&len, 4, file);

    len = LittleLong(cmsg.cursize);
    FS_Write(&len, 4, file);

    FS_Write(cmsg.data, cmsg.cursize, file); // XXX don't use len!

    #ifdef USE_DEMO_FORMAT_42
        // add size of packet in the end for backward play /* holblin */
        FS_Write(&len, 4, file);
    #endif

    FS_Flush(file);
}

/*
Stop a server-side demo.

This finishes out the file and clears the demo-related stuff
in client_t again.
*/
static void SVD_StopDemoFile(client_t *client) {

    int marker = -1;
    fileHandle_t file = client->demo_file;

    Com_DPrintf("SVD_StopDemoFile\n");
    assert(client->demo_recording);

    // write the necessary trailer and close the demo file
    FS_Write(&marker, 4, file);
    FS_Write(&marker, 4, file);
    FS_Flush(file);
    FS_FCloseFile(file);

    // adjust client_t to reflect demo stopped
    client->demo_recording = qfalse;
    client->demo_file = -1;
    client->demo_waiting = qfalse;
    client->demo_backoff = 1;
    client->demo_deltas = 0;
}

/*
Clean up player name to be suitable as path name.
Similar to Q_CleanStr() but tweaked.
*/
static void SVD_CleanPlayerName(char *name) {

    char *src = name, *dst = name, c;

    while ((c = *src)) {
        // note Q_IsColorString(src++) won't work since it's a macro
        if (Q_IsColorString(src)) {
            src++;
        } else if (c == ':' || c == '\\' || c == '/' || c == '*' || c == '?') {
            *dst++ = '%';
        } else if (c > ' ' && c < 0x7f) {
            *dst++ = c;
        }
        src++;
    }

    *dst = '\0';

    if (strlen(name) == 0) {
        strcpy(name, "UnnamedPlayer");
    }
}

/*
Generate unique name for a new server demo file.
(We pretend there are no race conditions.)
*/
static void SV_NameServerDemo(char *filename, int length, const client_t *client, char *fn) {

    qtime_t time;
    char playername[32];
    char demoName[64]; //@Barbatos

    Com_DPrintf("SV_NameServerDemo\n");

    Com_RealTime(&time);
    Q_strncpyz(playername, client->name, sizeof(playername));
    SVD_CleanPlayerName(playername);

    if (fn != NULL) {

        Q_strncpyz(demoName, fn, sizeof(demoName));

        #ifdef USE_DEMO_FORMAT_42
            Q_snprintf(filename, length-1, "%s/%s.urtdemo", sv_demofolder->string, demoName );
            if (FS_FileExists(filename)) {
                Q_snprintf(filename, length-1, "%s/%s_%d.urtdemo", sv_demofolder->string, demoName, Sys_Milliseconds() );
            }
        #else
            Q_snprintf(filename, length-1, "%s/%s.dm_%d", sv_demofolder->string, demoName , PROTOCOL_VERSION );
            if (FS_FileExists(filename)) {
                Q_snprintf(filename, length-1, "%s/%s_%d.dm_%d", sv_demofolder->string, demoName, Sys_Milliseconds() , PROTOCOL_VERSION );
            }
        #endif
    } else {
        #ifdef USE_DEMO_FORMAT_42
            Q_snprintf(
                filename, length-1, "%s/%.4d-%.2d-%.2d_%.2d-%.2d-%.2d_%s_%d.urtdemo",
                sv_demofolder->string, time.tm_year+1900, time.tm_mon + 1, time.tm_mday,
                time.tm_hour, time.tm_min, time.tm_sec,
                playername,
                Sys_Milliseconds()
            );
        #else
            Q_snprintf(
                filename, length-1, "%s/%.4d-%.2d-%.2d_%.2d-%.2d-%.2d_%s_%d.dm_%d",
                sv_demofolder->string, time.tm_year+1900, time.tm_mon + 1, time.tm_mday,
                time.tm_hour, time.tm_min, time.tm_sec,
                playername,
                Sys_Milliseconds(),
                PROTOCOL_VERSION
            );
        #endif
        filename[length-1] = '\0';

        if (FS_FileExists(filename)) {
            filename[0] = 0;
            return;
        }
    }
}

static void SV_StartRecordOne(client_t *client, char *filename) {

    char path[MAX_OSPATH];

    Com_DPrintf("SV_StartRecordOne\n");

    if (client->demo_recording) {
        Com_Printf("startserverdemo: %s ^7is already being recorded\n", client->name);
        return;
    }

    if (client->state != CS_ACTIVE) {
        Com_Printf("startserverdemo: %s ^7is not active\n", client->name);
        return;
    }

    if (client->netchan.remoteAddress.type == NA_BOT) {
        Com_Printf("startserverdemo: %s ^7is a bot\n", client->name);
        return;
    }

    SV_NameServerDemo(path, sizeof(path), client, filename);
    SVD_StartDemoFile(client, path);

    if(sv_demonotice->string) {
        SV_SendServerCommand(client, "print \"%s\"\n", sv_demonotice->string);
    }

    Com_Printf("startserverdemo: recording %s ^7to %s\n", client->name, path);
}

static void SV_StartRecordAll(void) {

    int slot;
    client_t *client;

    Com_DPrintf("SV_StartRecordAll\n");

    for (slot = 0, client = svs.clients; slot < sv_maxclients->integer; slot++, client++) {
        // filter here to avoid lots of bogus messages from SV_StartRecordOne()
        if (client->netchan.remoteAddress.type == NA_BOT
            || client->state != CS_ACTIVE
            || client->demo_recording) {
            continue;
        }
        SV_StartRecordOne(client, NULL);
    }
}

static void SV_StopRecordOne(client_t *client) {

    Com_DPrintf("SV_StopRecordOne\n");

    if (!client->demo_recording) {
        Com_Printf("stopserverdemo: %s ^7is not being recorded\n", client->name);
        return;
    }

    if (client->state != CS_ACTIVE) { // disconnects are handled elsewhere
        Com_Printf("stopserverdemo: %s ^7is not active\n", client->name);
        return;
    }

    if (client->netchan.remoteAddress.type == NA_BOT) {
        Com_Printf("stopserverdemo: %s ^7is a bot\n", client->name);
        return;
    }

    SVD_StopDemoFile(client);
    Com_Printf("stopserverdemo: stopped recording %s\n", client->name);


}

static void SV_StopRecordAll(void) {

    int slot;
    client_t *client;

    Com_DPrintf("SV_StopRecordAll\n");

    for (slot=0, client=svs.clients; slot < sv_maxclients->integer; slot++, client++) {
        // filter here to avoid lots of bogus messages from SV_StopRecordOne()
        if (client->netchan.remoteAddress.type == NA_BOT
            || client->state != CS_ACTIVE // disconnects are handled elsewhere
            || !client->demo_recording) {
            continue;
        }
        SV_StopRecordOne(client);
    }

}

/*
==================
SV_StartServerDemo_f

Record a server-side demo for given player/slot. The demo
will be called "YYYY-MM-DD_hh-mm-ss_playername_id.urtdemo",
in the "demos" directory under your game directory. Note
that "startserverdemo all" will start demos for all players
currently in the server. Players who join later require a
new "startserverdemo" command. If you are already recording
demos for some players, another "startserverdemo all" will
start new demos only for players not already recording. Note
that bots will never be recorded, not even if "all" is given.
The server-side demos will stop when "stopserverdemo" is issued
or when the server restarts for any reason (such as a new map
loading).
==================
*/
static void SV_StartServerDemo_f(void) {

    client_t *client;

    Com_DPrintf("SV_StartServerDemo_f\n");

    if (!com_sv_running->integer) {
        Com_Printf("startserverdemo: Server not running\n");
        return;
    }

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: startserverdemo <client-or-all> [<optional-demo-name>]\n");
        return;
    }

    if (!Q_stricmp(Cmd_Argv(1), "all")) {

        SV_StartRecordAll();

    } else {

        client = SV_GetPlayerByHandle();
        if (!client) {
            return;
        }

        if (Cmd_Argc() > 2) {
            SV_StartRecordOne(client, Cmd_ArgsFrom(2));
        } else {
            SV_StartRecordOne(client, NULL);
        }

    }
}

/*
==================
SV_StopServerDemo_f

Stop a server-side demo for given player/slot. Note that
"stopserverdemo all" will stop demos for all players in
the server.
==================
*/
static void SV_StopServerDemo_f(void)
{
	client_t *client;

	Com_DPrintf("SV_StopServerDemo_f\n");

	if (!com_sv_running->integer) {
		Com_Printf("stopserverdemo: Server not running\n");
		return;
	}

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: stopserverdemo <client-or-all>\n");
		return;
	}

	if (!Q_stricmp(Cmd_Argv(1), "all")) {

		SV_StopRecordAll();
	}
	else {

	    client = SV_GetPlayerByHandle();

	    if (!client) {
	        return;
	    }

		SV_StopRecordOne(client);
	}

}

/*
==================
SV_CompleteMapName
==================
*/
/*@Barbatos: unused for now
static void SV_CompleteMapName( char *args, int argNum ) {
	if ( argNum == 2 ) {
		Field_CompleteFilename( "maps", "bsp", qtrue, qfalse );
	}
}*/



/*
===============================================================================
                            TitanMod Commands
===============================================================================
*/

/////////////////////////////////////////////////////////////////////
// SV_SetScore_f
/////////////////////////////////////////////////////////////////////
static void SV_SetScore_f(void) {

	client_t      *cl;
	playerState_t *ps;
	int           value, i;

	if (!com_sv_running->integer) {
		Com_Printf("Server is not running\n");
		return;
	}

	if(Cmd_Argc() < 3) {
        Com_Printf("Usage: setscore <player> <value>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(cl == NULL)
		return;

	if (sscanf(Cmd_Argv(2), "%d", &value) == 0) {
		Com_Printf("Invalid value.\n");
		return;
	}

	ps = SV_GameClientNum(cl - svs.clients);
	ps->persistant[PERS_SCORE] = value;
	for(i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++)	{
		if(cl->state == CS_ACTIVE)
			SV_ExecuteClientCommand(cl, "score", qtrue);
	}
}

/////////////////////////////////////////////////////////////////////
// SV_SetDeaths_f
/////////////////////////////////////////////////////////////////////
static void SV_SetDeaths_f(void) {

	client_t      *cl;
	playerState_t *ps;
	int           value, i;

	if (!com_sv_running->integer) {
		Com_Printf("Server is not running\n");
		return;
	}

	if(Cmd_Argc() < 3) {
		 Com_Printf("Usage: setdeaths <player> <value>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(cl == NULL)
		return;

	if (sscanf(Cmd_Argv(2), "%d", &value) == 0) {
		Com_Printf("Invalid value.\n");
		return;
	}

	ps = SV_GameClientNum(cl - svs.clients);
	ps->persistant[PERS_KILLED] = value;
	for(i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++)	{
		if(cl->state == CS_ACTIVE)
			SV_ExecuteClientCommand(cl, "score", qtrue);
	}
}

/////////////////////////////////////////////////////////////////////
// SV_Invisible_f
/////////////////////////////////////////////////////////////////////
static void SV_Invisible_f(void) {

	client_t       *cl;
	sharedEntity_t *e;

	if (!com_sv_running->integer) {
		Com_Printf("Server is not running\n");
		return;
	}

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: invisible <player>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if (!cl) {
		return;
	}

	e = SV_GentityNum(cl - svs.clients);
	e->r.svFlags ^= SVF_NOCLIENT;

	if (e->r.svFlags & SVF_NOCLIENT) {
		Com_Printf("Player %s ^7Invisible.\n", cl->name);
        SV_SendServerCommand(cl, "cchat \"\" \"%s^7Your invisibility mode turned: [^2ON^7]\"", sv_tellprefix->string);
	} else {
		Com_Printf("Player %s ^7Visible.\n", cl->name);
        SV_SendServerCommand(cl, "cchat \"\" \"%s^7Your invisibility mode turned: [^1OFF^7]\"", sv_tellprefix->string);
	}
}

/////////////////////////////////////////////////////////////////////
// SV_PlaySoundFile_f
/////////////////////////////////////////////////////////////////////
static void SV_PlaySoundFile_f (void)
{
	client_t *cl;

	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running\n");
		return;
	}

	if(Cmd_Argc() != 3)
	{
		Com_Printf("Usage: playsoundfile <player> <file>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(!cl)
	{
		return;
	}

	MOD_PlaySoundFile (cl, Cmd_Argv(2));
}

/////////////////////////////////////////////////////////////////////
// SV_PlaySound_f
/////////////////////////////////////////////////////////////////////
static void SV_PlaySound_f (void)
{
	client_t *cl;
	int index;

	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running\n");
		return;
	}

	if(Cmd_Argc() != 3)
	{
		Com_Printf("Usage: playsound <player> <index>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(!cl)
	{
		return;
	}
	if(!sscanf(Cmd_Argv(2), "%d", &index) && (index > 0 && index <256))
	{
		Com_Printf("Incorrect value!\n");
	}

	MOD_SetExternalEvent(cl, EV_GLOBAL_SOUND, index);
}

/////////////////////////////////////////////////////////////////////
// SV_GiveWeapon_f
/////////////////////////////////////////////////////////////////////
static void SV_GiveWeapon_f (void) {

	client_t      *cl;
	weapon_t      wp = 0;
	playerState_t *ps;
	int targetweapon;

	if(!com_sv_running->integer) {
		Com_Printf("Server is not running\n");
		return;
	}

	if(Cmd_Argc() < 3) {
		Com_Printf("Usage: giveweapon <player> <weapon> [ [bullets] [clips] ]\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if (!cl) {
        return;
    }

	wp = SV_Char2Weapon(Cmd_Argv(2));

	if(wp == 0) {
		Com_Printf("Weapon not found\n");
		return;
	}

	ps = SV_GameClientNum(cl - svs.clients);

	//If the player have the weapon only add the clips.
	targetweapon = SV_FirstMatchFor(ps, wp);

    if(Cmd_Argc() <= 4) {
        if (targetweapon == -1) {
            SV_GiveWeapon(ps, wp);
        } else {
            SV_GiveClipsAW(ps, *(int*)QVM_clips(wp)+UT_WEAPON_GETCLIPS(targetweapon), targetweapon);
        }
    }
    else if(Cmd_Argc()  == 5)
    {
        if(targetweapon == -1)
        {
            SV_GiveWeaponCB(ps, wp, atoi(Cmd_Argv(3)), atoi(Cmd_Argv(4)));
        }else
        {
            SV_GiveBulletsAW(ps, atoi(Cmd_Argv(3))+UT_WEAPON_GETBULLETS(targetweapon), targetweapon);
            SV_GiveClipsAW(ps, atoi(Cmd_Argv(4))+UT_WEAPON_GETCLIPS(targetweapon), targetweapon);
        }
    }
}

/////////////////////////////////////////////////////////////////////
// SV_GiveItem_f
/////////////////////////////////////////////////////////////////////
static void SV_GiveItem_f (void) {

	client_t      *cl;
	utItemID_t      item = 0;
	playerState_t *ps;

	if(!com_sv_running->integer) {
		Com_Printf("Server is not running\n");
		return;
	}

	if(Cmd_Argc() < 3) {
		Com_Printf("Usage: giveitem <player> <item>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if (!cl) {
        return;
    }

	item = SV_Char2Item(Cmd_Argv(2));

	if(item == 0) {
		Com_Printf("Item not found\n");
		return;
	}

	ps = SV_GameClientNum(cl - svs.clients);
    if(utPSFirstMath(ps,item) == -1)
	    utPSGiveItem(ps, item);
}

/////////////////////////////////////////////////////////////////////
// SV_RemoveWeapon_f
/////////////////////////////////////////////////////////////////////
static void SV_RemoveWeapon_f (void) {

	client_t      *cl;
	weapon_t      wp = 0;
	playerState_t *ps;

	if(!com_sv_running->integer) {
		Com_Printf("Server is not running\n");
		return;
	}

	if(Cmd_Argc() < 3) {
		Com_Printf("Usage: removeweapon <player> <weapon>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
    if (!cl) {
        return;
    }

	wp = SV_Char2Weapon(Cmd_Argv(2));

	if(wp == 0) {
		Com_Printf("Weapon not found\n");
		return;
	}

	ps = SV_GameClientNum(cl - svs.clients);

	SV_RemoveWeapon(ps, wp);
}

/////////////////////////////////////////////////////////////////////
// SV_RemoveItem_f
/////////////////////////////////////////////////////////////////////
static void SV_RemoveItem_f (void) {

	client_t      *cl;
	utItemID_t      item = 0;
	playerState_t *ps;

	if(!com_sv_running->integer) {
		Com_Printf("Server is not running\n");
		return;
	}

	if(Cmd_Argc() < 3) {
		Com_Printf("Usage: removeitem <player> <item>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if (!cl) {
        return;
    }

	item = SV_Char2Item(Cmd_Argv(2));

    if(item == 0) {
		Com_Printf("Item not found\n");
		return;
	}

	ps = SV_GameClientNum(cl - svs.clients);

	utPSRemoveItem(ps, item);
}

/////////////////////////////////////////////////////////////////////
// SV_GiveClips_f
/////////////////////////////////////////////////////////////////////
static void SV_GiveClips_f (void)
{
	client_t *cl;
	playerState_t *ps;
	int value;
	int toweapon;

	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running\n");
		return;
	}
	if(Cmd_Argc() < 3)
	{
		Com_Printf("Usage: giveclips <player> <value> [weapon]\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(!cl)
		return;
	if (sscanf(Cmd_Argv(2), "%d", &value) == 0) {
		Com_Printf("Invalid value.\n");
		return;
	}

	ps = SV_GameClientNum(cl - svs.clients);

    //if contain weapon
    if(Cmd_Argc() > 3)
    {
        toweapon = SV_FirstMatchFor(ps, SV_Char2Weapon(Cmd_Argv(3)));
        if(toweapon == -1)
        {
            Com_Printf("Can't find the weapon\n");
            return;
        }
    }else
    {
        toweapon = ps->weapon;
    }

	SV_GiveClipsAW(ps, value, toweapon);
}

/////////////////////////////////////////////////////////////////////
// SV_GiveBullets_f
/////////////////////////////////////////////////////////////////////
static void SV_GiveBullets_f (void)
{
	client_t *cl;
	playerState_t *ps;
	int value;
    int toweapon;
	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running\n");
		return;
	}
	if(Cmd_Argc() < 3)
	{
		Com_Printf("Usage: givebullets <player> <value> [weapon]\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(!cl)
		return;
	if (sscanf(Cmd_Argv(2), "%d", &value) == 0) {
		Com_Printf("Invalid value.\n");
		return;
	}

	ps = SV_GameClientNum(cl - svs.clients);

    //if contain weapon
    if(Cmd_Argc() > 3)
    {
        toweapon = SV_FirstMatchFor(ps, SV_Char2Weapon(Cmd_Argv(3)));
        if(toweapon == -1)
        {
            Com_Printf("Can't find the weapon\n");
            return;
        }
    }else
    {
        toweapon = ps->weapon;
    }

	SV_GiveBulletsAW(ps, value, toweapon);
}

/////////////////////////////////////////////////////////////////////
// SV_SetClips_f
/////////////////////////////////////////////////////////////////////
static void SV_SetClips_f (void)
{
	client_t *cl;
	playerState_t *ps;
	int value;
	int toweapon;

	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running\n");
		return;
	}
	if(Cmd_Argc() < 3)
	{
		Com_Printf("Usage: setclips <player> <value> [weapon]\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(!cl)
		return;
	if (sscanf(Cmd_Argv(2), "%d", &value) == 0) {
		Com_Printf("Invalid value.\n");
		return;
	}

	ps = SV_GameClientNum(cl - svs.clients);

    //if contain weapon
    if(Cmd_Argc() > 3)
    {
        toweapon = SV_FirstMatchFor(ps, SV_Char2Weapon(Cmd_Argv(3)));
        if(toweapon == -1)
        {
            Com_Printf("Can't find the weapon\n");
            return;
        }
    }else
    {
        toweapon = ps->weapon;
    }

	SV_SetClipsAW(ps, value, toweapon);
}

/////////////////////////////////////////////////////////////////////
// SV_SetBullets_f
/////////////////////////////////////////////////////////////////////
static void SV_SetBullets_f (void)
{
	client_t *cl;
	playerState_t *ps;
	int value;
	int toweapon;

	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running\n");
		return;
	}
	if(Cmd_Argc() < 3)
	{
		Com_Printf("Usage: setbullets <player> <value> [weapon]\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(!cl)
		return;
	if (sscanf(Cmd_Argv(2), "%d", &value) == 0) {
		Com_Printf("Invalid value.\n");
		return;
	}

	ps = SV_GameClientNum(cl - svs.clients);

    //if contain weapon
    if(Cmd_Argc() > 3)
    {
        toweapon = SV_FirstMatchFor(ps, SV_Char2Weapon(Cmd_Argv(3)));
        if(toweapon == -1)
        {
            Com_Printf("Can't find the weapon\n");
            return;
        }
    }else
    {
        toweapon = ps->weapon;
    }

	SV_SetBulletsAW(ps, value, toweapon);
}

/////////////////////////////////////////////////////////////////////
// SV_Teleport_f
/////////////////////////////////////////////////////////////////////
static void SV_Teleport_f(void) {

    client_t      *cl;
    playerState_t *ps;

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running.\n");
        return;
    }

    // check for correct number of arguments
    if (Cmd_Argc() != 2 && Cmd_Argc() != 3 && Cmd_Argc() != 5) {
        Com_Printf("Usage: teleport <player1> <player2>\n"
                   "       teleport <player> <x> <y> <z>\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if (!cl) {
        return;
    }

    ps = SV_GameClientNum(cl - svs.clients);

    // print a player's position
    if (Cmd_Argc() == 2) {
        Com_Printf("Position of %s^7: (x: %f, y: %f, z: %f)\n", cl->name, ps->origin[0], ps->origin[1], ps->origin[2]);
        return;
    }

    // teleport a player to another player's position
    if (Cmd_Argc() == 3) {
        client_t      *cl_src;
        playerState_t *ps_src;

        Cmd_TokenizeString(Cmd_Args());

        cl_src = SV_GetPlayerByHandle();
        // return if no client2 or if both players are the same
        if (!cl_src || cl_src == cl) {
            return;
        }

        ps_src = SV_GameClientNum(cl_src - svs.clients);
        VectorCopy(ps_src->origin, ps->origin);

        Com_Printf("Teleported %s ^7to %s\n", cl->name, cl_src->name);
        SV_SendServerCommand(cl, "cchat \"\" \"%s^7You have been ^2teleported ^7to: %s\"", sv_tellprefix->string, cl_src->colourName);
        SV_SendServerCommand(cl_src, "cchat \"\" \"%s^7Player %s ^7has been ^2teleported ^7to you!\"", sv_tellprefix->string, cl->colourName);

    // teleport a player to the specified x, y, z coordinates
    } else {
        int i;
        for (i = 0; i < 3; ++i) {
            ps->origin[i] = atof(Cmd_Argv(i + 2));
        }
    }
    VectorClear(ps->velocity);
}

/////////////////////////////////////////////////////////////////////
// SV_Switch_f
/////////////////////////////////////////////////////////////////////
static void SV_Switch_f(void) {

    client_t      *cl, *cl2;
    playerState_t *ps, *ps2;

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running.\n");
        return;
    }

    // check for correct number of arguments
    if (Cmd_Argc() != 3) {
        Com_Printf("Usage: switch <player1> <player2>\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if (!cl) {
        return;
    }
    ps = SV_GameClientNum(cl - svs.clients);

    Cmd_TokenizeString(Cmd_Args());

    cl2 = SV_GetPlayerByHandle();
    // return if no client2 or if both players are the same
    if (!cl2 || cl2 == cl) {
        return;
    }
    ps2 = SV_GameClientNum(cl2 - svs.clients);

    // switch clients positions
    vec3_t tmp;
    VectorCopy(ps2->origin, tmp);
    VectorCopy(ps->origin, ps2->origin);
    VectorCopy(tmp, ps->origin);

    Com_Printf("Positions of players %s ^7and %s ^7were switched.\n", cl->name, cl2->name);
    SV_SendServerCommand(cl, "cchat \"\" \"%s^7Your position has been ^2switched ^7with: %s\"", sv_tellprefix->string, cl2->colourName);
    SV_SendServerCommand(cl2, "cchat \"\" \"%s^7Your position has been ^2switched ^7with: %s\"", sv_tellprefix->string, cl->colourName);

    VectorClear(ps->velocity);
    VectorClear(ps2->velocity);
}

/////////////////////////////////////////////////////////////////////
// SV_SetHealth_f
/////////////////////////////////////////////////////////////////////
static void SV_SetHealth_f(void) {

	client_t      *cl;
    int           value;

	if(!com_sv_running->integer) {
		Com_Printf("Server is not running\n");
	}

	if(Cmd_Argc() < 3) {
        Com_Printf("Usage: sethealth <player> <value>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(cl == NULL)
		return;

	if (sscanf(Cmd_Argv(2), "%d", &value) == 0) {
		Com_Printf("Invalid value.\n");
		return;
	}
	MOD_SetHealth(cl, value);
}

/////////////////////////////////////////////////////////////////////
// SV_AddHealth_f
/////////////////////////////////////////////////////////////////////
static void SV_AddHealth_f(void) {

	client_t      *cl;
    int           value;

	if(!com_sv_running->integer) {
		Com_Printf("Server is not running\n");
	}

	if(Cmd_Argc() < 3) {
		Com_Printf("Usage: addhealth <player> <value>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(cl == NULL)
		return;

	if (sscanf(Cmd_Argv(2), "%d", &value) == 0) {
		Com_Printf("Invalid value.\n");
		return;
	}
	MOD_AddHealth(cl, value);
}

/////////////////////////////////////////////////////////////////////
// SV_AutoHealth_f
/////////////////////////////////////////////////////////////////////
void SV_AutoHealth_f(void)
{
    if(!com_sv_running->integer)
    {
        Com_Printf("Server is not running\n");
        return;
    }
    client_t *cl;

    cl = SV_GetPlayerByHandle();

    if(!cl)
        return;

    if(Cmd_Argc() == 2)
    {
        cl->cm.perPlayerHealth = 0;
        return;
    }

    if(Cmd_Argc() < 7)
    {
        Com_Printf("Usage: autohealth <player> <limit> <moving> <step> <timeout> <turnoffwhenfinish>\n");
        return;
    }

    cl->cm.perPlayerHealth = 1;
    cl->cm.turnOffUsed = 0;
    cl->cm.limitHealth = atoi(Cmd_Argv(2));
    cl->cm.whenmovingHealth = atoi(Cmd_Argv(3));
    cl->cm.stepHealth = atoi(Cmd_Argv(4));
    cl->cm.timeoutHealth = atoi(Cmd_Argv(5));
    cl->cm.turnOffWhenFinish = atoi(Cmd_Argv(6));
}

/////////////////////////////////////////////////////////////////////
// SV_QVMReload_f
/////////////////////////////////////////////////////////////////////
static void SV_QVMReload_f (void)
{
	if(!com_sv_running->integer) {
		Com_Printf("Server is not running\n");
		return;
	}
	if(overrideQVMData())
	{
		Com_Printf("Reloaded correctly!\n");
	}
	else
	{
		Com_Printf("Unable to modify qvm data!\n");
	}
}

/////////////////////////////////////////////////////////////////////
// SV_ResquestDownload_f
/////////////////////////////////////////////////////////////////////
static void SV_ResquestDownload_f (void)
{
	client_t *cl;

	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running\n");
		return;
	}

	if(Cmd_Argc() != 3)
	{
		Com_Printf("Usage: resquestdownload <player> <file>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(!cl)
	{
		return;
	}

	 MOD_ResquestPk3DownloadByClientGameState (cl, Cmd_Argv(2));
}

/////////////////////////////////////////////////////////////////////
// SV_CheckWeaponOffset_f
/////////////////////////////////////////////////////////////////////
void SV_CheckOffset_f (void)
{
	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running\n");
		return;
	}

	if(Cmd_Argc() < 3)
	{
		Com_Printf("Usage: dumpoffset <offset> <size>\n");
		return;
	}

	//Dump all data of a given weapon
	int offset = atoi(Cmd_Argv(1));
	int size = atoi(Cmd_Argv(2));

	FILE *f = fopen("qvmoffset.txt", "a");
	int i;

	if(!f)
	{
		Com_Printf("Could not open the file!\n");
		return;
	}

	void* startPosition = (int *)VM_ArgPtr((offset));

	for(i = 0; i < size; i++)
	{
		fprintf(f, "[%d] c %c d %d i %i \n", (offset+i), *((char*)(startPosition+i)),*((int*)(startPosition+i)),*((int*)(startPosition+i)));
	}
	fclose(f);
}

/////////////////////////////////////////////////////////////////////
// SV_CheckGOffset_f
/////////////////////////////////////////////////////////////////////
void SV_CheckGOffset_f (void)
{
	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running\n");
		return;
	}

	if(Cmd_Argc() < 4)
	{
		Com_Printf("Usage: dumpgoffset <player> <offset> <size>\n");
		return;
	}

	client_t *cl;
	cl = SV_GetPlayerByHandle();
	if(!cl)
		return;

	playerState_t *ps;
	ps = SV_GameClientNum(cl - svs.clients);

	//Dump all data
	int offset = atoi(Cmd_Argv(2));
	int size = atoi(Cmd_Argv(3));

	FILE *f = fopen("gdump.txt", "a");
	int i;

	if(!f)
	{
		Com_Printf("Could not open the file!\n");
		return;
	}

	void* startPosition = (ps+offset);

	for(i = 0; i < size; i++)
	{
		fprintf(f, "[%d] c %c d %d \n", (offset+i), *((char*)(startPosition+i)),*((int*)(startPosition+i)));
	}
	fclose(f);
}


/////////////////////////////////////////////////////////////////////
// SV_Location_f
/////////////////////////////////////////////////////////////////////
void SV_Location_f (void)
{
	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running!\n");
		return;
	}
	if(Cmd_Argc() < 4)
	{
		Com_Printf("Usage: location <player/all> <string> <index> <lock>\n");
		return;
	}

	int j;
	client_t *cl;

	//Check index
	int index = atoi(Cmd_Argv(3));
	if(index >= 0 && 360 > index)
	{
		if(Q_strncmp(Cmd_Argv(1), "all", 3) == 0)
		{
			for (j = 0, cl = svs.clients; j < sv_maxclients->integer ; j++, cl++)
			{
				MOD_SendCustomLocation(cl, Cmd_Argv(2), index);
				if(atoi(Cmd_Argv(4)))
				{
					MOD_ChangeLocation(cl, index, 1);
				}else
				{
					cl->cm.locationLocked = 0;
				}
			}
			return;
		}

		cl = SV_GetPlayerByHandle();
		if(!cl)
			return;
		MOD_SendCustomLocation(cl, Cmd_Argv(2),  index);
		if(atoi(Cmd_Argv(4)))
		{
			MOD_ChangeLocation(cl, index, 1);
		}else
		{
			cl->cm.locationLocked = 0;
		}
	}else{
		Com_Printf("Index must be between 0 and 360\n");
	}
}

/////////////////////////////////////////////////////////////////////
// SV_Freeze_f
/////////////////////////////////////////////////////////////////////
static void SV_Freeze_f(void) {

    client_t *cl;

    if (!com_sv_running->integer) {
        Com_Printf("Server is not running\n");
        return;
    }

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: freeze <player>\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if (!cl) {
        return;
    }

    if (cl->cm.frozen > 0) {
        cl->cm.frozen = 0;
        Com_Printf("Player %s ^7Thawed.\n", cl->name);
        SV_SendServerCommand(cl, "cchat \"\" \"%s^7You have been ^2thawed^7!\"", sv_tellprefix->string);

    } else {
        cl->cm.frozen = 1;
        Com_Printf("Player %s ^7Frozen.\n", cl->name);
        SV_SendServerCommand(cl, "cchat \"\" \"%s^7You have been ^1frozen^7!\"", sv_tellprefix->string);
    }
}

/////////////////////////////////////////////////////////////////////
// SV_SendClientCommand_f
/////////////////////////////////////////////////////////////////////
static void SV_SendClientCommand_f(void) {

    char      *cmd;
    client_t  *cl;

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running\n");
        return;
    }

    // check for correct parameters
    if (Cmd_Argc() < 3 || !strlen(Cmd_Argv(2))) {
        Com_Printf("Usage: sendclientcommand or scc <player/all> <command>\n");
        return;
    }

    // get the command
    cmd = Cmd_ArgsFromRaw(2);

    if (!Q_stricmp(Cmd_Argv(1), "all")) {
        // send the command to everyone
        SV_SendServerCommand(NULL, "%s", cmd);

    } else {

        // search the client
        cl = SV_GetPlayerByHandle();
        if (!cl) {
            return;
        }

        // send the command to the client
        SV_SendServerCommand(cl, "%s", cmd); 
    }
}

/////////////////////////////////////////////////////////////////////
// SV_Spoof_f
/////////////////////////////////////////////////////////////////////
static void SV_Spoof_f(void) {

    char      *cmd;
    client_t  *cl;

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running\n");
        return;
    }

    // check for correct parameters
    if (Cmd_Argc() < 3 || !strlen(Cmd_Argv(2))) {
        Com_Printf("Usage: spoof <player> <command>\n");
        return;
    }

    // search the client
    cl = SV_GetPlayerByHandle();
    if (!cl) {
        return;
    }

    // get the command
    cmd = Cmd_ArgsFromRaw(2);
    Cmd_TokenizeString(cmd);

    // send the command
    VM_Call(gvm, GAME_CLIENT_COMMAND, cl - svs.clients);
}

/////////////////////////////////////////////////////////////////////
// SV_ForceCvar_f_helper
// Set a CVAR for a user (helper)
/////////////////////////////////////////////////////////////////////
static void SV_ForceCvar_f_helper(client_t *cl) {

    // if the client is not connected
    if (cl->state < CS_CONNECTED) {
        return;
    }

    Info_SetValueForKey(cl->userinfo, Cmd_Argv(2), Cmd_Argv(3));

    SV_UserinfoChanged(cl);

    // call prog code to allow overrides
    VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients);
}

/////////////////////////////////////////////////////////////////////
// SV_ForceCvar_f
// Set a CVAR for a user
/////////////////////////////////////////////////////////////////////
static void SV_ForceCvar_f(void) {

    int       i;
    client_t  *cl;

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running\n");
        return;
    }

    if (Cmd_Argc() != 4 || strlen(Cmd_Argv(2)) == 0) {
        Com_Printf("Usage: forcecvar <player/all/allbots> <cvar> <value>\n");
        return;
    }

    if (!Q_stricmp(Cmd_Argv(1), "all")) {

        for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {

            // if not connected
            if (!cl->state) {
                continue;
            }

            // call internal helper
            SV_ForceCvar_f_helper(cl);
        }

    } else if (!Q_stricmp(Cmd_Argv(1), "allbots")) {

        for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {

            // if not connected
            if (!cl->state) {
                continue;
            }

            // if the player is not a bot
            if (cl->netchan.remoteAddress.type != NA_BOT) {
                continue;
            }

            // call internal helper
            SV_ForceCvar_f_helper(cl);
        }

    } else {

        // search the client
        cl = SV_GetPlayerByHandle();
        if (!cl) {
            return;
        }

        // call internal helper
        SV_ForceCvar_f_helper(cl);
    }
}

/////////////////////////////////////////////////////////////////////
// SV_ChangeAuth_f
// Change auth of the scoreboard
/////////////////////////////////////////////////////////////////////
void SV_ChangeAuth_f (void)
{
	client_t *cl;

	if(!com_sv_running->integer)
		return;

	if(Cmd_Argc() != 3)
	{
		Com_Printf("Usage: changeauth <player> <newauth>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(!cl)
		return;

	Q_strncpyz(cl->cm.authcl, Cmd_Argv(2), MAX_NAME_LENGTH);
}

void SV_InfiniteStamina_fc(void)
{
    client_t *cl;

    if (!com_sv_running->integer)
        return;

    if (Cmd_Argc() != 3) {
        Com_Printf("Usage: infinitestamina <player> <integer>\n");
        Com_Printf("0 = Default config; 1 = Enable; 2 = Disable\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if (!cl)
        return;

    if (Cvar_VariableIntegerValue("g_stamina") == 2) {
        Com_Printf("Warning: g_stamina is 2.\n");
        //return;
    }

    cl->cm.infiniteStamina = atoi(Cmd_Argv(2));
}

void SV_InfiniteWallJumps_fc(void)
{
    client_t *cl;

    if (!com_sv_running->integer)
        return;

    if (Cmd_Argc() != 3) {
        Com_Printf("Usage: infinitewalljumps <player> <integer>\n");
        Com_Printf("0 = Default config; 1 = Enable; 2 = Disable\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if (!cl)
        return;

    cl->cm.infiniteWallJumps = atoi(Cmd_Argv(2));
}

void SV_IsJumpTimerEnabled(void)
{
    client_t *cl;

    if (!com_sv_running->integer)
        return;

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: isJumpTimerOn <player>");
    }

    cl = SV_GetPlayerByHandle();
    if (!cl)
        return;

    Com_Printf("%d\n", cl->cm.ready);
}

/////////////////////////////////////////////////////////////////////
// SV_Setuser_f
/////////////////////////////////////////////////////////////////////
static void SV_Setuser_f (void)
{
	client_t *cl;

	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running\n");
		return;
	}
	if(Cmd_Argc() != 2)
	{
		Com_Printf("Usage: setuser <player> \n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(!cl)
		return;
    cl->isuser = qtrue;
    Com_Printf("^7Player ^5%s ^7set as user.\n", cl->name);
}

/////////////////////////////////////////////////////////////////////
// SV_Setadmin_f
/////////////////////////////////////////////////////////////////////
static void SV_Setadmin_f (void)
{
	client_t *cl;

	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running\n");
		return;
	}
	if(Cmd_Argc() != 2)
	{
		Com_Printf("Usage: setadmin <player> \n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(!cl)
		return;
    cl->isadmin = qtrue;
    Com_Printf("^7Player ^5%s ^7set as admin.\n", cl->name);
}

/////////////////////////////////////////////////////////////////////
// SV_Setbot_f
/////////////////////////////////////////////////////////////////////
static void SV_Setbot_f (void)
{
	client_t *cl;

	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running\n");
		return;
	}
	if(Cmd_Argc() != 2)
	{
		Com_Printf("Usage: setbot <bot> \n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(!cl)
		return;
    cl->isbot = qtrue;
    Com_Printf("^7Player ^5%s ^7set as bot.\n", cl->name);
}

/////////////////////////////////////////////////////////////////////
// SV_Setowner_f
/////////////////////////////////////////////////////////////////////
static void SV_Setowner_f (void)
{
	client_t *cl;

	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running\n");
		return;
	}
	if(Cmd_Argc() != 2)
	{
		Com_Printf("Usage: setowner <player> \n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(!cl)
		return;
    cl->isowner = qtrue;
    Com_Printf("^7Player ^5%s ^7set as owner.\n", cl->name);
}

/////////////////////////////////////////////////////////////////////
// SV_Setauthed_f
/////////////////////////////////////////////////////////////////////
static void SV_Setauthed_f (void)
{
	client_t *cl;

	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running\n");
		return;
	}
	if(Cmd_Argc() != 2)
	{
		Com_Printf("Usage: setauthed <player> \n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(!cl)
		return;
    cl->isauthed = qtrue;
    Com_Printf("^7Player ^5%s ^7set as authed.\n", cl->name);
}

/////////////////////////////////////////////////////////////////////
// SV_Setcolour_f
/////////////////////////////////////////////////////////////////////
static void SV_Setcolour_f (void)
{
	client_t *cl;

	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running\n");
		return;
	}
	if(Cmd_Argc() != 3)
	{
		Com_Printf("Usage: setcolour <player> <colour>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(!cl)
		return;
    cl->chatcolour = atoi(Cmd_Argv(2));
    Com_Printf("^7Players colour set to ^5%s.\n", Cmd_Argv(2));
}

char *str_replace66(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep (the string to remove)
    int len_with; // length of with (the string to replace rep with)
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    // sanity checks and initialization
    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL; // empty rep causes infinite loop during count
    if (!with)
        with = "";
    len_with = strlen(with);

    // count the number of replacements needed
    ins = orig;
    for (count = 0; tmp = strstr(ins, rep); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}


/////////////////////////////////////////////////////////////////////
// SV_BigText_f
//
// This will override the default bigtext function
// multilines should be declared with /n instead of \n
/////////////////////////////////////////////////////////////////////
static void SV_BigText_f(void) {

	if (!com_sv_running->integer) {
		Com_Printf("Server is not running\n");
		return;
	}

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: bigtext <text>\n");
        Com_Printf("Note: '/n' will count as a newline for multiline bigtext.\n");
		return;
	}

    char* newLine = "\n";
    char* toReplace = "/n";
    char* bigText = str_replace66(Cmd_Argv(1), toReplace, newLine);

    SV_SendServerCommand(NULL, "cp \"%s\"", bigText);
}

/////////////////////////////////////////////////////////////////////
// SV_ToggleParticles_f
/////////////////////////////////////////////////////////////////////

static void SV_ToggleParticles_f (void)
{
    client_t *cl;

    if (!com_sv_running->integer) {
        Com_Printf("Server is not running\n");
        return;
    }

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: particlefx <player>\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if (!cl) {
        Com_Printf("Unable to get client\n");
        return;
    }

    if (cl->particlefx) {
        cl->particlefx = qfalse;
        SV_SendServerCommand(cl, "cchat \"\" \"^5Particles ^3have been ^7[^1DISABLED^7]\"");
    } else {
        cl->particlefx = qtrue;
        SV_SendServerCommand(cl, "cchat \"\" \"^5Particles ^3have been ^7[^2ENABLED^7]\"");
    }
}

/////////////////////////////////////////////////////////////////////
// SV_Attach_f
/////////////////////////////////////////////////////////////////////
static void SV_Attach_f(void) {
    Com_Printf("Debug 1");
    client_t      *cl, *cl2;

    // make sure server is running
    if (!com_sv_running->integer) {
        Com_Printf("Server is not running.\n");
        return;
    }

    // check for correct number of arguments
    if (Cmd_Argc() != 3) {
        Com_Printf("Usage: attach <player1> <player2>\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if (!cl) {
        return;
    }

    Cmd_TokenizeString(Cmd_Args());

    cl2 = SV_GetPlayerByHandle();
    // return if no client2 or if both players are the same
    if (!cl2 || cl2 == cl) {
        return;
    }

    if (cl->isattached || cl2->hasattached) {
        return;
    }
    
    unsigned int time = Com_Milliseconds();
    int cid = cl2 - svs.clients;
    cl2->hasattached = qtrue;
    cl->attachedto = cid;
    cl->isattached = qtrue;
    cl->stopattach = time + 30000;
}

/////////////////////////////////////////////////////////////////////
// SV_Disarm_f
/////////////////////////////////////////////////////////////////////
static void SV_Disarm_f(void) {

    client_t *cl;

    if (!com_sv_running->integer) {
        Com_Printf("Server is not running\n");
        return;
    }

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: disarm <player>\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if (!cl) {
        return;
    }
    char* weapons_all[23] = { "hk69", "lr", "spas12", "m4", "psg1", "mp5k", "ak103", "negev", "g36", "sr8", "ump45", "mac11", "p90", "frf1", "benelli", "fstod", "beretta", "deagle", "colt", "glock", "magnum", "knife", "he" };
    for (int i = 0; i < sizeof(weapons_all) / sizeof(const char*); i++) {
	    Cmd_ExecuteString(va("rw %s %s", cl->name, weapons_all[i]));
    }

}

/////////////////////////////////////////////////////////////////////
// SV_GetWeapons_f
/////////////////////////////////////////////////////////////////////
static void SV_GetWeapons_f(void) {

    client_t *cl;
    playerState_t *ps;

    char* returnstring = "";

    if (!com_sv_running->integer) {
        Com_Printf("Server is not running\n");
        return;
    }

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: getweapons <player>\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if (!cl) {
        return;
    }

    int counter = 0;

    ps = SV_GameClientNum(cl - svs.clients);
    char* weapons_all[22] = { "hk69", "lr", "spas12", "m4", "psg1", "mp5k", "ak103", "negev", "g36", "sr8", "ump45", "mac11", "p90", "frf1", "benelli", "fstod", "beretta", "deagle", "colt", "glock", "magnum", "knife" };
    for (int i = 0; i < sizeof(weapons_all) / sizeof(const char*); i++) {
        if (SV_FirstMatchFor(ps, SV_Char2Weapon(weapons_all[i])) != -1) {
            if (counter == 0) {
                returnstring = weapons_all[i];
            } else {
                returnstring = va("%s,%s", returnstring, weapons_all[i]);
            }
            counter++;
        }
    }
    Com_Printf("%s", returnstring);
}

/////////////////////////////////////////////////////////////////////
// SV_Silentname_f
/////////////////////////////////////////////////////////////////////
static void SV_Silentname_f (void)
{
    client_t *cl;

    if (!com_sv_running->integer) {
        Com_Printf("Server is not running\n");
        SV_LogPrintf("Silentname: Server is not running\n");
        return;
    }

    if (Cmd_Argc() < 3) {
        Com_Printf("Usage: silentname <player> <newname>\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if (!cl) {
        Com_Printf("Unable to get client\n");
        SV_LogPrintf("Silentname: Unable to get client\n");
        return;
    }

    // clear the current customname
    memset(cl->lastcustomname,0,sizeof(cl->lastcustomname));    
    Q_strncpyz(cl->lastcustomname, Cmd_Argv(2), 128);
    cl->customname = qtrue;
}

/////////////////////////////////////////////////////////////////////
// SV_Getplayerteam_f
/////////////////////////////////////////////////////////////////////
static void SV_Getplayerteam_f (void)
{
    client_t *cl;
    playerState_t *ps;

    if (!com_sv_running->integer) {
        Com_Printf("Server is not running\n");
        return;
    }

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: getplayerteam <player>\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if (!cl) {
        Com_Printf("Unable to get client\n");
        return;
    }

    ps = SV_GameClientNum(cl - svs.clients);

    if (ps->pm_flags & 1024 || ps->persistant[3] == 3) {
        Com_Printf("SPEC\n");
        return;
    }
    if (ps->persistant[3] == 0) {
        Com_Printf("FREE\n");
        return;
    }
    if (ps->persistant[3] == 1) {
        Com_Printf("RED\n");
        return;
    }
    if (ps->persistant[3] == 2) {
        Com_Printf("BLUE\n");
        return;
    }
}

static void SV_Setlevel_f (void)
{
    client_t *cl;

    if (!com_sv_running->integer) {
        Com_Printf("Server is not running\n");
        return;
    }

    if (Cmd_Argc() < 3) {
        Com_Printf("Usage: setlevel <player> <level>\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if (!cl) {
        Com_Printf("Unable to get client\n");
        return;
    }

    cl->level = atoi(Cmd_Argv(2));
    Com_Printf("Client level now: %i\n", cl->level);

}

static void SV_GetTeam_f (void)
{
    client_t *cl;
    playerState_t *ps;

    if (!com_sv_running->integer) {
        Com_Printf("Server is not running\n");
        return;
    }

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: getteam <player>\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if (!cl) {
        Com_Printf("Unable to get client\n");
        return;
    }

    ps = SV_GameClientNum(cl - svs.clients);

    int clientTeam = *(int*)((void*)ps+gclientOffsets[getVersion()][OFFSET_TEAM]);
    Com_Printf("%i", clientTeam);
}

static void SV_SpawnEntity_f (void)
{
    client_t *cl;
    playerState_t *ps;
    sharedEntity_t *ent;

    if (!com_sv_running->integer) {
        Com_Printf("Server is not running\n");
        return;
    }

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage:\n"
        "spawnentity <player> <path to .md3>\n"
        "spawnentity <x coordinates> <y coordinates> <z coordinates> <path to .md3>\n");
        return;
    }

    // get ent id from sv.entity blabla descending
    ent = SV_GentityNum(sv.lastEntityNum);

    if (Cmd_Argc() == 3) {

        cl = SV_GetPlayerByHandle();

        if (!cl) {
            Com_Printf("Unable to get client\n");
            return;
        }

        ps = SV_GameClientNum(cl - svs.clients);

        ent->s.number = sv.lastEntityNum;
        ent->s.eType = 4;
        ent->s.eFlags |= 32; // This might not be needed
        ent->r.currentAngles[1] = ent->r.currentAngles[1] - 90; // fixing viewangles
        ent->s.constantLight = 1;
        ent->s.modelindex = sv.lastIndexNum; // cannot be too high, otherwise server crashes
        ent->r.bmodel = qfalse;

        VectorCopy(ps->origin, ent->s.pos.trBase);
        VectorCopy(ps->origin, ent->r.currentOrigin);
        VectorCopy(ps->viewangles, ent->r.currentAngles);

        // Set the model
        SV_SetConfigstring((CS_MODELS + ent->s.modelindex), Cmd_Argv(2));

        // Link it in
        SV_LinkEntity(ent);

        // For tracking
        sv.lastIndexNum--;
        sv.lastEntityNum--;
    }
    else if (Cmd_Argc() == 5) {
        vec3_t coords;
        coords[0] = atoi(Cmd_Argv(1));
        coords[1] = atoi(Cmd_Argv(2));
        coords[2] = atoi(Cmd_Argv(3));

        ent->s.number = sv.lastEntityNum;
        ent->s.eType = 4;
        ent->s.eFlags |= 32; // This might not be needed
        ent->s.constantLight = 1;
        ent->s.modelindex = sv.lastIndexNum; // cannot be too high, otherwise server crashes
        ent->r.bmodel = qfalse;

        VectorCopy(coords, ent->s.pos.trBase);
        VectorCopy(coords, ent->r.currentOrigin);

        // Set the model
        SV_SetConfigstring((CS_MODELS + ent->s.modelindex), Cmd_Argv(4));

        // Link it in
        SV_LinkEntity(ent);

        // For tracking
        sv.lastIndexNum--;
        sv.lastEntityNum--;
    }
    else {
        Com_Printf("Usage:\n"
        "spawnentity <player> <path to .md3>\n"
        "spawnentity <x coordinates> <y coordinates> <z coordinates> <path to .md3>\n");
    }
}

/////////////////////////////////////////////////////////////////////
// SV_Setskill_f
/////////////////////////////////////////////////////////////////////
static void SV_Setskill_f (void)
{
    client_t *cl;

    if (!com_sv_running->integer) {
        Com_Printf("Server is not running\n");
        return;
    }

    if (Cmd_Argc() < 3) {
        Com_Printf("Usage: setskill <player> <skill>\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if (!cl) {
        Com_Printf("Unable to get client\n");
        return;
    }

    cl->skill = atoi(Cmd_Argv(2));
    Com_Printf("Client skill now: %i\n", cl->skill);

}

/////////////////////////////////////////////////////////////////////
// SV_Set1v1wpn_f
/////////////////////////////////////////////////////////////////////
static void SV_Set1v1wpn_f(void) {

    client_t *cl;

    if (!com_sv_running->integer) {
        return;
    }

    cl = SV_GetPlayerByHandle();
    if (!cl) {
        return;
    }

    playerState_t *ps;
    ps = SV_GameClientNum(cl - svs.clients);

    ps->weapon = 0;
}

/////////////////////////////////////////////////////////////////////
// SV_GetPlayerSkill_f
/////////////////////////////////////////////////////////////////////
static void SV_GetPlayerSkill_f(void) {

    client_t *cl;

    if (!com_sv_running->integer) {
        Com_Printf("Server is not running\n");
        return;
    }

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: getskill <player>\n");
        return;
    }

    cl = SV_GetPlayerByHandle();
    if (!cl) {
        return;
    }

    Com_Printf("%i", cl->skill);
}


/*
==================
SV_AddOperatorCommands
==================
*/
void SV_AddOperatorCommands( void ) {

    static qboolean initialized;

    if (initialized) {
        return;
    }

    initialized = qtrue;

    Cmd_AddCommand ("heartbeat", SV_Heartbeat_f);
    Cmd_AddCommand ("kick", SV_Kick_f);
    Cmd_AddCommand ("banUser", SV_Ban_f);
    Cmd_AddCommand ("banClient", SV_BanNum_f);
    Cmd_AddCommand ("clientkick", SV_KickNum_f);
    Cmd_AddCommand ("status", SV_Status_f);
    Cmd_AddCommand ("serverinfo", SV_Serverinfo_f);
    Cmd_AddCommand ("systeminfo", SV_Systeminfo_f);
    Cmd_AddCommand ("dumpuser", SV_DumpUser_f);
    Cmd_AddCommand ("map_restart", SV_MapRestart_f);
    Cmd_AddCommand ("sectorlist", SV_SectorList_f);
    Cmd_AddCommand ("map", SV_Map_f);
    Cmd_AddCommand ("killserver", SV_KillServer_f);

#ifndef PRE_RELEASE_DEMO
    Cmd_AddCommand ("devmap", SV_Map_f);
    Cmd_AddCommand ("spmap", SV_Map_f);
    Cmd_AddCommand ("spdevmap", SV_Map_f);
#endif

    // TitanMod Cmds
    Cmd_AddCommand ("setscore", SV_SetScore_f);
    Cmd_AddCommand ("setdeaths", SV_SetDeaths_f);
    Cmd_AddCommand ("invisible", SV_Invisible_f);
    Cmd_AddCommand ("playsoundfile", SV_PlaySoundFile_f);
    Cmd_AddCommand ("playsound", SV_PlaySound_f);
    Cmd_AddCommand ("giveweapon", SV_GiveWeapon_f);
    Cmd_AddCommand ("gw", SV_GiveWeapon_f);
    Cmd_AddCommand ("removeweapon", SV_RemoveWeapon_f);
    Cmd_AddCommand ("rw", SV_RemoveWeapon_f);
    Cmd_AddCommand ("giveitem", SV_GiveItem_f);
    Cmd_AddCommand ("gi", SV_GiveItem_f);
    Cmd_AddCommand ("removeitem", SV_RemoveItem_f);
    Cmd_AddCommand ("ri", SV_RemoveItem_f);
	Cmd_AddCommand ("giveclips", SV_GiveClips_f);
	Cmd_AddCommand ("givebullets", SV_GiveBullets_f);
	Cmd_AddCommand ("setclips", SV_SetClips_f);
	Cmd_AddCommand ("setbullets", SV_SetBullets_f);
	Cmd_AddCommand ("addhealth", SV_AddHealth_f);
	Cmd_AddCommand ("sethealth", SV_SetHealth_f);
    Cmd_AddCommand ("autohealth", SV_AutoHealth_f);
	Cmd_AddCommand ("teleport", SV_Teleport_f);
    Cmd_AddCommand ("switch", SV_Switch_f);
    Cmd_AddCommand ("tp", SV_Teleport_f);
    Cmd_AddCommand ("resquestdownload", SV_ResquestDownload_f);
    Cmd_AddCommand ("dumpoffset", SV_CheckOffset_f);
    Cmd_AddCommand ("dumpgoffset", SV_CheckGOffset_f);
    Cmd_AddCommand ("location", SV_Location_f);
    Cmd_AddCommand ("qvmreload", SV_QVMReload_f);
    Cmd_AddCommand ("freeze", SV_Freeze_f);
    Cmd_AddCommand ("sendclientcommand", SV_SendClientCommand_f);
    Cmd_AddCommand ("scc", SV_SendClientCommand_f);
    Cmd_AddCommand ("spoof", SV_Spoof_f);
    Cmd_AddCommand ("forcecvar", SV_ForceCvar_f);
    Cmd_AddCommand ("changeauth", SV_ChangeAuth_f);
    Cmd_AddCommand ("infinitestamina", SV_InfiniteStamina_fc);
    Cmd_AddCommand ("infinitewalljumps", SV_InfiniteWallJumps_fc);
    Cmd_AddCommand ("isJumpTimerOn", SV_IsJumpTimerEnabled);

	// B3 specific commands (used with the custom chat)
    Cmd_AddCommand ("setuser", SV_Setuser_f);
    Cmd_AddCommand ("setadmin", SV_Setadmin_f);
    Cmd_AddCommand ("setowner", SV_Setowner_f);
    Cmd_AddCommand ("setbot", SV_Setbot_f);
    Cmd_AddCommand ("setauthed", SV_Setauthed_f);
    Cmd_AddCommand ("setcolour", SV_Setcolour_f);

    Cmd_AddCommand ("bigtext", SV_BigText_f);
    Cmd_AddCommand ("particlefx", SV_ToggleParticles_f);
    Cmd_AddCommand ("attach", SV_Attach_f);
    Cmd_AddCommand ("disarm", SV_Disarm_f);
    Cmd_AddCommand ("getweapons", SV_GetWeapons_f);
    Cmd_AddCommand ("silentname", SV_Silentname_f);
    Cmd_AddCommand ("getplayerteam", SV_Getplayerteam_f);

    Cmd_AddCommand ("setlevel", SV_Setlevel_f);

    Cmd_AddCommand ("getteam", SV_GetTeam_f);

    Cmd_AddCommand ("spawnentity", SV_SpawnEntity_f);

    Cmd_AddCommand ("setskill", SV_Setskill_f);
    Cmd_AddCommand ("set1v1wpn", SV_Set1v1wpn_f);
    Cmd_AddCommand ("getskill", SV_GetPlayerSkill_f);

    if( com_dedicated->integer ) {
        Cmd_AddCommand ("say", SV_ConSay_f);
        Cmd_AddCommand ("tell", SV_ConTell_f);
        Cmd_AddCommand("startserverdemo", SV_StartServerDemo_f);
        Cmd_AddCommand("stopserverdemo", SV_StopServerDemo_f);
    }
}

/*
==================
SV_RemoveOperatorCommands
==================
*/
void SV_RemoveOperatorCommands( void ) {
#if 0
    // removing these won't let the server start again
    Cmd_RemoveCommand ("heartbeat");
    Cmd_RemoveCommand ("kick");
    Cmd_RemoveCommand ("banUser");
    Cmd_RemoveCommand ("banClient");
    Cmd_RemoveCommand ("status");
    Cmd_RemoveCommand ("serverinfo");
    Cmd_RemoveCommand ("systeminfo");
    Cmd_RemoveCommand ("dumpuser");
    Cmd_RemoveCommand ("map_restart");
    Cmd_RemoveCommand ("sectorlist");
    Cmd_RemoveCommand ("say");
    Cmd_RemoveCommand ("tell");
    Cmd_RemoveCommand ("startserverdemo");
    Cmd_RemoveCommand ("stopserverdemo");
#endif
}
