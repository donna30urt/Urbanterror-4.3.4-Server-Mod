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
// sv_game.c -- interface to the game dll

#include "server.h"

#include "../botlib/botlib.h"

botlib_export_t	*botlib_export;

char (*weaponString)[15];

char weaponstring42[(UT_WP_NUM_WEAPONS+3)][15] =
{
	"none",
	"knife",
	"beretta",
	"deagle",
	"spas12",
	"mp5k",
	"ump45",
	"hk69",
	"lr",
	"g36",
	"psg1",
	"he",
	"flash",
	"smoke",
	"sr8",
	"ak103",
	"bomb",
	"negev",
	"frag",
	"m4",
	"glock",
	"colt",
	"mac11",
	"nothing",
	"boot",
	"knifefly"
};
char weaponstring43[(UT_WP_NUM_WEAPONS+3)][15] =
{
	"none",
	"knife",
	"beretta",
	"deagle",
	"spas12",
	"mp5k",
	"ump45",
	"hk69",
	"lr",
	"g36",
	"psg1",
	"he",
	"flash",
	"smoke",
	"sr8",
	"ak103",
	"bomb",
	"negev",
	"frag",
	"m4",
	"glock",
	"colt",
	"mac11",
	"frf1",
	"benelli",
	"p90",
	"magnum",
	"fstod",
	"nothing",
	"boot",
	"knifefly"
};



void SV_GameError( const char *string ) {
	Com_Error( ERR_DROP, "%s", string );
}

void SV_GamePrint( const char *string ) {
	Com_Printf( "%s", string );
}

// these functions must be used instead of pointer arithmetic, because
// the game allocates gentities with private information after the server shared part
int	SV_NumForGentity( sharedEntity_t *ent ) {
	int		num;

	num = ( (byte *)ent - (byte *)sv.gentities ) / sv.gentitySize;

	return num;
}

sharedEntity_t *SV_GentityNum( int num ) {
	sharedEntity_t *ent;

	ent = (sharedEntity_t *)((byte *)sv.gentities + sv.gentitySize*(num));

	return ent;
}

playerState_t *SV_GameClientNum( int num ) {
	playerState_t	*ps;

	ps = (playerState_t *)((byte *)sv.gameClients + sv.gameClientSize*(num));

	return ps;
}

svEntity_t	*SV_SvEntityForGentity( sharedEntity_t *gEnt ) {
	if ( !gEnt || gEnt->s.number < 0 || gEnt->s.number >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "SV_SvEntityForGentity: bad gEnt" );
	}
	return &sv.svEntities[ gEnt->s.number ];
}

sharedEntity_t *SV_GEntityForSvEntity( svEntity_t *svEnt ) {
	int		num;

	num = svEnt - sv.svEntities;
	return SV_GentityNum( num );
}

/*
===============
SV_GameSendServerCommand

Sends a command string to a client
===============
*/
void SV_GameSendServerCommand( int clientNum, const char *text ) {
    int  cid, ready, val[10];
    int  current;
    char cmd[32], auth[MAX_NAME_LENGTH];

	if ( clientNum == -1 ) {
		SV_SendServerCommand( NULL, "%s", text );
	} else {
		if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
			return;
		}

        // Scan scoress game command
        if (!Q_strncmp(text, "scoress ", 8)) {
            if (sscanf(text, "%s %i %i %i %i %i %i %i %i %i %i %s", cmd, &cid, &val[1], &val[2], &val[3], &val[4],
                       &val[5], &ready, &val[7], &val[8], &val[9], auth) != EOF)
            {
                // update the ready flag (jump timer)
                current = svs.clients[cid].cm.ready;
                if (current != ready) {
                    svs.clients[cid].cm.ready = (qboolean) ready;
                    // Set stamina and walljumps back to default
                    svs.clients[cid].cm.infiniteStamina = 0;
                    svs.clients[cid].cm.infiniteWallJumps = 0;
                }
            }
        }

		SV_SendServerCommand( svs.clients + clientNum, "%s", text );
	}
}


/*
===============
SV_GameDropClient

Disconnects the client with a message
===============
*/
void SV_GameDropClient( int clientNum, const char *reason ) {
	if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
		return;
	}
	SV_DropClient( svs.clients + clientNum, reason );	
}

#ifdef USE_AUTH
/*
===============
SV_Auth_GameDropClient

Disconnects the client with a public reason and private message
===============
*/
void SV_Auth_GameDropClient( int clientNum, const char *reason, const char *message ) {
	if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
		return;
	}
	SV_Auth_DropClient( svs.clients + clientNum, reason, message );	
}
#endif

/*
=================
SV_SetBrushModel

sets mins and maxs for inline bmodels
=================
*/
void SV_SetBrushModel( sharedEntity_t *ent, const char *name ) {
	clipHandle_t	h;
	vec3_t			mins, maxs;

	if (!name) {
		Com_Error( ERR_DROP, "SV_SetBrushModel: NULL" );
	}

	if (name[0] != '*') {
		Com_Error( ERR_DROP, "SV_SetBrushModel: %s isn't a brush model", name );
	}


	ent->s.modelindex = atoi( name + 1 );

	h = CM_InlineModel( ent->s.modelindex );
	CM_ModelBounds( h, mins, maxs );
	VectorCopy (mins, ent->r.mins);
	VectorCopy (maxs, ent->r.maxs);
	ent->r.bmodel = qtrue;

	ent->r.contents = -1;		// we don't know exactly what is in the brushes

	SV_LinkEntity( ent );		// FIXME: remove
}



/*
=================
SV_inPVS

Also checks portalareas so that doors block sight
=================
*/
qboolean SV_inPVS (const vec3_t p1, const vec3_t p2)
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	area1 = CM_LeafArea (leafnum);
	mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	area2 = CM_LeafArea (leafnum);
	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return qfalse;
	if (!CM_AreasConnected (area1, area2))
		return qfalse;		// a door blocks sight
	return qtrue;
}


/*
=================
SV_inPVSIgnorePortals

Does NOT check portalareas
=================
*/
qboolean SV_inPVSIgnorePortals( const vec3_t p1, const vec3_t p2)
{
	int		leafnum;
	int		cluster;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);

	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return qfalse;

	return qtrue;
}


/*
========================
SV_AdjustAreaPortalState
========================
*/
void SV_AdjustAreaPortalState( sharedEntity_t *ent, qboolean open ) {
	svEntity_t	*svEnt;

	svEnt = SV_SvEntityForGentity( ent );
	if ( svEnt->areanum2 == -1 ) {
		return;
	}
	CM_AdjustAreaPortalState( svEnt->areanum, svEnt->areanum2, open );
}


/*
==================
SV_GameAreaEntities
==================
*/
qboolean	SV_EntityContact( vec3_t mins, vec3_t maxs, const sharedEntity_t *gEnt, int capsule ) {
	const float	*origin, *angles;
	clipHandle_t	ch;
	trace_t			trace;

	// check for exact collision
	origin = gEnt->r.currentOrigin;
	angles = gEnt->r.currentAngles;

	ch = SV_ClipHandleForEntity( gEnt );
	CM_TransformedBoxTrace ( &trace, vec3_origin, vec3_origin, mins, maxs,
		ch, -1, origin, angles, capsule );

	return trace.startsolid;
}


/*
===============
SV_GetServerinfo
===============
*/
void SV_GetServerinfo( char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetServerinfo: bufferSize == %i", bufferSize );
	}
	Q_strncpyz( buffer, Cvar_InfoString( CVAR_SERVERINFO ), bufferSize );
}

/*
===============
SV_LocateGameData
===============
*/
void SV_LocateGameData( sharedEntity_t *gEnts, int numGEntities, int sizeofGEntity_t,
					   playerState_t *clients, int sizeofGameClient ) {
	sv.gentities = gEnts;
	sv.gentitySize = sizeofGEntity_t;
	sv.num_entities = numGEntities;

	sv.gameClients = clients;
	sv.gameClientSize = sizeofGameClient;

}


/*
===============
SV_GetUsercmd
===============
*/
void SV_GetUsercmd( int clientNum, usercmd_t *cmd ) {
	if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
		Com_Error( ERR_DROP, "SV_GetUsercmd: bad clientNum:%i", clientNum );
	}
	*cmd = svs.clients[clientNum].lastUsercmd;

	client_t *cl;
	cl = &svs.clients[clientNum];

	if (cl->cm.frozen > 0) {
		cmd->forwardmove = ClampChar(0);
		cmd->rightmove = ClampChar(0);
		cmd->upmove = ClampChar(0);
	}
}

// Replace function
char *string_replace(char *orig, char *rep, char *with) {
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


/*
===============
GunMoney related
===============
*/

// [Guns] Weapons & Items variables
char* weapons_list[16] = { "hk69", "lr", "spas12", "m4", "psg1", "mp5k", "ak103", "negev", "g36", "sr8", "ump45", "mac11", "p90", "frf1", "benelli", "fstod" };
char* items[3] = { "silencer", "laser", "medkit" }; // Removed helmet & vest as now given always for a throwing knife kill. 
char* pistols[5] = { "beretta", "deagle", "colt", "glock", "magnum" };

int weaponsnum = sizeof(weapons_list) / sizeof(const char*);
int itemsnum = sizeof(items) / sizeof(const char*);
int pistolsnum = sizeof(pistols) / sizeof(const char*);

// [Guns] Ammo, Nades & Health variables
char healthops[2] = { '+', '-' };

/*
===============
[Guns]
SV_NameWeapon
Returns Weapon name
===============
*/
char* SV_NameWeapon(char* weap2) {
	char* weapon = " ";
	if (weap2 == "sr8") {
		weapon = "^6Sr8";
	}
	else if (weap2 == "ak103") {
		weapon = "^5AK103";
	}
	else if (weap2 == "negev") {
		weapon = "^4NEGEV";
	}
	else if (weap2 == "ump45") {
		weapon = "^3UMP45";
	}
	else if (weap2 == "g36") {
		weapon = "^5G36";
	}
	else if (weap2 == "hk69") {
		weapon = "^1HK69";
	}
	else if (weap2 == "lr") {
		weapon = "^5LR300";
	}
	else if (weap2 == "spas12") {
		weapon = "^3Spas12";
	}
	else if (weap2 == "m4") {
		weapon = "^5M4A1";
	}
	else if (weap2 == "psg1") {
		weapon = "^6PSG1";
	}
	else if (weap2 == "mp5k") {
		weapon = "^3MP5K";
	}
	else if (weap2 == "beretta") {
		weapon = "^2Beretta";
	}
	else if (weap2 == "deagle") {
		weapon = "^2Desert Eagle";
	}
	else if (weap2 == "colt") {
		weapon = "^2Colt1911";
	}
	else if (weap2 == "magnum") {
		weapon = "^2Magnum";
	}
	else if (weap2 == "glock") {
		weapon = "^2Glock-18";
	}
	else if (weap2 == "fstod") {
		weapon = "^6TOD-50";
	}
	else if (weap2 == "p90") {
		weapon = "^3P90";
	}
	else if (weap2 == "mac11") {
		weapon = "^3Mac11";
	}
	else if (weap2 == "benelli") {
		weapon = "^3Benelli";
	}
	else if (weap2 == "frf1") {
		weapon = "^6FRF1";
	}
	return weapon;
}

/*
===============
[Guns]
SV_NameItem
Returns Item name
===============
*/
char* SV_NameItem(char* item2) {
	char* item = " ";
	if (item2 == "silencer") {
		item = "Silencer";
	}
	else if (item2 == "laser") {
		item = "Laser Sight";
	}
	else if (item2 == "medkit") {
		item = "^6Super^3 ^2Medkit";
	}
	else if (item2 == "vest") {
		item = "Kevlar";
	}
	else if (item2 == "helmet") {
		item = "Helmet";
	}
	return item;
}

qboolean wasKillNoscope(client_t* klr) {
	qboolean noscope = qfalse;
	if (klr->lastUsercmd.buttons == 67585 || klr->lastUsercmd.buttons == 2081 || klr->lastUsercmd.buttons == 2049 || klr->lastUsercmd.buttons == 67841 || klr->lastUsercmd.buttons == 2049 || klr->lastUsercmd.buttons == 2305) {
		noscope = qtrue;
	}
	return noscope;
}

/*
====================
Events that we created in files.c
====================
*/


void SV_OnMapExit(void)
{	
	if (mod_zombiemod->integer) {
		Cmd_ExecuteString("swapteams");
		Cmd_ExecuteString("say \"^3The ^2Teams ^3have been ^5swapped\n^3Press ^1TAB ^3to view the ^5scoreboard\"");
		Cmd_ExecuteString("bigtext \"^3The ^2Teams ^3have been ^5swapped\n\n\n^3Press ^1TAB ^3to view the ^5scoreboard\"");
	}
}

void SV_onKill(char* killer, char* killed, char* wpn)
{
	if (mod_announceNoscopes->integer) {
		// No suicide or world kills
		if ((Q_stricmp(killer, "1022") != 0) || ((Q_stricmp(killer, killed) != 0))) {

			// Check that the weapon for the kill is a sniper
			if ((!Q_stricmp(wpn, "28:")) || (!Q_stricmp(wpn, "21:")) || (!Q_stricmp(wpn, "42:"))) {
				client_t* klr = &svs.clients[atoi(killer)];;
				if (wasKillNoscope(klr)) {
					client_t* kld = &svs.clients[atoi(killed)];;
					SV_SendServerCommand(NULL, "cchat \"\" \"^6%s ^3just ^2noscoped ^5%s!\"", klr->name, kld->name);
				}
			}
		}
	}

	if (mod_levelsystem->integer) {
		// No suicide or world kills
		if ((Q_stricmp(killer, "1022") != 0) || ((Q_stricmp(killer, killed) != 0))) {
			// Knife thrown
			if (!Q_stricmp(wpn, "13:")) {
				Cmd_ExecuteString(va("exec sound_throwkill.txt"));
			}
			// Boot (kicked)
			else if (!Q_stricmp(wpn, "24:")) {
				Cmd_ExecuteString(va("exec sound_bootkill.txt"));
			}
			// Curbstomp (goomba)
			else if (!Q_stricmp(wpn, "48:")) {
				Cmd_ExecuteString(va("exec sound_goombakill.txt"));
			}
		}
	}

	if (mod_gunsmod->integer){

		client_t* klr;
		client_t* kld;

		if (Q_stricmp(killer, "1022") != 0)
		{
			klr = &svs.clients[atoi(killer)];
			kld = &svs.clients[atoi(killed)];
			playerState_t *ps;
    	    int cid;
    	    cid = klr - svs.clients;
			ps = SV_GameClientNum(cid);
			kld->hasmedkit = qfalse;
			if (Q_stricmp(killer, killed) != 0){ // no suicide

				// Knife
				//SV_SendServerCommand(klr, "cchat \"\" \"^1DEBUG: ^5Weapon key: ^6%s\"", wpn);
				if (!Q_stricmp(wpn, "12:")) {
					int health2 = (rand() % 100) + 1;
					Cmd_ExecuteString(va("addhealth %s +%i", killer, health2));
					SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^1Knife ^7kill ^1= ^7Health: ^2+%i\"", health2);
				}
				// Knife thrown
				else if (!Q_stricmp(wpn, "13:")) {
					int random = rand() % itemsnum;
					char* item = items[random];
					Cmd_ExecuteString(va("gi %i vest", ps->clientNum));
					Cmd_ExecuteString(va("gi %i helmet", ps->clientNum));
					Cmd_ExecuteString(va("gi %i %s", ps->clientNum, item));
					SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^1Throwing Knife ^7kill ^1= ^6%s ^3and ^2Vest+Helmet\"", SV_NameItem(item));
					Cmd_ExecuteString(va("exec thrown.txt"));
					if (!Q_stricmp(SV_NameItem(item), "^6Super^3 ^2Medkit")) {
						klr->hasmedkit = qtrue;
					}
				}
				// Beretta
				else if (!Q_stricmp(wpn, "14:")) {
					int random = rand() % weaponsnum;
					char* weapon = weapons_list[random];
					Cmd_ExecuteString(va("gw %s %s", killer, weapon));
					SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^2Beretta ^7kill ^1= %s\"", SV_NameWeapon(weapon));
				}
				// Desert Eagle
				else if (!Q_stricmp(wpn, "15:")) {
					int random = rand() % weaponsnum;
					char* weapon = weapons_list[random];
					Cmd_ExecuteString(va("gw %s %s", killer, weapon));
					SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^2Desert Eagle ^7kill ^1= %s\"", SV_NameWeapon(weapon));
				}
				// Spas
				else if (!Q_stricmp(wpn, "16:")) {
					int random = rand() % weaponsnum;
					char* weapon = weapons_list[random];
					int amo2 = (rand() % 255) + 1;
					Cmd_ExecuteString(va("gw %s %s +%i 0", killer, weapon, amo2));
					SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^3Spas12 ^7kill ^1= %s ^4+%i\"", SV_NameWeapon(weapon), amo2);
				}
				// Benelli
				else if (!Q_stricmp(wpn, "43:")) {
					int random = rand() % weaponsnum;
					char* weapon = weapons_list[random];
					int amo2 = (rand() % 255) + 1;
					Cmd_ExecuteString(va("gw %s %s +%i 0", killer, weapon, amo2));
					SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^3Benelli ^7kill ^1= %s ^4+%i\"", SV_NameWeapon(weapon), amo2);
				}
				// UMP45
				else if (!Q_stricmp(wpn, "17:")) {
					char* weapon = "beretta";
					Cmd_ExecuteString(va("gw %s %s +30 0", killer, weapon));
					SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^3UMP45 ^7kill ^1= %s ^4+30\"", SV_NameWeapon(weapon));
				}
				// MP5K
				else if (!Q_stricmp(wpn, "18:")) {
					char* weapon = "deagle";
					Cmd_ExecuteString(va("gw %s %s +15 0", killer, weapon));
					SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^3MP5K ^7kill ^1= %s ^4+15\"", SV_NameWeapon(weapon));
				}
				// LR
				// else if (!Q_stricmp( wpn, "19:" )) {

				// }
				// G36
				// else if (!Q_stricmp( wpn, "20:" )) {

				// }
				// PSG1
				else if (!Q_stricmp(wpn, "21:")) {
					char* weapon = "he";
					int nades2 = (rand() % 50) + 1;
					Cmd_ExecuteString(va("gw %s %s %i 0", killer, weapon, nades2));
					SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^6PSG1 ^7kill ^1= ^6Grenades ^4+%i\"", nades2);
				}
				// HK69
				else if ((!Q_stricmp(wpn, "22:")) || (!Q_stricmp(wpn, "37:"))) {
					int health2 = (rand() % 100) + 1;
					char operator = healthops[rand() % 2];
					Cmd_ExecuteString(va("addhealth %s %c%i", killer, operator, health2));
					if (operator == '-') {
						SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^1HK69 ^7kill ^1= ^7Random Health: ^1-%i\"", health2);
					}
					else if (operator == '+') {
						SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^1HK69 ^7kill ^1= ^7Random Health: ^2+%i\"", health2);
					}
				}
				// BLEED
				else if (!Q_stricmp(wpn, "23:")) {
					int random = rand() % itemsnum;
					char* item = items[random];
					Cmd_ExecuteString(va("gi %s %s", killer, item));
					SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^1Bleeding ^7kill ^1= ^6%s\"", SV_NameItem(item));
					if (!Q_stricmp(SV_NameItem(item), "^6Super^3 ^2Medkit")) {
						klr->hasmedkit = qtrue;
					}
				}
				// BOOT (KICKED)
				else if (!Q_stricmp(wpn, "24:")) {
					Cmd_ExecuteString(va("addhealth %s +100", killer));
					SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^6Boot ^7kill ^1= ^2Full ^7Health\"");
					Cmd_ExecuteString(va("exec kicked.txt"));
				}
				// HE NADE
				else if (!Q_stricmp(wpn, "25:")) {
					int random = rand() % itemsnum;
					char* item = items[random];
					Cmd_ExecuteString(va("gi %s %s", killer, item));
					SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^1HE Grenade ^7kill ^1= ^6%s\"", SV_NameItem(item));
					if (!Q_stricmp(SV_NameItem(item), "^6Super^3 ^2Medkit")) {
						klr->hasmedkit = qtrue;
					}
				}
				// SR8
				else if (!Q_stricmp(wpn, "28:")) {
					char* weapon = "he";
					int nades2 = (rand() % 50) + 1;
					Cmd_ExecuteString(va("gw %s %s +%i 0", killer, weapon, nades2));
					SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^6Sr8 ^7kill ^1= ^6Grenades ^4+%i\"", nades2);
				}
				// FRF1
				else if (!Q_stricmp(wpn, "42:")) {
					char* weapon = "he";
					int nades2 = (rand() % 50) + 1;
					Cmd_ExecuteString(va("gw %s %s +%i 0", killer, weapon, nades2));
					SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^6FRF1 ^7kill ^1= ^6Grenades ^4+%i\"", nades2);
				}
				// AK103
				// else if (!Q_stricmp( wpn, "30:" )) {

				// }
				// NEGEV
				else if (!Q_stricmp(wpn, "36:")) {
					int health2 = (rand() % 100) + 1;
					char operator = healthops[rand() % 2];
					Cmd_ExecuteString(va("addhealth %s %c%i", killer, operator, health2));
					if (operator == '-') {
						SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^4NEGEV ^7kill ^1= ^7Random Health: ^1-%i\"", health2);
					}
					else if (operator == '+') {
						SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^4NEGEV ^7kill ^1= ^7Random Health: ^2+%i\"", health2);
					}
				}
				// M4
				// else if (!Q_stricmp( wpn, "38:" )) {

				// }
				// CURB (GOOMBA)
				else if (!Q_stricmp(wpn, "48:")) {
					Cmd_ExecuteString(va("gw %i hk69 100 100", ps->clientNum));
					Cmd_ExecuteString(va("gw %i spas12 255 255", ps->clientNum));
					Cmd_ExecuteString(va("gw %i smoke 25 0", ps->clientNum));
					Cmd_ExecuteString(va("gi %i silencer", ps->clientNum));
					Cmd_ExecuteString(va("gi %i laser", ps->clientNum));
					Cmd_ExecuteString(va("gi %i medkit", ps->clientNum));
					Cmd_ExecuteString(va("gi %i vest", ps->clientNum));
					Cmd_ExecuteString(va("gi %i helmet", ps->clientNum));
					SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^6Curb Stomp ^7kill ^1= ^5HK69/SPAS12/Smokes ^3and ^6all Items\"");
					Cmd_ExecuteString(va("bigtext \"%s made a ^6Curb Stomp^7!!\n^3He won ^5HK69/SPAS12/Smokes ^3and ^6all Items\"", klr->name));
					Cmd_ExecuteString(va("exec goomba.txt"));
				}
				// Glock
				else if (!Q_stricmp(wpn, "39:")) {
					int random = rand() % weaponsnum;
					char* weapon = weapons_list[random];
					Cmd_ExecuteString(va("gw %s %s", killer, weapon));
					SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^2Glock 18 ^7kill ^1= %s\"", SV_NameWeapon(weapon));
				}
				// Colt
				else if (!Q_stricmp(wpn, "40:")) {
					int random = rand() % weaponsnum;
					char* weapon = weapons_list[random];
					Cmd_ExecuteString(va("gw %s %s", killer, weapon));
					SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^2Colt-1911 ^7kill ^1= %s\"", SV_NameWeapon(weapon));
				}
				// Magnum
				else if (!Q_stricmp(wpn, "45:")) {
					int random = rand() % weaponsnum;
					char* weapon = weapons_list[random];
					Cmd_ExecuteString(va("gw %s %s", killer, weapon));
					SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^2Magnum ^7kill ^1= %s\"", SV_NameWeapon(weapon));
				}
				// MAC-11
				else if (!Q_stricmp(wpn, "41:")) {
					int random = rand() % pistolsnum;
					char* weapon = pistols[random];
					Cmd_ExecuteString(va("gw %s %s +30 0", killer, weapon));
					SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^3MAC-11 ^7kill ^1= %s ^4+30\"", SV_NameWeapon(weapon));
				}
				// P90
				else if (!Q_stricmp(wpn, "44:")) {
				int random = rand() % pistolsnum;
				char* weapon = pistols[random];
				Cmd_ExecuteString(va("gw %s %s +30 0", killer, weapon));
				SV_SendServerCommand(klr, "cchat \"\" \"^6~^8> ^3P90 ^7kill ^1= %s ^4+30\"", SV_NameWeapon(weapon));
				}
			}
		}
	}
}

void parseClientSettings(client_t *client, char* message) {
	const char *s;
	s = message;
	Cmd_TokenizeString(s);
	char *type;
	char *data;

	if (strcmp(Cmd_Argv(0), "!preference") == 0) {
		data = Cmd_Argv(1);		
		
		if (strcmp(data, "primary") == 0) {
			client->preference = ARENA_PRIMARY;
			SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Preference ^3set to ^2PRIMARY.\"");
		}
		else if (strcmp(data, "secondary") == 0) {
			client->preference = ARENA_SECONDARY;
			SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Preference ^3set to ^2SECONDARY.\"");
		}
		else if (strcmp(data, "pistol") == 0) {
			client->preference = ARENA_PISTOL;
			SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Preference ^3set to ^2PISTOL.\"");
		}
		else if (strcmp(data, "sniper") == 0) {
			client->preference = ARENA_SNIPER;
			SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Preference ^3set to ^2SNIPER.\"");
		}
		else if (strcmp(data, "knife") == 0) {
			client->preference = ARENA_KNIFE;
			SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Preference ^3set to ^2KNIFE.\"");
		}
		else if (strcmp(data, "gib") == 0) {
			client->preference = ARENA_GIB;
			SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Preference ^3set to ^2GIB.\"");
		} else {
			SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^1Error: [Unknown preference] ^3See ^2!guns ^3for a list of preferences.\"");
		}
	}

	else if (strcmp(Cmd_Argv(0), "!set") == 0) {
		type = Cmd_Argv(1);
		data = Cmd_Argv(2);

		if (strcmp(type, "primary") != 0 && strcmp(type, "secondary") != 0 && strcmp(type, "pistol") != 0 && strcmp(type, "sniper") != 0) {
			SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^1Error: [Unknown Type] ^3See ^2!guns ^3for a list of types.\"");
		}                 

		else if (strcmp(type, "primary") == 0) {
			if (strcmp(data, "lr300") == 0) {
				client->primary = ARENA_LR300;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Primary ^3set to ^2LR300.\"");
			}
			else if (strcmp(data, "g36") == 0) {
				client->primary = ARENA_G36;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Primary ^3set to ^2G36.\"");
			}
			else if (strcmp(data, "ak103") == 0) {
				client->primary = ARENA_AK103;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Primary ^3set to ^2AK103.\"");
			}
			else if (strcmp(data, "m4") == 0) {
				client->primary = ARENA_M4;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Primary ^3set to ^2M4.\"");
			}
			else if (strcmp(data, "negev") == 0) {
				client->primary = ARENA_NEGEV;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Primary ^3set to ^2NEGEV.\"");
			}
			else if (strcmp(data, "hk69") == 0) {
				client->primary = ARENA_HK69;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Primary ^3set to ^2HK69.\"");
			}
			else {
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^1Error: [Unknown Weapon] ^3See ^2!guns ^3for exact weapon names.\"");
			}
		}

		else if (strcmp(type, "secondary") == 0) {				
			if (strcmp(data, "ump45") == 0) {
				client->secondary = ARENA_UMP45;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Secondary ^3set to ^2UMP45.\"");		
			}		
			else if (strcmp(data, "mac11") == 0) {
				client->secondary = ARENA_MAC11;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Secondary ^3set to ^2MAC11.\"");
			}
			else if (strcmp(data, "p90") == 0) {
				client->secondary = ARENA_P90;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Secondary ^3set to ^2P90.\"");
			}
			else if (strcmp(data, "mp5k") == 0) {
				client->secondary = ARENA_MP5K;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Secondary ^3set to ^2MP5K.\"");
			}
			else if (strcmp(data, "spas12") == 0) {
				client->secondary = ARENA_SPAS12;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Secondary ^3set to ^2SPAS12.\"");
			}
			else if (strcmp(data, "benelli") == 0) {
				client->secondary = ARENA_BENELLI;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Secondary ^3set to ^2BENELLI.\"");
			}
			else {
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^1Error: [Unknown Weapon] ^3See ^2!guns ^3for exact weapon names.\"");
			}
		}

		else if (strcmp(type, "pistol") == 0) {
			if (strcmp(data, "beretta") == 0) {
				client->pistol = ARENA_BERETTA;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Pistol ^3set to ^2BERETTA.\"");
			}
			else if (strcmp(data, "deagle") == 0) {
				client->pistol = ARENA_DEAGLE;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Pistol ^3set to ^2DEAGLE.\"");
			}
			else if (strcmp(data, "colt") == 0) {
				client->pistol = ARENA_COLT;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Pistol ^3set to ^2COLT.\"");
			}
			else if (strcmp(data, "glock") == 0) {
				client->pistol = ARENA_GLOCK;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Pistol ^3set to ^2GLOCK.\"");
			}
			else if (strcmp(data, "magnum") == 0) {
				client->pistol = ARENA_MAGNUM;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Pistol ^3set to ^2MAGNUM.\"");
			}
			else {
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^1Error: [Unknown Weapon] ^3See ^2!guns ^3for exact weapon names.\"");
			}
		}

		else if (strcmp(type, "sniper") == 0) {
			if (strcmp(data, "psg1") == 0) {
				client->sniper = ARENA_PSG1;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Sniper ^3set to ^2PSG1.\"");
			}
			else if (strcmp(data, "sr8") == 0) {
				client->sniper = ARENA_SR8;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Sniper ^3set to ^2SR8.\"");
			}
			else if (strcmp(data, "frf1") == 0) {
				client->sniper = ARENA_FRF1;
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^5Sniper ^3set to ^2FRF1.\"");
			}
			else {
				SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^1Error: [Unknown Weapon] ^3See ^2!guns ^3for exact weapon names.\"");
			}
		}
	}
}

void SV_onChat(char* playerid, char* playername, char* message)
{
	client_t* cl;
	cl = &svs.clients[atoi(playerid)];

	if (mod_gunsmod->integer){
        if (strcmp(message, "!kitlist") == 0) {
            SV_SendServerCommand(cl, "print  \"^7 Kitlist v0.1 (29/01/2022)\n\"");
            SV_SendServerCommand(cl, "print  \"^3////////////////////////////////////////////////////////////////////////////\n\"");
        	SV_SendServerCommand(cl, "print  \"^3//               ^1! BUYING A KIT WILL LOSE ALL YOUR WEAPONS !              ^3//\n\"");
	        SV_SendServerCommand(cl, "print  \"^3//-----------------------------------^3/------------------------------------^3//\n\"");
			SV_SendServerCommand(cl, "print  \"^7////////////////////////////////////////////////////////////////////////////\n\"");
			SV_SendServerCommand(cl, "print  \"^7//               Armor               ^7/             Attachments            ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//-----------------------------------^7/------------------------------------^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7// ^3Kevlar Vest                       ^7/  ^3Laser                             ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7// ^3Helmet                            ^7/  ^3Silencer                          ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//           ^2Price:^6 1620 coins       ^7/  ^3Clips (-2x500 coins per weapon)   ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//                                   ^7/      ^2Base Price:^6  900 coins        ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//........................................................................^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//               Rifles              ^7/               Snipers              ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//-----------------------------------^7/------------------------------------^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7// ^3M4                                ^7/  ^3SR8                               ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7// ^3LR300                             ^7/  ^3FRF1                              ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7// ^3AK103                             ^7/  ^3PSG1                              ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//           ^2Price:^6 13545 coins      ^7/           ^2Price:^6 21600 coins       ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//........................................................................^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//            Secondaries            ^7/                                    ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//-----------------------------------^7/------------------------------------^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7// ^3MP5K                              ^7/                                    ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3Mac11                             ^7/                                    ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3P90                               ^7/                                    ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3Spas12                            ^7/                                    ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//           ^2Price:^6 19575 coins      ^7/                                    ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//........................................................................^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//  ^5Usage:                                                                ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//  !kit <KIT SHORT/NAME>  for example: !kit armor / !kit ar              ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//                                      !kit attachments / !kit att       ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7////////////////////////////////////////////////////////////////////////////\n\"");
			SV_SendServerCommand(cl, "chat  \"^2Kitlist ^3sent to your console -> switch to the ^5'Chat' tab!");
		}
      else if (strcmp(message, "!buylist") == 0) {
            SV_SendServerCommand(cl, "print  \"^7 Buylist v2.1 (12/12/2021)\n\"");
            SV_SendServerCommand(cl, "print  \"^7////////////////////////////////////////////////////////////////////////////\n\"");
            SV_SendServerCommand(cl, "print  \"^7//           Sniper Rifles           ^7/         Assault Rifles             ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//-----------------------------------^7/------------------------------------^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3sr8     ^2Price:^6 8000 coins         ^7/  ^3lr300    ^2Price:^6 5500 coins        ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3psg1    ^2Price:^6 8000 coins         ^7/  ^3g36      ^2Price:^6 6000 coins        ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3frf1    ^2Price:^6 8000 coins         ^7/  ^3ak103    ^2Price:^6 7000 coins        ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//                                   ^7/  ^3M4       ^2Price:^6 2550 coins        ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//........................................................................^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//             SMG's                 ^7/              Shotguns              ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//-----------------------------------^7/------------------------------------^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3ump45   ^2Price:^6 3250 coins         ^7/  ^3spas12   ^2Price:^6 12000 coins       ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3mp5k    ^2Price:^6 3250 coins         ^7/  ^3benelli  ^2Price:^6 10000 coins       ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3p90     ^2Price:^6 3250 coins         ^7/                                    ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3mac11   ^2Price:^6 3250 coins         ^7/                                    ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//........................................................................^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//           Machine Guns            ^7/             Specials               ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//-----------------------------------^7/------------------------------------^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3negev      ^2Price:^6 4000 coins      ^7/  ^3fstod    ^2Price:^6  30000 coins      ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//                                   ^7/  ^3hk69     ^2Price:^6  10000 coins      ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//........................................................................^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//             Abilites              ^7/             Equipment              ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//-----------------------------------^7/------------------------------------^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3!disarm    ^2Price:^6 500000  coins   ^7/  ^3Vest     ^2Price:^6 1000 coins        ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3!invisible ^2Price:^6 750000  coins   ^7/  ^3Helmet   ^2Price:^6  800 coins        ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3!ammo      ^2Price:^6  65000  coins   ^7/  ^3Silencer ^2Price:^6  500 coins        ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3!clips     ^2Price:^6  45000  coins   ^7/  ^3Laser    ^2Price:^6  500 coins        ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3!maptp     ^2Price:^6  50000  coins   ^7/  ^3NVG      ^2Price:^6 3000 coins        ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3!lowgrav   ^2Price:^6 1000000 coins   ^7/                                    ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3!slapall   ^2Price:^6 1000000 coins   ^7/                                    ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3!b health  ^2Price:^6  15000  coins   ^7/                                    ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3!laugh     ^2Price:^6  25000  coins   ^7/                                    ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3!teleport  ^2Price:^6 250000  coins   ^7/                                    ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3!freeze    ^2Price:^6 550000  coins   ^7/                                    ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3!nades     ^2Price:^6 500000  coins   ^7/                                    ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3!slick     ^2Price:^6 500000  coins   ^7/                                    ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3!crazy     ^2Price:^6 1000000 coins   ^7/                                    ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3!overclock ^2Price:^6 500000  coins   ^7/                                    ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3!airjumps  ^2Price:^6 1000000 coins   ^7/                                    ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3!superbots ^2Price:^6 1000000 coins   ^7/                                    ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7// ^3!vampire <amount> 1 minute = 10000 coins                               ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//........................................................................^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//  ^5Specials:                                                             ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//  !gamble <amount> (Gamble for a 50/50 chance to tripple your input!)   ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//  !particles (Will activate particles around you)^2Price:^610000000 coins^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//  !buycolour <colour number> ^2Price: ^610000000 coins                      ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//  colours: ^11^7=^1red ^22^7=^2green ^33^7=^3yellow ^44^7=^4blue ^55^7=^5cyan ^66^7=^6purple ^88^7=^8orange ^99^7=^9grey^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//  ^5Usage:                                                                ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//  !buy <WEAPON NAME>     for example: !buy sr8                          ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//  !buy <ITEM NAME>       for example: !buy vest                         ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//  ^5Autobuy:                                                              ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//  !buy <WEAPON NAME> ON  for example: !buy sr8 on                       ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//  !buy <ITEM NAME> ON    for example: !buy vest on                      ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//  ^5Abilites:                                                             ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//  !maptp (no !b required, simply use !<ability name>)                   ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7//  !heal                                                                 ^7//\n\"");
            SV_SendServerCommand(cl, "print  \"^7////////////////////////////////////////////////////////////////////////////\n\"");
            SV_SendServerCommand(cl, "chat  \"^2Buylist ^3sent to your console -> switch to the ^5'Chat' tab!");
        }
	}
	if (mod_1v1arena->integer){
    	if (strcmp(message, "!guns") == 0) {
			SV_SendServerCommand(cl, "print  \"^7////////////////////////////////////////////////////////////////////////////\n\"");
			SV_SendServerCommand(cl, "print  \"^7//                                                                        ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//  ^5Gun menu                                                              ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//                                                                        ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//  ^5Usage:                                                                ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//  ^2!set ^6<type> <weapon>                                                  ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//  ^2!preference ^6<type>                                                    ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//                                                                        ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//  ^5Types:                                                                ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//  ^4primary / secondary / pistol / sniper / knife / gib                   ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//                                                                        ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//  ^5Weapons:                                                              ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//  ^2Primary: ^4lr300, g36, ak103, m4, negev, hk69                           ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//  ^2Secondary: ^4ump45, mac11, p90, mp5k, spas12, benelli                   ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//  ^2Pistol: ^4beretta, deagle, colt, glock, magnum                          ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//  ^2Sniper: ^4psg1, sr8, frf1                                               ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//                                                                        ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//                                                                        ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//  ^5Usage:                                                                ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//  ^2!set ^6primary lr300                                                    ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//  ^2!set ^6secondary ump45                                                  ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//  ^2!set ^6pistol deagle                                                    ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//  ^2!set ^6sniper sr8                                                       ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//  ^2!preference ^6primary                                                   ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7//                                                                        ^7//\n\"");
			SV_SendServerCommand(cl, "print  \"^7////////////////////////////////////////////////////////////////////////////\n\"");
		}
		// check if !set is part of the message string
		if (strstr(message, "!set") != NULL || strstr(message, "!preference") != NULL) {
			parseClientSettings(cl, message);
		}

	}
	if (mod_gunsmod->integer || mod_levelsystem->integer) {

		if (cl->muted) {
			// prevent muted players from spamming sounds
			return;
		}

		unsigned int time = Com_Milliseconds();
		if ((time - cl->lastsoundtime) < 15000) {
			// prevent spammers from spamming sounds (1 sound every 15 seconds)
			return;
		}

		if ((strcmp(message, "hi") == 0) || (strcmp(message, "hello") == 0) || (strcmp(message, "hallo") == 0) || (strcmp(message, "salut") == 0) || (strcmp(message, "hey") == 0) || (strcmp(message, "ciao") == 0))  {
			Cmd_ExecuteString(va("exec sound_hello.txt"));
			cl->lastsoundtime = time;
		}
		if ((strcmp(message, "n1") == 0) || (strcmp(message, "nice one") == 0) || (strcmp(message, "nice") == 0))  {
			Cmd_ExecuteString(va("exec sound_nice.txt"));
			cl->lastsoundtime = time;
		}
		if ((strcmp(message, "wtf") == 0) || (strcmp(message, "WTF") == 0) || (strcmp(message, "what the fuck") == 0))  {
			Cmd_ExecuteString(va("exec sound_wtf.txt"));
			cl->lastsoundtime = time;
		}
		if ((strcmp(message, "hru") == 0) || (strcmp(message, "how you doin") == 0) || (strcmp(message, "how are you") == 0))  {
			Cmd_ExecuteString(va("exec sound_hru.txt"));
			cl->lastsoundtime = time;
		}
		if (strcmp(message, "whatever") == 0) {
			Cmd_ExecuteString(va("exec sound_whatever.txt"));
			cl->lastsoundtime = time;
		}
		if ((strcmp(message, "thanks") == 0) || (strcmp(message, "ty") == 0) || (strcmp(message, "thx") == 0))  {
			Cmd_ExecuteString(va("exec sound_thanks.txt"));
			cl->lastsoundtime = time;
		}
		if ((strcmp(message, "idiot") == 0) || (strcmp(message, "dumbass") == 0) || (strcmp(message, "you idiot") == 0))  {
			Cmd_ExecuteString(va("exec sound_idiot.txt"));
			cl->lastsoundtime = time;
		}
		if ((strcmp(message, "sorry") == 0) || (strcmp(message, "sry") == 0) || (strcmp(message, "soz") == 0))  {
			Cmd_ExecuteString(va("exec sound_sorry.txt"));
			cl->lastsoundtime = time;
		}
		if ((strcmp(message, "np") == 0) || (strcmp(message, "no problem") == 0) || (strcmp(message, "no worries") == 0))  {
			Cmd_ExecuteString(va("exec sound_np.txt"));
			cl->lastsoundtime = time;
		}
		if ((strcmp(message, "status") == 0) || (strcmp(message, "whats up") == 0) || (strcmp(message, "status?") == 0) || (strcmp(message, "sup") == 0))  {
			Cmd_ExecuteString(va("exec sound_status.txt"));
			cl->lastsoundtime = time;
		}
		if (strcmp(message, "lol") == 0)  {
			Cmd_ExecuteString(va("exec sound_lol.txt"));
			cl->lastsoundtime = time;
		}
	}
}

int getClientTeam(client_t *cl) {
	int team = -1;
	playerState_t *ps;
	ps = SV_GameClientNum(cl - svs.clients);
	team = *(int*)((void*)ps+gclientOffsets[getVersion()][OFFSET_TEAM]);
	return team;
}

/*
===============
Ghostmode on roundstart
===============
*/

void SV_SurvivorRoundStart(void) {

	// The entities reset every single round, so reset the counter too
	if (sv.lastEntityNum != 1010) {
		sv.lastEntityNum = 1010;
	}
	if (sv.lastIndexNum != 255) {
		sv.lastIndexNum = 255;
	}

	if (sv_ghostOnRoundstart->integer) {
		sv.checkGhostMode = qtrue;
		sv.stopAntiBlockTime = Com_Milliseconds() + 10000;;
		Cmd_ExecuteString(va("mod_ghostPlayers 1"));
	}

	if (sv_TurnpikeBlocker->integer) {

		sv.currplayers = 0;

		client_t *cl;
		int i;

        for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
			if (cl->state == CS_ACTIVE) {
				int team = getClientTeam(cl);
				if (team != 3 && team != -1) { // Do not count specs
					sv.currplayers++;
				}
            }
		}
	}
}

/*
===============
1v1 Arena related

This has to be done here
The files.c event is fired
too quick
===============
*/



/*
===============
Levelsystem Related

using different event handler here
as the one from gunmoney doesn't
work as intended for this
===============
*/

/////////////////////////////////////////////////////////////////////
// EV_PlayerSpawn
/////////////////////////////////////////////////////////////////////
void EV_PlayerSpawn(int cnum)
{
	if(cnum < 0 || cnum > sv_maxclients->integer)
		return;

	SV_WeaponMod(cnum);

    if (sv_gametype->integer == GT_JUMP) {
        client_t *cl;
        cl = cnum + svs.clients;
        cl->cm.ready = 0;
    }

}

/////////////////////////////////////////////////////////////////////
// EV_ClientUserInfoChanged
/////////////////////////////////////////////////////////////////////
void EV_ClientUserInfoChanged(int cnum)
{
	if(cnum < 0 || cnum > sv_maxclients->integer)
		return;

	SV_WeaponMod(cnum);

	
	if (mod_1v1arena->integer) {

		client_t* client;
		client = cnum + svs.clients;

		// clean playername
    	char cleanName[64];
        Q_strncpyz(cleanName, client->name, sizeof(cleanName));
        Q_CleanStr(cleanName );

		memset(client->defaultconfigstr,0,sizeof(client->defaultconfigstr));

		char* mystring = Q_CleanStr(sv.configstrings[cnum + 544]);
		char* toreplace = va("%s", cleanName);
		char* replacewith = "XXXXXXXXXXXXX";
		char* somestring = string_replace(mystring, toreplace, replacewith);
		char mynewstring[256];
		Q_strncpyz(mynewstring, somestring, sizeof(mynewstring));
		Q_strncpyz(client->defaultconfigstr, mynewstring, sizeof(client->defaultconfigstr));

		// clear the current customname
		memset(client->lastcustomname,0,sizeof(client->lastcustomname));
		
		const char* mynewname;
		// Set our name:
		if (client->arena == 99) {
			mynewname = va("^7[^5No Arena^7]^3%s^2", cleanName);
		} else {
			mynewname = va("^7[^3Arena ^5%i^7]^3%s^2", client->arena+1, cleanName);
		}

		// Set the new custom name
		Q_strncpyz(client->lastcustomname, mynewname, sizeof(client->lastcustomname));

		// Finally tell the server to use the new name
		client->customname = qtrue;

		// Shoot a location update for the new name
		playerState_t *ps;
		ps = SV_GameClientNum(cnum);

		int team;
		team = *(int*)((void*)ps+gclientOffsets[getVersion()][OFFSET_TEAM]);
		if (team == 3) {
			Cmd_ExecuteString(va("location %i \"^7[^3Arena ^5SPEC^7][^3Skill ^5%i^7]\" 0 1", client->playernum, client->skill));
		} else if (sv.Arenas[client->arena].population == 1) {
			Cmd_ExecuteString(va("location %i \"^7[^3Arena ^5%i^7][^3Skill ^5%i^7] ^5- ^3facing ^1no opponent\" 0 1", client->playernum, client->arena, client->skill));
		} else {
			client_t *cl2;
			if (sv.Arenas[client->arena].player1num == client->playernum) {
				cl2 = &svs.clients[sv.Arenas[client->arena].player2num];
			} else {
				cl2 = &svs.clients[sv.Arenas[client->arena].player1num];
			}
			Cmd_ExecuteString(va("location %i \"^7[^3Arena ^5%i^7][^3Skill ^5%i^7] ^5- ^3facing ^6%s\" 0 1", client->playernum, client->arena, client->skill, cl2->colourName));
		}
	}

	if (mod_levelsystem->integer) {
		client_t* client;
		client = cnum + svs.clients;
	
		// clean playername
    	char cleanName[64];
		memset(cleanName, 0, sizeof(cleanName));
        Q_strncpyz(cleanName, client->name, sizeof(cleanName));
        Q_CleanStr(cleanName );

		// Saving the configstring so we can easily edit it later on.
		// clear the current saved configstring
		memset(client->defaultconfigstr,0,sizeof(client->defaultconfigstr));
		char* replacewith = "XXXXXXXXXXXXX";
		char* mystring = Q_CleanStr(sv.configstrings[cnum + 544]);

		char* toreplace = va("%s", cleanName);
		char* somestring;
		if (strstr(mystring, "n\\\\t")) {
			somestring = string_replace(mystring, "n\\\\t", "n\\XXXXXXXXXXXXX\\t");
		} else {
			somestring = string_replace(mystring, toreplace, replacewith);
		}
	
		char mynewstring[256];
		Q_strncpyz(mynewstring, somestring, sizeof(mynewstring));
		Q_strncpyz(client->defaultconfigstr, mynewstring, sizeof(client->defaultconfigstr));


		// clear the current customname
		memset(client->lastcustomname,0,sizeof(client->lastcustomname));
		
		if (strlen(cleanName) < 1) {
			Q_strncpyz(cleanName, "nameError", sizeof(cleanName));
		}

		// Set our name:
		char* mynewname = va("^7[^2%i^7]^3%s^1", client->level, cleanName); //The ^1 at the end will affect the kills/deaths on the mini scoreboard

		// Set the new custom name
		Q_strncpyz(client->lastcustomname, mynewname, sizeof(client->lastcustomname));

		// Finally tell the server to use the new name
		client->customname = qtrue;

		// Shoot a location update for the new name
		Cmd_ExecuteString(va("location %i \"%s ^3Level:^7[^2%i^7/^2999^7] ^4- ^3XP:^7[^2%i^7/^25000^7] ^4- ^3Spree: ^2%i\" 0 1", client->clientgamenum, client->name, client->level, client->experience, client->kills));
	}

	if (mod_jumpSpecAnnouncer->integer) {

		// Save the client cid
        client_t* 	client;
        client = cnum + svs.clients;
		client->cid = cnum;

		// clean playername
    	char cleanName[64];
        Q_strncpyz(cleanName, client->name, sizeof(cleanName));
        Q_CleanStr(cleanName );

		// check if this is called on a namechange, currspecd etc must exist
		if (client->cidCurrSpecd != -1) {
			client_t *speccedClient = client->cidCurrSpecd + svs.clients;
			removeFromClientsList(client);
			addToClientsList(speccedClient, client);
			//forceLocationUpdate(speccedClient); // FIXME
		}

		// Set it to 0 just in case
		memset(client->nameConfigString,0,sizeof(client->nameConfigString));

		// Save a config string for easy name editing
		char* mystring = Q_CleanStr(sv.configstrings[cnum + 544]);
		char* toreplace = va("%s", cleanName);
		char* replacewith = "XXXXXXXXXXXXX"; // This is what we're replacing in the future
		char* somestring = string_replace(mystring, toreplace, replacewith);
		char mynewstring[256];
		Q_strncpyz(mynewstring, somestring, sizeof(mynewstring));
		Q_strncpyz(client->nameConfigString, mynewstring, sizeof(client->nameConfigString));

		// Save the clients name for future namechange updates
		Q_strncpyz(client->lastName, cleanName, 32);
	}
}



/////////////////////////////////////////////////////////////////////
// EV_ClientConnect
/////////////////////////////////////////////////////////////////////
void EV_ClientConnect(int cnum)
{
	if (mod_1v1arena->integer) {
        client_t* client;
        client = cnum + svs.clients;
		client->arena = 99;
		client->playernum = cnum;
		client->clientgamenum = cnum;
		client->arenaKills = 0;
		client->arenaDeaths = 0;
	}

    if (mod_1v1arena->integer) {
        fileHandle_t   file;
        char           buffer[MAX_STRING_CHARS];
        client_t* client;
        client = cnum + svs.clients;

        client->clientgamenum = cnum;
		client->weaponGiven = qfalse;

        char *qpath;
        char* guid = Info_ValueForKey(client->userinfo, "cl_guid");
        int            len;

        if (!guid){ // return if theres no guid to use.
            return;
        }
        // open the data file
        qpath = va("1v1arena/%s.txt", guid);
        FS_FOpenFileByMode(qpath, &file, FS_READ);

        // if not valid
        if (!file) {
			// The clients first connection, set the defaults
			client->skill = 1000;
			client->preference = ARENA_PRIMARY;
			client->primary = ARENA_LR300;
			client->secondary = ARENA_UMP45;
			client->pistol = ARENA_DEAGLE;
			client->sniper = ARENA_SR8;
			Cmd_ExecuteString(va("say \"^5[Arena MOD] ^7initialized for %s \"",client->name));
            return;
        }

        // read the file in the buffer
		memset(buffer, 0, sizeof(buffer));
        len = FS_Read(buffer, sizeof(buffer), file);
        if (len > 0) {
            // copy back the data
            sscanf(buffer, "%i,%i,%i,%i,%i,%i", &client->skill, &client->preference, &client->primary, &client->secondary, &client->pistol, &client->sniper);
        }

        // close the file handle
        FS_FCloseFile(file);
        Cmd_ExecuteString(va("say \"^5[Arena MOD] ^7initialized for %s \"",client->name));
    }

    if (mod_levelsystem->integer) {
        fileHandle_t   file;
        char           buffer[MAX_STRING_CHARS];
        client_t* client;
        client = cnum + svs.clients;
        client->clientgamenum = cnum;

        char *qpath;
        char* guid = Info_ValueForKey(client->userinfo, "cl_guid");
        int            len;

        if (!guid){ // return if theres no guid to use.
            return;
        }
        // open the level file
        qpath = va("levelsystem/%s.txt", guid);
        FS_FOpenFileByMode(qpath, &file, FS_READ);

        // if not valid
        if (!file) {
            return;
        }

        // read the file in the buffer
		memset(buffer, 0, sizeof(buffer));
        len = FS_Read(buffer, sizeof(buffer), file);
        if (len > 0) {
            // copy back saved level and xp
            sscanf(buffer, "%i,%i", &client->level, &client->experience);
        }

        // close the file handle
        FS_FCloseFile(file);
        Cmd_ExecuteString(va("say \"^5[auth] ^7Levelsystem initialized for %s \"",client->name));
    }
	if (mod_jumpSpecAnnouncer->integer) {
        client_t* client;
        client = cnum + svs.clients;

		client->cidCurrSpecd = -1;
		client->cidLastSpecd = -1;
		
		Q_strncpyz(client->lastName, client->name, 32);
	}
}

/////////////////////////////////////////////////////////////////////
// EV_ClientDisconnect
/////////////////////////////////////////////////////////////////////
void EV_ClientDisconnect(int cnum)
{
    // Make sure the client is valid
    if(cnum < 0 || cnum > sv_maxclients->integer)
        return;

    if (mod_1v1arena->integer) {
        fileHandle_t   file;
        char           buffer[MAX_STRING_CHARS];
        client_t* client;
        client = cnum + svs.clients;
        char *qpath;
        char* guid = Info_ValueForKey(client->userinfo, "cl_guid");

        if (!guid){ // return if theres no guid to use.
            return;
        }

        // open the file
        qpath = va("1v1arena/%s.txt", guid);
        FS_FOpenFileByMode(qpath, &file, FS_WRITE);

        // if not valid
        if (!file) {
            return;
        }

        // compute the text to be stored in the .txt file
		memset(buffer, 0, sizeof(buffer));

        Com_sprintf(buffer, sizeof(buffer), "%i,%i,%i,%i,%i,%i", client->skill, client->preference, client->primary, client->secondary, client->pistol, client->sniper);

        // Write the data
        FS_Write(buffer, strlen(buffer), file);
        FS_FCloseFile(file);
    }

	if (mod_jumpSpecAnnouncer->integer) {
        client_t* client;
        client = cnum + svs.clients;
		removeFromClientsList(client);
	}

    if (mod_levelsystem->integer) {
        fileHandle_t   file;
        char           buffer[MAX_STRING_CHARS];
        client_t* client;
        client = cnum + svs.clients;
        char *qpath;
        char* guid = Info_ValueForKey(client->userinfo, "cl_guid");

        if (!guid){ // return if theres no guid to use.
            return;
        }

        // open the level file
        qpath = va("levelsystem/%s.txt", guid);
        FS_FOpenFileByMode(qpath, &file, FS_WRITE);

        // if not valid
        if (!file) {
            return;
        }

        // compute the text to be stored in the .txt file
		memset(buffer, 0, sizeof(buffer));
        Com_sprintf(buffer, sizeof(buffer), "%i,%i", client->level, client->experience);
    
        // write the client level and xp and close
        FS_Write(buffer, strlen(buffer), file);
        FS_FCloseFile(file);
    }
	if (mod_jumpSpecAnnouncer->integer) {
        client_t* client;
        client = cnum + svs.clients;
		removeFromClientsList(client);
	}
}

/////////////////////////////////////////////////////////////////////
// EV_ClientBegin
/////////////////////////////////////////////////////////////////////
void EV_ClientBegin(int cnum)
{
	if (mod_levelsystem->integer) {

		client_t* client;
		client = cnum + svs.clients;

		// clear the current customname
		memset(client->lastcustomname,0,sizeof(client->lastcustomname));

		// clean playername
    	char cleanName[64];
        Q_strncpyz(cleanName, client->name, sizeof(cleanName));
        Q_CleanStr(cleanName);
	
		if (strlen(cleanName) < 1) {
		Q_strncpyz(cleanName, "nameError", sizeof(cleanName));
		}

		// Set our name:
		char* mynewname = va("^7[^2%i^7]^3%s^1", client->level, cleanName); //The ^1 at the end will affect the kills/deaths on the mini scoreboard

		// Set the new custom name
		Q_strncpyz(client->lastcustomname, mynewname, sizeof(client->lastcustomname));

		// Shoot a location update for the new name
		Cmd_ExecuteString(va("location %i \"%s ^3Level:^7[^2%i^7/^2999^7] ^4- ^3XP:^7[^2%i^7/^25000^7] ^4- ^3Spree: ^2%i\" 0 1", client->clientgamenum, client->name, client->level, client->experience, client->kills));
	}
	if (mod_jumpSpecAnnouncer->integer) {

		// Save the client cid
        client_t* 	client;
        client = cnum + svs.clients;
		client->cid = cnum;

		// clean playername
    	char cleanName[64];
        Q_strncpyz(cleanName, client->name, sizeof(cleanName));
        Q_CleanStr(cleanName );

		// Set it to 0 just in case
		memset(client->nameConfigString,0,sizeof(client->nameConfigString));

		// Save a config string for easy name editing
		char* mystring = Q_CleanStr(sv.configstrings[cnum + 544]);
		char* toreplace = va("%s", cleanName);
		char* replacewith = "XXXXXXXXXXXXX"; // This is what we're replacing in the future
		char* somestring = string_replace(mystring, toreplace, replacewith);
		char mynewstring[256];
		Q_strncpyz(mynewstring, somestring, sizeof(mynewstring));
		Q_strncpyz(client->nameConfigString, mynewstring, sizeof(client->nameConfigString));
	}
}

/////////////////////////////////////////////////////////////////////
// EV_ClientKill
/////////////////////////////////////////////////////////////////////
void EV_ClientKill(int cnum, int target)
{
	// Make sure the client is valid
	if(cnum < 0 || cnum > sv_maxclients->integer)
		return;

	// Make sure the target is valid
	if(target < 0 || target > sv_maxclients->integer)
		return;

	if (mod_levelsystem->integer){
		client_t* klr = cnum + svs.clients;
		client_t* kld = target + svs.clients;

		// Do not give kills/xp to suicide
		if (cnum == target) {
			klr->kills = 0;
			return;
		}

		// set the killing spree for the person that died to 0
		kld->kills = 0;

		// update the kills of the killer
		int currkills = klr->kills;
		klr->kills = klr->kills + 1;

		// handle killing sprees now
		if ((currkills + 1) == 4) {
			Cmd_ExecuteString(va("say \"%s ^3is on a ^2Rampage! ^6-> ^54 ^3Kills in a row!\"", klr->name));
			Cmd_ExecuteString(va("exec killing_spree_4.txt"));
		}
		if ((currkills + 1) == 7) {
			Cmd_ExecuteString(va("say \"%s ^3is on a ^2Unstoppable! ^6-> ^57 ^3Kills in a row!\"", klr->name));
			Cmd_ExecuteString(va("exec killing_spree_7.txt"));
		}
		if ((currkills + 1) == 12) {
			Cmd_ExecuteString(va("say \"%s ^3is ^2Dominating! ^6-> ^512 ^3Kills in a row!\"", klr->name));
			Cmd_ExecuteString(va("exec killing_spree_12.txt"));
		}
		if ((currkills + 1) == 15) {
			Cmd_ExecuteString(va("say \"%s ^3is ^2Godlike! ^6-> ^515 ^3Kills in a row!\"", klr->name));
			Cmd_ExecuteString(va("exec killing_spree_15.txt"));
		}
		if ((currkills + 1) == 20) {
			Cmd_ExecuteString(va("say \"%s ^3is ^1WICKEDSICK!!! ^6-> ^520 ^3Kills in a row!\"", klr->name));
			Cmd_ExecuteString(va("exec killing_spree_20.txt"));
		}
		// die if the client is maxlevel already
		if (klr->level == 999){
			return;
		}

		Cmd_ExecuteString(va("location %i \"%s ^3Level:^7[^2%i^7/^2999^7] ^4- ^3XP:^7[^2%i^7/^25000^7] ^4- ^3Spree: ^2%i\" 0 1", cnum, kld->name, kld->level, kld->experience, kld->kills));

		if ((klr->experience + 200) >= 5000) {
			klr->experience = 0;
			klr->level = (klr->level + 1);
			Cmd_ExecuteString(va("bigtext \"%s ^3has just ^2Leveled UP! ^3Level:^7[^2%i^3/^5999^7]\"", klr->name, klr->level));
			Cmd_ExecuteString(va("exec levelup.txt"));
			// clear the current customname
			memset(klr->lastcustomname,0,sizeof(klr->lastcustomname));
			// clean playername
    		char cleanName[64];
        	Q_strncpyz(cleanName, klr->name, sizeof(cleanName));
        	Q_CleanStr(cleanName );
			// Set our name:
			char* mynewname = va("^7[^2%i^7]^3%s^1", klr->level, cleanName); //The ^1 at the end will affect the kills/deaths on the mini scoreboard
			// Set the new custom name
			Q_strncpyz(klr->lastcustomname, mynewname, sizeof(klr->lastcustomname));
			// Shoot a location update for the new name
			Cmd_ExecuteString(va("location %i \"%s ^3Level:^7[^2%i^7/^2999^7] ^4- ^3XP:^7[^2%i^7/^25000^7] ^4- ^3Spree: ^2%i\" 0 1", cnum, klr->name, klr->level, klr->experience, klr->kills));
		}
		else {
			klr->experience = klr->experience + 200;
			SV_SendServerCommand(klr, "cp \"^2+^7200^2xp\"");
			Cmd_ExecuteString(va("location %i \"%s ^3Level:^7[^2%i^7/^2999^7] ^4- ^3XP:^7[^2%i^7/^25000^7] ^4- ^3Spree: ^2%i\" 0 1", cnum, klr->name, klr->level, klr->experience, klr->kills));
		}
	}
}


//==============================================

static int	FloatAsInt( float f ) {
	union
	{
	    int i;
	    float f;
	} temp;
	
	temp.f = f;
	return temp.i;
}

/*
====================
SV_GameSystemCalls

The module is making a system call
====================
*/
intptr_t SV_GameSystemCalls( intptr_t *args ) {
	switch( args[0] ) {
	case G_PRINT:
		Com_Printf( "%s", (const char*)VMA(1) );
		return 0;
	case G_ERROR:
		Com_Error( ERR_DROP, "%s", (const char*)VMA(1) );
		return 0;
	case G_MILLISECONDS:
		return Sys_Milliseconds();
	case G_CVAR_REGISTER:
		Cvar_Register( VMA(1), VMA(2), VMA(3), args[4] ); 
		return 0;
	case G_CVAR_UPDATE:
		Cvar_Update( VMA(1) );
		return 0;
	case G_CVAR_SET:
	    // let's exclude some game module cvars
        if (!Q_stricmp((char *)VMA(1), "sv_fps")) {
            return 0;
        }
		Cvar_Set( (const char *)VMA(1), (const char *)VMA(2) );
		return 0;
	case G_CVAR_VARIABLE_INTEGER_VALUE:
		return Cvar_VariableIntegerValue( (const char *)VMA(1) );
	case G_CVAR_VARIABLE_STRING_BUFFER:
		Cvar_VariableStringBuffer( VMA(1), VMA(2), args[3] );
		return 0;
	case G_ARGC:
		return Cmd_Argc();
	case G_ARGV:
		Cmd_ArgvBuffer( args[1], VMA(2), args[3] );
		return 0;
	case G_SEND_CONSOLE_COMMAND:
		Cbuf_ExecuteText( args[1], VMA(2) );
		return 0;

	case G_FS_FOPEN_FILE:
		return FS_FOpenFileByMode( VMA(1), VMA(2), args[3] );
	case G_FS_READ:
		FS_Read2( VMA(1), args[2], args[3] );
		return 0;
	case G_FS_WRITE:
		FS_Write( VMA(1), args[2], args[3] );
		return 0;
	case G_FS_FCLOSE_FILE:
		FS_FCloseFile( args[1] );
		return 0;
	case G_FS_GETFILELIST:
		return FS_GetFileList( VMA(1), VMA(2), VMA(3), args[4] );
	case G_FS_SEEK:
		return FS_Seek( args[1], args[2], args[3] );

	case G_LOCATE_GAME_DATA:
		SV_LocateGameData( VMA(1), args[2], args[3], VMA(4), args[5] );
		return 0;
	case G_DROP_CLIENT:
		SV_GameDropClient( args[1], VMA(2) );
		return 0;

#ifdef USE_AUTH
	case G_AUTH_DROP_CLIENT:
		SV_Auth_GameDropClient( args[1], VMA(2), VMA(3) );
		return 0;
#endif
		
	case G_SEND_SERVER_COMMAND:
		SV_GameSendServerCommand( args[1], VMA(2) );
		return 0;
	case G_LINKENTITY:
		SV_LinkEntity( VMA(1) );
		return 0;
	case G_UNLINKENTITY:
		SV_UnlinkEntity( VMA(1) );
		return 0;
	case G_ENTITIES_IN_BOX:
		return SV_AreaEntities( VMA(1), VMA(2), VMA(3), args[4] );
	case G_ENTITY_CONTACT:
		return SV_EntityContact( VMA(1), VMA(2), VMA(3), /*int capsule*/ qfalse );
	case G_ENTITY_CONTACTCAPSULE:
		return SV_EntityContact( VMA(1), VMA(2), VMA(3), /*int capsule*/ qtrue );
	case G_TRACE:
		SV_Trace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], /*int capsule*/ qfalse );
		if (((trace_t*)(VMA(1)))->entityNum > sv_maxclients->integer && mod_nofallDamage->integer) {
			((trace_t*)(VMA(1)))->surfaceFlags |= SURF_NODAMAGE;
		}
		if (((trace_t*)(VMA(1)))->entityNum > sv_maxclients->integer && mod_slickSurfaces->integer) {
			((trace_t*)(VMA(1)))->surfaceFlags |= SURF_SLICK;
		}
		return 0;
	case G_TRACECAPSULE:
		SV_Trace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], /*int capsule*/ qtrue );
		return 0;
	case G_POINT_CONTENTS:
		return SV_PointContents( VMA(1), args[2] );
	case G_SET_BRUSH_MODEL:
		SV_SetBrushModel( VMA(1), VMA(2) );
		return 0;
	case G_IN_PVS:
		return SV_inPVS( VMA(1), VMA(2) );
	case G_IN_PVS_IGNORE_PORTALS:
		return SV_inPVSIgnorePortals( VMA(1), VMA(2) );

	case G_SET_CONFIGSTRING:
		SV_SetConfigstring( args[1], VMA(2) );
		return 0;
	case G_GET_CONFIGSTRING:
		SV_GetConfigstring( args[1], VMA(2), args[3] );
		return 0;
	case G_SET_USERINFO:
		SV_SetUserinfo( args[1], VMA(2) );
		return 0;
	case G_GET_USERINFO:
		SV_GetUserinfo( args[1], VMA(2), args[3] );
		return 0;
	case G_GET_SERVERINFO:
		SV_GetServerinfo( VMA(1), args[2] );
		return 0;
	case G_ADJUST_AREA_PORTAL_STATE:
		SV_AdjustAreaPortalState( VMA(1), args[2] );
		return 0;
	case G_AREAS_CONNECTED:
		return CM_AreasConnected( args[1], args[2] );

	case G_BOT_ALLOCATE_CLIENT:
		return SV_BotAllocateClient();
	case G_BOT_FREE_CLIENT:
		SV_BotFreeClient( args[1] );
		return 0;

	case G_GET_USERCMD:
		SV_GetUsercmd( args[1], VMA(2) );
		return 0;
	case G_GET_ENTITY_TOKEN:
		{
			const char	*s;

			s = COM_Parse( &sv.entityParsePoint );
			Q_strncpyz( VMA(1), s, args[2] );
			if ( !sv.entityParsePoint && !s[0] ) {
				return qfalse;
			} else {
				return qtrue;
			}
		}

	case G_DEBUG_POLYGON_CREATE:
		return BotImport_DebugPolygonCreate( args[1], args[2], VMA(3) );
	case G_DEBUG_POLYGON_DELETE:
		BotImport_DebugPolygonDelete( args[1] );
		return 0;
	case G_REAL_TIME:
		return Com_RealTime( VMA(1) );
	case G_SNAPVECTOR:
		Sys_SnapVector( VMA(1) );
		return 0;

		//====================================

	case BOTLIB_SETUP:
		return SV_BotLibSetup();
	case BOTLIB_SHUTDOWN:
		return SV_BotLibShutdown();
	case BOTLIB_LIBVAR_SET:
		return botlib_export->BotLibVarSet( VMA(1), VMA(2) );
	case BOTLIB_LIBVAR_GET:
		return botlib_export->BotLibVarGet( VMA(1), VMA(2), args[3] );

	case BOTLIB_PC_ADD_GLOBAL_DEFINE:
		return botlib_export->PC_AddGlobalDefine( VMA(1) );
	case BOTLIB_PC_LOAD_SOURCE:
		return botlib_export->PC_LoadSourceHandle( VMA(1) );
	case BOTLIB_PC_FREE_SOURCE:
		return botlib_export->PC_FreeSourceHandle( args[1] );
	case BOTLIB_PC_READ_TOKEN:
		return botlib_export->PC_ReadTokenHandle( args[1], VMA(2) );
	case BOTLIB_PC_SOURCE_FILE_AND_LINE:
		return botlib_export->PC_SourceFileAndLine( args[1], VMA(2), VMA(3) );

	case BOTLIB_START_FRAME:
		return botlib_export->BotLibStartFrame( VMF(1) );
	case BOTLIB_LOAD_MAP:
		return botlib_export->BotLibLoadMap( VMA(1) );
	case BOTLIB_UPDATENTITY:
		return botlib_export->BotLibUpdateEntity( args[1], VMA(2) );
	case BOTLIB_TEST:
		return botlib_export->Test( args[1], VMA(2), VMA(3), VMA(4) );

	case BOTLIB_GET_SNAPSHOT_ENTITY:
		return SV_BotGetSnapshotEntity( args[1], args[2] );
	case BOTLIB_GET_CONSOLE_MESSAGE:
		return SV_BotGetConsoleMessage( args[1], VMA(2), args[3] );
	case BOTLIB_USER_COMMAND:
		SV_ClientThink( &svs.clients[args[1]], VMA(2) );
		return 0;

	case BOTLIB_AAS_BBOX_AREAS:
		return botlib_export->aas.AAS_BBoxAreas( VMA(1), VMA(2), VMA(3), args[4] );
	case BOTLIB_AAS_AREA_INFO:
		return botlib_export->aas.AAS_AreaInfo( args[1], VMA(2) );
	case BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL:
		return botlib_export->aas.AAS_AlternativeRouteGoals( VMA(1), args[2], VMA(3), args[4], args[5], VMA(6), args[7], args[8] );
	case BOTLIB_AAS_ENTITY_INFO:
		botlib_export->aas.AAS_EntityInfo( args[1], VMA(2) );
		return 0;

	case BOTLIB_AAS_INITIALIZED:
		return botlib_export->aas.AAS_Initialized();
	case BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX:
		botlib_export->aas.AAS_PresenceTypeBoundingBox( args[1], VMA(2), VMA(3) );
		return 0;
	case BOTLIB_AAS_TIME:
		return FloatAsInt( botlib_export->aas.AAS_Time() );

	case BOTLIB_AAS_POINT_AREA_NUM:
		return botlib_export->aas.AAS_PointAreaNum( VMA(1) );
	case BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX:
		return botlib_export->aas.AAS_PointReachabilityAreaIndex( VMA(1) );
	case BOTLIB_AAS_TRACE_AREAS:
		return botlib_export->aas.AAS_TraceAreas( VMA(1), VMA(2), VMA(3), VMA(4), args[5] );

	case BOTLIB_AAS_POINT_CONTENTS:
		return botlib_export->aas.AAS_PointContents( VMA(1) );
	case BOTLIB_AAS_NEXT_BSP_ENTITY:
		return botlib_export->aas.AAS_NextBSPEntity( args[1] );
	case BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY:
		return botlib_export->aas.AAS_ValueForBSPEpairKey( args[1], VMA(2), VMA(3), args[4] );
	case BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY:
		return botlib_export->aas.AAS_VectorForBSPEpairKey( args[1], VMA(2), VMA(3) );
	case BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY:
		return botlib_export->aas.AAS_FloatForBSPEpairKey( args[1], VMA(2), VMA(3) );
	case BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY:
		return botlib_export->aas.AAS_IntForBSPEpairKey( args[1], VMA(2), VMA(3) );

	case BOTLIB_AAS_AREA_REACHABILITY:
		return botlib_export->aas.AAS_AreaReachability( args[1] );

	case BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA:
		return botlib_export->aas.AAS_AreaTravelTimeToGoalArea( args[1], VMA(2), args[3], args[4] );
	case BOTLIB_AAS_ENABLE_ROUTING_AREA:
		return botlib_export->aas.AAS_EnableRoutingArea( args[1], args[2] );
	case BOTLIB_AAS_PREDICT_ROUTE:
		return botlib_export->aas.AAS_PredictRoute( VMA(1), args[2], VMA(3), args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11] );

	case BOTLIB_AAS_SWIMMING:
		return botlib_export->aas.AAS_Swimming( VMA(1) );
	case BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT:
		return botlib_export->aas.AAS_PredictClientMovement( VMA(1), args[2], VMA(3), args[4], args[5],
			VMA(6), VMA(7), args[8], args[9], VMF(10), args[11], args[12], args[13] );

	case BOTLIB_EA_SAY:
		botlib_export->ea.EA_Say( args[1], VMA(2) );
		return 0;
	case BOTLIB_EA_SAY_TEAM:
		botlib_export->ea.EA_SayTeam( args[1], VMA(2) );
		return 0;
	case BOTLIB_EA_COMMAND:
		botlib_export->ea.EA_Command( args[1], VMA(2) );
		return 0;

	case BOTLIB_EA_ACTION:
		botlib_export->ea.EA_Action( args[1], args[2] );
		break;
	case BOTLIB_EA_GESTURE:
		botlib_export->ea.EA_Gesture( args[1] );
		return 0;
	case BOTLIB_EA_TALK:
		botlib_export->ea.EA_Talk( args[1] );
		return 0;
	case BOTLIB_EA_ATTACK:
		botlib_export->ea.EA_Attack( args[1] );
		return 0;
	case BOTLIB_EA_USE:
		botlib_export->ea.EA_Use( args[1] );
		return 0;
	case BOTLIB_EA_RESPAWN:
		botlib_export->ea.EA_Respawn( args[1] );
		return 0;
	case BOTLIB_EA_CROUCH:
		botlib_export->ea.EA_Crouch( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_UP:
		botlib_export->ea.EA_MoveUp( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_DOWN:
		botlib_export->ea.EA_MoveDown( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_FORWARD:
		botlib_export->ea.EA_MoveForward( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_BACK:
		botlib_export->ea.EA_MoveBack( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_LEFT:
		botlib_export->ea.EA_MoveLeft( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_RIGHT:
		botlib_export->ea.EA_MoveRight( args[1] );
		return 0;

	case BOTLIB_EA_SELECT_WEAPON:
		botlib_export->ea.EA_SelectWeapon( args[1], args[2] );
		return 0;
	case BOTLIB_EA_JUMP:
		botlib_export->ea.EA_Jump( args[1] );
		return 0;
	case BOTLIB_EA_DELAYED_JUMP:
		botlib_export->ea.EA_DelayedJump( args[1] );
		return 0;
	case BOTLIB_EA_MOVE:
		botlib_export->ea.EA_Move( args[1], VMA(2), VMF(3) );
		return 0;
	case BOTLIB_EA_VIEW:
		botlib_export->ea.EA_View( args[1], VMA(2) );
		return 0;

	case BOTLIB_EA_END_REGULAR:
		botlib_export->ea.EA_EndRegular( args[1], VMF(2) );
		return 0;
	case BOTLIB_EA_GET_INPUT:
		botlib_export->ea.EA_GetInput( args[1], VMF(2), VMA(3) );
		return 0;
	case BOTLIB_EA_RESET_INPUT:
		botlib_export->ea.EA_ResetInput( args[1] );
		return 0;

	case BOTLIB_AI_LOAD_CHARACTER:
		return botlib_export->ai.BotLoadCharacter( VMA(1), VMF(2) );
	case BOTLIB_AI_FREE_CHARACTER:
		botlib_export->ai.BotFreeCharacter( args[1] );
		return 0;
	case BOTLIB_AI_CHARACTERISTIC_FLOAT:
		return FloatAsInt( botlib_export->ai.Characteristic_Float( args[1], args[2] ) );
	case BOTLIB_AI_CHARACTERISTIC_BFLOAT:
		return FloatAsInt( botlib_export->ai.Characteristic_BFloat( args[1], args[2], VMF(3), VMF(4) ) );
	case BOTLIB_AI_CHARACTERISTIC_INTEGER:
		return botlib_export->ai.Characteristic_Integer( args[1], args[2] );
	case BOTLIB_AI_CHARACTERISTIC_BINTEGER:
		return botlib_export->ai.Characteristic_BInteger( args[1], args[2], args[3], args[4] );
	case BOTLIB_AI_CHARACTERISTIC_STRING:
		botlib_export->ai.Characteristic_String( args[1], args[2], VMA(3), args[4] );
		return 0;

	case BOTLIB_AI_ALLOC_CHAT_STATE:
		return botlib_export->ai.BotAllocChatState();
	case BOTLIB_AI_FREE_CHAT_STATE:
		botlib_export->ai.BotFreeChatState( args[1] );
		return 0;
	case BOTLIB_AI_QUEUE_CONSOLE_MESSAGE:
		botlib_export->ai.BotQueueConsoleMessage( args[1], args[2], VMA(3) );
		return 0;
	case BOTLIB_AI_REMOVE_CONSOLE_MESSAGE:
		botlib_export->ai.BotRemoveConsoleMessage( args[1], args[2] );
		return 0;
	case BOTLIB_AI_NEXT_CONSOLE_MESSAGE:
		return botlib_export->ai.BotNextConsoleMessage( args[1], VMA(2) );
	case BOTLIB_AI_NUM_CONSOLE_MESSAGE:
		return botlib_export->ai.BotNumConsoleMessages( args[1] );
	case BOTLIB_AI_INITIAL_CHAT:
		botlib_export->ai.BotInitialChat( args[1], VMA(2), args[3], VMA(4), VMA(5), VMA(6), VMA(7), VMA(8), VMA(9), VMA(10), VMA(11) );
		return 0;
	case BOTLIB_AI_NUM_INITIAL_CHATS:
		return botlib_export->ai.BotNumInitialChats( args[1], VMA(2) );
	case BOTLIB_AI_REPLY_CHAT:
		return botlib_export->ai.BotReplyChat( args[1], VMA(2), args[3], args[4], VMA(5), VMA(6), VMA(7), VMA(8), VMA(9), VMA(10), VMA(11), VMA(12) );
	case BOTLIB_AI_CHAT_LENGTH:
		return botlib_export->ai.BotChatLength( args[1] );
	case BOTLIB_AI_ENTER_CHAT:
		botlib_export->ai.BotEnterChat( args[1], args[2], args[3] );
		return 0;
	case BOTLIB_AI_GET_CHAT_MESSAGE:
		botlib_export->ai.BotGetChatMessage( args[1], VMA(2), args[3] );
		return 0;
	case BOTLIB_AI_STRING_CONTAINS:
		return botlib_export->ai.StringContains( VMA(1), VMA(2), args[3] );
	case BOTLIB_AI_FIND_MATCH:
		return botlib_export->ai.BotFindMatch( VMA(1), VMA(2), args[3] );
	case BOTLIB_AI_MATCH_VARIABLE:
		botlib_export->ai.BotMatchVariable( VMA(1), args[2], VMA(3), args[4] );
		return 0;
	case BOTLIB_AI_UNIFY_WHITE_SPACES:
		botlib_export->ai.UnifyWhiteSpaces( VMA(1) );
		return 0;
	case BOTLIB_AI_REPLACE_SYNONYMS:
		botlib_export->ai.BotReplaceSynonyms( VMA(1), args[2] );
		return 0;
	case BOTLIB_AI_LOAD_CHAT_FILE:
		return botlib_export->ai.BotLoadChatFile( args[1], VMA(2), VMA(3) );
	case BOTLIB_AI_SET_CHAT_GENDER:
		botlib_export->ai.BotSetChatGender( args[1], args[2] );
		return 0;
	case BOTLIB_AI_SET_CHAT_NAME:
		botlib_export->ai.BotSetChatName( args[1], VMA(2), args[3] );
		return 0;

	case BOTLIB_AI_RESET_GOAL_STATE:
		botlib_export->ai.BotResetGoalState( args[1] );
		return 0;
	case BOTLIB_AI_RESET_AVOID_GOALS:
		botlib_export->ai.BotResetAvoidGoals( args[1] );
		return 0;
	case BOTLIB_AI_REMOVE_FROM_AVOID_GOALS:
		botlib_export->ai.BotRemoveFromAvoidGoals( args[1], args[2] );
		return 0;
	case BOTLIB_AI_PUSH_GOAL:
		botlib_export->ai.BotPushGoal( args[1], VMA(2) );
		return 0;
	case BOTLIB_AI_POP_GOAL:
		botlib_export->ai.BotPopGoal( args[1] );
		return 0;
	case BOTLIB_AI_EMPTY_GOAL_STACK:
		botlib_export->ai.BotEmptyGoalStack( args[1] );
		return 0;
	case BOTLIB_AI_DUMP_AVOID_GOALS:
		botlib_export->ai.BotDumpAvoidGoals( args[1] );
		return 0;
	case BOTLIB_AI_DUMP_GOAL_STACK:
		botlib_export->ai.BotDumpGoalStack( args[1] );
		return 0;
	case BOTLIB_AI_GOAL_NAME:
		botlib_export->ai.BotGoalName( args[1], VMA(2), args[3] );
		return 0;
	case BOTLIB_AI_GET_TOP_GOAL:
		return botlib_export->ai.BotGetTopGoal( args[1], VMA(2) );
	case BOTLIB_AI_GET_SECOND_GOAL:
		return botlib_export->ai.BotGetSecondGoal( args[1], VMA(2) );
	case BOTLIB_AI_CHOOSE_LTG_ITEM:
		return botlib_export->ai.BotChooseLTGItem( args[1], VMA(2), VMA(3), args[4] );
	case BOTLIB_AI_CHOOSE_NBG_ITEM:
		return botlib_export->ai.BotChooseNBGItem( args[1], VMA(2), VMA(3), args[4], VMA(5), VMF(6) );
	case BOTLIB_AI_TOUCHING_GOAL:
		return botlib_export->ai.BotTouchingGoal( VMA(1), VMA(2) );
	case BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE:
		return botlib_export->ai.BotItemGoalInVisButNotVisible( args[1], VMA(2), VMA(3), VMA(4) );
	case BOTLIB_AI_GET_LEVEL_ITEM_GOAL:
		return botlib_export->ai.BotGetLevelItemGoal( args[1], VMA(2), VMA(3) );
	case BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL:
		return botlib_export->ai.BotGetNextCampSpotGoal( args[1], VMA(2) );
	case BOTLIB_AI_GET_MAP_LOCATION_GOAL:
		return botlib_export->ai.BotGetMapLocationGoal( VMA(1), VMA(2) );
	case BOTLIB_AI_AVOID_GOAL_TIME:
		return FloatAsInt( botlib_export->ai.BotAvoidGoalTime( args[1], args[2] ) );
	case BOTLIB_AI_SET_AVOID_GOAL_TIME:
		botlib_export->ai.BotSetAvoidGoalTime( args[1], args[2], VMF(3));
		return 0;
	case BOTLIB_AI_INIT_LEVEL_ITEMS:
		botlib_export->ai.BotInitLevelItems();
		return 0;
	case BOTLIB_AI_UPDATE_ENTITY_ITEMS:
		botlib_export->ai.BotUpdateEntityItems();
		return 0;
	case BOTLIB_AI_LOAD_ITEM_WEIGHTS:
		return botlib_export->ai.BotLoadItemWeights( args[1], VMA(2) );
	case BOTLIB_AI_FREE_ITEM_WEIGHTS:
		botlib_export->ai.BotFreeItemWeights( args[1] );
		return 0;
	case BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC:
		botlib_export->ai.BotInterbreedGoalFuzzyLogic( args[1], args[2], args[3] );
		return 0;
	case BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC:
		botlib_export->ai.BotSaveGoalFuzzyLogic( args[1], VMA(2) );
		return 0;
	case BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC:
		botlib_export->ai.BotMutateGoalFuzzyLogic( args[1], VMF(2) );
		return 0;
	case BOTLIB_AI_ALLOC_GOAL_STATE:
		return botlib_export->ai.BotAllocGoalState( args[1] );
	case BOTLIB_AI_FREE_GOAL_STATE:
		botlib_export->ai.BotFreeGoalState( args[1] );
		return 0;

	case BOTLIB_AI_RESET_MOVE_STATE:
		botlib_export->ai.BotResetMoveState( args[1] );
		return 0;
	case BOTLIB_AI_ADD_AVOID_SPOT:
		botlib_export->ai.BotAddAvoidSpot( args[1], VMA(2), VMF(3), args[4] );
		return 0;
	case BOTLIB_AI_MOVE_TO_GOAL:
		botlib_export->ai.BotMoveToGoal( VMA(1), args[2], VMA(3), args[4] );
		return 0;
	case BOTLIB_AI_MOVE_IN_DIRECTION:
		return botlib_export->ai.BotMoveInDirection( args[1], VMA(2), VMF(3), args[4] );
	case BOTLIB_AI_RESET_AVOID_REACH:
		botlib_export->ai.BotResetAvoidReach( args[1] );
		return 0;
	case BOTLIB_AI_RESET_LAST_AVOID_REACH:
		botlib_export->ai.BotResetLastAvoidReach( args[1] );
		return 0;
	case BOTLIB_AI_REACHABILITY_AREA:
		return botlib_export->ai.BotReachabilityArea( VMA(1), args[2] );
	case BOTLIB_AI_MOVEMENT_VIEW_TARGET:
		return botlib_export->ai.BotMovementViewTarget( args[1], VMA(2), args[3], VMF(4), VMA(5) );
	case BOTLIB_AI_PREDICT_VISIBLE_POSITION:
		return botlib_export->ai.BotPredictVisiblePosition( VMA(1), args[2], VMA(3), args[4], VMA(5) );
	case BOTLIB_AI_ALLOC_MOVE_STATE:
		return botlib_export->ai.BotAllocMoveState();
	case BOTLIB_AI_FREE_MOVE_STATE:
		botlib_export->ai.BotFreeMoveState( args[1] );
		return 0;
	case BOTLIB_AI_INIT_MOVE_STATE:
		botlib_export->ai.BotInitMoveState( args[1], VMA(2) );
		return 0;

	case BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON:
		return botlib_export->ai.BotChooseBestFightWeapon( args[1], VMA(2) );
	case BOTLIB_AI_GET_WEAPON_INFO:
		botlib_export->ai.BotGetWeaponInfo( args[1], args[2], VMA(3) );
		return 0;
	case BOTLIB_AI_LOAD_WEAPON_WEIGHTS:
		return botlib_export->ai.BotLoadWeaponWeights( args[1], VMA(2) );
	case BOTLIB_AI_ALLOC_WEAPON_STATE:
		return botlib_export->ai.BotAllocWeaponState();
	case BOTLIB_AI_FREE_WEAPON_STATE:
		botlib_export->ai.BotFreeWeaponState( args[1] );
		return 0;
	case BOTLIB_AI_RESET_WEAPON_STATE:
		botlib_export->ai.BotResetWeaponState( args[1] );
		return 0;

	case BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION:
		return botlib_export->ai.GeneticParentsAndChildSelection(args[1], VMA(2), VMA(3), VMA(4), VMA(5));

	//@Barbatos
	#ifdef USE_AUTH
	case G_NET_STRINGTOADR:
		return NET_StringToAdr( VMA(1), VMA(2));
		
	case G_NET_SENDPACKET:
		{
			netadr_t addr;
			const char * destination = VMA(4);     
			
			NET_StringToAdr( destination, &addr );                                                                                                                                                                                                                                   
			NET_SendPacket( args[1], args[2], VMA(3), addr ); 
		}
		return 0;
	
	//case G_SYS_STARTPROCESS:
	//	Sys_StartProcess( VMA(1), VMA(2) );
	//	return 0;
		
	#endif
	
	case TRAP_MEMSET:
		Com_Memset( VMA(1), args[2], args[3] );
		return 0;

	case TRAP_MEMCPY:
		Com_Memcpy( VMA(1), VMA(2), args[3] );
		return 0;

	case TRAP_STRNCPY:
		strncpy( VMA(1), VMA(2), args[3] );
		return args[1];

	case TRAP_SIN:
		return FloatAsInt( sin( VMF(1) ) );

	case TRAP_COS:
		return FloatAsInt( cos( VMF(1) ) );

	case TRAP_ATAN2:
		return FloatAsInt( atan2( VMF(1), VMF(2) ) );

	case TRAP_SQRT:
		return FloatAsInt( sqrt( VMF(1) ) );

	case TRAP_MATRIXMULTIPLY:
		MatrixMultiply( VMA(1), VMA(2), VMA(3) );
		return 0;

	case TRAP_ANGLEVECTORS:
		AngleVectors( VMA(1), VMA(2), VMA(3), VMA(4) );
		return 0;

	case TRAP_PERPENDICULARVECTOR:
		PerpendicularVector( VMA(1), VMA(2) );
		return 0;

	case TRAP_FLOOR:
		return FloatAsInt( floor( VMF(1) ) );

	case TRAP_CEIL:
		return FloatAsInt( ceil( VMF(1) ) );

	default:
		Com_Error( ERR_DROP, "Bad game system trap: %ld", (long int) args[0] );
		break;
	}

	return -1;
}

/*
===============
SV_ShutdownGameProgs

Called every time a map changes
===============
*/
void SV_ShutdownGameProgs( void ) {
	if ( !gvm ) {
		return;
	}
	VM_Call( gvm, GAME_SHUTDOWN, qfalse );
	VM_Free( gvm );
	gvm = NULL;
}

/*
==================
SV_EntityFile_Path

TODO: handle string tokens for gametypes, server ports etc?
TODO/FIXME: fix any possible incorrect pathing in cvar values
==================
*/
const char *SV_EntityFile_Path( const char *in, const char *mapname ) {
	char fname[MAX_QPATH];

	Com_sprintf(fname, sizeof(fname), "maps/%s%s%s.ent", in, in[0] ? "/" : "", mapname);
	return va( "%s", fname );
}

/*
* Find the first occurrence of find in s.
*/
const char *Qrr_stristr( const char *s, const char *find)
{
  char c, sc;
  size_t len;

  if ((c = *find++) != 0)
  {
    if (c >= 'a' && c <= 'z')
    {
      c -= ('a' - 'A');
    }
    len = strlen(find);
    do
    {
      do
      {
        if ((sc = *s++) == 0)
          return NULL;
        if (sc >= 'a' && sc <= 'z')
        {
          sc -= ('a' - 'A');
        }
      } while (sc != c);
    } while (Q_stricmpn(s, find, len) != 0);
    s--;
  }
  return s;
}


/*
==================
SV_EntityFile_Read

==================
*/
static qboolean SV_EntityFile_Read( void ) {
	const char	*fname;
	qboolean	entor = qfalse;
	union {
		char	*c;
		void	*v;
	} f;
	long		len;
	char		*text;

	if ( !sv_ent_load->integer ) return qfalse;

	// load ent file if it exists
	fname = SV_EntityFile_Path( sv_ent_load_path->string, sv_mapname->string );
	if ( !FS_FileExists( fname ) ) return qfalse;

	len = FS_ReadFile( fname, &f.v );
	if ( f.c ) {
		text = f.c;

		if ( !Qrr_stristr( text, "{" ) || !Qrr_stristr( text, "}" ) ) {
			Com_Printf( "Entity file with missing brace, discarded: " S_COL_VAL "%s\n", fname );
			FS_FreeFile( f.v );
			return qfalse;
		}

		// start the entity parsing at the beginning
		CMod_OverrideEntityString( text, len );
		sv.entityParsePoint = CM_EntityString();
		Com_Printf( S_COL_BASE "Entities loaded from " S_COL_VAL "%s" S_COL_BASE ".\n", fname );

		entor = qtrue;
	} else {
		Com_Printf( "Empty entity file: " S_COL_VAL "%s\n", fname );
	}
	FS_FreeFile( f.v );

	return entor;
}


/*
==================
SV_EntityFile_Write

==================
*/
static void SV_EntityFile_Write(const qboolean entor) {
	fileHandle_t	fw;
	const char		*fname;

	// start the entity parsing at the beginning
	sv.entityParsePoint = CM_EntityString();

	// dump ent file if dumping is enabled and there's no override loaded
	if (entor || !sv_ent_dump->integer) return;

	fname = SV_EntityFile_Path(sv_ent_dump_path->string, sv_mapname->string);

	if ( FS_FileExists( fname ) ) return;

	fw = FS_FOpenFileWrite( fname );

	if ( fw ) {
		FS_Write( sv.entityParsePoint, strlen(sv.entityParsePoint), fw );
		Com_Printf( S_COL_BASE "Exported entities to file: " S_COL_VAL "%s" S_COL_BASE ".\n", fname );
		FS_FCloseFile( fw );
	}
}


/*
==================
SV_InitEntities

==================
*/
static void SV_InitEntities( void ) {
	SV_EntityFile_Write( SV_EntityFile_Read() );
}

/*
==================
SV_InitGameVM

Called for both a full init and a restart
==================
*/
static void SV_InitGameVM( qboolean restart ) {
	int		i;

	// start the entity parsing at the beginning
	SV_InitEntities();
	// sv.entityParsePoint = CM_EntityString();

	// clear all gentity pointers that might still be set from
	// a previous level
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=522
	//   now done before GAME_INIT call
	for ( i = 0 ; i < sv_maxclients->integer ; i++ ) {
		svs.clients[i].gentity = NULL;
	}
	
	// For entity tracking
	sv.lastEntityNum = 1010;
	sv.lastIndexNum = 255;

	// use the current msec count for a random seed
	// init for this gamestate
	VM_Call (gvm, GAME_INIT, sv.time, Com_Milliseconds(), restart);

	urtVersion vurt = getVersion();
	Com_Printf("Loading WeaponStrings... %s ", versionString[vurt]);
	switch(vurt)
	{
	default:
        break;
	case vunk:
		Com_Printf("[FAIL]\n **The actual version (%s) is not supported**\n", Cvar_VariableString("g_modversion"));
		break;
	case v42023:
		weaponString = weaponstring42;
		Com_Printf("[OK]\n");
		break;
	case v43:
	case v431:
	case v432:
	case v433:
	case v434:
		weaponString = weaponstring43;
		Com_Printf("[OK]\n");
		break;
	}

	if(!overrideQVMData())
	{
		Com_Printf("********************************\n");
		Com_Printf("Unable to override weapon data\n");
		Com_Printf("********************************\n");
	}

}



/*
===================
SV_RestartGameProgs

Called on a map_restart, but not on a normal map change
===================
*/
void SV_RestartGameProgs( void ) {
	if ( !gvm ) {
		return;
	}
	VM_Call( gvm, GAME_SHUTDOWN, qtrue );

	// do a restart instead of a free
	gvm = VM_Restart( gvm );
	if ( !gvm ) {
		Com_Error( ERR_FATAL, "VM_Restart on game failed" );
	}

	SV_InitGameVM( qtrue );
}


/*
===============
SV_InitGameProgs

Called on a normal map change, not on a map_restart
===============
*/
void SV_InitGameProgs( void ) {
	cvar_t	*var;
	//FIXME these are temp while I make bots run in vm
	extern int	bot_enable;
	int i;


	var = Cvar_Get( "bot_enable", "1", CVAR_LATCH );
	if ( var ) {
		bot_enable = var->integer;
	}
	else {
		bot_enable = 0;
	}

	// Barbatos - force a DNS lookup for the master servers
	// This way server admins don't have to restart their
	// servers when a master server IP changes.
	for ( i = 0 ; i < MAX_MASTER_SERVERS ; i++ ) {
		if ( !sv_master[i]->string[0] ) {
			continue;
		}
		sv_master[i]->modified = qtrue;
	}

	// load the dll or bytecode
	gvm = VM_Create( "qagame", SV_GameSystemCalls, Cvar_VariableValue( "vm_game" ) );
	if ( !gvm ) {
		Com_Error( ERR_FATAL, "VM_Create on game failed" );
	}

	SV_InitGameVM( qfalse );

}


/*
====================
SV_GameCommand

See if the current console command is claimed by the game
====================
*/
qboolean SV_GameCommand( void ) {
	if ( sv.state != SS_GAME ) {
		return qfalse;
	}

	return VM_Call( gvm, GAME_CONSOLE_COMMAND );
}


/*
====================
getVersion
====================
*/
urtVersion getVersion(void)
{
	int i;
	for(i = 0; i < vMAX; i++)
	{
		if(!strcmp(Cvar_VariableString("g_modversion"), versionString[i]))
			return i;
	}
	return vunk;
}
