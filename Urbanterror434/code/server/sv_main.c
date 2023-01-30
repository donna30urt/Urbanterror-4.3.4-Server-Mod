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

serverStatic_t	svs;				// persistant server info
server_t		sv;					// local server
vm_t			*gvm = NULL;		// game virtual machine

cvar_t	*sv_fps;					// time rate for running non-clients
cvar_t	*sv_timeout;				// seconds without any message
cvar_t	*sv_zombietime;				// seconds to sink messages after disconnect
cvar_t	*sv_rconPassword;			// password for remote server commands
cvar_t	*sv_rconRecoveryPassword;	// password for recovery the password remote server commands
cvar_t	*sv_rconAllowedSpamIP;			// this ip is allowed to do spam on rcon
cvar_t	*sv_privatePassword;		// password for the privateClient slots
cvar_t	*sv_allowDownload;
cvar_t	*sv_maxclients;

cvar_t	*sv_privateClients;			// number of clients reserved for password
cvar_t	*sv_hostname;
cvar_t	*sv_master[MAX_MASTER_SERVERS];		// master server ip address
cvar_t	*sv_reconnectlimit;			// minimum seconds between connect messages
cvar_t	*sv_showloss;				// report when usercmds are lost
cvar_t	*sv_padPackets;				// add nop bytes to messages
cvar_t	*sv_killserver;				// menu system can set to 1 to shut server down
cvar_t	*sv_mapname;
cvar_t	*sv_mapChecksum;
cvar_t	*sv_serverid;
cvar_t	*sv_minRate;
cvar_t	*sv_maxRate;
cvar_t	*sv_minPing;
cvar_t	*sv_maxPing;
cvar_t	*sv_gametype;
cvar_t	*sv_pure;
cvar_t	*sv_newpurelist;
cvar_t	*sv_floodProtect;
cvar_t	*sv_lanForceRate;			// dedicated 1 (LAN) server forces local client rates to 99999 (bug #491)
cvar_t	*sv_strictAuth;
cvar_t	*sv_clientsPerIp;

cvar_t	*sv_demonotice;				// notice to print to a client being recorded server-side
cvar_t  *sv_tellprefix;
cvar_t  *sv_sayprefix;
cvar_t 	*sv_demofolder;				//@Barbatos - the name of the folder that contains server-side demos

cvar_t  *mod_infiniteStamina;
cvar_t  *mod_infiniteWallJumps;
cvar_t  *mod_nofallDamage;

cvar_t  *mod_colourNames;

cvar_t  *mod_playerCount;
cvar_t  *mod_botsCount;
cvar_t  *mod_mapName;
cvar_t  *mod_mapColour;
cvar_t  *mod_hideCmds;
cvar_t  *mod_infiniteAmmo;
cvar_t  *mod_forceGear;
cvar_t  *mod_checkClientGuid;
cvar_t  *mod_disconnectMsg;
cvar_t  *mod_badRconMessage;

cvar_t  *mod_allowTell;
cvar_t  *mod_allowRadio;
cvar_t  *mod_allowWeapDrop;
cvar_t  *mod_allowItemDrop;
cvar_t  *mod_allowFlagDrop;
cvar_t  *mod_allowSuicide;
cvar_t  *mod_allowVote;
cvar_t  *mod_allowTeamSelection;
cvar_t  *mod_allowWeapLink;

cvar_t  *mod_minKillHealth;
cvar_t  *mod_minTeamChangeHealth;

cvar_t  *mod_limitHealth;
cvar_t  *mod_timeoutHealth;
cvar_t  *mod_enableHealth;
cvar_t  *mod_addAmountOfHealth;
cvar_t  *mod_whenMoveHealth;

cvar_t  *mod_allowPosSaving;
cvar_t  *mod_persistentPositions;
cvar_t  *mod_freeSaving;
cvar_t  *mod_enableJumpCmds;
cvar_t  *mod_enableHelpCmd;
cvar_t  *mod_loadSpeedCmd;
cvar_t  *mod_ghostRadius;

cvar_t  *mod_slickSurfaces;
cvar_t  *mod_gameType;
cvar_t  *mod_ghostPlayers;
cvar_t  *mod_noWeaponRecoil;
cvar_t  *mod_noWeaponCycle;
cvar_t  *mod_specChatGlobal;
cvar_t  *mod_cleanMapPrefixes;

cvar_t  *mod_disableScope;
cvar_t  *mod_fastTeamChange;

cvar_t  *mod_auth;
cvar_t  *mod_defaultauth;

cvar_t  *mod_hideServer;
cvar_t  *mod_enableWeaponsCvars;

cvar_t	*mod_gunsmod;
cvar_t	*mod_infiniteAirjumps;
cvar_t	*mod_customchat;
cvar_t  *mod_punishCampers;

cvar_t	*mod_levelsystem;
cvar_t	*sv_TurnpikeBlocker;
cvar_t	*sv_ghostOnRoundstart;

cvar_t	*mod_announceNoscopes;

cvar_t 	*sv_ent_dump;
cvar_t 	*sv_ent_dump_path;
cvar_t 	*sv_ent_load;
cvar_t 	*sv_ent_load_path;

cvar_t	*mod_jumpSpecAnnouncer;

cvar_t	*mod_turnpikeTeleporter;

cvar_t	*mod_battleroyale;

cvar_t	*mod_1v1arena;

cvar_t	*mod_fastSr8;
cvar_t	*mod_zombiemod;

//@Barbatos
#ifdef USE_AUTH
cvar_t	*sv_authServerIP;
cvar_t  *sv_auth_engine;
#endif

int ut_weapon_xor;

const char versionString[vMAX][10] = {
		"unknow",
		"4.2.023",
		"4.3",
		"4.3.1",
		"4.3.2",
		"4.3.3",
		"4.3.4"
};

char teamstring[4][5] =
{
		"free",
		"red",
		"blue",
		"spec"
};

char itemString[UT_ITEM_MAX][10] =
{
	[UT_ITEM_REDFLAG] = {"redflag"},
	[UT_ITEM_BLUEFLAG] = {"blueflag"},
	[UT_ITEM_VEST] = {"vest"},
	[UT_ITEM_NVG]  = {"nvg"},
	[UT_ITEM_SILENCER]  = {"silencer"},
	[UT_ITEM_LASER]  = {"laser"},
	[UT_ITEM_MEDKIT] = {"medkit"},
	[UT_ITEM_HELMET]  = {"helmet"},
	[UT_ITEM_AMMO]  = {"ammo"},
	[UT_ITEM_APR]  = {"apr"},
};

char hitlocationstring[HL_MAX][15] =
{
    "unknown",
    "head",
    "helmet",
    "torso",
    "vest",
    "arml",
    "armr",
    "groin",
    "butt",
    "legul",
    "legur",
    "legll",
    "leglr",
    "footl",
    "footr"
};

/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/

/////////////////////////////////////////////////////////////////////
// SV_LogPrintf
/////////////////////////////////////////////////////////////////////
void QDECL SV_LogPrintf(const char *fmt, ...) {

	va_list       argptr;
    fileHandle_t  file;
    fsMode_t      mode;
    char          *logfile;
    char          buffer[MAX_STRING_CHARS];
    int           min, tens, sec;
    int           logsync;

	// retrieve the logfile name
	logfile = Cvar_VariableString("g_log");
	if (!logfile[0]) {
		return;
	}

	// retrieve the writing mode
	logsync = Cvar_VariableIntegerValue("g_logSync");
	mode = logsync ? FS_APPEND_SYNC : FS_APPEND;

	// opening the log file
	FS_FOpenFileByMode(logfile, &file, mode);
	if (!file) {
		return;
	}

	// get current level time
	sec = sv.time / 1000;
	min = sec / 60;
	sec -= min * 60;
	tens = sec / 10;
	sec -= tens * 10;

	// prepend current level time
	Com_sprintf(buffer, sizeof(buffer), "%3i:%i%i ", min, tens, sec);

	// get the arguments
	va_start(argptr, fmt);
	vsprintf(buffer + 7, fmt, argptr);
	va_end(argptr);

	// write in the log file
	FS_Write(buffer, strlen(buffer), file);
	FS_FCloseFile(file);
}

/////////////////////////////////////////////////////////////////////
// SV_GetClientTeam
// Retrieve the team of the given client
/////////////////////////////////////////////////////////////////////
int SV_GetClientTeam(int cid) {
    playerState_t *ps;
    ps = SV_GameClientNum(cid);
    team_t team = *(int*)((void*)ps+gclientOffsets[getVersion()][OFFSET_TEAM]);
    return team;
}

/////////////////////////////////////////////////////////////////////
// SV_IsClientGhost
// Tells whether a client has cg_ghost activated
/////////////////////////////////////////////////////////////////////
qboolean SV_IsClientGhost(client_t *cl) {

    int ghost;

    // if we are not playing jump mode
    if (sv_gametype->integer != GT_JUMP) {
        return qfalse;
    }

    // get the cg_ghost value from the userinfo string
    ghost = atoi(Info_ValueForKey(cl->userinfo, "cg_ghost"));
    return ghost > 0 ? qtrue : qfalse;
}

/////////////////////////////////////////////////////////////////////
// SV_IsClientInPosition
// Tells whether a client is in the specified position
/////////////////////////////////////////////////////////////////////
qboolean SV_IsClientInPosition(int cid, float x, float y, float z, float xPlus, float yPlus, float zPlus) {
    playerState_t *ps;
    ps = SV_GameClientNum(cid);
    if (ps->origin[0] >= x-xPlus && ps->origin[0] <= x+xPlus &&
        ps->origin[1] >= y-yPlus && ps->origin[1] <= y+yPlus &&
        ps->origin[2] >= z-zPlus && ps->origin[2] <= z+zPlus) {
        return qtrue;
    }
    return qfalse;
}

/////////////////////////////////////////////////////////////////////
// SV_SetClientPosition
// Teleport a client to the specified position
/////////////////////////////////////////////////////////////////////
void SV_SetClientPosition(int cid, float x, float y, float z) {
    playerState_t *ps;
    ps = SV_GameClientNum(cid);
    ps->origin[0] = x;
    ps->origin[1] = y;
    ps->origin[2] = z;
}

/////////////////////////////////////////////////////////////////////
// SV_LoadPositionFromFile
// Load the client saved position from a file
/////////////////////////////////////////////////////////////////////
void SV_LoadPositionFromFile(client_t *cl, char *mapname) {

    fileHandle_t   file;
    char           buffer[MAX_STRING_CHARS];
    char           *guid;
    char           *qpath;
    int            len;

    // if we are not supposed to save the position on a file
    if (!mod_allowPosSaving->integer || !mod_persistentPositions->integer) {
        return;
    }

    // get the client guid from the userinfo string
    guid = Info_ValueForKey(cl->userinfo, "cl_guid");
    if (!guid || !guid[0]) { 
        return;
    }

    // open the position file
    qpath = va("positions/%s/%s.pos", mapname, guid);
    FS_FOpenFileByMode(qpath, &file, FS_READ);

    // if not valid
    if (!file) {
        return;
    }

    // read the file in the buffer
    len = FS_Read(buffer, sizeof(buffer), file);
    if (len > 0) {
        // copy back saved position
        sscanf(buffer, "%f,%f,%f,%f,%f,%f", &cl->cm.savedPosition[0],
                                            &cl->cm.savedPosition[1],
                                            &cl->cm.savedPosition[2],
                                            &cl->cm.savedPositionAngle[0],
                                            &cl->cm.savedPositionAngle[1],
                                            &cl->cm.savedPositionAngle[2]);
    }

    // close the file handle
    FS_FCloseFile(file);  
}

/////////////////////////////////////////////////////////////////////
// SV_SavePositionToFile
// Save the client position to a file
/////////////////////////////////////////////////////////////////////
void SV_SavePositionToFile(client_t *cl, char *mapname) {

    fileHandle_t   file;
    char           buffer[MAX_STRING_CHARS];
    char           *guid;
    char           *qpath;

    // if we are not supposed to save the position on a file
    if (!mod_allowPosSaving->integer || !mod_persistentPositions->integer) {
        return;
    }

    // get the client guid from the userinfo string
    guid = Info_ValueForKey(cl->userinfo, "cl_guid");
    if (!guid || !guid[0] || (!cl->cm.savedPosition[0] && !cl->cm.savedPosition[1] && !cl->cm.savedPosition[2])) { 
        return;
    }

    // open the position file
    qpath = va("positions/%s/%s.pos", mapname, guid);
    FS_FOpenFileByMode(qpath, &file, FS_WRITE);

    // if not valid
    if (!file) {
        return;
    }

    // compute the text to be stored in the .pos file
    Com_sprintf(buffer, sizeof(buffer), "%f,%f,%f,%f,%f,%f", cl->cm.savedPosition[0],
                                                             cl->cm.savedPosition[1],
                                                             cl->cm.savedPosition[2],
                                                             cl->cm.savedPositionAngle[0],
                                                             cl->cm.savedPositionAngle[1],
                                                             cl->cm.savedPositionAngle[2]);
    
    // write the client position and close
    FS_Write(buffer, strlen(buffer), file);
    FS_FCloseFile(file);  
}

/*
===============
SV_ExpandNewlines

Converts newlines to "\n" so a line prints nicer
===============
*/
char	*SV_ExpandNewlines( char *in ) {
	static	char	string[1024];
	int		l;

	l = 0;
	while ( *in && l < sizeof(string) - 3 ) {
		if ( *in == '\n' ) {
			string[l++] = '\\';
			string[l++] = 'n';
		} else {
			string[l++] = *in;
		}
		in++;
	}
	string[l] = 0;

	return string;
}

/*
======================
SV_ReplacePendingServerCommands

  This is ugly
======================
*/
int SV_ReplacePendingServerCommands( client_t *client, const char *cmd ) {
	int i, index, csnum1, csnum2;

	for ( i = client->reliableSent+1; i <= client->reliableSequence; i++ ) {
		index = i & ( MAX_RELIABLE_COMMANDS - 1 );
		//
		if ( !Q_strncmp(cmd, client->reliableCommands[ index ], strlen("cs")) ) {
			sscanf(cmd, "cs %i", &csnum1);
			sscanf(client->reliableCommands[ index ], "cs %i", &csnum2);
			if ( csnum1 == csnum2 ) {
				Q_strncpyz( client->reliableCommands[ index ], cmd, sizeof( client->reliableCommands[ index ] ) );
				/*
				if ( client->netchan.remoteAddress.type != NA_BOT ) {
					Com_Printf( "WARNING: client %i removed double pending config string %i: %s\n", client-svs.clients, csnum1, cmd );
				}
				*/
				return qtrue;
			}
		}
	}
	return qfalse;
}

/*
======================
SV_AddServerCommand

The given command will be transmitted to the client, and is guaranteed to
not have future snapshot_t executed before it is executed
======================
*/
void SV_AddServerCommand( client_t *client, const char *cmd ) {
	int		index, i;

	// this is very ugly but it's also a waste to for instance send multiple config string updates
	// for the same config string index in one snapshot
//	if ( SV_ReplacePendingServerCommands( client, cmd ) ) {
//		return;
//	}


	// do not send commands until the gamestate has been sent
	if( client->state < CS_PRIMED )
		return;

	client->reliableSequence++;
	// if we would be losing an old command that hasn't been acknowledged,
	// we must drop the connection
	// we check == instead of >= so a broadcast print added by SV_DropClient()
	// doesn't cause a recursive drop client
	if ( client->reliableSequence - client->reliableAcknowledge == MAX_RELIABLE_COMMANDS + 1 ) {
		Com_Printf( "===== pending server commands =====\n" );
		for ( i = client->reliableAcknowledge + 1 ; i <= client->reliableSequence ; i++ ) {
			Com_Printf( "cmd %5d: %s\n", i, client->reliableCommands[ i & (MAX_RELIABLE_COMMANDS-1) ] );
		}
		Com_Printf( "cmd %5d: %s\n", i, cmd );
		SV_DropClient( client, "Server command overflow" );
		return;
	}
	index = client->reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 );
	Q_strncpyz( client->reliableCommands[ index ], cmd, sizeof( client->reliableCommands[ index ] ) );
}

void MOD_modifiedAuth(char *auth, char *newauth, int cnum)
{
	char ccnum[3];
	char realauth[MAX_NAME_LENGTH*2];
	client_t *client;

	client = &svs.clients[cnum];
	sprintf(ccnum, "%d", cnum);

	//Save auth of the player
	Q_strncpyz(realauth,auth,MAX_NAME_LENGTH*2);
	//Get the format for the new auth
	Q_strncpyz(newauth, mod_auth->string, MAX_NAME_LENGTH*2);


	if(Q_stricmp(realauth, "---")==0 && mod_defaultauth->string[0])
	{
		//If a default auth is spotted is replaced with the new defaultauth
		Q_strncpyz(realauth, mod_defaultauth->string,MAX_NAME_LENGTH*2);
	}

	if(client->cm.authcl[0])
	{
		//If a player has a custum auth it is changed
		Q_strncpyz(realauth, client->cm.authcl, MAX_NAME_LENGTH*2);
	}

	//Apply format on the new auth
	str_replace(newauth, "%s",realauth, MAX_NAME_LENGTH*2);
	str_replace(newauth, "%d",ccnum, MAX_NAME_LENGTH*2);

	//Clean newauth
	Q_strncpyz(newauth, SV_CleanName(newauth), MAX_NAME_LENGTH*2);
}

void MOD_parseScore(char *cmd)
{
	int j = 0,cnum;
	byte        csmessage[MAX_MSGLEN] = {0};
	char newauth[MAX_NAME_LENGTH*2]; //Replace string is generated here

	char* token = strtok(cmd, " ");
	//Recursive loop into each args
	while(token) {
		//This arg should be the cnum of the client
		if((j-4)%13==0)
		{
			cnum = atoi (token);
		}
		//This arg should be the auth of a player
		if((j-16)%13==0 && j!=3)
		{
			MOD_modifiedAuth(token, newauth, cnum);

			//Append to the command
			strcat((char *)csmessage, va("%s ", newauth));

		}else // If its not the auth just save as it is
		{
			strcat((char *)csmessage, va("%s ", token));
		}
		j++;
		token=strtok(NULL, " ");
	}
	strcpy(cmd, (char *)csmessage);
}
void MOD_parseScoresAndDouble(char *cmd)
{
	int j = 0, cnum;
	byte csmessage[MAX_MSGLEN] = {0};
	char newauth[MAX_NAME_LENGTH*2];

	char* token = strtok(cmd, " ");
	while(token)
	{
		if((j-1)%13==0)
		{
			cnum = atoi(token);
		}
		if((j-13)%13==0 && j!=0)
		{
			MOD_modifiedAuth(token, newauth, cnum);

			strcat((char *)csmessage, va("%s ", newauth));
		}else
		{
			strcat((char *)csmessage, va("%s ", token));
		}
		j++;
		token = strtok(NULL, " ");
	}
	strcpy(cmd, (char *)csmessage);
}

/*
=================
SV_SendServerCommand

Sends a reliable command string to be interpreted by
the client game module: "cp", "print", "chat", etc
A NULL client will broadcast to all clients
=================
*/
void QDECL SV_SendServerCommand(client_t *cl, const char *fmt, ...) {
	va_list		argptr;
	byte		message[MAX_MSGLEN];
	client_t	*client;
	int			j;

	va_start (argptr,fmt);
	Q_vsnprintf ((char *)message, sizeof(message), fmt,argptr);
	va_end (argptr);

	// Keep track of the clients location for jump use
	 if ((Q_strncmp((char *)message, "location", 8) == 0) && (sv_gametype->integer == GT_JUMP) && (mod_jumpSpecAnnouncer->integer)) {

		// Update the clients location every second
		unsigned int time = Com_Milliseconds();
		if (time - cl->lastLocationTime < 1000) {
			return;
		}

		char workString[256];
		char actualClientLocation[256];

		// Save the clients location in the clientState
		sprintf(workString, "%s: %s", cl->name, (char *)message);
		int jumplength = strlen(cl->name) + 11;
		strcpy(actualClientLocation, workString + jumplength);
		cl->location = sv.configstrings[atoi(actualClientLocation) + 640];

		// Set the last location time
		cl->lastLocationTime = time;

		// check if the client has spectators
		if (cl->spectators[0] != '\0') {
			char actualspecs[256];
			Q_strncpyz(actualspecs, cl->spectators, 256);
			Cmd_ExecuteString(va("location %s \"^3%s ^7[^8%s^7] | ^9Spectators: ^8%s\" 0 1\n", cl->name, cl->name, cl->location, actualspecs));
			return;
		} else {
			// Client has no spectators, send the default string
			Cmd_ExecuteString(va("location %s \"^3%s ^7[^8%s^7] | ^9Spectators: ^8Nobody is watching you.\" 0 1\n", cl->name, cl->name, cl->location));
		}
	 }

	// Location is locked
	if((Q_strncmp((char *)message, "location", 8) == 0) && cl->cm.locationLocked)
		return;

	// Grabbing server mutes
    if (cl != NULL) {
        if (!strcmp((char *) message, "print \"The admin muted you: you cannot talk\"\n")) {
            cl->muted = qtrue;
        }
        else if (!strcmp((char *) message, "print \"The admin unmuted you\"\n")) {
            cl->muted = qfalse;
        }
        else if (!strcmp((char *) message, "print \"You have been unmuted\"\n")) {
            cl->muted = qfalse;
        }
	}

	// Modify scoreboard for auth change
    if (Q_strncmp((char *)message, "scores ", 7) == 0) {
        MOD_parseScore((char *)message);
    }
    if (Q_strncmp((char *)message, "scoress ", 8) == 0 || Q_strncmp((char *)message, "scoresd ", 8) == 0) {
        MOD_parseScoresAndDouble((char *)message);
    }

	// Fix to http://aluigi.altervista.org/adv/q3msgboom-adv.txt
	// The actual cause of the bug is probably further downstream
	// and should maybe be addressed later, but this certainly
	// fixes the problem for now
	if ( strlen ((char *)message) > 1022 ) {
		return;
	}

	if ( cl != NULL ) {
		SV_AddServerCommand( cl, (char *)message );
		return;
	}

	// hack to echo broadcast prints to console
	if ( com_dedicated->integer && !strncmp( (char *)message, "print", 5) ) {
		Com_Printf ("broadcast: %s\n", SV_ExpandNewlines((char *)message) );
	}

	// send the data to all relevent clients
	for (j = 0, client = svs.clients; j < sv_maxclients->integer ; j++, client++) {
		SV_AddServerCommand( client, (char *)message );
	}
}


/*
==============================================================================

MASTER SERVER FUNCTIONS

==============================================================================
*/

/*
================
SV_MasterHeartbeat

Send a message to the masters every few minutes to
let it know we are alive, and log information.
We will also have a heartbeat sent when a server
changes from empty to non-empty, and full to non-full,
but not on every player enter or exit.
================
*/
#define	HEARTBEAT_MSEC	300*1000
#define	HEARTBEAT_GAME	"QuakeArena-1"
void SV_MasterHeartbeat( void ) {
	static netadr_t	adr[MAX_MASTER_SERVERS];
	int			i;

	// "dedicated 1" is for lan play, "dedicated 2" is for inet public play
	if ( !com_dedicated || com_dedicated->integer != 2 ) {
		return;		// only dedicated servers send heartbeats
	}

	// do not send a heartbeat if hidden server
	if (mod_hideServer->integer)
	    return;

	// if not time yet, don't send anything
	if ( svs.time < svs.nextHeartbeatTime ) {
		return;
	}
	svs.nextHeartbeatTime = svs.time + HEARTBEAT_MSEC;


	#ifdef USE_AUTH
	VM_Call( gvm, GAME_AUTHSERVER_HEARTBEAT );
	#endif
	
	// send to group masters
	for ( i = 0 ; i < MAX_MASTER_SERVERS ; i++ ) {
		if ( !sv_master[i]->string[0] ) {
			continue;
		}

		// see if we haven't already resolved the name
		// resolving usually causes hitches on win95, so only
		// do it when needed
		if ( sv_master[i]->modified ) {
			sv_master[i]->modified = qfalse;
	
			Com_Printf( "Resolving %s\n", sv_master[i]->string );
			if ( !NET_StringToAdr( sv_master[i]->string, &adr[i] ) ) {
				// if the address failed to resolve, clear it
				// so we don't take repeated dns hits
				Com_Printf( "Couldn't resolve address: %s\n", sv_master[i]->string );
				Cvar_Set( sv_master[i]->name, "" );
				sv_master[i]->modified = qfalse;
				continue;
			}
			if ( !strchr( sv_master[i]->string, ':' ) ) {
				adr[i].port = BigShort( PORT_MASTER );
			}
			Com_Printf( "%s resolved to %i.%i.%i.%i:%i\n", sv_master[i]->string,
				adr[i].ip[0], adr[i].ip[1], adr[i].ip[2], adr[i].ip[3],
				BigShort( adr[i].port ) );
		}


		Com_Printf ("Sending heartbeat to %s\n", sv_master[i]->string );
		// this command should be changed if the server info / status format
		// ever incompatably changes
		NET_OutOfBandPrint( NS_SERVER, adr[i], "heartbeat %s\n", HEARTBEAT_GAME );
	}
}

/*
=================
SV_MasterShutdown

Informs all masters that this server is going down
=================
*/
void SV_MasterShutdown( void ) {
	// send a hearbeat right now
	svs.nextHeartbeatTime = -9999;
	SV_MasterHeartbeat();

	// send it again to minimize chance of drops
	svs.nextHeartbeatTime = -9999;
	SV_MasterHeartbeat();

	// when the master tries to poll the server, it won't respond, so
	// it will be removed from the list
	
	#ifdef USE_AUTH
	VM_Call( gvm, GAME_AUTHSERVER_SHUTDOWN );
	#endif
}


/*
==============================================================================

CONNECTIONLESS COMMANDS

==============================================================================
*/

/*
================
SVC_Status

Responds with all the info that qplug or qspy can see about the server
and all connected players.  Used for getting detailed information after
the simple info query.
================
*/
void SVC_Status( netadr_t from ) {
	char	player[1024];
	char	status[MAX_MSGLEN];
	int		i;
	client_t	*cl;
	playerState_t	*ps;
	int		statusLength;
	int		playerLength;
	char	infostring[MAX_INFO_STRING];

	// ignore if we are in single player
	if ( Cvar_VariableValue( "g_gametype" ) == GT_SINGLE_PLAYER ) {
		return;
	}

	// ignore if hidden server
	if (mod_hideServer->integer)
        return;

	strcpy( infostring, Cvar_InfoString( CVAR_SERVERINFO ) );

	// echo back the parameter to status. so master servers can use it as a challenge
	// to prevent timed spoofed reply packets that add ghost servers
	Info_SetValueForKey( infostring, "challenge", Cmd_Argv(1) );

	status[0] = 0;
	statusLength = 0;

	for (i=0 ; i < sv_maxclients->integer ; i++) {
		cl = &svs.clients[i];
		if ( cl->state >= CS_CONNECTED ) {
			ps = SV_GameClientNum( i );
			Com_sprintf (player, sizeof(player), "%i %i \"%s\"\n",
				ps->persistant[PERS_SCORE], cl->ping, cl->name);
			playerLength = strlen(player);
			if (statusLength + playerLength >= sizeof(status) ) {
				break;		// can't hold any more
			}
			strcpy (status + statusLength, player);
			statusLength += playerLength;
		}
	}

	NET_OutOfBandPrint( NS_SERVER, from, "statusResponse\n%s\n%s", infostring, status );
}

/*
================
SVC_Info

Responds with a short info message that should be enough to determine
if a user is interested in a server to do a full status
================
*/
void SVC_Info( netadr_t from ) {
	int		i, count, bots;
	char	*gamedir;
	char	infostring[MAX_INFO_STRING];

	// ignore if we are in single player
	if ( Cvar_VariableValue( "g_gametype" ) == GT_SINGLE_PLAYER || Cvar_VariableValue("ui_singlePlayerActive")) {
		return;
	}

	// ignore if hidden server
	if (mod_hideServer->integer)
	    return;

	/*
	 * Check whether Cmd_Argv(1) has a sane length. This was not done in the original Quake3 version which led
	 * to the Infostring bug discovered by Luigi Auriemma. See http://aluigi.altervista.org/ for the advisory.
	 */

	// A maximum challenge length of 128 should be more than plenty.
	if (strlen(Cmd_Argv(1)) > 128)
		return;

	// don't count privateclients
	count = 0;
	bots = 0;
	for ( i = sv_privateClients->integer ; i < sv_maxclients->integer ; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			count++;

			if (svs.clients[i].netchan.remoteAddress.type == NA_BOT) {
				if (mod_botsCount->integer == 1) {
					bots++;
				} else {
					count--;
				}
			}
		}
	}

	infostring[0] = 0;

	// echo back the parameter to status. so servers can use it as a challenge
	// to prevent timed spoofed reply packets that add ghost servers
	Info_SetValueForKey( infostring, "challenge", Cmd_Argv(1) );

	Info_SetValueForKey( infostring, "protocol", va("%i", PROTOCOL_VERSION) );
	Info_SetValueForKey( infostring, "hostname", sv_hostname->string );

	char mapname[MAX_QPATH];
	Q_strncpyz(mapname, sv_mapname->string, sizeof(mapname));

	if (mod_cleanMapPrefixes->integer) {
		char *loc;
		char *prefix42 = "ut42_";
		char *prefix43 = "ut43_";

		if ((loc = strstr(mapname, prefix42)) != NULL) {
			strcpy(loc, loc + strlen(prefix42));
		} else if ((loc = strstr(mapname, prefix43)) != NULL) {
			strcpy(loc, loc + strlen(prefix43));
		}

		// Make first letter capital if prefixes were removed
		if (mapname != sv_mapname->string) {
			mapname[0] = toupper(mapname[0]);
		}
	}

	if(!strcmp(mod_mapName->string, ""))
		Info_SetValueForKey( infostring, "mapname", va("^%i%s", mod_mapColour->integer, mapname) );
	else
		Info_SetValueForKey( infostring, "mapname", va("^%i%s", mod_mapColour->integer, mod_mapName->string) );

	// If playerCount is positive, the number will be added to the real player count
	// If playerCount is negative a random number between playerCount and 1 will be added
	if(0 < mod_playerCount->integer) {
		count += mod_playerCount->integer;
	}
	else if(0 > mod_playerCount->integer)
	{
		int rand, seed;
		rand = 0;
		seed = Com_Milliseconds();
		while (1 > rand) {
		    rand = Q_rand(&seed) % mod_playerCount->integer +1;
	    }
		count += rand;
	}
	if (count > sv_maxclients->integer) {
		count = sv_maxclients->integer;
	}

	Info_SetValueForKey( infostring, "clients", va("%i", count) );
	Info_SetValueForKey( infostring, "bots", va("%i", bots) );
	Info_SetValueForKey( infostring, "sv_maxclients", va("%i", sv_maxclients->integer - sv_privateClients->integer ) );

	if(!strcmp(mod_gameType->string, ""))
		Info_SetValueForKey( infostring, "gametype", va("%i", sv_gametype->integer ) );
	else
		Info_SetValueForKey( infostring, "gametype", va("%i", mod_gameType->integer ) );

	Info_SetValueForKey( infostring, "pure", va("%i", sv_pure->integer ) );
	
	//@Barbatos
	#ifdef USE_AUTH
	Info_SetValueForKey( infostring, "auth", Cvar_VariableString("auth") );
	#endif

	//@Barbatos: if it's a passworded server, let the client know (for the server browser)
	if(Cvar_VariableValue("g_needpass") == 1)
		Info_SetValueForKey( infostring, "password", va("%i", 1));
		
	if( sv_minPing->integer ) {
		Info_SetValueForKey( infostring, "minPing", va("%i", sv_minPing->integer) );
	}
	if( sv_maxPing->integer ) {
		Info_SetValueForKey( infostring, "maxPing", va("%i", sv_maxPing->integer) );
	}
	gamedir = Cvar_VariableString( "fs_game" );
	if( *gamedir ) {
		Info_SetValueForKey( infostring, "game", gamedir );
	}

	Info_SetValueForKey(infostring, "modversion", Cvar_VariableString("g_modversion"));

	NET_OutOfBandPrint( NS_SERVER, from, "infoResponse\n%s", infostring );
}

/*
================
SVC_FlushRedirect

================
*/
void SV_FlushRedirect( char *outputbuf ) {
	NET_OutOfBandPrint( NS_SERVER, svs.redirectAddress, "print\n%s", outputbuf );
}

/*
===============
SVC_RconRecoveryRemoteCommand

An rcon packet arrived from the network.
Shift down the remaining args
Redirect all printfs
===============
*/
void SVC_RconRecoveryRemoteCommand( netadr_t from, msg_t *msg ) {
	qboolean	valid;
	unsigned int time;
	// TTimo - scaled down to accumulate, but not overflow anything network wise, print wise etc.
	// (OOB messages are the bottleneck here)
#define SV_OUTPUTBUF_LENGTH (1024 - 16)
	char		sv_outputbuf[SV_OUTPUTBUF_LENGTH];
	static unsigned int lasttime = 0;

	// TTimo - https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=534
	time = Com_Milliseconds();
	
	if ( !strlen( sv_rconRecoveryPassword->string ) || strcmp (Cmd_Argv(1), sv_rconRecoveryPassword->string) )
	{
		// MaJ - If the rconpassword is bad and one just happned recently, don't spam the log file, just die.
		if ( (unsigned)( time - lasttime ) < 600u )
			return;
			
		valid = qfalse;
		Com_Printf ("Bad rcon recovery from %s:\n%s\n", NET_AdrToString (from), Cmd_Argv(2) );
	} else {
		// Same here.. don't limit good rcon commands
		valid = qtrue;
		Com_Printf ("Rcon recovery from %s:\n%s\n", NET_AdrToString (from), Cmd_Argv(2) );
	}
	lasttime = time;

	// start redirecting all print outputs to the packet
	svs.redirectAddress = from;
	Com_BeginRedirect (sv_outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect);

	if ( !strlen( sv_rconPassword->string ) ) {
		Com_Printf ("No rcon recovery password set on the server.\n");
	} else if ( !valid ) {
		Com_Printf ("Bad rcon recovery password.\n");
	} else {
		Com_Printf ("rconPassword %s\n" , sv_rconPassword->string );
	}

	Com_EndRedirect ();
}

/*
===============
SVC_RemoteCommand

An rcon packet arrived from the network.
Shift down the remaining args
Redirect all printfs
===============
*/
void SVC_RemoteCommand( netadr_t from, msg_t *msg ) {
	qboolean	valid;
	unsigned int time;
	char		remaining[1024];
	netadr_t	allowedSpamIPAdress;
	// TTimo - scaled down to accumulate, but not overflow anything network wise, print wise etc.
	// (OOB messages are the bottleneck here)
#define SV_OUTPUTBUF_LENGTH (1024 - 16)
	char		sv_outputbuf[SV_OUTPUTBUF_LENGTH];
	static unsigned int lasttime = 0;
	char *cmd_aux;

	// TTimo - https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=534
	time = Com_Milliseconds();
	
	
	NET_StringToAdr( sv_rconAllowedSpamIP->string , &allowedSpamIPAdress);
	
	
	if ( !strlen( sv_rconPassword->string ) || strcmp (Cmd_Argv(1), sv_rconPassword->string) )
	{
		// let's the sv_rconAllowedSpamIP do spam rcon
		if ( ( !strlen( sv_rconAllowedSpamIP->string ) || !NET_CompareBaseAdr( from , allowedSpamIPAdress ) ) && !NET_IsLocalAddress(from) ){
			// MaJ - If the rconpassword is bad and one just happned recently, don't spam the log file, just die.
			if ( (unsigned)( time - lasttime ) < 600u )
				return;
		}
		
		valid = qfalse;
		Com_Printf ("Bad rcon from %s:\n%s\n", NET_AdrToString (from), Cmd_Argv(2) );
	} else {
		
		// Why limit a good rcon command? Just let bots run at max speed.. ffs
		valid = qtrue;
		Com_Printf ("Rcon from %s:\n%s\n", NET_AdrToString (from), Cmd_Argv(2) );
	}
	lasttime = time;

	// start redirecting all print outputs to the packet
	svs.redirectAddress = from;
	Com_BeginRedirect (sv_outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect);

	if ( !strlen( sv_rconPassword->string ) ) {
		Com_Printf ("No rconpassword set on the server.\n");
	} else if ( !valid ) {
		Com_Printf ("%s\n", mod_badRconMessage->string);
	} else {
		remaining[0] = 0;
		
		// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=543
		// get the command directly, "rcon <pass> <command>" to avoid quoting issues
		// extract the command by walking
		// since the cmd formatting can fuckup (amount of spaces), using a dumb step by step parsing
		cmd_aux = Cmd_Cmd();
		cmd_aux+=4;
		while(cmd_aux[0]==' ')
			cmd_aux++;
		while(cmd_aux[0] && cmd_aux[0]!=' ') // password
			cmd_aux++;
		while(cmd_aux[0]==' ')
			cmd_aux++;
		
		Q_strcat( remaining, sizeof(remaining), cmd_aux);
		
		Cmd_ExecuteString (remaining);

	}

	Com_EndRedirect ();
}

/*
=================
SV_CheckDRDoS

DRDoS stands for "Distributed Reflected Denial of Service".
See here: http://www.lemuria.org/security/application-drdos.html

Returns qfalse if we're good.  qtrue return value means we need to block.
If the address isn't NA_IP, it's automatically denied.
=================
*/
qboolean SV_CheckDRDoS(netadr_t from)
{
	int		i;
	int		globalCount;
	int		specificCount;
	receipt_t	*receipt;
	netadr_t	exactFrom;
	int		oldest;
	int		oldestTime;
	static int	lastGlobalLogTime = 0;
	static int	lastSpecificLogTime = 0;

	// Usually the network is smart enough to not allow incoming UDP packets
	// with a source address being a spoofed LAN address.  Even if that's not
	// the case, sending packets to other hosts in the LAN is not a big deal.
	// NA_LOOPBACK qualifies as a LAN address.
	if (Sys_IsLANAddress(from)) { return qfalse; }

	exactFrom = from;
	
	if (from.type == NA_IP) {
		from.ip[3] = 0; // xx.xx.xx.0
	}
	else {
		// So we got a connectionless packet but it's not IPv4, so
		// what is it?  I don't care, it doesn't matter, we'll just block it.
		// This probably won't even happen.
		return qtrue;
	}

	// Count receipts in last 2 seconds.
	globalCount = 0;
	specificCount = 0;
	receipt = &svs.infoReceipts[0];
	oldest = 0;
	oldestTime = 0x7fffffff;
	for (i = 0; i < MAX_INFO_RECEIPTS; i++, receipt++) {
		if (receipt->time + 2000 > svs.time) {
			if (receipt->time) {
				// When the server starts, all receipt times are at zero.  Furthermore,
				// svs.time is close to zero.  We check that the receipt time is already
				// set so that during the first two seconds after server starts, queries
				// from the master servers don't get ignored.  As a consequence a potentially
				// unlimited number of getinfo+getstatus responses may be sent during the
				// first frame of a server's life.
				globalCount++;
			}
			if (NET_CompareBaseAdr(from, receipt->adr)) {
				specificCount++;
			}
		}
		if (receipt->time < oldestTime) {
			oldestTime = receipt->time;
			oldest = i;
		}
	}

	if (globalCount == MAX_INFO_RECEIPTS) { // All receipts happened in last 2 seconds.
		if (lastGlobalLogTime + 1000 <= svs.time){ // Limit one log every second.
			Com_Printf("Detected flood of getinfo/getstatus connectionless packets\n");
			lastGlobalLogTime = svs.time;
		}
		return qtrue;
	}
	if (specificCount >= 3) { // Already sent 3 to this IP in last 2 seconds.
		if (lastSpecificLogTime + 1000 <= svs.time) { // Limit one log every second.
			Com_DPrintf("Possible DRDoS attack to address %i.%i.%i.%i, ignoring getinfo/getstatus connectionless packet\n",
					exactFrom.ip[0], exactFrom.ip[1], exactFrom.ip[2], exactFrom.ip[3]);
			lastSpecificLogTime = svs.time;
		}
		return qtrue;
	}

	receipt = &svs.infoReceipts[oldest];
	receipt->adr = from;
	receipt->time = svs.time;
	return qfalse;
}

/*
=================
SV_ConnectionlessPacket

A connectionless packet has four leading 0xff
characters to distinguish it from a game channel.
Clients that are in the game can still send
connectionless packets.
=================
*/
void SV_ConnectionlessPacket( netadr_t from, msg_t *msg ) {
	char	*s;
	char	*c;
	#ifdef USE_AUTH
	netadr_t	authServerIP;
	#endif

	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg );		// skip the -1 marker

	if (!Q_strncmp("connect", (char *) &msg->data[4], 7)) {
		Huff_Decompress(msg, 12);
	}

	s = MSG_ReadStringLine( msg );
	Cmd_TokenizeString( s );

	c = Cmd_Argv(0);
	Com_DPrintf ("SV packet %s : %s\n", NET_AdrToString(from), c);

	if (!Q_stricmp(c, "getstatus")) {
		if (SV_CheckDRDoS(from)) { return; }
		SVC_Status( from  );
  } else if (!Q_stricmp(c, "getinfo")) {
		if (SV_CheckDRDoS(from)) { return; }
		SVC_Info( from );
	} else if (!Q_stricmp(c, "getchallenge")) {
		SV_GetChallenge( from );
	} else if (!Q_stricmp(c, "connect")) {
		SV_DirectConnect( from );
	} else if (!Q_stricmp(c, "ipAuthorize")) {
		SV_AuthorizeIpPacket( from );
	}
	#ifdef USE_AUTH
	// @Barbatos @Kalish
	else if ( (!Q_stricmp(c, "AUTH:SV")))
	{
		NET_StringToAdr(sv_authServerIP->string, &authServerIP);
		
		if ( !NET_CompareBaseAdr( from, authServerIP ) ) {
			Com_Printf( "AUTH not from the Auth Server\n" );
			return;
		}
		VM_Call(gvm, GAME_AUTHSERVER_PACKET);
	}
	#endif
	
	else if (!Q_stricmp(c, "rcon")) {
		SVC_RemoteCommand( from, msg );
	}else if (!Q_stricmp(c, "rconRecovery")) {
		SVC_RconRecoveryRemoteCommand( from, msg );
	} else if (!Q_stricmp(c, "disconnect")) {
		// if a client starts up a local server, we may see some spurious
		// server disconnect messages when their new server sees our final
		// sequenced messages to the old client
	} else {
		Com_DPrintf ("bad connectionless packet from %s:\n%s\n"
		, NET_AdrToString (from), s);
	}
}

//============================================================================

/*
=================
SV_ReadPackets
=================
*/
void SV_PacketEvent( netadr_t from, msg_t *msg ) {
	int			i;
	client_t	*cl;
	int			qport;

	// check for connectionless packet (0xffffffff) first
	if ( msg->cursize >= 4 && *(int *)msg->data == -1) {
		SV_ConnectionlessPacket( from, msg );
		return;
	}

	// read the qport out of the message so we can fix up
	// stupid address translating routers
	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg );				// sequence number
	qport = MSG_ReadShort( msg ) & 0xffff;

	// find which client the message is from
	for (i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++) {
		if (cl->state == CS_FREE) {
			continue;
		}
		if ( !NET_CompareBaseAdr( from, cl->netchan.remoteAddress ) ) {
			continue;
		}
		// it is possible to have multiple clients from a single IP
		// address, so they are differentiated by the qport variable
		if (cl->netchan.qport != qport) {
			continue;
		}

		// the IP port can't be used to differentiate them, because
		// some address translating routers periodically change UDP
		// port assignments
		if (cl->netchan.remoteAddress.port != from.port) {
			Com_Printf( "SV_PacketEvent: fixing up a translated port\n" );
			cl->netchan.remoteAddress.port = from.port;
		}

		// make sure it is a valid, in sequence packet
		if (SV_Netchan_Process(cl, msg)) {
			// zombie clients still need to do the Netchan_Process
			// to make sure they don't need to retransmit the final
			// reliable message, but they don't do any other processing
			if (cl->state != CS_ZOMBIE) {
				cl->lastPacketTime = svs.time;	// don't timeout
				SV_ExecuteClientMessage( cl, msg );
			}
		}
		return;
	}
	
	// if we received a sequenced packet from an address we don't recognize,
	// send an out of band disconnect packet to it
	NET_OutOfBandPrint( NS_SERVER, from, "disconnect" );
}


/*
===================
SV_CalcPings

Updates the cl->ping variables
===================
*/
static void SV_CalcPings( void ) {
	int			i, j;
	client_t	*cl;
    int			total, count;
	int			delta;
	playerState_t	*ps;

	for (i=0 ; i < sv_maxclients->integer ; i++) {
		cl = &svs.clients[i];
		if ( cl->state != CS_ACTIVE ) {
			cl->ping = 999;
			continue;
		}
		if ( !cl->gentity ) {
			cl->ping = 999;
			continue;
		}
		if ( cl->gentity->r.svFlags & SVF_BOT ) {
			cl->ping = 0;
			continue;
		}

		total = 0;
		count = 0;

        // Treat unacknowledged frames when there is a later acknowledged frame as
        // acknowledged at the same time as the later frame, instead of ignoring.
        // This should more accurately account for ping increases in certain conditions such as packet loss.
        int current_frame = cl->netchan.outgoingSequence - 1;
        int current_ack_time = -1;
        for ( j = 0 ; j < PACKET_BACKUP && current_frame > 0 ; j++ ) {
            // Read frames backwards from latest to earliest
            clientSnapshot_t *frame = &cl->frames[(current_frame--) & PACKET_MASK];

            // Use the earliest valid ack time as current
            if(frame->messageAcked != -1 && (current_ack_time == -1 || frame->messageAcked < current_ack_time)) {
                current_ack_time = frame->messageAcked;
            }

            if(current_ack_time != -1) {
                delta = current_ack_time - frame->messageSent;
                count++;
                total += delta;
            }
        }

#if 0
        for ( j = 0 ; j < PACKET_BACKUP ; j++ ) {
            if ( cl->frames[j].messageAcked == -1 ) {  // <= 0
                continue;
            }
            delta = cl->frames[j].messageAcked - cl->frames[j].messageSent;
            count++;
            total += delta;
        }
#endif

		if (!count) {
			cl->ping = 999;
		} else {
            cl->ping = total/count;
			if ( cl->ping > 999 ) {
				cl->ping = 999;
			}
            if ( cl->ping < 1 ) {
                cl->ping = 1; // minimum ping for humans
            }
		}

		// let the game dll know about the ping
		ps = SV_GameClientNum( i );
		ps->ping = cl->ping;
	}
}

/*
==================
SV_CheckTimeouts

If a packet has not been received from a client for timeout->integer
seconds, drop the conneciton.  Server time is used instead of
realtime to avoid dropping the local client while debugging.

When a client is normally dropped, the client_t goes into a zombie state
for a few seconds to make sure any final reliable message gets resent
if necessary
==================
*/
void SV_CheckTimeouts( void ) {
	int		i;
	client_t	*cl;
	int			droppoint;
	int			zombiepoint;

	droppoint = svs.time - 1000 * sv_timeout->integer;
	zombiepoint = svs.time - 1000 * sv_zombietime->integer;

	for (i=0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++) {
		// message times may be wrong across a changelevel
		if (cl->lastPacketTime > svs.time) {
			cl->lastPacketTime = svs.time;
		}

		if (cl->state == CS_ZOMBIE
		&& cl->lastPacketTime < zombiepoint) {
			// using the client id cause the cl->name is empty at this point
			Com_DPrintf( "Going from CS_ZOMBIE to CS_FREE for client %d\n", i );
			cl->state = CS_FREE;	// can now be reused
			continue;
		}
		if ( cl->state >= CS_CONNECTED && cl->lastPacketTime < droppoint) {
			// wait several frames so a debugger session doesn't
			// cause a timeout
			if ( ++cl->timeoutCount > 5 ) {
				SV_DropClient (cl, "timed out");
				cl->state = CS_FREE;	// don't bother with zombie state
			}
		} else {
			cl->timeoutCount = 0;
		}
	}
	
}


/*
==================
SV_CheckPaused
==================
*/
qboolean SV_CheckPaused( void ) {
	int		count;
	client_t	*cl;
	int		i;

	if ( !cl_paused->integer ) {
		return qfalse;
	}

	// only pause if there is just a single client connected
	count = 0;
	for (i=0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++) {
		if ( cl->state >= CS_CONNECTED && cl->netchan.remoteAddress.type != NA_BOT ) {
			count++;
		}
	}

	if ( count > 1 ) {
		// don't pause
		if (sv_paused->integer)
			Cvar_Set("sv_paused", "0");
		return qfalse;
	}

	if (!sv_paused->integer)
		Cvar_Set("sv_paused", "1");
	return qtrue;
}

/*
==================
SV_Frame

Player movement occurs as a result of packet events, which
happen before SV_Frame is called
==================
*/
void SV_Frame( int msec ) {
	int		frameMsec;
	int		startTime;

	// the menu kills the server with this cvar
	if ( sv_killserver->integer ) {
		SV_Shutdown ("Server was killed");
		Cvar_Set( "sv_killserver", "0" );
		return;
	}

	if (!com_sv_running->integer)
	{
		if(com_dedicated->integer)
		{
			// Block indefinitely until something interesting happens
			// on STDIN.
			NET_Sleep(-1);
		}
		
		return;
	}

	// allow pause if only the local client is connected
	if ( SV_CheckPaused() ) {
		return;
	}

	// if it isn't time for the next frame, do nothing
	if ( sv_fps->integer < 1 ) {
		Cvar_Set( "sv_fps", "10" );
	}

	frameMsec = 1000 / sv_fps->integer * com_timescale->value;
	// don't let it scale below 1ms
	if(frameMsec < 1)
	{
		Cvar_Set("timescale", va("%f", sv_fps->integer / 1000.0f));
		frameMsec = 1;
	}

	sv.timeResidual += msec;

	if (!com_dedicated->integer) SV_BotFrame (sv.time + sv.timeResidual);

	if ( com_dedicated->integer && sv.timeResidual < frameMsec ) {
		// NET_Sleep will give the OS time slices until either get a packet
		// or time enough for a server frame has gone by
		NET_Sleep(frameMsec - sv.timeResidual);
		return;
	}

	// if time is about to hit the 32nd bit, kick all clients
	// and clear sv.time, rather
	// than checking for negative time wraparound everywhere.
	// 2giga-milliseconds = 23 days, so it won't be too often
	if ( svs.time > 0x70000000 ) {
		SV_Shutdown( "Restarting server due to time wrapping" );
		Cbuf_AddText( va( "map %s\n", Cvar_VariableString( "mapname" ) ) );
		return;
	}
	// this can happen considerably earlier when lots of clients play and the map doesn't change
	if ( svs.nextSnapshotEntities >= 0x7FFFFFFE - svs.numSnapshotEntities ) {
		SV_Shutdown( "Restarting server due to numSnapshotEntities wrapping" );
		Cbuf_AddText( va( "map %s\n", Cvar_VariableString( "mapname" ) ) );
		return;
	}

	if( sv.restartTime && sv.time >= sv.restartTime ) {
		sv.restartTime = 0;
		Cbuf_AddText( "map_restart 0\n" );
		return;
	}

	// update infostrings if anything has been changed
	if ( cvar_modifiedFlags & CVAR_SERVERINFO ) {
		SV_SetConfigstring( CS_SERVERINFO, Cvar_InfoString( CVAR_SERVERINFO ) );
		cvar_modifiedFlags &= ~CVAR_SERVERINFO;
	}
	if ( cvar_modifiedFlags & CVAR_SYSTEMINFO ) {
		SV_SetConfigstring( CS_SYSTEMINFO, Cvar_InfoString_Big( CVAR_SYSTEMINFO ) );
		cvar_modifiedFlags &= ~CVAR_SYSTEMINFO;
	}

	if ( com_speeds->integer ) {
		startTime = Sys_Milliseconds ();
	} else {
		startTime = 0;	// quite a compiler warning
	}

	// update ping based on the all received frames
	SV_CalcPings();

	if (com_dedicated->integer) SV_BotFrame (sv.time);

	// run the game simulation in chunks
	while ( sv.timeResidual >= frameMsec ) {
		sv.timeResidual -= frameMsec;
		svs.time += frameMsec;
		sv.time += frameMsec;

		// let everything in the world think and move
		VM_Call (gvm, GAME_RUN_FRAME, sv.time);
	}

	if ( com_speeds->integer ) {
		time_game = Sys_Milliseconds () - startTime;
	}

	// check timeouts
	SV_CheckTimeouts();
	
	// check user info buffer thingy
	SV_CheckClientUserinfoTimer();

	// send messages back to the clients
	SV_SendClientMessages();

	// send a heartbeat to the master if needed
	SV_MasterHeartbeat();
}

//============================================================================

