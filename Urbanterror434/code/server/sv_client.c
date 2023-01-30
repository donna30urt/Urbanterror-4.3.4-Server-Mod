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
// sv_client.c -- server code for dealing with clients

#include "server.h"

static void SV_CloseDownload( client_t *cl );


/////////////////////////////////////////////////////////////////////
// MOD_SendCustomLocation
/////////////////////////////////////////////////////////////////////
void MOD_SendCustomLocation(client_t *cl, char *csstring, int index)
{
	if(!cl->state)
		return;

	if(index < 0 || index > 360)
	{
		Com_Printf("Index must be between 0 and 360\n");
		return;
	}
	index += 640;
	SV_SendCustomConfigString (cl, csstring, index);
}

/////////////////////////////////////////////////////////////////////
// MOD_ChangeLocation
/////////////////////////////////////////////////////////////////////
void MOD_ChangeLocation (client_t *cl, int changeto, int lock)
{
	//First of all we should unlock
	cl->cm.locationLocked = 0;

	if(changeto < 0 || changeto > 360)
	{
		Com_Printf("Location must be between 0 and 360\n");
		return;
	}
	SV_SendServerCommand(cl, "location %d", changeto);

	cl->cm.locationLocked = lock;
}

/////////////////////////////////////////////////////////////////////
// MOD_PlaySoundFile
/////////////////////////////////////////////////////////////////////
void MOD_PlaySoundFile (client_t *cl, char *file)
{
	// Set config string
	SV_SendCustomConfigString(cl, file, 543);
	// Make a delay to reproduce
	cl->cm.delayedSound = sv.snapshotCounter+15;
}

/////////////////////////////////////////////////////////////////////
// MOD_SetExternalEvent
/////////////////////////////////////////////////////////////////////
void MOD_SetExternalEvent (client_t *cl, entity_event_t event, int eventarg)
{
	playerState_t *ps;
	int bits;

	ps = SV_GameClientNum(cl - svs.clients);
	bits = ps->externalEvent & EV_EVENT_BITS;
	bits = (bits + EV_EVENT_BIT1) & EV_EVENT_BITS;
	ps->externalEvent =  event | bits; // 59 Is the event num for reproduce sounds
	ps->externalEventParm = eventarg;
	ps->externalEventTime = svs.time;
}

/////////////////////////////////////////////////////////////////////
// MOD_AddHealth
/////////////////////////////////////////////////////////////////////
void MOD_AddHealth(client_t *cl, int value) {

	gentity_t     *ent;
	playerState_t *ps;

	ps = SV_GameClientNum(cl - svs.clients);
	ent = (gentity_t *)SV_GentityNum(cl - svs.clients);
	if(ent->health <= 0 || ps->persistant[PERS_TEAM] == TEAM_SPECTATOR || *(int*)((void*)ps+gclientOffsets[getVersion()][OFFSET_TEAM]) == TEAM_SPECTATOR)
		return;

	if(value + ent->health > 100) {
		ent->health = 100;
		return;
	}
    if(value + ent->health <= 0) {
		ent->health = 1;
		return;
	}

	ent->health += value;
}

/////////////////////////////////////////////////////////////////////
// MOD_SetHealth
/////////////////////////////////////////////////////////////////////
void MOD_SetHealth(client_t *cl, int value) {

	gentity_t     *ent;
	playerState_t *ps;

	ps = SV_GameClientNum(cl - svs.clients);
	ent = (gentity_t *)SV_GentityNum(cl - svs.clients);

	if(ent->health <= 0 || ps->persistant[PERS_TEAM] == TEAM_SPECTATOR || *(int*)((void*)ps+gclientOffsets[getVersion()][OFFSET_TEAM]) == TEAM_SPECTATOR)
		return;

	if(value > 100) {
		ent->health = 100;
		return;
	}

    if(value <= 0) {
		ent->health = 1;
		return;
	}

	ent->health = value;
}

/////////////////////////////////////////////////////////////////////
// SV_ClientIsMoving
/////////////////////////////////////////////////////////////////////
int SV_ClientIsMoving(client_t *cl) {

	playerState_t *ps;
	ps = SV_GameClientNum(cl - svs.clients);

	if(ps->velocity[0] == 0 && ps->velocity[1] == 0 && ps->velocity[2] == 0) {
		return 0;
	}

	return 1;
}

/////////////////////////////////////////////////////////////////////
// SV_CleanName
/////////////////////////////////////////////////////////////////////
char *SV_CleanName(char *name) {

	static char cleaned[MAX_NAME_LENGTH];
	int i, j = 0;

	if ( !Q_stricmp( name, "UnnamedPlayer" ) ) {
		sprintf(cleaned, "%s^7", "UnnamedPlayer");
		return cleaned;
	}

	if ( !Q_stricmp( name, "" ) ) {
		sprintf(cleaned, "%s^7", "UnnamedPlayer");
		return cleaned;
	}

	for (i = 0; i < strlen(name) && j < MAX_NAME_LENGTH-1; i++) {
		if (name[i] >= 39 && name[i] <= 126)
            if(name[i] != ' ' && name[i] != '\\')
                cleaned[j++] = name[i];
	}

	cleaned[j] = 0;
	return cleaned;
}

/////////////////////////////////////////////////////////////////////
// SV_ApproveGuid
// --------------
// A cl_guid string must have length 32 and consist of characters '0'
// through '9' and 'A' through 'F'.
/////////////////////////////////////////////////////////////////////
qboolean SV_ApproveGuid(const char *guid) {

    int    i;
    int    length;
    char   c;
    
    if (mod_checkClientGuid->integer > 0) {

        length = strlen(guid); 
        if (length != 32) { 
            return qfalse; 
        }

        for (i = 31; i >= 0;) {
            c = guid[i--];
            if (!(('0' <= c && c <= '9') || ('A' <= c && c <= 'F'))) {
                return qfalse;
            }
        }
    }
    return qtrue;
}

/*
=================
SV_GetChallenge

A "getchallenge" OOB command has been received
Returns a challenge number that can be used
in a subsequent connectResponse command.
We do this to prevent denial of service attacks that
flood the server with invalid connection IPs.  With a
challenge, they must give a valid IP address.

If we are authorizing, a challenge request will cause a packet
to be sent to the authorize server.

When an authorizeip is returned, a challenge response will be
sent to that ip.
=================
*/
void SV_GetChallenge(netadr_t from) {

    int          i;
    int          oldest;
    int          oldestTime;
    challenge_t  *challenge;

    oldest = 0;
    oldestTime = 0x7fffffff;

    // see if we already have a challenge for this ip
    challenge = &svs.challenges[0];
    for (i = 0 ; i < MAX_CHALLENGES ; i++, challenge++) {

        if (!challenge->connected && NET_CompareAdr(from, challenge->adr)) {
            break;
        }

        if (challenge->time < oldestTime) {
            oldestTime = challenge->time;
            oldest = i;
        }

    }

    if (i == MAX_CHALLENGES) {

        // this is the first time this client has asked for a challenge
        challenge = &svs.challenges[oldest];

        challenge->challenge = ((rand() << 16) ^ rand()) ^ svs.time;
        challenge->adr = from;
        challenge->pingTime = -1;
        challenge->firstTime = svs.time;
        challenge->time = svs.time;
        challenge->connected = qfalse;
        i = oldest;

    }

    if (challenge->pingTime == -1) {
        challenge->pingTime = svs.time;
    }

    NET_OutOfBandPrint(NS_SERVER, from, "challengeResponse %i", challenge->challenge);
    return;

}

/*
====================
SV_AuthorizeIpPacket

A packet has been returned from the authorize server.
If we have a challenge adr for that ip, send the
challengeResponse to it
====================
*/
void SV_AuthorizeIpPacket( netadr_t from ) {
	int		challenge;
	int		i;
	char	*s;
	char	*r;

	if ( !NET_CompareBaseAdr( from, svs.authorizeAddress ) ) {
		Com_Printf( "SV_AuthorizeIpPacket: not from authorize server\n" );
		return;
	}

	challenge = atoi( Cmd_Argv( 1 ) );

	for (i = 0 ; i < MAX_CHALLENGES ; i++) {
		if ( svs.challenges[i].challenge == challenge ) {
			break;
		}
	}
	if ( i == MAX_CHALLENGES ) {
		Com_Printf( "SV_AuthorizeIpPacket: challenge not found\n" );
		return;
	}

	// send a packet back to the original client
	if (svs.challenges[i].pingTime == -1) {
		svs.challenges[i].pingTime = svs.time; 
	}

	s = Cmd_Argv( 2 );
	r = Cmd_Argv( 3 );			// reason

	if ( !Q_stricmp( s, "demo" ) ) {
		// they are a demo client trying to connect to a real server
		NET_OutOfBandPrint( NS_SERVER, svs.challenges[i].adr, "print\nServer is not a demo server\n" );
		// clear the challenge record so it won't timeout and let them through
		Com_Memset( &svs.challenges[i], 0, sizeof( svs.challenges[i] ) );
		return;
	}
	if ( !Q_stricmp( s, "accept" ) ) {
		NET_OutOfBandPrint( NS_SERVER, svs.challenges[i].adr, 
			"challengeResponse %i", svs.challenges[i].challenge );
		return;
	}
	if ( !Q_stricmp( s, "unknown" ) ) {
		if (!r) {
			NET_OutOfBandPrint( NS_SERVER, svs.challenges[i].adr, "print\nAwaiting CD key authorization\n" );
		} else {
			NET_OutOfBandPrint( NS_SERVER, svs.challenges[i].adr, "print\n%s\n", r);
		}
		// clear the challenge record so it won't timeout and let them through
		Com_Memset( &svs.challenges[i], 0, sizeof( svs.challenges[i] ) );
		return;
	}

	// authorization failed
	if (!r) {
		NET_OutOfBandPrint( NS_SERVER, svs.challenges[i].adr, "print\nSomeone is using this CD Key\n" );
	} else {
		NET_OutOfBandPrint( NS_SERVER, svs.challenges[i].adr, "print\n%s\n", r );
	}

	// clear the challenge record so it won't timeout and let them through
	Com_Memset( &svs.challenges[i], 0, sizeof( svs.challenges[i] ) );
}

/*
==================
SV_DirectConnect

A "connect" OOB command has been received
==================
*/

#define PB_MESSAGE "PunkBuster Anti-Cheat software must be installed " \
                   "and Enabled in order to join this server. An updated game patch can be downloaded from " \
                   "www.idsoftware.com"

void SV_DirectConnect(netadr_t from) {

    char            userinfo[MAX_INFO_STRING];
    int             i, j;
    client_t        temp;
    client_t       *cl, *newcl;
    sharedEntity_t *ent;
    char           *password;
    int             clientNum;
    int             version;
    int             qport;
    int             challenge;
    int             startIndex;
    intptr_t        denied;
    int             count;
    int             numIpClients = 0;

    Com_DPrintf("SV_DirectConnect()\n");

    Q_strncpyz(userinfo, Cmd_Argv(1), sizeof(userinfo));

    version = atoi(Info_ValueForKey(userinfo, "protocol"));
    if (version != PROTOCOL_VERSION) {
        NET_OutOfBandPrint(NS_SERVER, from, "print\nServer uses protocol version %i.\n", PROTOCOL_VERSION);
        Com_DPrintf("    rejected connect from version %i\n", version);
        return;
    }

    challenge = atoi(Info_ValueForKey(userinfo, "challenge"));
    qport = atoi(Info_ValueForKey(userinfo, "qport"));

    // quick reject
    for (i = 0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++) {

        if (cl->state == CS_FREE) {
            continue;
        }

        if (NET_CompareBaseAdr(from, cl->netchan.remoteAddress) && (cl->netchan.qport == qport || from.port == cl->netchan.remoteAddress.port)) {

            if ((svs.time - cl->lastConnectTime) < (sv_reconnectlimit->integer * 1000)) {
                Com_DPrintf("%s:reconnect rejected : too soon\n", NET_AdrToString (from));
                return;
            }

            break;
        }
    }

    // see if the challenge is valid (LAN clients don't need to challenge)
    if (!NET_IsLocalAddress (from)) {

        int ping;

        for (i = 0 ; i < MAX_CHALLENGES; i++) {
            if (NET_CompareAdr(from, svs.challenges[i].adr)) {
                if (challenge == svs.challenges[i].challenge) {
                    break;  // good
                }
            }
        }

        if (i == MAX_CHALLENGES) {
            NET_OutOfBandPrint(NS_SERVER, from, "print\nNo or bad challenge for address.\n");
            return;
        }

        // force the IP key/value pair so the game can filter based on ip
        Info_SetValueForKey(userinfo, "ip", NET_AdrToString(from));

        //Reject attemps of connections using zenny hack
        if(strcmp(Info_ValueForKey(userinfo, "PROCESSOR_IDENTIFIER"), "") != 0 ||
        	strcmp(Info_ValueForKey(userinfo, "OS"), "") != 0||
			strcmp(Info_ValueForKey(userinfo, "COMPUTERNAME"), "") != 0)
        {
        	  NET_OutOfBandPrint(NS_SERVER, from, "print\nDPI analyzer detected some type of cheats. ^1Connection rejected\n");
        	  return;
        }

        // Note that it is totally possible to flood the console and qconsole.log by being rejected
        // (high ping, ban, server full, or other) and repeatedly sending a connect packet against the same
        // challenge.  Prevent this situation by only logging the first time we hit SV_DirectConnect()
        // for this challenge.
        if (!svs.challenges[i].connected) {
            ping = svs.time - svs.challenges[i].pingTime;
            svs.challenges[i].challengePing = ping;
            Com_Printf("Client %i connecting with %i challenge ping\n", i, ping);
        }
        else {
            ping = svs.challenges[i].challengePing;
            Com_DPrintf("Client %i connecting again with %i challenge ping\n", i, ping);
        }

        svs.challenges[i].connected = qtrue;

        // never reject a LAN client based on ping
        if (!Sys_IsLANAddress(from)) {

            for (j=0,cl=svs.clients ; j < sv_maxclients->integer ; j++,cl++) {
                if ( cl->state == CS_FREE ) {
                    continue;
                }   
                if ( NET_CompareBaseAdr( from, cl->netchan.remoteAddress )
                    && !( cl->netchan.qport == qport 
                    || from.port == cl->netchan.remoteAddress.port ) ) {
                    numIpClients++; 
                }   
            }

            if (sv_clientsPerIp->integer && numIpClients >= sv_clientsPerIp->integer) {
                NET_OutOfBandPrint(NS_SERVER, from, "print\nToo many connections from the same IP.\n");
                Com_DPrintf ("Client %i rejected due to too many connections from the same IP\n", i);
                return;
            }

            // Check for valid guid
            if (!SV_ApproveGuid(Info_ValueForKey(userinfo, "cl_guid"))) {
                NET_OutOfBandPrint(NS_SERVER, from, "print\nInvalid GUID detected.\n");
                Com_DPrintf("Invalid cl_guid: rejected connection from %s\n", NET_AdrToString(from));
                return;
            }

            if (sv_minPing->value && ping < sv_minPing->value) {
                NET_OutOfBandPrint(NS_SERVER, from, "print\nServer is for high pings only.\n");
                Com_DPrintf ("Client %i rejected on a too low ping\n", i);
                return;
            }

            if (sv_maxPing->value && ping > sv_maxPing->value) {
                NET_OutOfBandPrint(NS_SERVER, from, "print\nServer is for low pings only.\n");
                Com_DPrintf ("Client %i rejected on a too high ping\n", i);
                return;
            }

        }

    } else {
        // force the "ip" info key to "localhost"
        Info_SetValueForKey(userinfo, "ip", "localhost");
    }

    newcl = &temp;
    Com_Memset(newcl, 0, sizeof(client_t));

    // if there is already a slot for this ip, reuse it
    for (i = 0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++) {

        if (cl->state == CS_FREE) {
            continue;
        }

        if (NET_CompareBaseAdr(from, cl->netchan.remoteAddress) && (cl->netchan.qport == qport || from.port == cl->netchan.remoteAddress.port)) {
            Com_Printf ("%s:reconnect\n", NET_AdrToString (from));
            newcl = cl;
            goto gotnewcl;
        }
    }

    // find a client slot
    // if "sv_privateClients" is set > 0, then that number
    // of client slots will be reserved for connections that
    // have "password" set to the value of "sv_privatePassword"
    // Info requests will report the maxclients as if the private
    // slots didn't exist, to prevent people from trying to connect
    // to a full server.
    // This is to allow us to reserve a couple slots here on our
    // servers so we can play without having to kick people.

    // check for privateClient password
    password = Info_ValueForKey(userinfo, "password");
    if (!strcmp(password, sv_privatePassword->string)) {
        startIndex = 0;
    } else {
        // skip past the reserved slots
        startIndex = sv_privateClients->integer;
    }

    newcl = NULL;
    for (i = startIndex; i < sv_maxclients->integer ; i++) {
        cl = &svs.clients[i];
        if (cl->state == CS_FREE) {
            newcl = cl;
            break;
        }
    }

    if (!newcl) {

        if (NET_IsLocalAddress(from)) {

            count = 0;
            for (i = startIndex; i < sv_maxclients->integer ; i++) {
                cl = &svs.clients[i];
                if (cl->netchan.remoteAddress.type == NA_BOT) {
                    count++;
                }
            }

            // if they're all bots
            if (count >= sv_maxclients->integer - startIndex) {
                SV_DropClient(&svs.clients[sv_maxclients->integer - 1], "only bots on server");
                newcl = &svs.clients[sv_maxclients->integer - 1];
            }
            else {
                Com_Error(ERR_FATAL, "server is full on local connect\n");
                return;
            }
        }
        else {
            NET_OutOfBandPrint(NS_SERVER, from, "print\nServer is full, try it later.\n");
            Com_DPrintf("Rejected a connection.\n");
            return;
        }

    }

    // we got a newcl, so reset the reliableSequence and reliableAcknowledge
    cl->reliableAcknowledge = 0;
    cl->reliableSequence = 0;

gotnewcl:
    // build a new connection
    // accept the new client
    // this is the only place a client_t is ever initialized
    *newcl = temp;
    clientNum = newcl - svs.clients;
    ent = SV_GentityNum(clientNum);
    newcl->gentity = ent;

    // save the challenge
    newcl->challenge = challenge;

    // save the address
    Netchan_Setup (NS_SERVER, &newcl->netchan , from, qport);
    // init the netchan queue
    newcl->netchan_end_queue = &newcl->netchan_start_queue;

    // clear server-side demo recording
    newcl->demo_recording = qfalse;
    newcl->demo_file = -1;
    newcl->demo_waiting = qfalse;
    newcl->demo_backoff = 1;
    newcl->demo_deltas = 0;

    // save the userinfo
    Q_strncpyz(newcl->userinfo, userinfo, sizeof(newcl->userinfo));
    Com_sprintf(cl->colourName, MAX_NAME_LENGTH, "%s", SV_CleanName(Info_ValueForKey(newcl->userinfo, "name")));

    //Allways start this value to -1
	newcl->cm.lastWeaponAfterScope = -1;


    // get the game a chance to reject this connection or modify the userinfo
    denied = VM_Call(gvm, GAME_CLIENT_CONNECT, clientNum, qtrue, qfalse); // firstTime = qtrue
    if (denied) {
        // we can't just use VM_ArgPtr, because that is only valid inside a VM_Call
        char *str = VM_ExplicitArgPtr(gvm, denied);
        NET_OutOfBandPrint(NS_SERVER, from, "print\n%s\n", str);
        Com_DPrintf ("Game rejected a connection: %s.\n", str);
        return;
    }

    SV_UserinfoChanged(newcl);

    // send the connect packet to the client
    NET_OutOfBandPrint(NS_SERVER, from, "connectResponse");

    Com_DPrintf("Going from CS_FREE to CS_CONNECTED for %s\n", newcl->name);

    newcl->state = CS_CONNECTED;
    newcl->nextSnapshotTime = svs.time;
    newcl->lastPacketTime = svs.time;
    newcl->lastConnectTime = svs.time;
    newcl->numcmds = 0;

    // when we receive the first packet from the client, we will
    // notice that it is from a different serverid and that the
    // gamestate message was not just sent, forcing a retransmit
    newcl->gamestateMessageNum = -1;

    // load the client saved position from a file
    SV_LoadPositionFromFile(newcl, sv_mapname->string);

    // if this was the first client on the server, or the last client
    // the server can hold, send a heartbeat to the master.
    count = 0;
    for (i=0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++) {
        if (svs.clients[i].state >= CS_CONNECTED) {
            count++;
        }
    }

    if (count == 1 || count == sv_maxclients->integer) {
        SV_Heartbeat_f();
    }

}


/*
=====================
SV_DropClient

Called when the player is totally leaving the server, either willingly
or unwillingly.  This is NOT called if the entire server is quiting
or crashing -- SV_FinalMessage() will handle that
=====================
*/
void SV_DropClient( client_t *drop, const char *reason ) {
	int		i;
	challenge_t	*challenge;

	if ( drop->state == CS_ZOMBIE ) {
		return;		// already dropped
	}

	if (drop->netchan.remoteAddress.type != NA_BOT) {
		// see if we already have a challenge for this ip
		challenge = &svs.challenges[0];

		for (i = 0 ; i < MAX_CHALLENGES ; i++, challenge++) {
			if ( NET_CompareAdr( drop->netchan.remoteAddress, challenge->adr ) ) {
				challenge->connected = qfalse;
				break;
			}
		}
	}

	// Kill any download
	SV_CloseDownload( drop );

	// tell everyone why they got dropped
	SV_SendServerCommand( NULL, "print \"%s" S_COLOR_WHITE " %s\n\"", drop->name, reason );

	if (drop->download)	{
		FS_FCloseFile( drop->download );
		drop->download = 0;
	}

	if (drop->demo_recording) {
	    // stop the server side demo if we were recording this client
       Cbuf_ExecuteText(EXEC_NOW, va("stopserverdemo %d", (int)(drop - svs.clients)));
	}

	// call the prog function for removing a client
	// this will remove the body, among other things
	VM_Call( gvm, GAME_CLIENT_DISCONNECT, drop - svs.clients );

	// add the disconnect command
	SV_SendServerCommand( drop, "disconnect \"%s\"", reason);

	if ( drop->netchan.remoteAddress.type == NA_BOT ) {
		SV_BotFreeClient( drop - svs.clients );
	}

    // save the client position to a file
    SV_SavePositionToFile(drop, sv_mapname->string);

	// nuke user info
	SV_SetUserinfo( drop - svs.clients, "" );

    //Clean Mod structure
    memset(&(drop->cm), 0, sizeof(clientMod_t));
    drop->colourName[0] = 0;

    Com_DPrintf( "Going to CS_ZOMBIE for %s\n", drop->name );
	drop->state = CS_ZOMBIE;		// become free in a few seconds

	// if this was the last client on the server, send a heartbeat
	// to the master so it is known the server is empty
	// send a heartbeat now so the master will get up to date info
	// if there is already a slot for this ip, reuse it
	for (i=0 ; i < sv_maxclients->integer ; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			break;
		}
	}

	if ( i == sv_maxclients->integer ) {
		SV_Heartbeat_f();
	}
}

#ifdef USE_AUTH
/*
=====================
SV_Auth_DropClient

Called when the player is totally leaving the server, either willingly
or unwillingly.  This is NOT called if the entire server is quiting
or crashing -- SV_FinalMessage() will handle that
=====================
*/
void SV_Auth_DropClient( client_t *drop, const char *reason, const char *message ) {

	int		i;
	challenge_t	*challenge;

	if ( drop->state == CS_ZOMBIE ) {
		return;		// already dropped
	}

	if (drop->netchan.remoteAddress.type != NA_BOT) {
		// see if we already have a challenge for this ip
		challenge = &svs.challenges[0];

		for (i = 0 ; i < MAX_CHALLENGES ; i++, challenge++) {
			if ( NET_CompareAdr( drop->netchan.remoteAddress, challenge->adr ) ) {
				challenge->connected = qfalse;
				break;
			}
		}
	}

	// Kill any download
	SV_CloseDownload( drop );

	// tell everyone why they got dropped
	if( strlen( reason ) > 0 ) SV_SendServerCommand( NULL, "print \"%s\n\"", reason );

	if (drop->download)	{
		FS_FCloseFile( drop->download );
		drop->download = 0;
	}

	if (drop->demo_recording) {
        // stop the server side demo if we were recording this client
       Cbuf_ExecuteText(EXEC_NOW, va("stopserverdemo %d", (int)(drop - svs.clients)));
    }

	// call the prog function for removing a client
	// this will remove the body, among other things
	VM_Call( gvm, GAME_CLIENT_DISCONNECT, drop - svs.clients );

	// add the disconnect command
	SV_SendServerCommand( drop, "disconnect \"%s\"", message);

	if ( drop->netchan.remoteAddress.type == NA_BOT ) {
		SV_BotFreeClient( drop - svs.clients );
	}

    // save the client position to a file
    SV_SavePositionToFile(drop, sv_mapname->string);

	// nuke user info
	SV_SetUserinfo( drop - svs.clients, "" );
	
	Com_DPrintf( "Going to CS_ZOMBIE for %s\n", drop->name );
	drop->state = CS_ZOMBIE;		// become free in a few seconds

	// if this was the last client on the server, send a heartbeat
	// to the master so it is known the server is empty
	// send a heartbeat now so the master will get up to date info
	// if there is already a slot for this ip, reuse it
	for (i=0 ; i < sv_maxclients->integer ; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			break;
		}
	}

	if ( i == sv_maxclients->integer ) {
		SV_Heartbeat_f();
	}
}
#endif

/*
================
SV_SendClientGameState

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each new map load.

It will be resent if the client acknowledges a later message but has
the wrong gamestate.
================
*/
void SV_SendClientGameState( client_t *client ) {
	int			start;
	entityState_t	*base, nullstate;
	msg_t		msg;
	byte		msgBuffer[MAX_MSGLEN];

 	Com_DPrintf ("SV_SendClientGameState() for %s\n", client->name);
	Com_DPrintf( "Going from CS_CONNECTED to CS_PRIMED for %s\n", client->name );
	client->state = CS_PRIMED;
	client->pureAuthentic = 0;
	client->gotCP = qfalse;

	// when we receive the first packet from the client, we will
	// notice that it is from a different serverid and that the
	// gamestate message was not just sent, forcing a retransmit
	client->gamestateMessageNum = client->netchan.outgoingSequence;

	MSG_Init( &msg, msgBuffer, sizeof( msgBuffer ) );

	// NOTE, MRE: all server->client messages now acknowledge
	// let the client know which reliable clientCommands we have received
	MSG_WriteLong( &msg, client->lastClientCommand );

	// send any server commands waiting to be sent first.
	// we have to do this cause we send the client->reliableSequence
	// with a gamestate and it sets the clc.serverCommandSequence at
	// the client side
	SV_UpdateServerCommandsToClient( client, &msg );

	// send the gamestate
	MSG_WriteByte( &msg, svc_gamestate );
	MSG_WriteLong( &msg, client->reliableSequence );

	// write the configstrings
	for ( start = 0 ; start < MAX_CONFIGSTRINGS ; start++ ) {
		if (sv.configstrings[start][0]) {
			MSG_WriteByte( &msg, svc_configstring );
			MSG_WriteShort( &msg, start );
			MSG_WriteBigString( &msg, sv.configstrings[start] );
		}
	}

	// write the baselines
	Com_Memset( &nullstate, 0, sizeof( nullstate ) );
	for ( start = 0 ; start < MAX_GENTITIES; start++ ) {
		base = &sv.svEntities[start].baseline;
		if ( !base->number ) {
			continue;
		}
		MSG_WriteByte( &msg, svc_baseline );
		MSG_WriteDeltaEntity( &msg, &nullstate, base, qtrue );
	}

	MSG_WriteByte( &msg, svc_EOF );

	MSG_WriteLong( &msg, client - svs.clients);

	// write the checksum feed
	MSG_WriteLong( &msg, sv.checksumFeed);

	// deliver this to the client
	SV_SendMessageToClient( &msg, client );
}

/*
================
MOD_ResquestPk3DownloadByClientGameState

HACK FOR Resquest DOWNLOAD OF A FAKE FILE, DLL INJECTION IS POSSIBLE
================
*/
void MOD_ResquestPk3DownloadByClientGameState( client_t *client , char *todownload) {
	int			start;
	entityState_t	*base, nullstate;
	msg_t		msg;
	byte		msgBuffer[MAX_MSGLEN];
	char        configmodified[MAX_CONFIGSTRINGS];
	int checksum;
	checksum = FS_GetCheckSumPakByName(todownload);
	if(!checksum)
	{
		Com_Printf("Pack isn't loaded on server...\n");
		Com_Printf("Using -999999999 as md5sum ...\n");
		checksum = -999999999;
	}
	client->state = CS_PRIMED;
	client->pureAuthentic = 0;
	client->gotCP = qfalse;

	// when we receive the first packet from the client, we will
	// notice that it is from a different serverid and that the
	// gamestate message was not just sent, forcing a retransmit
	client->gamestateMessageNum = client->netchan.outgoingSequence;

	MSG_Init( &msg, msgBuffer, sizeof( msgBuffer ) );

	// NOTE, MRE: all server->client messages now acknowledge
	// let the client know which reliable clientCommands we have received
	MSG_WriteLong( &msg, client->lastClientCommand );

	// send any server commands waiting to be sent first.
	// we have to do this cause we send the client->reliableSequence
	// with a gamestate and it sets the clc.serverCommandSequence at
	// the client side
	SV_UpdateServerCommandsToClient( client, &msg );

	// send the gamestate
	MSG_WriteByte( &msg, svc_gamestate );
	MSG_WriteLong( &msg, client->reliableSequence );

	// For force this download, we need to change just the configstrings for add referenced
	// files where they doesn't exist.
	for ( start = 0 ; start < MAX_CONFIGSTRINGS ; start++ ) {
		if (sv.configstrings[start][0]) {
			MSG_WriteByte( &msg, svc_configstring );
			MSG_WriteShort( &msg, start );
			if(start==0)
			{
				strcpy(configmodified, sv.configstrings[start]);
				Info_SetValueForKey(configmodified, "mapname", todownload);
				MSG_WriteBigString( &msg, configmodified );
			}else if(start==1)
			{
				strcpy(configmodified, sv.configstrings[start]);
				Info_SetValueForKey(configmodified, "sv_referencedPaks", va("%i ", checksum));
				//Get sum of md5
				Info_SetValueForKey(configmodified, "sv_referencedPakNames", va("q3ut4/%s ", todownload));
				MSG_WriteBigString( &msg, configmodified );
			}else
				MSG_WriteBigString( &msg, sv.configstrings[start] );
		}
	}

	// write the baselines
	Com_Memset( &nullstate, 0, sizeof( nullstate ) );
	for ( start = 0 ; start < MAX_GENTITIES; start++ ) {
		base = &sv.svEntities[start].baseline;
		if ( !base->number ) {
			continue;
		}
		MSG_WriteByte( &msg, svc_baseline );
		MSG_WriteDeltaEntity( &msg, &nullstate, base, qtrue );
	}

	MSG_WriteByte( &msg, svc_EOF );

	MSG_WriteLong( &msg, client - svs.clients);

	// write the checksum feed
	MSG_WriteLong( &msg, sv.checksumFeed);

	// deliver this to the client
	SV_SendMessageToClient( &msg, client );
}

/*
==================
SV_ClientEnterWorld
==================
*/
void SV_ClientEnterWorld( client_t *client, usercmd_t *cmd ) {
	int		clientNum;
	sharedEntity_t *ent;

	Com_DPrintf( "Going from CS_PRIMED to CS_ACTIVE for %s\n", client->name );
	client->state = CS_ACTIVE;

	// resend all configstrings using the cs commands since these are
	// no longer sent when the client is CS_PRIMED
	SV_UpdateConfigstrings( client );

	// set up the entity for the client
	clientNum = client - svs.clients;
	ent = SV_GentityNum( clientNum );
	ent->s.number = clientNum;
	client->gentity = ent;

	client->deltaMessage = -1;
	client->nextSnapshotTime = svs.time;	// generate a snapshot immediately

	if(cmd) {
		memcpy(&client->lastUsercmd, cmd, sizeof(client->lastUsercmd));
	} else {
		memset(&client->lastUsercmd, '\0', sizeof(client->lastUsercmd));
	}

	// call the game begin function
	VM_Call( gvm, GAME_CLIENT_BEGIN, client - svs.clients );
}

/*
============================================================

CLIENT COMMAND EXECUTION

============================================================
*/

/*
==================
SV_CloseDownload

clear/free any download vars
==================
*/
static void SV_CloseDownload( client_t *cl ) {
	int i;

	// EOF
	if (cl->download) {
		FS_FCloseFile( cl->download );
	}
	cl->download = 0;
	*cl->downloadName = 0;

	// Free the temporary buffer space
	for (i = 0; i < MAX_DOWNLOAD_WINDOW; i++) {
		if (cl->downloadBlocks[i]) {
			Z_Free( cl->downloadBlocks[i] );
			cl->downloadBlocks[i] = NULL;
		}
	}

}

/*
==================
SV_StopDownload_f

Abort a download if in progress
==================
*/
void SV_StopDownload_f( client_t *cl ) {
	if (*cl->downloadName)
		Com_DPrintf( "clientDownload: %d : file \"%s\" aborted\n", (int) (cl - svs.clients), cl->downloadName );

	SV_CloseDownload( cl );
}

/*
==================
SV_DoneDownload_f

Downloads are finished
==================
*/
void SV_DoneDownload_f( client_t *cl ) {
	if ( cl->state == CS_ACTIVE )
		return;
	
	Com_DPrintf( "clientDownload: %s ^7Done\n", cl->name);
	// resend the game state to update any clients that entered during the download
	SV_SendClientGameState(cl);
}

/*
==================
SV_NextDownload_f

The argument will be the last acknowledged block from the client, it should be
the same as cl->downloadClientBlock
==================
*/
void SV_NextDownload_f( client_t *cl )
{
	int block = atoi( Cmd_Argv(1) );

	if (block == cl->downloadClientBlock) {
		Com_DPrintf( "clientDownload: %d : client acknowledge of block %d\n", (int) (cl - svs.clients), block );

		// Find out if we are done.  A zero-length block indicates EOF
		if (cl->downloadBlockSize[cl->downloadClientBlock % MAX_DOWNLOAD_WINDOW] == 0) {
			Com_Printf( "clientDownload: %d : file \"%s\" completed\n", (int) (cl - svs.clients), cl->downloadName );
			SV_CloseDownload( cl );
			return;
		}

		cl->downloadSendTime = svs.time;
		cl->downloadClientBlock++;
		return;
	}
	// We aren't getting an acknowledge for the correct block, drop the client
	// FIXME: this is bad... the client will never parse the disconnect message
	//			because the cgame isn't loaded yet
	SV_DropClient( cl, "broken download" );
}

/*
==================
SV_BeginDownload_f
==================
*/
void SV_BeginDownload_f( client_t *cl ) {

	// Kill any existing download
	SV_CloseDownload( cl );

	// cl->downloadName is non-zero now, SV_WriteDownloadToClient will see this and open
	// the file itself
	Q_strncpyz( cl->downloadName, Cmd_Argv(1), sizeof(cl->downloadName) );
}

/*
==================
SV_WriteDownloadToClient

Check to see if the client wants a file, open it if needed and start pumping the client
Fill up msg with data 
==================
*/
void SV_WriteDownloadToClient( client_t *cl , msg_t *msg )
{
	int curindex;
	int rate;
	int blockspersnap;
	int idPack = 0, missionPack = 0, unreferenced = 1;
	char errorMessage[1024];
	char pakbuf[MAX_QPATH], *pakptr;
	int numRefPaks;

	if (!*cl->downloadName)
		return;	// Nothing being downloaded

	if (!cl->download) {
 		// Chop off filename extension.
		Com_sprintf(pakbuf, sizeof(pakbuf), "%s", cl->downloadName);
		pakptr = Q_strrchr(pakbuf, '.');
		
		if(pakptr)
		{
			*pakptr = '\0';

			// Check for pk3 filename extension
			if(!Q_stricmp(pakptr + 1, "pk3"))
			{
				const char *referencedPaks = FS_ReferencedPakNames();

				// Check whether the file appears in the list of referenced
				// paks to prevent downloading of arbitrary files.
				Cmd_TokenizeStringIgnoreQuotes(referencedPaks);
				numRefPaks = Cmd_Argc();

				for(curindex = 0; curindex < numRefPaks; curindex++)
				{
					if(!FS_FilenameCompare(Cmd_Argv(curindex), pakbuf))
					{
						unreferenced = 0;

						// now that we know the file is referenced,
						// check whether it's legal to download it.
						missionPack = FS_idPak(pakbuf, "missionpack");
						idPack = missionPack || FS_idPak(pakbuf, BASEGAME);

						break;
					}
				}
			}
		}

		// We open the file here
		if ( !(sv_allowDownload->integer & DLF_ENABLE) ||
			(sv_allowDownload->integer & DLF_NO_UDP) ||
			idPack || unreferenced ||
			( cl->downloadSize = FS_SV_FOpenFileRead( cl->downloadName, &cl->download ) ) <= 0 ) {
			// cannot auto-download file
			if(unreferenced)
			{
				Com_Printf("clientDownload: %d : \"%s\" is not referenced and cannot be downloaded.\n", (int) (cl - svs.clients), cl->downloadName);
				Com_sprintf(errorMessage, sizeof(errorMessage), "File \"%s\" is not referenced and cannot be downloaded.", cl->downloadName);
			}
			else if (idPack) {
				Com_Printf("clientDownload: %d : \"%s\" cannot download id pk3 files\n", (int) (cl - svs.clients), cl->downloadName);
				if (missionPack) {
					Com_sprintf(errorMessage, sizeof(errorMessage), "Cannot autodownload Team Arena file \"%s\"\n"
									"The Team Arena mission pack can be found in your local game store.", cl->downloadName);
				}
				else {
					Com_sprintf(errorMessage, sizeof(errorMessage), "Cannot autodownload id pk3 file \"%s\"", cl->downloadName);
				}
			}
			else if ( !(sv_allowDownload->integer & DLF_ENABLE) ||
				(sv_allowDownload->integer & DLF_NO_UDP) ) {

				Com_Printf("clientDownload: %d : \"%s\" download disabled", (int) (cl - svs.clients), cl->downloadName);
				if (sv_pure->integer) {
					Com_sprintf(errorMessage, sizeof(errorMessage), "Could not download \"%s\" because autodownloading is disabled on the server.\n\n"
										"You will need to get this file elsewhere before you "
										"can connect to this pure server.\n", cl->downloadName);
				} else {
					Com_sprintf(errorMessage, sizeof(errorMessage), "Could not download \"%s\" because autodownloading is disabled on the server.\n\n"
                    "The server you are connecting to is not a pure server, "
                    "set autodownload to No in your settings and you might be "
                    "able to join the game anyway.\n", cl->downloadName);
				}
			} else {
        // NOTE TTimo this is NOT supposed to happen unless bug in our filesystem scheme?
        //   if the pk3 is referenced, it must have been found somewhere in the filesystem
				Com_Printf("clientDownload: %d : \"%s\" file not found on server\n", (int) (cl - svs.clients), cl->downloadName);
				Com_sprintf(errorMessage, sizeof(errorMessage), "File \"%s\" not found on server for autodownloading.\n", cl->downloadName);
			}
			MSG_WriteByte( msg, svc_download );
			MSG_WriteShort( msg, 0 ); // client is expecting block zero
			MSG_WriteLong( msg, -1 ); // illegal file size
			MSG_WriteString( msg, errorMessage );

			*cl->downloadName = 0;
			return;
		}
 
		Com_Printf( "clientDownload: %d : beginning \"%s\"\n", (int) (cl - svs.clients), cl->downloadName );
		
		// Init
		cl->downloadCurrentBlock = cl->downloadClientBlock = cl->downloadXmitBlock = 0;
		cl->downloadCount = 0;
		cl->downloadEOF = qfalse;
	}

	// Perform any reads that we need to
	while (cl->downloadCurrentBlock - cl->downloadClientBlock < MAX_DOWNLOAD_WINDOW &&
		cl->downloadSize != cl->downloadCount) {

		curindex = (cl->downloadCurrentBlock % MAX_DOWNLOAD_WINDOW);

		if (!cl->downloadBlocks[curindex])
			cl->downloadBlocks[curindex] = Z_Malloc( MAX_DOWNLOAD_BLKSIZE );

		cl->downloadBlockSize[curindex] = FS_Read( cl->downloadBlocks[curindex], MAX_DOWNLOAD_BLKSIZE, cl->download );

		if (cl->downloadBlockSize[curindex] < 0) {
			// EOF right now
			cl->downloadCount = cl->downloadSize;
			break;
		}

		cl->downloadCount += cl->downloadBlockSize[curindex];

		// Load in next block
		cl->downloadCurrentBlock++;
	}

	// Check to see if we have eof condition and add the EOF block
	if (cl->downloadCount == cl->downloadSize &&
		!cl->downloadEOF &&
		cl->downloadCurrentBlock - cl->downloadClientBlock < MAX_DOWNLOAD_WINDOW) {

		cl->downloadBlockSize[cl->downloadCurrentBlock % MAX_DOWNLOAD_WINDOW] = 0;
		cl->downloadCurrentBlock++;

		cl->downloadEOF = qtrue;  // We have added the EOF block
	}

	// Loop up to window size times based on how many blocks we can fit in the
	// client snapMsec and rate

	// based on the rate, how many bytes can we fit in the snapMsec time of the client
	// normal rate / snapshotMsec calculation
	rate = cl->rate;
	if ( sv_maxRate->integer ) {
		if ( sv_maxRate->integer < 1000 ) {
			Cvar_Set( "sv_MaxRate", "1000" );
		}
		if ( sv_maxRate->integer < rate ) {
			rate = sv_maxRate->integer;
		}
	}
	if ( sv_minRate->integer ) {
		if ( sv_minRate->integer < 1000 )
			Cvar_Set( "sv_minRate", "1000" );
		if ( sv_minRate->integer > rate )
			rate = sv_minRate->integer;
	}

	if (!rate) {
		blockspersnap = 1;
	} else {
		blockspersnap = ( (rate * cl->snapshotMsec) / 1000 + MAX_DOWNLOAD_BLKSIZE ) /
			MAX_DOWNLOAD_BLKSIZE;
	}

	if (blockspersnap < 0)
		blockspersnap = 1;

	while (blockspersnap--) {

		// Write out the next section of the file, if we have already reached our window,
		// automatically start retransmitting

		if (cl->downloadClientBlock == cl->downloadCurrentBlock)
			return; // Nothing to transmit

		if (cl->downloadXmitBlock == cl->downloadCurrentBlock) {
			// We have transmitted the complete window, should we start resending?

			//FIXME:  This uses a hardcoded one second timeout for lost blocks
			//the timeout should be based on client rate somehow
			if (svs.time - cl->downloadSendTime > 1000)
				cl->downloadXmitBlock = cl->downloadClientBlock;
			else
				return;
		}

		// Send current block
		curindex = (cl->downloadXmitBlock % MAX_DOWNLOAD_WINDOW);

		MSG_WriteByte( msg, svc_download );
		MSG_WriteShort( msg, cl->downloadXmitBlock );

		// block zero is special, contains file size
		if ( cl->downloadXmitBlock == 0 )
			MSG_WriteLong( msg, cl->downloadSize );
 
		MSG_WriteShort( msg, cl->downloadBlockSize[curindex] );

		// Write the block
		if ( cl->downloadBlockSize[curindex] ) {
			MSG_WriteData( msg, cl->downloadBlocks[curindex], cl->downloadBlockSize[curindex] );
		}

		Com_DPrintf( "clientDownload: %d : writing block %d\n", (int) (cl - svs.clients), cl->downloadXmitBlock );

		// Move on to the next block
		// It will get sent with next snap shot.  The rate will keep us in line.
		cl->downloadXmitBlock++;

		cl->downloadSendTime = svs.time;
	}
}

/*
=================
SV_Disconnect_f

The client is going to disconnect, so remove the connection immediately  FIXME: move to game?
=================
*/
static void SV_Disconnect_f( client_t *cl ) {
    SV_DropClient(cl, mod_disconnectMsg->string);
}

/*
=================
SV_VerifyPaks_f

If we are pure, disconnect the client if they do no meet the following conditions:

1. the first two checksums match our view of cgame and ui
2. there are no any additional checksums that we do not have

This routine would be a bit simpler with a goto but i abstained

=================
*/
static void SV_VerifyPaks_f( client_t *cl ) {
	int nChkSum1, nChkSum2, nClientPaks, nServerPaks, i, j, nCurArg;
	int nClientChkSum[1024];
	int nServerChkSum[1024];
	const char *pPaks, *pArg;
	qboolean bGood = qtrue;

	// if we are pure, we "expect" the client to load certain things from 
	// certain pk3 files, namely we want the client to have loaded the
	// ui and cgame that we think should be loaded based on the pure setting
	//
	if ( sv_pure->integer != 0 ) {

		bGood = qtrue;
		nChkSum1 = nChkSum2 = 0;
		// we run the game, so determine which cgame and ui the client "should" be running
		bGood = (FS_FileIsInPAK("vm/cgame.qvm", &nChkSum1) == 1);
		if (bGood)
			bGood = (FS_FileIsInPAK("vm/ui.qvm", &nChkSum2) == 1);

		nClientPaks = Cmd_Argc();

		// start at arg 2 ( skip serverId cl_paks )
		nCurArg = 1;

		pArg = Cmd_Argv(nCurArg++);
		if(!pArg) {
			bGood = qfalse;
		}
		else
		{
			// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=475
			// we may get incoming cp sequences from a previous checksumFeed, which we need to ignore
			// since serverId is a frame count, it always goes up
			if (atoi(pArg) < sv.checksumFeedServerId)
			{
				Com_DPrintf("ignoring outdated cp command from client %s\n", cl->name);
				return;
			}
		}
	
		// we basically use this while loop to avoid using 'goto' :)
		while (bGood) {

			// must be at least 6: "cl_paks cgame ui @ firstref ... numChecksums"
			// numChecksums is encoded
			if (nClientPaks < 6) {
				bGood = qfalse;
				break;
			}
			// verify first to be the cgame checksum
			pArg = Cmd_Argv(nCurArg++);
			if (!pArg || *pArg == '@' || atoi(pArg) != nChkSum1 ) {
				bGood = qfalse;
				break;
			}
			// verify the second to be the ui checksum
			pArg = Cmd_Argv(nCurArg++);
			if (!pArg || *pArg == '@' || atoi(pArg) != nChkSum2 ) {
				bGood = qfalse;
				break;
			}
			// should be sitting at the delimeter now
			pArg = Cmd_Argv(nCurArg++);
			if (*pArg != '@') {
				bGood = qfalse;
				break;
			}
			// store checksums since tokenization is not re-entrant
			for (i = 0; nCurArg < nClientPaks; i++) {
				nClientChkSum[i] = atoi(Cmd_Argv(nCurArg++));
			}

			// store number to compare against (minus one cause the last is the number of checksums)
			nClientPaks = i - 1;

			// make sure none of the client check sums are the same
			// so the client can't send 5 the same checksums
			for (i = 0; i < nClientPaks; i++) {
				for (j = 0; j < nClientPaks; j++) {
					if (i == j)
						continue;
					if (nClientChkSum[i] == nClientChkSum[j]) {
						bGood = qfalse;
						break;
					}
				}
				if (bGood == qfalse)
					break;
			}
			if (bGood == qfalse)
				break;

			// get the pure checksums of the pk3 files loaded by the server
			pPaks = FS_LoadedPakPureChecksums();
			Cmd_TokenizeString( pPaks );
			nServerPaks = Cmd_Argc();
			if (nServerPaks > 1024)
				nServerPaks = 1024;

			for (i = 0; i < nServerPaks; i++) {
				nServerChkSum[i] = atoi(Cmd_Argv(i));
			}

			// check if the client has provided any pure checksums of pk3 files not loaded by the server
			for (i = 0; i < nClientPaks; i++) {
				for (j = 0; j < nServerPaks; j++) {
					if (nClientChkSum[i] == nServerChkSum[j]) {
						break;
					}
				}
				if (j >= nServerPaks) {
					bGood = qfalse;
					break;
				}
			}
			if ( bGood == qfalse ) {
				break;
			}

			// check if the number of checksums was correct
			nChkSum1 = sv.checksumFeed;
			for (i = 0; i < nClientPaks; i++) {
				nChkSum1 ^= nClientChkSum[i];
			}
			nChkSum1 ^= nClientPaks;
			if (nChkSum1 != nClientChkSum[nClientPaks]) {
				bGood = qfalse;
				break;
			}

			// break out
			break;
		}

		cl->gotCP = qtrue;

		if (bGood) {
			cl->pureAuthentic = 1;
		} 
		else {
			cl->pureAuthentic = 0;
			cl->nextSnapshotTime = -1;
			cl->state = CS_ACTIVE;
			SV_SendClientSnapshot( cl );
			SV_DropClient( cl, "Unpure client detected. Invalid .PK3 files referenced!" );
		}
	}
}

/*
=================
SV_ResetPureClient_f
=================
*/
static void SV_ResetPureClient_f( client_t *cl ) {
	cl->pureAuthentic = 0;
	cl->gotCP = qfalse;
}

/*
=================
SV_UserinfoChanged

Pull specific info from a newly changed userinfo string
into a more C friendly form.
=================
*/
void SV_UserinfoChanged( client_t *cl ) {
	char	*val;
	char	*ip;
	int		i;
	int	len;

	// name for C code
	Q_strncpyz( cl->name, Info_ValueForKey (cl->userinfo, "name"), sizeof(cl->name) );
	Com_sprintf(cl->colourName, MAX_NAME_LENGTH, "%s", SV_CleanName(cl->name));

	// rate command

	// if the client is on the same subnet as the server and we aren't running an
	// internet public server, assume they don't need a rate choke
	if ( Sys_IsLANAddress( cl->netchan.remoteAddress ) && com_dedicated->integer != 2 && sv_lanForceRate->integer == 1) {
		cl->rate = 99999;	// lans should not rate limit
	} else {
		val = Info_ValueForKey (cl->userinfo, "rate");
		if (strlen(val)) {
			i = atoi(val);
			cl->rate = i;
			if (cl->rate < 1000) {
				cl->rate = 1000;
			} else if (cl->rate > 90000) {
				cl->rate = 90000;
			}
		} else {
			cl->rate = 3000;
		}
	}
	val = Info_ValueForKey (cl->userinfo, "handicap");
	if (strlen(val)) {
		i = atoi(val);
		if (i<=0 || i>100 || strlen(val) > 4) {
			Info_SetValueForKey( cl->userinfo, "handicap", "100" );
		}
	}

	// snaps command
	val = Info_ValueForKey (cl->userinfo, "snaps");
	if (strlen(val)) {
		i = atoi(val);
		if ( i < 1 ) {
			i = 1;
		} else if ( i > sv_fps->integer ) {
			i = sv_fps->integer;
		}
		cl->snapshotMsec = 1000/i;
	} else {
		cl->snapshotMsec = 50;
	}
	
	// TTimo
	// maintain the IP information
	// the banning code relies on this being consistently present
	if( NET_IsLocalAddress(cl->netchan.remoteAddress) )
		ip = "localhost";
	else
		ip = (char*)NET_AdrToString( cl->netchan.remoteAddress );

	val = Info_ValueForKey( cl->userinfo, "ip" );
	if( val[0] )
		len = strlen( ip ) - strlen( val ) + strlen( cl->userinfo );
	else
		len = strlen( ip ) + 4 + strlen( cl->userinfo );

	if( len >= MAX_INFO_STRING )
		SV_DropClient( cl, "userinfo string length exceeded" );
	else
		Info_SetValueForKey( cl->userinfo, "ip", ip );

    // get the client's cg_ghost value if we are in jump mode
    if (sv_gametype->integer == GT_JUMP) {
        cl->cm.ghost = SV_IsClientGhost(cl);
    }

    if (mod_forceGear->string && Q_stricmp(mod_forceGear->string, "")) {
        Info_SetValueForKey(cl->userinfo, "gear", mod_forceGear->string);
    }
}

/*
==================
SV_UpdateUserinfo_f
==================
*/
void SV_UpdateUserinfo_f( client_t *cl ) {
    gclient_t *gl;

	if ( (sv_floodProtect->integer) && (cl->state >= CS_ACTIVE) && (svs.time < cl->nextReliableUserTime) ) {
		Q_strncpyz( cl->userinfobuffer, Cmd_Argv(1), sizeof(cl->userinfobuffer) );
		SV_SendServerCommand(cl, "print \"^7Command ^1delayed ^7due to sv_floodprotect!\n\"");
		return;
	}

    qboolean ghost = cl->cm.ghost; // Save here the current cg_ghost value in order to know after if it changed

    gl = (gclient_t *)SV_GameClientNum(cl - svs.clients);
	cl->userinfobuffer[0]=0;
	cl->nextReliableUserTime = svs.time + 5000;

	Q_strncpyz( cl->userinfo, Cmd_Argv(1), sizeof(cl->userinfo) );

	SV_UserinfoChanged( cl );

	// call prog code to allow overrides
	VM_Call( gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients );

    if (mod_colourNames->integer) {
        if(cl->colourName[0] != 0)
            Q_strncpyz(gl->pers.netname, va("%s^7", cl->colourName), MAX_NETNAME);
    }

    // get the client's cg_ghost value if we are in jump mode
    if (sv_gametype->integer == GT_JUMP) {
        cl->cm.ghost = SV_IsClientGhost(cl);
        // display ghost mode status if it changed
        if (ghost != cl->cm.ghost) {
            if (cl->cm.ghost) {
                SV_SendServerCommand(cl, "print \"^7Ghost Mode turned: [^2ON^7]\n\"");
            } else {
                SV_SendServerCommand(cl, "print \"^7Ghost Mode turned: [^1OFF^7]\n\"");
            }
        }
    }
}

/*
===============================================================================
                        TitanMod Client Commands
===============================================================================
*/

/////////////////////////////////////////////////////////////////////
// SV_SavePosition_f
// Save the current client position
/////////////////////////////////////////////////////////////////////
static void SV_SavePosition_f(client_t *cl) {

    int             cid;
    playerState_t   *ps;

    // get the client slot and playerState_t
    cid = cl - svs.clients;
    ps = SV_GameClientNum(cid);

    // if the server doesn't allow position save/load
    if (!mod_allowPosSaving->integer) {
        return;
    }

    // if in a jumprun
    if (cl->cm.ready && mod_loadSpeedCmd->integer != 2) {
        SV_SendServerCommand(cl, "print \"^1You can't save your position while being in a jump run!\n\"");
        return;
    }

    if (!mod_freeSaving->integer || cl->cm.noFreeSave) {

        // disallow if in spectator mode
        if (SV_GetClientTeam(cid) == TEAM_SPECTATOR) {
            SV_SendServerCommand(cl, "print \"^1You can't save your position from the spectator team!\n\"");
            return;
        }
   
       // disallow if moving
       if (SV_ClientIsMoving(cl) == 1) {
           SV_SendServerCommand(cl, "print \"^1You can't save your position while moving!\n\"");
           return;
        }
    
       // disallow if dead
       if (ps->pm_type != PM_NORMAL) {
           SV_SendServerCommand(cl, "print \"^1You must be alive and in-game to save your position!\n\"");
           return;
        }
    
       // disallow if crouched
       if (ps->pm_flags & PMF_DUCKED) {
           SV_SendServerCommand(cl, "print \"^1You can't save your position while being crouched!\n\"");
           return;
        }
    
       // disallow if not on a solid ground
       if (ps->groundEntityNum != ENTITYNUM_WORLD) {
           SV_SendServerCommand(cl, "print \"^1You must be standing on a solid ground to save your position!\n\"");
           return;
        }
    } 

    // save the position and angles
    VectorCopy(ps->origin, cl->cm.savedPosition);
    VectorCopy(ps->viewangles, cl->cm.savedPositionAngle);

    // log command execution
    SV_LogPrintf("ClientSavePosition: %d - %f - %f - %f\n",
                                      cid,
                                      cl->cm.savedPosition[0],
                                      cl->cm.savedPosition[1],
                                      cl->cm.savedPosition[2]);

    SV_SendServerCommand(cl, "print \"^7Your ^6position ^7has been ^2saved\n\"");
}

/////////////////////////////////////////////////////////////////////
// SV_LoadPosition_f
// Load a previously saved position
/////////////////////////////////////////////////////////////////////
static void SV_LoadPosition_f(client_t *cl) {

    int             i;
    int             cid;
    int             angle;
    qboolean        jumprun;
    playerState_t   *ps;
    sharedEntity_t  *ent;

    cid = cl - svs.clients;
    jumprun = cl->cm.ready;

    // if the server doesn't allow position save/load
    if (!mod_allowPosSaving->integer) {
        return;
    }

    ent = SV_GentityNum(cid);
    ps = SV_GameClientNum(cid);

    // if there is no saved position
    if (!cl->cm.savedPosition[0] || !cl->cm.savedPosition[1] || !cl->cm.savedPosition[2]) {
        SV_SendServerCommand(cl, "print \"^1There is no position to load on this map!\n\"");
        return;
    }

    if (jumprun) {
        // stop the timers
        Cmd_TokenizeString("ready");
        VM_Call(gvm, GAME_CLIENT_COMMAND, cl - svs.clients);
    }

    // copy back saved position
    VectorCopy(cl->cm.savedPosition, ps->origin);

    // set the view angle
    for (i = 0; i < 3; i++) {
        angle = ANGLE2SHORT(cl->cm.savedPositionAngle[i]);
        ps->delta_angles[i] = angle - cl->lastUsercmd.angles[i];
    }
    ps->delta_angles[0] -= 2000;

    VectorCopy(cl->cm.savedPositionAngle, ent->s.angles);
    VectorCopy(ent->s.angles, ps->viewangles);

    // clear client velocity
    VectorClear(ps->velocity);

    // regenerate stamina
    ps->stats[playerStatsOffsets[getVersion()][OFFSET_PS_STAMINA]] = ps->stats[playerStatsOffsets[getVersion()][OFFSET_PS_HEALTH]] * 300;

    if (jumprun) {
        // restore ready status
        Cmd_TokenizeString("ready");
        VM_Call(gvm, GAME_CLIENT_COMMAND, cl - svs.clients);
    }

    // log command execution
    SV_LogPrintf("ClientLoadPosition: %d - %f - %f - %f\n",
                                      cid,
                                      cl->cm.savedPosition[0],
                                      cl->cm.savedPosition[1],
                                      cl->cm.savedPosition[2]);
    
    SV_SendServerCommand(cl, "print \"^7Your ^6position ^7has been ^5loaded\n\"");
}

/////////////////////////////////////////////////////////////////////
// SV_LoadSpeed_f
// Load a saved position with current speed and view angles
/////////////////////////////////////////////////////////////////////
static void SV_LoadSpeed_f(client_t *cl) {
    int             cid;
    playerState_t   *ps;

    cid = cl - svs.clients;
    ps = SV_GameClientNum(cid);

    if (!mod_allowPosSaving->integer || !mod_loadSpeedCmd->integer || SV_GetClientTeam(cid) == TEAM_SPECTATOR) {
        return;
    }

    if (cl->cm.ready && mod_loadSpeedCmd->integer != 2) {
        return;
    }

    if (!cl->cm.savedPosition[0] || !cl->cm.savedPosition[1] || !cl->cm.savedPosition[2]) {
        SV_SendServerCommand(cl, "print \"^1There is no position to load on this map!\n\"");
        return;
    }

    // copy back saved position and regenerate stamina
    VectorCopy(cl->cm.savedPosition, ps->origin);
    ps->stats[playerStatsOffsets[getVersion()][OFFSET_PS_STAMINA]] = ps->stats[playerStatsOffsets[getVersion()][OFFSET_PS_HEALTH]] * 300;

    SV_SendServerCommand(cl, "print \"^7Your ^6position ^7has been ^4loaded\n\"");
}

/////////////////////////////////////////////////////////////////////
// SV_NoFreeSave_f
/////////////////////////////////////////////////////////////////////
void SV_NoFreeSave_f(client_t *cl) {

    if (!mod_allowPosSaving->integer || !mod_freeSaving->integer) {
        return;
    }

    if (cl->cm.noFreeSave) {
        cl->cm.noFreeSave = qfalse;
        SV_SendServerCommand(cl, "print \"^7Free position saving turned: [^2ON^7]\n\"");
    } else {
        cl->cm.noFreeSave = qtrue;
        SV_SendServerCommand(cl, "print \"^7Free position saving turned: [^1OFF^7]\n\"");
    }
}

/////////////////////////////////////////////////////////////////////
// SV_HidePlayers_f
/////////////////////////////////////////////////////////////////////
void SV_HidePlayers_f(client_t *cl) {

    if (sv_gametype->integer != GT_JUMP) {
        return;
    }

    if (!mod_enableJumpCmds->integer) {
        return;
    }

    if (cl->cm.hidePlayers > 0) {
        cl->cm.hidePlayers = 0;
        SV_SendServerCommand(cl, "print \"^7Hide Players turned: [^1OFF^7]\n\"");
    } else {
        cl->cm.hidePlayers = 1;
        SV_SendServerCommand(cl, "print \"^7Hide Players turned: [^2ON^7]\n\"");
    }
}

/////////////////////////////////////////////////////////////////////
// SV_InfiniteStamina_f
/////////////////////////////////////////////////////////////////////
void SV_InfiniteStamina_f(client_t *cl) {
    int cid;
    cid = cl - svs.clients;

    if (sv_gametype->integer != GT_JUMP || !mod_enableJumpCmds->integer)
        return;

    if (SV_GetClientTeam(cid) == TEAM_SPECTATOR || cl->cm.ready)
        return;

    if (Cvar_VariableIntegerValue("g_stamina") == 2)
       return;

    if (cl->cm.infiniteStamina == 1 || (!cl->cm.infiniteStamina && mod_infiniteStamina->integer)) {
       cl->cm.infiniteStamina = 2;
       SV_SendServerCommand(cl, "print  \"^7Infinite Stamina turned: [^1OFF^7]\n\""); // back to g_stamina
    } else {
       cl->cm.infiniteStamina = 1;
       SV_SendServerCommand(cl, "print  \"^7Infinite Stamina turned: [^2ON^7]\n\"");
    }
}

/////////////////////////////////////////////////////////////////////
// SV_InfiniteWallJumps_f
/////////////////////////////////////////////////////////////////////
void SV_InfiniteWallJumps_f(client_t *cl) {
    int cid;
    cid = cl - svs.clients;

    if (sv_gametype->integer != GT_JUMP || !mod_enableJumpCmds->integer)
        return;

    if (SV_GetClientTeam(cid) == TEAM_SPECTATOR || cl->cm.ready)
       return;

    if (cl->cm.infiniteWallJumps == 1 || (!cl->cm.infiniteWallJumps && mod_infiniteWallJumps->integer)) {
       cl->cm.infiniteWallJumps = 2;
       SV_SendServerCommand(cl, "print  \"^7Infinite Wall Jumps turned: [^1OFF^7]\n\""); // back to g_walljumps
    } else {
       cl->cm.infiniteWallJumps = 1;
       SV_SendServerCommand(cl, "print  \"^7Infinite Wall Jumps turned: [^2ON^7]\n\"");
    }
}

/////////////////////////////////////////////////////////////////////
// SV_ServerModInfo_f
/////////////////////////////////////////////////////////////////////
void SV_ServerModInfo_f(client_t* cl) {
    SV_SendServerCommand(cl, "chat \"^2==============================================================\"");
    SV_SendServerCommand(cl, "chat \"%s^7Server Version: %s\"", sv_tellprefix->string, Cvar_VariableString("version"));
    SV_SendServerCommand(cl, "chat \"%s^7Game Version: ^1Urban Terror %s\"", sv_tellprefix->string, Cvar_VariableString("g_modversion"));
    SV_SendServerCommand(cl, "chat \"^2==============================================================\"");
    SV_SendServerCommand(cl, "chat \"%s^7Credits: ^3Titan Mod ^7was developed by ^5Pedrxd ^7& ^5Th3K1ll3r\"", sv_tellprefix->string);
    SV_SendServerCommand(cl, "chat \"^2==============================================================\"");
}

/////////////////////////////////////////////////////////////////////
// SV_HelpCmdsList_f
/////////////////////////////////////////////////////////////////////
void SV_HelpCmdsList_f(client_t* cl) {

    char *version;
    char *jump = "Jump Mode";
    char *frag = "Frag Modes";

    if (!mod_enableHelpCmd->integer) {
        return;
    }

    if (sv_gametype->integer == GT_JUMP) {
        version = jump;
    } else {
        version = frag;
    }

    if (version == jump) {
        SV_SendServerCommand(cl, "print  \"^8----------------------------------------------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^8-^7--------> Help Client - ^6List of Commands ^7[Jump Mode] <--------^8-\n\"");    
        SV_SendServerCommand(cl, "print  \"^8----------------------------------------------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^8------------------------^7General Commands^8------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^8----------------------------------------------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^8-    ^1/help           ^8-     ^2/playerlist     ^8-    ^2/cg_rgb        ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8-    ^2/reconnect      ^8-                     ^8-    ^2/r_gamma       ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8-    ^2/disconnect     ^8-     ^1/svModInfo      ^8-    ^2/sensitivity   ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8-    ^2/connect        ^8-                     ^8-    ^2/bind          ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8----------------------------------------------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^8--------------------------^7Game Commands^8-------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^8----------------------------------------------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^8-         ^2$location             ^8-       ^2/regainstamina         ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8-         ^2/name                 ^8-       ^2/save                  ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8-         ^2/kill                 ^8-       ^2/load                  ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8-         ^2/ready ^5(RUN)          ^8-       ^2/allowgoto             ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8-         ^2/cg_ghost ^5<0|1>       ^8-       ^2/goto                  ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8----------------------------------------------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^8- ^2/infiniteStamina       ^7[Turn ON/OFF Infinite Stamina]        ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8- ^2/infiniteWallJumps     ^7[Turn ON/OFF Infinite Wall Jumps]     ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8- ^2/hidePlayers           ^7[Make all other players invisible]    ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8- ^2/freeSave              ^7[Turn ON/OFF the free position saving]^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8----------------------------------------------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^8- ^2/tell ^5<client> <message>                  ^7[Private Messages] ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8- ^2/callvote ^5nextmap <mapname> ^7or ^5cyclemap   ^7[Map Votes]        ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8----------------------------------------------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^2[INFO] ^7Free position saving:  [%s^7]\n\"", (mod_freeSaving->integer > 0) ? "^2ON" : "^1OFF");
        SV_SendServerCommand(cl, "print  \"^2[INFO] ^7Current Map:  [^3%s^7]\n\"", sv_mapname->string);
        SV_SendServerCommand(cl, "print  \"^8----------------------------------------------------------------\n\"");

    } else if (version == frag) {
        SV_SendServerCommand(cl, "print  \"^8----------------------------------------------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^8-^7--------> Help Client - ^6List of Commands ^7[Frag Mode] <--------^8-\n\"");    
        SV_SendServerCommand(cl, "print  \"^8----------------------------------------------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^8------------------------^7General Commands^8------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^8----------------------------------------------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^8-    ^1/help           ^8-     ^2/playerlist     ^8-    ^2/cg_rgb        ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8-    ^2/reconnect      ^8-                     ^8-    ^2/r_gamma       ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8-    ^2/disconnect     ^8-     ^1/svModInfo      ^8-    ^2/sensitivity   ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8-    ^2/connect        ^8-                     ^8-    ^2/bind          ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8----------------------------------------------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^8--------------------------^7Game Commands^8-------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^8----------------------------------------------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^8-           ^2/name            ^8-            ^2$location            ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8-           ^2/kill            ^8-                                 ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8----------------------------------------------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^8- ^2/tell ^5<client> <message>                  ^7[Private Messages] ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8- ^2/callvote ^5nextmap <mapname> ^7or ^5cyclemap   ^7[Map Votes]        ^8-\n\"");
        SV_SendServerCommand(cl, "print  \"^8----------------------------------------------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^8----------------------------------------------------------------\n\"");
        SV_SendServerCommand(cl, "print  \"^2[INFO] ^7Current Map:  [^3%s^7]\n\"", sv_mapname->string);
        SV_SendServerCommand(cl, "print  \"^8----------------------------------------------------------------\n\"");
    }
}

//===============================================================================
typedef struct {
	char	*name;
	void	(*func)( client_t *cl );
} ucmd_t;

static ucmd_t ucmds[] = {
	{"userinfo", SV_UpdateUserinfo_f},
	{"disconnect", SV_Disconnect_f},
	{"cp", SV_VerifyPaks_f},
	{"vdr", SV_ResetPureClient_f},
	{"download", SV_BeginDownload_f},
	{"nextdl", SV_NextDownload_f},
	{"stopdl", SV_StopDownload_f},
	{"donedl", SV_DoneDownload_f},

    {"save", SV_SavePosition_f},
    {"savepos", SV_SavePosition_f},
    {"load", SV_LoadPosition_f},
    {"loadpos", SV_LoadPosition_f},
    {"loadSpeed", SV_LoadSpeed_f},
    {NULL, NULL}
};

static ucmd_t ucmds_floodControl[] = {
    {"freeSave", SV_NoFreeSave_f},
    {"hidePlayers", SV_HidePlayers_f},
    {"infiniteStamina", SV_InfiniteStamina_f},
    {"stamina", SV_InfiniteStamina_f},
    {"infiniteWallJumps", SV_InfiniteWallJumps_f},
    {"walljumps", SV_InfiniteWallJumps_f},
    {"svModInfo", SV_ServerModInfo_f},
    {"help", SV_HelpCmdsList_f},
    {NULL, NULL}
};
//===============================================================================

char *str_replace2(char *orig, char *rep, char *with) {
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


char* badwords[43] = { 	"fuck", "dick", "shit", "slut", "hure", "cunt", "homo", "puta", "nazi", "fick", "fotze", "opfer", "arsch", 
						"spack", "nutte", "spast", "bitch", "kurwa", "pussy", "penis", "hitler", "muschi", "biatch", "nigger", "putain", 
						"asshole", "maricon", "wixxer", "fdp", "ta geule", "fucker", "mother fucker", "mofo", "motherfucker", "kys", 
						"neger", "nigg", "niga", "nigga", "retard", "retarded", "whore", "fucking" 
					};

char* replacefor[5] = { "n1", "cya", "hello", "hey", "thanks" };
char* replacewith[5] = { "^2Nice One", "^8See you later", "^2Hey",  "^6hi", "^2Thanks!" };

char* getNewMessage(char* message, client_t *cl) {
	for (int i = 0; i < sizeof(badwords) / sizeof(const char*); i++){
		message = str_replace2(message, badwords[i], va("^1*****^%i", cl->chatcolour));
	}
	for (int i = 0; i < sizeof(replacefor) / sizeof(const char*); i++){
		message = str_replace2(message, replacefor[i], va("%s^%i", replacewith[i] ,cl->chatcolour));
	}
	return message;
}

char* getNewPrefixFFA(client_t *cl, playerState_t *ps) {
	
	char* specTag 	= "^7(SPEC)";
	char* authedTag = "^2[authed]";
	char* userTag 	= "^7[user]";
	char* adminTag 	= "^3[admin]";
	char* ownerTag 	= "^1[owner]";

	char* messagePrefix = "";

	if (ps->persistant[3] == 3 || (ps->pm_flags & 1024)) {
		messagePrefix = va("%s%s", messagePrefix, specTag);
	}
	if (cl->isauthed) {
		messagePrefix = va("%s%s", messagePrefix, authedTag);
	}
	if (cl->isuser) {
		messagePrefix = va("%s%s", messagePrefix, userTag);
	}
	if (cl->isadmin) {
		messagePrefix = va("%s%s", messagePrefix, adminTag);
	}
	if (cl->isowner) {
		messagePrefix = va("%s%s", messagePrefix, ownerTag);
	}
	return(messagePrefix);
}

char* getNewPrefixSR8(playerState_t *ps) {
	char *prefix = "";
	qboolean isDead = qfalse;

	int clientTeam = *(int*)((void*)ps+gclientOffsets[getVersion()][OFFSET_TEAM]);

	// Check if the client is dead
	if ((clientTeam == 1 || clientTeam == 2) && ps->pm_flags & 1024) {
		// The client is in team RED or BLUE and is currently spectating someone (so he must be dead)
		isDead = qtrue;
	}

	// Spec Team
	if (clientTeam == 3) {
		prefix = "^7[SPEC]";
	}
	// Red Team
	else if (clientTeam == 1) {
		prefix = "^1[RED]";
	}
	// Blue Team
	else if (clientTeam == 2) {
		prefix = "^4[BLUE]";
	}
	// Add the DEAD tag if required
	if (isDead) {
		prefix = va("%s%s", prefix, "^7(DEAD)");
	}
	return(prefix);
}

char* getZombiePrefix(playerState_t *ps) {
	char *prefix = "";
	qboolean isDead = qfalse;

	int clientTeam = *(int*)((void*)ps+gclientOffsets[getVersion()][OFFSET_TEAM]);

	// Check if the client is dead
	if ((clientTeam == 1 || clientTeam == 2) && ps->pm_flags & 1024) {
		// The client is in team RED or BLUE and is currently spectating someone (so he must be dead)
		isDead = qtrue;
	}

	// Spec Team
	if (clientTeam == 3) {
		prefix = "^7[SPEC]";
	}
	// Red Team
	else if (clientTeam == 1) {
		prefix = "^1[ZOMBIE]";
	}
	// Blue Team
	else if (clientTeam == 2) {
		prefix = "^4[HUMAN]";
	}
	// Add the DEAD tag if required
	if (isDead) {
		prefix = va("%s%s", prefix, "^7(DEAD)");
	}
	return(prefix);
}

int getColourPrefix(client_t *cl) {
	int defaultColour = 7;
	int level = cl->level;
	
	if (level < 100) {
		return(defaultColour);
	}
	else if (level >= 100 && level < 200) {
		return(3); // Yellow
	}
	else if (level >= 200 && level < 300) {
		return(8); // Orange
	}
	else if (level >= 300 && level < 400) {
		return(4); // Blue
	}
	else if (level >= 400 && level < 500) {
		return(5); // Cyan
	}
	else if (level >= 500 && level < 750) {
		return(2); // Green
	}
	else if (level == 999) {
		return(1); // Red

	} else {
		return(defaultColour);
	}
}

/*
==================
SV_ExecuteClientCommand

Also called by bot code
==================
*/
void SV_ExecuteClientCommand( client_t *cl, const char *s, qboolean clientOK ) {

	ucmd_t		*u;
	int			argsFromOneMaxlen;
	int			charCount;
	int			dollarCount;
	int			i;
	char		*arg;
	qboolean 	bProcessed = qfalse;
	qboolean 	exploitDetected = qfalse;
	playerState_t *ps;

	Cmd_TokenizeString( s );

	// see if it is a server level command
	for (u=ucmds ; u->name ; u++) {
		if (!Q_stricmp (Cmd_Argv(0), u->name) ) {
			u->func( cl );
			bProcessed = qtrue;
			break;
		}
	}

    // cmds with floodControl
    if (!bProcessed) {
        for (u=ucmds_floodControl ; u->name ; u++) {
            if (!Q_stricmp(Cmd_Argv(0), u->name)) {
                if (clientOK) { 
                    u->func(cl); 
                }
                bProcessed = qtrue;
                break;
            }
        }
    }

	if (clientOK) {
        int cid;
        cid = cl - svs.clients;
		ps = SV_GameClientNum(cid);
		// pass unknown strings to the game
		if ((!u->name) && (sv.state == SS_GAME) && (cl->state == CS_ACTIVE)) {
			Cmd_Args_Sanitize();
			char* message = getNewMessage(Cmd_ArgsFrom(1), cl);
			argsFromOneMaxlen = -1;
			if (Q_stricmp("say", Cmd_Argv(0)) == 0 ) {

				ps = SV_GameClientNum(cl - svs.clients);
				argsFromOneMaxlen = MAX_SAY_STRLEN;

				//Com_Printf( "->%s.\n", CopyString(Cmd_Args()) );
				//Com_Printf( "->%s.\n", Cmd_ArgsFrom(1) );
				//Cmd_ArgsFrom(1) will not work for logfile parsers as binds are not included here.

				if (mod_customchat->integer && sv_gametype->integer == GT_FFA){
                	if (!cl->muted){
						// Send the chat message
                	    SV_SendServerCommand(NULL, "chat \"%s^7%s^3:^%i %s\"", getNewPrefixFFA(cl, ps), cl->name, cl->chatcolour, message);
					}
					SV_LogPrintf("say: %i %s: %s\n", ps->clientNum, cl->name, CopyString(Cmd_Args()));			
					return;
				}

				else if (mod_customchat->integer && sv_gametype->integer == GT_SURVIVOR && mod_levelsystem->integer && mod_zombiemod->integer){
					if (!cl->muted){
						// Send the chat message
						SV_SendServerCommand(NULL, "chat \"%s^7[^2%i^7]^3%s:^%i %s\"", getZombiePrefix(ps), cl->level, cl->name, getColourPrefix(cl), message);
					}
					SV_LogPrintf("say: %i %s: %s\n", ps->clientNum, cl->name, CopyString(Cmd_Args()));
					return;
				}

				else if (mod_customchat->integer && sv_gametype->integer == GT_SURVIVOR && mod_levelsystem->integer){
					// Default custom chat with levelsystem
					if (!cl->muted){
						// Send the chat message
                		SV_SendServerCommand(NULL, "chat \"%s^7[^2%i^7]^3%s:^%i %s\"", getNewPrefixSR8(ps), cl->level, cl->name, getColourPrefix(cl), message);
					}
					SV_LogPrintf("say: %i %s: %s\n", ps->clientNum, cl->name, CopyString(Cmd_Args()));
					return;
				}

				else if (mod_customchat->integer && mod_1v1arena->integer){
					if (!cl->muted){
						int team;
						team = *(int*)((void*)ps+gclientOffsets[getVersion()][OFFSET_TEAM]);
						if (team != 3) {
							// Send the chat message
							SV_SendServerCommand(NULL, "chat \"^7[^3Arena ^5%i^7][^3SP:^5%i^7]^3%s: %s\"", cl->arena+1, cl->skill, cl->colourName, message);
						} else {
							SV_SendServerCommand(NULL, "chat \"^7[^5No Arena^7][^3SP:^5%i^7]^3%s: %s\"", cl->skill, cl->colourName, message);
						}
					}
					// Note: Do NOT use the playerstate number here, when parsing, the client might be dead
					// 		 and therefore ps->clientNum will hold the id of the person being spectated
					SV_LogPrintf("say: %i %s: %s\n", cl->playernum, cl->name, CopyString(Cmd_Args()));
					return;
				}


			}

			/*
			if (Q_stricmp("say", Cmd_Argv(0)) == 0 || Q_stricmp("say_team", Cmd_Argv(0)) == 0) {
				argsFromOneMaxlen = MAX_SAY_STRLEN;

				p = Cmd_Argv(1);
				while (*p == ' ') p++;

				//Check if the message start with some of the command triggereds
				if (((*p == '!') || (*p == '@') || (*p == '&') || (*p == '/')) && mod_hideCmds->integer)
				{
					if(mod_hideCmds->integer == 1)
						SV_SendServerCommand(cl, "chat \"%s^7%s^3: %s\"", sv_tellprefix->string, cl->colourName, Cmd_Args());
					SV_LogPrintf("say: %i %s: %s\n", ps->clientNum, cl->name, CopyString(Cmd_Args()));
					return;
				}

				//If pm was not mute previusly, force mute now
                if (Q_stricmp("!pm", p) == 0 || Q_stricmp("!tell", p) == 0)
                {
                    SV_SendServerCommand(cl, "chat \"%s^7%s^3: %s\"", sv_tellprefix->string, cl->colourName, Cmd_Args());
    				SV_LogPrintf("say: %i %s: %s\n", ps->clientNum, cl->name, CopyString(Cmd_Args()));
    				return;
                }
				// Hidden cmd prefix (always active)
                if (Q_stricmp("<!", p) == 0)
                {
                    SV_SendServerCommand(cl, "chat \"%s^7%s^3: %s\"", sv_tellprefix->string, cl->colourName, Cmd_Args());
    				SV_LogPrintf("say: %i %s: %s\n", ps->clientNum, cl->name, CopyString(Cmd_Args()));
    				return;
                }
			*/
            // Global Spectator Chat Patch
            // It's only applied on frag gametypes. Spectators can still talking between them with say_team
            // @Th3K1ll3r: As spectators, I don't think they need to use chat variables (public), so it's fine
            if (Q_stricmp("say", Cmd_Argv(0)) == 0 && sv_gametype->integer != GT_JUMP && SV_GetClientTeam(cid) == TEAM_SPECTATOR && mod_specChatGlobal->integer) {
                argsFromOneMaxlen = MAX_SAY_STRLEN;
                SV_SendServerCommand(NULL, "chat \"^7(SPEC) %s^3: %s\"", cl->colourName, Cmd_Args());
                SV_LogPrintf("say: %i %s^7: %s\n", ps->clientNum, cl->colourName, CopyString(Cmd_Args()));
                return;
            }
			else if (Q_stricmp("tell", Cmd_Argv(0)) == 0) {
				// A command will look like "tell 12 hi" or "tell foo hi".  The "12"
				// and "foo" in the examples will be counted towards MAX_SAY_STRLEN,
				// plus the space.
				argsFromOneMaxlen = MAX_SAY_STRLEN;
				if(!mod_allowTell->integer)
				{
	                SV_SendServerCommand(cl, "print \"^1This server doesn't allow private messages!\n\"");
	                return;
				}
				if (mod_gunsmod->integer) {
					if (!cl->isadmin && !cl->isowner) {
						SV_SendServerCommand(cl, "print \"^1You must be an admin to send private messages!\n\"");
						return;
					}
				}
			}
			else if (Q_stricmp("ut_radio", Cmd_Argv(0)) == 0) {
				// We add 4 to this value because in a command such as
				// "ut_radio 1 1 affirmative", the args at indices 1 and 2 each
				// have length 1 and there is a space after them.
				argsFromOneMaxlen = MAX_RADIO_STRLEN + 4;
				if(!mod_allowRadio->integer)
				{
	                SV_SendServerCommand(cl, "print \"^1The radio chat is disabled on this server!\n\"");
	                return;
				}

			} else if (Q_stricmp("ut_weapdrop", Cmd_Argv(0)) == 0 && !mod_allowWeapDrop->integer) {
                SV_SendServerCommand(cl, "print \"^1This server doesn't allow dropping weapons!\n\"");
                return;

            } else if (Q_stricmp("ut_itemdrop", Cmd_Argv(0)) == 0 && !mod_allowItemDrop->integer) {
                SV_SendServerCommand(cl, "print \"^1This server doesn't allow dropping items!\n\"");
                return;

            } else if (Q_stricmp("ut_itemdrop", Cmd_Argv(0)) == 0 && (!Q_stricmp("flag", Cmd_Argv(1)) || !Q_stricmp("1", Cmd_Argv(1)) || !Q_stricmp("2", Cmd_Argv(1))) && !mod_allowFlagDrop->integer) {
                SV_SendServerCommand(cl,"print \"^1This server doesn't allow dropping the flag!\n\"");
                return;

            } else if (Q_stricmp("kill", Cmd_Argv(0)) == 0 && !mod_allowSuicide->integer) {
                SV_SendServerCommand(cl, "print \"^1This server doesn't allow suiciding!\n\"");
                return;
            } else if (Q_stricmp("kill", Cmd_Argv(0)) == 0 && ps->stats[playerStatsOffsets[getVersion()][OFFSET_PS_HEALTH]] < mod_minKillHealth->integer) {
                SV_SendServerCommand(cl, "print \"^1You need a minimum of ^2%i percent ^1of health to kill yourself!\n\"", mod_minKillHealth->integer);
                return;
            } else if (Q_stricmp("team", Cmd_Argv(0)) == 0 && ps->stats[playerStatsOffsets[getVersion()][OFFSET_PS_HEALTH]] < mod_minTeamChangeHealth->integer && SV_GetClientTeam(cid) != TEAM_SPECTATOR) {
                SV_SendServerCommand(cl, "print \"^1You need a minimum of ^2%i percent ^1of health to change of team!\n\"", mod_minTeamChangeHealth->integer);
                return;
            } else if(Q_stricmp("callvote", Cmd_Argv(0)) == 0 && !mod_allowVote->integer) {
                SV_SendServerCommand(cl, "print \"^1Vote system is disabled on this server!\n\"");
                return;

            } else if(Q_stricmp("team", Cmd_Argv(0)) == 0) {
                switch(mod_allowTeamSelection->integer) {
                    case 0:
                        SV_SendServerCommand(cl, "print \"^1Team selection system is disabled on this server!\n\"");
                        return;
                    case 2:
                        if(Q_stricmp("s", Cmd_Argv(1)) != 0 && Q_stricmp("free", Cmd_Argv(1)) != 0) {
                            SV_SendServerCommand(cl, "print \"^1You can only spectate or auto join in this server!\n\"");
                            return;
                        }
                        break;
                    case 3:
                    {
                        team_t team = *(int*)((void*)ps+gclientOffsets[getVersion()][OFFSET_TEAM]);
                        if((Q_stricmp("free", Cmd_Argv(1)) == 0)) {
                            if(team == TEAM_SPECTATOR) {
                                break;
                            } else {
                                SV_SendServerCommand(cl, "print \"^1Only spectators can do auto join in this server!\n\"");
                                return;
                            }
                        }
                        if(Q_stricmp("s", Cmd_Argv(1)) != 0) {
                            SV_SendServerCommand(cl, "print \"^1You can only spectate or auto join in this server!\n\"");
                            return;
                        }
                        break;
                    }
                }

                if (mod_fastTeamChange->integer) {
                    Cmd_ExecuteString(va("forceteam %d %s", cid, Cmd_Argv(1)));
                    return;
                }

            } else if(Q_stricmp("ready", Cmd_Argv(0)) == 0) {
                // In-game client's timer in jump mode will be handled by parsing scoress
            }

			if (argsFromOneMaxlen >= 0) {
				charCount = 0;
				dollarCount = 0;
				for (i = Cmd_Argc() - 1; i >= 1; i--) {
					arg = Cmd_Argv(i);
					while (*arg) {
						if (++charCount > argsFromOneMaxlen) {
							exploitDetected = qtrue; break;
						}
						if (*arg == '$') {
							if (++dollarCount > MAX_DOLLAR_VARS) {
								exploitDetected = qtrue; break;
							}
							charCount += STRLEN_INCREMENT_PER_DOLLAR_VAR;
							if (charCount > argsFromOneMaxlen) {
								exploitDetected = qtrue; break;
							}
						}
						arg++;
					}
					if (exploitDetected) { break; }
					if (i != 1) { // Cmd_ArgsFrom() will add space
						if (++charCount > argsFromOneMaxlen) {
							exploitDetected = qtrue; break;
						}
					}
				}
			}
			if (exploitDetected) {
				Com_Printf("Buffer overflow exploit radio/say, possible attempt from %s\n", NET_AdrToString(cl->netchan.remoteAddress));
				SV_SendServerCommand(cl, "print \"Chat dropped due to message length constraints!\n\"");
				return;
			}

			VM_Call( gvm, GAME_CLIENT_COMMAND, cl - svs.clients );
		}
	}
	else if (!bProcessed) {
		Com_DPrintf( "client text ignored for %s^7: %s\n", cl->name, Cmd_Argv(0) );
	}
}

/*
===============
SV_ClientCommand
===============
*/
static qboolean SV_ClientCommand( client_t *cl, msg_t *msg ) {
	int		seq;
	const char	*s;
	qboolean clientOk = qtrue;

	seq = MSG_ReadLong( msg );
	s = MSG_ReadString( msg );

	// see if we have already executed it
	if ( cl->lastClientCommand >= seq ) {
		return qtrue;
	}

	Com_DPrintf( "clientCommand: %s ^7: %i : %s\n", cl->name, seq, s );

	// drop the connection if we have somehow lost commands
	if ( seq > cl->lastClientCommand + 1 ) {
		Com_Printf( "Client %s ^7lost %i clientCommands\n", cl->name,
			seq - cl->lastClientCommand + 1 );
		SV_DropClient( cl, "Lost reliable commands" );
		return qfalse;
	}

	// malicious users may try using too many string commands
	// to lag other players.  If we decide that we want to stall
	// the command, we will stop processing the rest of the packet,
	// including the usercmd.  This causes flooders to lag themselves
	// but not other people
	// We don't do this when the client hasn't been active yet since its
	// normal to spam a lot of commands when downloading
	if ( !com_cl_running->integer && 
		cl->state >= CS_ACTIVE &&
		sv_floodProtect->integer ) {
		if (svs.time < cl->nextReliableTime ) {
			if (++(cl->numcmds) > sv_floodProtect->integer ) {
				// ignore any other text messages from this client but let them keep playing
				// TTimo - moved the ignored verbose to the actual processing in SV_ExecuteClientCommand, only printing if the core doesn't intercept
				clientOk = qfalse;
			}
		} else {
			 cl->numcmds = 1;
		}
	} 

	// don't allow another command for one second
	cl->nextReliableTime = svs.time + 1000;

	SV_ExecuteClientCommand( cl, s, clientOk );

	cl->lastClientCommand = seq;
	Com_sprintf(cl->lastClientCommandString, sizeof(cl->lastClientCommandString), "%s", s);

	return qtrue;		// continue procesing
}


//==================================================================================

void MOD_AutoHealth(client_t *cl)
{
	gentity_t *ent;

	//If a player have custom configuration for health this function will preserver it over the global config
	if(!mod_enableHealth->integer && !cl->cm.perPlayerHealth)
		return;

	if(cl->cm.lastAutoHealth < svs.time) {
		cl->cm.lastAutoHealth = svs.time + (cl->cm.perPlayerHealth ? cl->cm.timeoutHealth : mod_timeoutHealth->integer);

		if(cl->state != CS_ACTIVE)
			return;

		ent = (gentity_t *)SV_GentityNum(cl - svs.clients);
		if(ent->health < (cl->cm.perPlayerHealth ? cl->cm.limitHealth : mod_limitHealth->integer)) {
			if((cl->cm.perPlayerHealth ? !cl->cm.whenmovingHealth : !mod_whenMoveHealth->integer) && SV_ClientIsMoving(cl))
				return;
			MOD_AddHealth(cl, (cl->cm.perPlayerHealth ? cl->cm.stepHealth : mod_addAmountOfHealth->integer));
			cl->cm.turnOffUsed = 1;
		}else
		{
			if(cl->cm.turnOffWhenFinish && cl->cm.turnOffUsed)
				cl->cm.perPlayerHealth = 0;
		}
	}
}

/////////////////////////////////////////////////////////////////////
// SV_GhostThink
// Fixes the bugged cg_ghost in jump mode
/////////////////////////////////////////////////////////////////////
void SV_GhostThink(client_t *cl) {

    int               i;
    int               num;
    int               touch[MAX_GENTITIES];
    float             rad;
    vec3_t            mins, maxs;
    sharedEntity_t    *ent;
    sharedEntity_t    *oth;

    int cid;
    cid = cl - svs.clients;

    // if we are not playing jump mode
    if (sv_gametype->integer != GT_JUMP) {
        return;
    }

    // if the client is a spectator
    if (SV_GetClientTeam(cid) == TEAM_SPECTATOR) {
        return;
    }

    // if the client has cg_ghost disabled
    if (!cl->cm.ghost) {
        return;
    }

    // get the correspondent entity
    ent = SV_GentityNum(cid);
    rad = Com_Clamp(4.0, 1000.0, mod_ghostRadius->value);

    // calculate the box
    for (i = 0; i < 3; i++) {
        mins[i] = ent->r.currentOrigin[i] - rad;
        maxs[i] = ent->r.currentOrigin[i] + rad;
    }

    // get the entities the client is touching (the bounding box)
    num = SV_AreaEntities(mins, maxs, touch, MAX_GENTITIES);

    for (i = 0; i < num; i++) {

        // if the entity we are touching is not a client 
        if (touch[i] < 0 || touch[i] >= sv_maxclients->integer) {
            continue;
        }

        // get the touched entity
        oth = SV_GentityNum(touch[i]);

        // if the entity is the client itself
        if (ent->s.number == oth->s.number) {
            continue;
        }

        // set the content mask to both
        if (ent->r.contents & CONTENTS_BODY) {
            ent->r.contents &= ~CONTENTS_BODY;
            ent->r.contents |= CONTENTS_CORPSE;
        }
        if (oth->r.contents & CONTENTS_BODY) {
            oth->r.contents &= ~CONTENTS_BODY;
            oth->r.contents |= CONTENTS_CORPSE;
        }
    }
}

void teleportAttached(client_t *cl)
{
	playerState_t *ps2;

	int cid = (cl - svs.clients);
	ps2 = SV_GameClientNum(cl->attachedto);

	char cmd2[64];
	Com_sprintf(cmd2, sizeof(cmd2), "tp %i %f %f %f", cid, ps2->origin[0], ps2->origin[1], ps2->origin[2] + 80);
	Cmd_ExecuteString(cmd2);

	unsigned int time = Com_Milliseconds();
	if (time > cl->stopattach) {
		client_t *cl2;
		cl2 = &svs.clients[cl->attachedto];
		cl->isattached = qfalse;
		cl->attachedto = 0;
		cl->stopattach = 0;
	    cl2->hasattached = qfalse;
	}
}

void addMedkitHealth(client_t *cl)
{
	char cmd2[64];
	unsigned int time = Com_Milliseconds();
    int cid;
    cid = cl - svs.clients;

	if (cl->lastmedkittime == 0 || (time - cl->lastmedkittime) > 430) {
		Com_sprintf(cmd2, sizeof(cmd2), "addhealth %i 1", cid);
		Cmd_ExecuteString(cmd2);
		cl->lastmedkittime = time;
	}
}



/*
==================
SV_CheckLocation
Returns 1 or -1
1 = yes player i is in area of xy with the r
-1 = no player i isnt in area of xy with the r
==================
*/

int SV_CheckLocation( float x, float y, float z, float r, int i ) {
	playerState_t *ps;
    ps = SV_GameClientNum(i);
    if (ps->origin[0] > x-r &&
        ps->origin[0] < x+r &&
        ps->origin[1] > y-r &&
        ps->origin[1] < y+r &&
        ps->origin[2] > z-r &&
        ps->origin[2] < z+r
        ) {
        return 1;
    }
	return -1;
}

/*
================
SV_TurnpikeBlocker
=================
*/

void SV_TurnpikeBlocker(float x, float y, float z, float r, float x2, float y2, float z2) {
    

	int i;
	char cmd[254];
	char cmd2[254];

	for (i=0 ; i < sv_maxclients->integer ; i++) {
        if (SV_CheckLocation(x, y, z, r, i) == 1) { // 
			Com_sprintf(cmd2, sizeof(cmd2), "tp %i %f %f %f",i, x2, y2, z2);
            Com_sprintf(cmd, sizeof(cmd), "sendclientcommand %i cp \"^1Restricted play area \n ^5Reason: ^6Not enough players online \n ^7[^1%i^7/^26^7] ^3Players ^2Online\"\n", i, sv.currplayers );
            Cmd_ExecuteString(cmd);
			Cmd_ExecuteString(cmd2);
		}
    }
}

void checkGhostMode (void) {
	unsigned int time = Com_Milliseconds();
	int stopTime = time - sv.stopAntiBlockTime;
	if (stopTime > 0 && sv.checkGhostMode) {
		sv.checkGhostMode = qfalse;
		sv.stopAntiBlockTime = 0;
		Cmd_ExecuteString(va("mod_ghostPlayers 0"));
	}
}

/*
==================
Levelsystem
==================
*/
void updateClientName(client_t *cl)
{
	char* toreplace = "XXXXXXXXXXXXX";

	// The client name we will use
	char customname[128];
	Q_strncpyz(customname, cl->lastcustomname, sizeof(customname));

	// The Configstring we will use
	char mycfgstring[256];
	Q_strncpyz(mycfgstring, cl->defaultconfigstr, sizeof(mycfgstring));

	// Now replace toreplace with the customname
	char* mynewstring = str_replace2(mycfgstring, toreplace, customname);

	// Finally: Set the string
	SV_SetConfigstring((544 + cl->clientgamenum), mynewstring);
}

/*
==================
Spectator feature
==================
*/

// Todo: Track namechanges and update names!

char* concat(const char *s1, const char *s2)
{
    const size_t len1 = strlen(s1);
    const size_t len2 = strlen(s2);
    char *result = malloc(len1 + len2 + 1);
    memcpy(result, s1, len1);
    memcpy(result + len1, s2, len2 + 1);
    return result;
}

char *strremove(char *str, const char *sub) {
    char *p, *q, *r;
    if ((q = r = strstr(str, sub)) != NULL) {
        size_t len = strlen(sub);
        while ((r = strstr(p = r + len, sub)) != NULL) {
            while (p < r)
                *q++ = *p++;
        }
        while ((*q++ = *p++) != '\0')
            continue;
    }
    return str;
}

void announceMessageToClient(client_t *speccedClient, char* spectatorName) {
	SV_SendServerCommand(speccedClient, "chat \"^6%s ^3 is now ^2spectating ^5you.\"", spectatorName);
}

void addToClientsList(client_t *speccedClient, client_t *cl){
	// Get the current spectator list
	char specs[256];
	Q_strncpyz(specs, speccedClient->spectators, 256);

	// The string we want to add
	char* toadd = va("^7|^8%s", cl->name);

	// Append to it
	char* newspecs = concat(specs, toadd);

	// Set the new list
	Q_strncpyz(speccedClient->spectators, newspecs, 256);
	free(newspecs);
}

void removeFromClientsList(client_t *cl) {
	// Return if no previous client
	if (cl->cidLastSpecd == -1){
		return;
	}
	client_t *lastSpeccedClient = &svs.clients[cl->cidLastSpecd];

	// Get the current spectator list
	char specs[256];
	Q_strncpyz(specs, lastSpeccedClient->spectators, 256);
	// The string we want to remove
	char* toremove = va("^7|^8%s", cl->lastName);
	// The new spectator list
	char* newspecs = strremove(specs, toremove);
	// Set the new list
	Q_strncpyz(lastSpeccedClient->spectators, newspecs, 256);
}

void updateJumpClientName(client_t *cl, int type, client_t *cl2, client_t *cl3) {

	static const int 	NEW 			= 1;
	static const int 	CLEAR 			= 2;
	static const int 	NOBODY			= 3;

	char* 				toreplace 		= "XXXXXXXXXXXXX";

	int clcid = cl->cid;

	// The client name we will use
	char clientName[128];

	// The config string we will use
	char nameCfgString[256];
	Q_strncpyz(nameCfgString, cl->nameConfigString, sizeof(nameCfgString));

	// The client is spectating someone for the first time
	// or the client is spectating someone else
	if (type == NEW) {
		char* nameToUse = va("%s ^3spectating ^6%s", cl->name, cl2->name);
		Q_strncpyz(clientName, nameToUse, sizeof(clientName));
	}

	// The client is not spectating anyone anymore, reset it to default
	// So the client is probably jumping
	if (type == CLEAR) {
		char* nameToUse = va("%s^7", cl->name);
		Q_strncpyz(clientName, nameToUse, sizeof(clientName));
	}

	// The client is a spectator but not spectating anyone (ghost)
	if (type == NOBODY) {
		char* nameToUse = va("%s ^3spectating ^8Nobody.", cl->name);
		Q_strncpyz(clientName, nameToUse, sizeof(clientName));
	}

	// Now set the new config string
	char* newCfgString = str_replace2(nameCfgString, toreplace, clientName);
	SV_SetConfigstring((544 + clcid), newCfgString);

}

/*
FIXME
This does not work, the clients location is recognized correctly however in the location string
it'll come up as "unknown" (ingame), the person spectating will get the correct location.


void forceLocationUpdate(client_t *speccedClient) {

	if (speccedClient->spectators[0] != '\0') {
		// client has spectators
		Cmd_ExecuteString(va("location %s \"^3%s ^7[^8%s^7] | ^9Spectators: ^8%s\" 0 1\n", speccedClient->name, speccedClient->name, currloc, speccedClient->spectators));
	} else {
		Cmd_ExecuteString(va("location %s \"^3%s ^7[^8%s^7] | ^9Spectators: ^8Nobody is watching you.\" 0 1\n", speccedClient->name, speccedClient->name, currloc));
	}
}
*/

void checkSpecClient(client_t *cl, playerState_t *ps) {
	if (*(int*)((void*)ps+gclientOffsets[getVersion()][OFFSET_TEAM]) != 3) {
		// Client is not a spectator
		return;
	}
	// If the client is currently spectating someone (if so, ps->clientNum will hold the cid of the person being spectated)
	qboolean currentlySpectating = ps->pm_flags & 1024;

	if (!currentlySpectating) {
		// The client is flying around in ghostmode, set the name to spectating nobody
		updateJumpClientName(cl, 3, 0, 0);
		client_t *lastSpeccedClient = &svs.clients[cl->cidLastSpecd];

		if (cl->cidLastSpecd != -1 && lastSpeccedClient) {
			removeFromClientsList(cl);
			cl->cidLastSpecd = -1;
			cl->cidCurrSpecd = -1;
			//updateJumpClientLocation(lastSpeccedClient);
		}
		return;
	}

	client_t *speccedClient = &svs.clients[ps->clientNum];
	playerState_t *speccedClientState = SV_GameClientNum(speccedClient - svs.clients);
	cl->cidCurrSpecd = speccedClientState->clientNum;

	if (currentlySpectating && cl->cidLastSpecd != cl->cidCurrSpecd) {

		// We are spectating a new person
		// -> Change our scoreboard name
		updateJumpClientName(cl, 1, speccedClient, 0);

		// -> Remove us from the old clients list
		removeFromClientsList(cl);

		// -> Add us to the new clients list
		addToClientsList(speccedClient, cl);

		// -> Announce to the new person that we are now spectating them
		announceMessageToClient(speccedClient, cl->colourName);

		// -> Update the clients location string FIXME
		//forceLocationUpdate(speccedClient);

		cl->cidLastSpecd = cl->cidCurrSpecd;

	}
}

/*
==================
Anti camp stuff
==================
*/

int checkCamperLocation( client_t *cl, playerState_t *ps, float r) {
    if (ps->origin[0] > cl->xlast-r &&
        ps->origin[0] < cl->xlast+r &&
        ps->origin[1] > cl->ylast-r &&
        ps->origin[1] < cl->ylast+r &&
        ps->origin[2] > cl->zlast-r &&
        ps->origin[2] < cl->zlast+r
        ) {
        return 1;
    }
	return 0;
}

void acceleratedSlap (client_t *cl, playerState_t *ps) {
	Cmd_ExecuteString(va("slap %s", cl->name));
	int vel0 = ps->velocity[0];
	int vel1 = ps->velocity[1];
	int vel2 = ps->velocity[2];
    ps->velocity[0] = vel0 *= 2;
	ps->velocity[1] = vel1 *= 2;
	ps->velocity[2] = vel2 *= 2;
}

void setCampCoords (client_t *cl, playerState_t *ps) {
	cl->xlast = ps->origin[0];
	cl->ylast = ps->origin[1];
	cl->zlast = ps->origin[2];
}

void checkCampers (client_t *cl) {
	int cid;
    cid = cl - svs.clients;

	// Don't check bots, let them camp
	if (cl->gentity->r.svFlags & SVF_BOT) {
		return;
	}

	// Don't check spectators
	if (SV_GetClientTeam(cid) == TEAM_SPECTATOR) {
		return;
	}

	unsigned int time = Com_Milliseconds();
	playerState_t *ps;
	ps = SV_GameClientNum(cl - svs.clients);

	// This is the first check
	if (cl->xlast == 0 && cl->ylast == 0 && cl->zlast == 0) {
		setCampCoords(cl, ps);
		cl->timechecked = time;
	} else {
		// Now check if 10 seconds have passed since the 1st check, if so do a proximity check
		// if the client is still close to his previous coordinates.
		if ((time - cl->timechecked) > 10000) {
			cl->timechecked = time;
			// 10 seconds have passed since the last check
			if (checkCamperLocation(cl, ps, 240) == 1) {
				// Client is still within his previous radius, 
				if (cl->campcounter == 3) {
					// punish now
					acceleratedSlap(cl, ps);
					acceleratedSlap(cl, ps);
					acceleratedSlap(cl, ps);
					acceleratedSlap(cl, ps);
					acceleratedSlap(cl, ps);
					cl->campcounter = 0;
					Cmd_ExecuteString(va("bigtext \"^5%s ^3was caught ^1Camping\"", cl->name));
				} else {
					cl->campcounter++;
				}
			} else {
				// set his new coordinates as the camper has moved on
				// reset the camp counter
				setCampCoords(cl, ps);
				cl->campcounter = 0;
			}
		}
	}
}

char* getWeaponModName(int wpn) {
	switch (wpn) {
		case ARENA_BERETTA: {return "beretta";}
		case ARENA_DEAGLE: {return "deagle";}
		case ARENA_COLT: {return "colt";}
		case ARENA_GLOCK: {return "glock";}
		case ARENA_MAGNUM: {return "magnum";}
		case ARENA_LR300: {return "lr";}
		case ARENA_G36: {return "g36";}
		case ARENA_AK103: {return "ak103";}
		case ARENA_M4: {return "m4";}
		case ARENA_NEGEV: {return "negev";}
		case ARENA_HK69: {return "hk69";}
		case ARENA_UMP45: {return "ump45";}
		case ARENA_MAC11: {return "mac11";}
		case ARENA_P90: {return "p90";}
		case ARENA_MP5K: {return "mp5k";}
		case ARENA_SPAS12: {return "spas12";}
		case ARENA_BENELLI: {return "benelli";}
		case ARENA_PSG1: {return "psg1";}
		case ARENA_SR8: {return "sr8";}
		case ARENA_FRF1: {return "frf1";}
		
		default: {return "";}
	}
}

void giveMultiClientWeapon (client_t *client, client_t *client2) {
	
	if (client->weaponGiven && client2->weaponGiven) {
		return;
	}

	const char *wpnModName = "";
	const char *wpnModName2 = "";
	int usePreference;

	// first check if their preference matches
	if (client->preference == client2->preference) {
		usePreference = client->preference;
	} else {

		// get a random choice
		srand(Com_Milliseconds());
		int rnd = rand();
		usePreference = (rnd > RAND_MAX/2) ? client->preference : client2->preference;
	}

	if (usePreference == ARENA_PRIMARY) {
		wpnModName = getWeaponModName(client->primary);
		wpnModName2 = getWeaponModName(client2->primary);
	}
	else if (usePreference == ARENA_SECONDARY) {
		wpnModName = getWeaponModName(client->secondary);
		wpnModName2 = getWeaponModName(client2->secondary);
	}
	else if (usePreference == ARENA_PISTOL) {
		wpnModName = getWeaponModName(client->pistol);
		wpnModName2 = getWeaponModName(client2->pistol);
	}
	else if (usePreference == ARENA_SNIPER) {
		wpnModName = getWeaponModName(client->sniper);
		wpnModName2 = getWeaponModName(client2->sniper);
	}
	else if (usePreference == ARENA_KNIFE) {
		wpnModName = "knife";
		wpnModName2 = "knife";
	}
	else if (usePreference == ARENA_GIB) {
		wpnModName = "fstod";
		wpnModName2 = "fstod";
	}
	else {
		SV_SendServerCommand(client, "cchat \"\" \"^5[^3Arena^5] ^1Error: [GiveMultiWeapon Failed] ^3contact ^2donna30.\"");
		SV_SendServerCommand(client2, "cchat \"\" \"^5[^3Arena^5] ^1Error: [GiveMultiWeapon Failed] ^3contact ^2donna30.\"");
		return;
	}

	// Mark the weapons as given
	client->weaponGiven = qtrue;
	client2->weaponGiven = qtrue;

	Cmd_ExecuteString(va("giveweapon %i %s", client->playernum, wpnModName));
	Cmd_ExecuteString(va("set1v1wpn %i", client->playernum));
	Cmd_ExecuteString(va("giveweapon %i %s", client2->playernum, wpnModName2));
	Cmd_ExecuteString(va("set1v1wpn %i", client2->playernum));

}

void setClientAngles(client_t *cl, int p) {

    sharedEntity_t	*ent;
	playerState_t 	*ps;
    int           	angle;
	int				i;

    vec3_t p1angles = { 0, 0, 0 };
    vec3_t p2angles = { 0, 180, 0 };

    
    ps = SV_GameClientNum(cl - svs.clients);
    ent = SV_GentityNum(cl->playernum);

	if (p == 1) {
    	for (i = 0; i < 3; i++) {
    	    angle = ANGLE2SHORT(p1angles[i]);
    	    ps->delta_angles[i] = angle - cl->lastUsercmd.angles[i];
    	}
    	ps->delta_angles[0] -= 2000;

    	VectorCopy(p1angles, ent->s.angles);
    	VectorCopy(ent->s.angles, ps->viewangles);
	} else {
    	for (i = 0; i < 3; i++) {
    	    angle = ANGLE2SHORT(p2angles[i]);
    	    ps->delta_angles[i] = angle - cl->lastUsercmd.angles[i];
    	}
    	ps->delta_angles[0] -= 2000;
	
    	VectorCopy(p2angles, ent->s.angles);
    	VectorCopy(ent->s.angles, ps->viewangles);
	}

    // set the view angle

}


void check1v1Teleport ( void ) {
	unsigned int time = Com_Milliseconds();
	if ((time - sv.doTeleportTime) > 10000) {
		// 10 seconds have passed since roundstart, teleport the clients to their arena
		for (int i = 0; i < MAX_1V1_ARENAS; i++) {
			if (sv.Arenas[i].population == 2) {
				client_t *cl;
				client_t *cl2;
				cl = &svs.clients[sv.Arenas[i].player1num];
				cl2 = &svs.clients[sv.Arenas[i].player2num];
				playerState_t *ps;
				playerState_t *ps2;
				ps = SV_GameClientNum(cl - svs.clients);
				ps2 = SV_GameClientNum(cl2 - svs.clients);
				VectorCopy(sv.Arenas[i].spawn1, ps->origin);
				VectorClear(ps->velocity);
				VectorCopy(sv.Arenas[i].spawn2, ps2->origin);
				VectorClear(ps2->velocity);
				giveMultiClientWeapon(cl, cl2);
				setClientAngles(cl, 1);
				setClientAngles(cl2, 2);
				MOD_AddHealth(cl, 100);
				MOD_AddHealth(cl2, 100);
				ps->stats[playerStatsOffsets[getVersion()][OFFSET_PS_STAMINA]] = ps->stats[playerStatsOffsets[getVersion()][OFFSET_PS_HEALTH]] * 300;
				ps2->stats[playerStatsOffsets[getVersion()][OFFSET_PS_STAMINA]] = ps2->stats[playerStatsOffsets[getVersion()][OFFSET_PS_HEALTH]] * 300;
			}
		}
		sv.doneTp = qtrue;
	}
}

void checkDoSmite ( void ) {
	unsigned int time = Com_Milliseconds();
	if ((time - sv.doSmite) > 2000) {
		// 2 seconds have passed since roundstart, do smite
		SV_SendServerCommand(NULL, "cchat \"\" \"^5[^3Arena^5] ^2All Arenas are finished.\"");
		SV_SendServerCommand(NULL, "cchat \"\" \"^5[^3Arena^5] ^1Smiting all players ^3to end this round.\"");
		client_t *plyr;
		int i;
    	for (i = 0, plyr = svs.clients; i < sv_maxclients->integer; i++, plyr++) {
			if (plyr->state < CS_CONNECTED) {
    		    continue;
    		}
			Cmd_ExecuteString(va("smite %i", plyr->playernum));
			Cmd_ExecuteString(va("setscore %i %i", plyr->playernum, plyr->arenaKills));
			Cmd_ExecuteString(va("setdeaths %i %i", plyr->playernum, plyr->arenaDeaths));
		}
		sv.doneSmite = qtrue;
	}
}

/*
==================
SV_ClientThink

Also called by bot code
==================
*/
void SV_ClientThink (client_t *cl, usercmd_t *cmd) {
	playerState_t *ps;
	cl->lastUsercmd = *cmd;
	int i, j;

	if ( cl->state != CS_ACTIVE ) {
		return;		// may have been kicked during the last usercmd
	}

	ps = SV_GameClientNum(cl - svs.clients);

	// attaching players
	if (cl->isattached) {
		teleportAttached(cl);
	}

	// Gunmoney related passive healing
	if (cl->hasmedkit){
		addMedkitHealth(cl);
	}

	// For the anti block
	if (sv.checkGhostMode && sv_ghostOnRoundstart->integer) {
		checkGhostMode();
	}

	// Levelsystem
	if (cl->customname == qtrue){
		updateClientName(cl);
	}

	// Spectator announcer
	if (mod_jumpSpecAnnouncer->integer) {
		// Future additional check: sv_gametype->integer == GT_JUMP
		checkSpecClient(cl, ps);
	}

	if (mod_turnpikeTeleporter->integer && strstr(sv_mapname->string, "ut4_turnpike")) {
		if (SV_CheckLocation(470, -1130, 32, 12, ps->clientNum)== 1){
			// Inside wc
			Cmd_ExecuteString(va("tp %i %i %i %i", ps->clientNum, 1619, -677, 357));
			SV_SendServerCommand(cl, "cp \"^3You ^6found ^3a ^2Secret Teleporter!\n^3The exit is ^5in the corner.\"");
		} 
		if (SV_CheckLocation(1497, -746, 345, 12, ps->clientNum)== 1){
			Cmd_ExecuteString(va("tp %i %i %i %i", ps->clientNum, 605, -1084, 32));
		} 
	}

	// Turnpikeblocker
	if (sv_TurnpikeBlocker->integer && sv.currplayers < 6 && (strstr(sv_mapname->string, "ut4_turnpike"))) {

		SV_TurnpikeBlocker(128,-1645,28, 64, 128, -1782, 28); // Entrance into metro
		SV_TurnpikeBlocker(961, -1773, 28, 64, 961, -1901, 28); // Office entrance van
		SV_TurnpikeBlocker(1586, -894, 28, 64, 1580, -993, 30); // side entrance into Office
		SV_TurnpikeBlocker(766, -720, 28, 64, 769, -599, 28); // main office entrance
		SV_TurnpikeBlocker(194, -479, 28, 64, 83, -483, 28); // under window entrance
		SV_TurnpikeBlocker(176, -956, 28, 64, 178, -1055, 28); // office metro entrance
		SV_TurnpikeBlocker(0, -1298, 28, 64, 1.5, -1182, 28); // metro back1
		SV_TurnpikeBlocker(-415, -1297, 28, 64, -425, -1185, 28); // metro back2
		SV_TurnpikeBlocker(312, -497, 224, 64, 420, -496, 235); // window
		SV_TurnpikeBlocker(1400, -1915, 56, 64, 1391, -2012, 28); // garage1
		SV_TurnpikeBlocker(1239, -1918, 56, 64, 1230, -2023, 28); // garage 2
		
	}

	if (cl->particlefx) {
		MOD_SetExternalEvent(cl, 106, 1);
	}

	if (mod_1v1arena->integer && !sv.doneTp) {
		check1v1Teleport();
	}

	if (mod_1v1arena->integer && !sv.doneSmite) {
		checkDoSmite();
	}

	if (mod_punishCampers->integer) {
		checkCampers(cl);
	}

	if(mod_disableScope->integer)
	{
		if(cl->cm.lastWeaponAfterScope != -1 )
		{
			ps->weapon = cl->cm.lastWeaponAfterScope;
			cl->cm.lastWeaponAfterScope = -1;
		}else
		{
			//24576 is the flag for zoom3 that include zoom1 and zoom2
			if(cmd->buttons&24576 && ps->weaponstate==WEAPON_READY)
			{
				cl->cm.lastWeaponAfterScope = ps->weapon;
				//Allways the change is with the weapon 15, but if the actual weapon is 15 we use the 14.
				if(ps->weapon==15)
					ps->weapon=14;
				else
					ps->weapon=15;
			}
		}
	}


    SV_GhostThink(cl);

    if (mod_infiniteAmmo->integer) {
        for (i = 0; i < MAX_POWERUPS; i++) {
            cl->cm.powerups[i] = ps->powerups[i];
        }
    }

    MOD_AutoHealth(cl);

	VM_Call( gvm, GAME_CLIENT_THINK, cl - svs.clients );

    if (mod_infiniteAmmo->integer) {
        if (cl->cm.lastEventSequence < ps->eventSequence - MAX_PS_EVENTS) {
            cl->cm.lastEventSequence = ps->eventSequence - MAX_PS_EVENTS;
        }
        for (j = cl->cm.lastEventSequence; j < ps->eventSequence; j++) {
            if (ps->events[j & (MAX_PS_EVENTS - 1)] == EV_FIRE_WEAPON || ps->weaponstate == WEAPON_FIRING) {
                for (i = 0; i < MAX_POWERUPS; i++) {
                    ps->powerups[i] = cl->cm.powerups[i];
                }
            }
        }
        cl->cm.lastEventSequence = ps->eventSequence;
    }
}

/*
==================
SV_UserMove

The message usually contains all the movement commands 
that were in the last three packets, so that the information
in dropped packets can be recovered.

On very fast clients, there may be multiple usercmd packed into
each of the backup packets.
==================
*/
static void SV_UserMove( client_t *cl, msg_t *msg, qboolean delta ) {
	int			i, key;
	int			cmdCount;
	usercmd_t	nullcmd;
	usercmd_t	cmds[MAX_PACKET_USERCMDS];
	usercmd_t	*cmd, *oldcmd;

	if ( delta ) {
		cl->deltaMessage = cl->messageAcknowledge;
	} else {
		cl->deltaMessage = -1;
	}

	cmdCount = MSG_ReadByte( msg );

	if ( cmdCount < 1 ) {
		Com_Printf( "cmdCount < 1\n" );
		return;
	}

	if ( cmdCount > MAX_PACKET_USERCMDS ) {
		Com_Printf( "cmdCount > MAX_PACKET_USERCMDS\n" );
		return;
	}

	// use the checksum feed in the key
	key = sv.checksumFeed;
	// also use the message acknowledge
	key ^= cl->messageAcknowledge;
	// also use the last acknowledged server command in the key
	key ^= Com_HashKey(cl->reliableCommands[ cl->reliableAcknowledge & (MAX_RELIABLE_COMMANDS-1) ], 32);

	Com_Memset( &nullcmd, 0, sizeof(nullcmd) );
	oldcmd = &nullcmd;
	for ( i = 0 ; i < cmdCount ; i++ ) {
		cmd = &cmds[i];
		MSG_ReadDeltaUsercmdKey( msg, key, oldcmd, cmd );
		oldcmd = cmd;
	}

    // save time for ping calculation
    //cl->frames[ cl->messageAcknowledge & PACKET_MASK ].messageAcked = svs.time;

    // Store the time of the first acknowledge, instead of the last. And use a time value not limited by sv_fps.
    if ( cl->frames[ cl->messageAcknowledge & PACKET_MASK ].messageAcked == -1 ) {
        cl->frames[ cl->messageAcknowledge & PACKET_MASK ].messageAcked = Sys_Milliseconds();
    }

	// TTimo
	// catch the no-cp-yet situation before SV_ClientEnterWorld
	// if CS_ACTIVE, then it's time to trigger a new gamestate emission
	// if not, then we are getting remaining parasite usermove commands, which we should ignore
	if (sv_pure->integer != 0 && cl->pureAuthentic == 0 && !cl->gotCP) {
		if (cl->state == CS_ACTIVE)
		{
			// we didn't get a cp yet, don't assume anything and just send the gamestate all over again
			Com_DPrintf( "%s^7: didn't get cp command, resending gamestate\n", cl->name);
			SV_SendClientGameState( cl );
		}
		return;
	}			
	
	// if this is the first usercmd we have received
	// this gamestate, put the client into the world
	if ( cl->state == CS_PRIMED ) {
		SV_ClientEnterWorld( cl, &cmds[0] );
		// the moves can be processed normaly
	}
	
	// a bad cp command was sent, drop the client
	if (sv_pure->integer != 0 && cl->pureAuthentic == 0) {		
		SV_DropClient( cl, "Cannot validate pure client!");
		return;
	}

	if ( cl->state != CS_ACTIVE ) {
		cl->deltaMessage = -1;
		return;
	}

	// usually, the first couple commands will be duplicates
	// of ones we have previously received, but the servertimes
	// in the commands will cause them to be immediately discarded
	for ( i =  0 ; i < cmdCount ; i++ ) {
		// if this is a cmd from before a map_restart ignore it
		if ( cmds[i].serverTime > cmds[cmdCount-1].serverTime ) {
			continue;
		}
		// extremely lagged or cmd from before a map_restart
		//if ( cmds[i].serverTime > svs.time + 3000 ) {
		//	continue;
		//}
		// don't execute if this is an old cmd which is already executed
		// these old cmds are included when cl_packetdup > 0
		if ( cmds[i].serverTime <= cl->lastUsercmd.serverTime ) {
			continue;
		}
		SV_ClientThink (cl, &cmds[ i ]);
	}
}


/*
===========================================================================

USER CMD EXECUTION

===========================================================================
*/

/*
===================
SV_ExecuteClientMessage

Parse a client packet
===================
*/
void SV_ExecuteClientMessage( client_t *cl, msg_t *msg ) {
	int			c;
	int			serverId;

	MSG_Bitstream(msg);

	serverId = MSG_ReadLong( msg );
	cl->messageAcknowledge = MSG_ReadLong( msg );

	if (cl->messageAcknowledge < 0) {
		// usually only hackers create messages like this
		// it is more annoying for them to let them hanging
#ifndef NDEBUG
		SV_DropClient( cl, "DEBUG: illegible client message" );
#endif
		return;
	}

	cl->reliableAcknowledge = MSG_ReadLong( msg );

	// NOTE: when the client message is fux0red the acknowledgement numbers
	// can be out of range, this could cause the server to send thousands of server
	// commands which the server thinks are not yet acknowledged in SV_UpdateServerCommandsToClient
	if (cl->reliableAcknowledge < cl->reliableSequence - MAX_RELIABLE_COMMANDS) {
		// usually only hackers create messages like this
		// it is more annoying for them to let them hanging
#ifndef NDEBUG
		SV_DropClient( cl, "DEBUG: illegible client message" );
#endif
		cl->reliableAcknowledge = cl->reliableSequence;
		return;
	}
	// if this is a usercmd from a previous gamestate,
	// ignore it or retransmit the current gamestate
	// 
	// if the client was downloading, let it stay at whatever serverId and
	// gamestate it was at.  This allows it to keep downloading even when
	// the gamestate changes.  After the download is finished, we'll
	// notice and send it a new game state
	//
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=536
	// don't drop as long as previous command was a nextdl, after a dl is done, downloadName is set back to ""
	// but we still need to read the next message to move to next download or send gamestate
	// I don't like this hack though, it must have been working fine at some point, suspecting the fix is somewhere else
	if ( serverId != sv.serverId && !*cl->downloadName && !strstr(cl->lastClientCommandString, "nextdl") ) {
		if ( serverId >= sv.restartedServerId && serverId < sv.serverId ) { // TTimo - use a comparison here to catch multiple map_restart
			// they just haven't caught the map_restart yet
			Com_DPrintf("%s ^7: ignoring pre map_restart / outdated client message\n", cl->name);
			return;
		}
		// if we can tell that the client has dropped the last
		// gamestate we sent them, resend it
		if ( cl->messageAcknowledge > cl->gamestateMessageNum ) {
			Com_DPrintf( "%s ^7: dropped gamestate, resending\n", cl->name );
			SV_SendClientGameState( cl );
		}
		return;
	}

	// this client has acknowledged the new gamestate so it's
	// safe to start sending it the real time again
	if( cl->oldServerTime && serverId == sv.serverId ){
		Com_DPrintf( "%s ^7acknowledged gamestate\n", cl->name );
		cl->oldServerTime = 0;
	}

	// read optional clientCommand strings
	do {
		c = MSG_ReadByte( msg );
		if ( c == clc_EOF ) {
			break;
		}
		if ( c != clc_clientCommand ) {
			break;
		}
		if ( !SV_ClientCommand( cl, msg ) ) {
			return;	// we couldn't execute it because of the flood protection
		}
		if (cl->state == CS_ZOMBIE) {
			return;	// disconnect command
		}
	} while ( 1 );

	// read the usercmd_t
	if ( c == clc_move ) {
		SV_UserMove( cl, msg, qtrue );
	} else if ( c == clc_moveNoDelta ) {
		SV_UserMove( cl, msg, qfalse );
	} else if ( c != clc_EOF ) {
		Com_Printf( "WARNING: bad command byte for client %i\n", (int) (cl - svs.clients) );
	}
//	if ( msg->readcount != msg->cursize ) {
//		Com_Printf( "WARNING: Junk at end of packet for client %i\n", cl - svs.clients );
//	}
}
