#include "../server/server.h"

const char *arenaPrefix = "^5[^3Arena^5]";

/*
===============
Other functions


===============
*/

void update1v1Name(client_t *client) {
	memset(client->lastcustomname,0,sizeof(client->lastcustomname));
    char cleanName[64];
    Q_strncpyz(cleanName, client->name, sizeof(cleanName));
    Q_CleanStr(cleanName );
	const char * newname;
	if (client->arena == 99) {
		newname = va("^7[^5No Arena^7]^3%s^2", cleanName);
	} else {
		newname = va("^7[^3Arena ^5%i^7]^3%s^2", client->arena+1, cleanName);
	}
	Q_strncpyz(client->lastcustomname, newname, sizeof(client->lastcustomname));
}

void teleportClientToArena(client_t *client, vec3_t spawn) {
	playerState_t *ps;
	ps = SV_GameClientNum(client - svs.clients);
	VectorCopy(spawn, ps->origin);
	VectorClear(ps->velocity);
}

void addClientToArena (client_t *cl, int target) {
	if (sv.Arenas[target].player1num != 99) {
		sv.Arenas[target].player2num = cl->playernum;
	} else {
		sv.Arenas[target].player1num = cl->playernum;
	}
	cl->arena = target;
	sv.Arenas[target].population++;
}


void update1v1Location(client_t *client, int type) {
	if (type == 1 || client->arena == 99) {
		playerState_t *ps;
		ps = SV_GameClientNum(client->playernum);
		int team;
		team = *(int*)((void*)ps+gclientOffsets[getVersion()][OFFSET_TEAM]);
		if (team == 3) {
			Cmd_ExecuteString(va("location %i \"^7[^5No Arena^7][^3Skill ^5%i^7]\" 0 1", client->playernum, client->skill));
		} else {
			Cmd_ExecuteString(va("location %i \"^7[^3Arena ^5%i^7][^3Skill ^5%i^7] ^5- ^3facing ^1no opponent\" 0 1", client->playernum, client->arena+1, client->skill));
		}
	}

	else if (type == 2 && sv.Arenas[client->arena].population == 2) {
		// get the clients opponent
		client_t *cl2;
		if (sv.Arenas[client->arena].player1num == client->playernum) {
			cl2 = &svs.clients[sv.Arenas[client->arena].player2num];
		} else {
			cl2 = &svs.clients[sv.Arenas[client->arena].player1num];
		}

		Cmd_ExecuteString(va("location %i \"^7[^3Arena ^5%i^7][^3Skill ^5%i^7] ^5- ^3facing ^6%s\" 0 1", client->playernum, client->arena+1, client->skill, cl2->colourName));

	} else if (type == 2 && sv.Arenas[client->arena].population == 1) {
		update1v1Location(client, 1);
	}
}

void arenaAdjustments( void ) {

	// this is as jank as it gets

	if (!mod_1v1arena->integer) {
		return;
	}

	int         i;
    client_t    *cl;

    for (int i = 0; i < MAX_1V1_ARENAS; i++) {
        if (sv.Arenas[i].population == 0) {
			
			// Move up empty arenas
            for (int j = i + 1; j < MAX_1V1_ARENAS; j++) {
                if (sv.Arenas[j].population == 2) {
                    sv.Arenas[i].population = 2;
                    sv.Arenas[j].population = 0;
					sv.Arenas[i].player1num = sv.Arenas[j].player1num;
					sv.Arenas[i].player2num = sv.Arenas[j].player2num;
					sv.Arenas[j].player1num = 99;
					sv.Arenas[j].player2num = 99;
					client_t *client;
					client = &svs.clients[sv.Arenas[i].player1num];
					client->arena = i;
					client = &svs.clients[sv.Arenas[i].player2num];
					client->arena = i;
                    break;
                }
            }
		}
	}

    for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {

		// valid client
        if (!cl->state) {
            continue;
		}

		// check if the client has played the last round, if so he needs adjusting
		if (cl->didplay) {
			if (cl->didWin) {
				// The client won
				if (cl->arena == 0) {
					// No need to adjust the client if he is in the highest arena already
					continue;
				}

				int newarena = cl->arena - 1;

				// find the looser from newarena
				client_t *looser;
				client_t *looser2;
				looser = &svs.clients[sv.Arenas[newarena].player1num];
				looser2 = &svs.clients[sv.Arenas[newarena].player2num];

				if (looser->didLose && looser->state) {

					// switch us with the looser
					sv.Arenas[newarena].player1num = cl->playernum;
					looser->arena = cl->arena;
					if (sv.Arenas[cl->arena].player1num == cl->playernum) {
						sv.Arenas[cl->arena].player1num = looser->playernum;
					} else {
						sv.Arenas[cl->arena].player2num = looser->playernum;
					}

					cl->arena = newarena;

					cl->didWin = qfalse;
					looser->didLose = qfalse;

				} else if (looser2->didLose && looser2->state) {

					sv.Arenas[newarena].player2num = cl->playernum;
					looser2->arena = cl->arena;
					if (sv.Arenas[cl->arena].player1num == cl->playernum) {
						sv.Arenas[cl->arena].player1num = looser2->playernum;
					} else {
						sv.Arenas[cl->arena].player2num = looser2->playernum;
					}

					
					cl->arena = newarena;
					cl->didWin = qfalse;
					looser2->didLose = qfalse;
				} else {
					
					// remove us from the current arena
					if (sv.Arenas[cl->arena].player1num == cl->playernum) {
						sv.Arenas[cl->arena].player1num = 99;
					} else {
						sv.Arenas[cl->arena].player2num = 99;
					}
					// add us to the new arena
					if (sv.Arenas[newarena].player1num == 99) {
						sv.Arenas[newarena].player1num = cl->playernum;
					} else {
						sv.Arenas[newarena].player2num = cl->playernum;
					}
					sv.Arenas[cl->arena].population--;
					cl->arena = newarena;
					sv.Arenas[cl->arena].population++;
					cl->didWin = qfalse;
				}
			}
		}
	}

    for (int i = 0; i < MAX_1V1_ARENAS; i++) {
        if (sv.Arenas[i].population == 0) {
			
			// Move up empty arenas
            for (int j = i + 1; j < MAX_1V1_ARENAS; j++) {
                if (sv.Arenas[j].population == 2) {
                    sv.Arenas[i].population = 2;
                    sv.Arenas[j].population = 0;
					sv.Arenas[i].player1num = sv.Arenas[j].player1num;
					sv.Arenas[i].player2num = sv.Arenas[j].player2num;
					sv.Arenas[j].player1num = 99;
					sv.Arenas[j].player2num = 99;
					client_t *client;
					client = &svs.clients[sv.Arenas[i].player1num];
					client->arena = i;
					client = &svs.clients[sv.Arenas[i].player2num];
					client->arena = i;
                }
            }
        } else if (sv.Arenas[i].population == 1) {
			client_t *client;
			// Merge arenas with 1 player in it
            for (int j = i + 1; j < MAX_1V1_ARENAS; j++) {
                if (sv.Arenas[j].population == 1) {
					if (sv.Arenas[i].player1num != 99) {
						if (sv.Arenas[j].player1num != 99) {
							sv.Arenas[i].player2num = sv.Arenas[j].player1num;
							sv.Arenas[j].player1num = 99;
							client = &svs.clients[sv.Arenas[i].player2num];
						} else {
							sv.Arenas[i].player2num = sv.Arenas[j].player2num;
							sv.Arenas[j].player2num = 99;
							client = &svs.clients[sv.Arenas[i].player2num];
						}
						client->arena = i;
						sv.Arenas[i].population++;
						sv.Arenas[j].population--;
					} else {
						if (sv.Arenas[j].player1num != 99) {
							sv.Arenas[i].player1num = sv.Arenas[j].player1num;
							sv.Arenas[j].player1num = 99;
							client = &svs.clients[sv.Arenas[i].player1num];
						} else {
							sv.Arenas[i].player1num = sv.Arenas[j].player2num;
							sv.Arenas[j].player2num = 99;
							client = &svs.clients[sv.Arenas[i].player1num];
						}
						client->arena = i;
						sv.Arenas[i].population++;
						sv.Arenas[j].population--;
					}
				}
			}
		}
    }

	for (int i = 0; i < MAX_1V1_ARENAS; i++) {
		if (sv.Arenas[i].population == 0 || sv.Arenas[i].population == 1) {
			sv.Arenas[i].finished = qtrue;
			sv.finishedArenas++;
		} else {
			sv.Arenas[i].finished = qfalse;
		}
	}
}

void adjustSkillRating(client_t *client_killer, client_t* client_killed) {
	// If the killer skill rating is higher than the killed one, increment the killer skill rating by one and decrement the killed skill rating by one if it is not 0
	if (client_killer->skill >= client_killed->skill) {
		client_killer->skill++;
		SV_SendServerCommand(client_killer, "cchat \"\" \"^5[^3Arena^5] ^3You gain ^21 SP ^3for killing ^6%s.\"", client_killed->colourName);
		if (client_killed->skill != 0) {
			client_killed->skill--;
			SV_SendServerCommand(client_killed, "cchat \"\" \"^5[^3Arena^5] ^3You loose ^11 SP ^3by dying to ^6%s.\"", client_killer->colourName);
		}
	} // otherwise increment the killer skill rating by 2 and decrement the killed skill rating by 2 if it is not 0
	else {
		client_killer->skill += 2;
		SV_SendServerCommand(client_killer, "cchat \"\" \"^5[^3Arena^5] ^3You gain ^22 SP ^3for killing ^6%s.\"", client_killed->colourName);
		if (client_killed->skill != 0) {
			client_killed->skill -= 2;
			SV_SendServerCommand(client_killed, "cchat \"\" \"^5[^3Arena^5] ^3You loose ^22 SP ^3by dying to ^6%s.\"", client_killer->colourName);
		}
	}
}

void updateScores (client_t *cl) {
	Cmd_ExecuteString(va("setscore %i %i", cl->playernum, cl->arenaKills));
	Cmd_ExecuteString(va("setdeaths %i %i", cl->playernum, cl->arenaDeaths));
}

void checkIfGameFinished ( void ) {
	if (sv.finishedArenas == MAX_1V1_ARENAS) {
		sv.finishedArenas = 0;
		unsigned int time = Com_Milliseconds();
		sv.doSmite = time;
		sv.doneSmite = qfalse;
	}
}

void EV_1v1PlayerSpawn (int cnum) {

	if (!mod_1v1arena->integer) {
		return;
	}

	if(cnum < 0 || cnum > sv_maxclients->integer) {
		return;
	}

    client_t *cl;
    cl = cnum + svs.clients;

	if (cl->state < CS_CONNECTED) {
        return;
    }

	// First disarm the client
	Cmd_ExecuteString(va("disarm %i", cnum));

	// choose a random number between 0 and 20
	int random = rand() % 20;
	teleportClientToArena(cl, sv.defaultSpawns[random]);
}

/*
=================
SV_onSurvivorWinner1v1


=================
*/
void SV_onSurvivorWinner1v1 (void) {
	if (!mod_1v1arena->integer) {
		return;
	}

    int       i;
    client_t  *cl;

    for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {

        if (!cl->state) {
            continue;
        }
		cl->weaponGiven = qfalse;
		updateScores(cl);
	}

	// iterate through all arenas
	for (i = 0; i < MAX_1V1_ARENAS; i++) {

		if (sv.Arenas[i].population == 2) {
			client_t *cl1;
			client_t *cl2;
			cl1 = &svs.clients[sv.Arenas[i].player1num];
			cl2 = &svs.clients[sv.Arenas[i].player2num];

			if (!cl1->didWin && !cl2->didWin) {
				int random = rand() % 2;
				if (random == 0) {
					cl1->didWin = qtrue;
					cl2->didLose = qtrue;
					SV_SendServerCommand(cl1, "cchat \"\" \"^5[^3Arena^5] ^1Draw: ^3By ^5Random Choice ^3you ^2WON ^3this round.\"");
					SV_SendServerCommand(cl2, "cchat \"\" \"^5[^3Arena^5] ^1Draw: ^3By ^5Random Choice ^3you ^1LOST ^3this round\"");
				} else {
					cl1->didLose = qtrue;
					cl2->didWin = qtrue;
					SV_SendServerCommand(cl2, "cchat \"\" \"^5[^3Arena^5] ^1Draw: ^3By ^5Random Choice ^3you ^2WON ^3this round.\"");
					SV_SendServerCommand(cl1, "cchat \"\" \"^5[^3Arena^5] ^1Draw: ^3By ^5Random Choice ^3you ^1LOST ^3this round\"");				
				}
			}
		}
	}
}

/*
===============
SV_1V1SurvivorRoundStart


===============
*/
void SV_1V1SurvivorRoundStart(void) {
	if (!mod_1v1arena->integer) {
		return;
	}

	sv.finishedArenas = 0;
	sv.doneSmite = qfalse;
	client_t *client;

	// A round just started, check if there is any arenas not filled properly and if so adjust them
	arenaAdjustments();

	client_t *cl;
	int i;
	int currplyrs = 0;
    for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
		if (cl->state == CS_ACTIVE) {
			cl->weaponGiven = qfalse;
			int team = SV_GetClientTeam(cl->playernum);
			if (team < 3) {
				currplyrs++;
			}
        }
	}
	if (currplyrs >= 2) {
		sv.doneTp = qfalse;
		unsigned int time = Com_Milliseconds();
		sv.doTeleportTime = time;
		SV_SendServerCommand(NULL, "chat \"^3You will be ^2Teleported ^3to ^5your Arena ^3in ^610 seconds.\"");
	}

	// tell each client what arena they are playing in and who their opponent is
	for (int i = 0; i < MAX_1V1_ARENAS; i++) {
		if (sv.Arenas[i].population == 1) {
			if (sv.Arenas[i].player1num ==  99 ) {
				client = &svs.clients[sv.Arenas[i].player2num];
				client->didplay = qtrue;
				client->didWin = qtrue;
				Cmd_ExecuteString(va("scc %i cp \"%s ^3You are in ^5Arena [^2%i^5] ^3facing ^1no opponent.\"", sv.Arenas[i].player2num, arenaPrefix, i+1));
				update1v1Location(client, 1);
			} else {
				client = &svs.clients[sv.Arenas[i].player1num];
				client->didplay = qtrue;
				client->didWin = qtrue;
				Cmd_ExecuteString(va("scc %i cp \"%s ^3You are in ^5Arena [^2%i^5] ^3facing ^1no opponent.\"", sv.Arenas[i].player1num, arenaPrefix, i+1));
				update1v1Location(client, 1);
			}
			// update the names
			update1v1Name(client);
			// update the location strings
			update1v1Location(client, 2);
		}

		else if (sv.Arenas[i].population == 2) {
			client_t* client;
			client_t* client2;
			client = &svs.clients[sv.Arenas[i].player1num];
			client2 = &svs.clients[sv.Arenas[i].player2num];
			client->didplay = qtrue;			
			client2->didplay = qtrue;
			client->didWin = qfalse;
			client2->didWin = qfalse;
			client->didLose = qfalse;
			client2->didLose = qfalse;

			Cmd_ExecuteString(va("scc %i cp \"%s ^3You are in ^5Arena [^2%i^5] ^3facing ^6%s.\"", sv.Arenas[i].player1num, arenaPrefix, i+1, client2->colourName));
			Cmd_ExecuteString(va("scc %i cp \"%s ^3You are in ^5Arena [^2%i^5] ^3facing ^6%s.\"", sv.Arenas[i].player2num, arenaPrefix, i+1, client->colourName));

			update1v1Location(client, 2);
			update1v1Location(client2, 2);
			// update the names
			update1v1Name(client);
			update1v1Name(client2);
			// update the location strings
			update1v1Location(client, 2);
			update1v1Location(client2, 2);
		}
	}
}

/*
===============
SV_onClientBegin


===============
*/
void SV_onClientBegin(char* player)
{
	if (!mod_1v1arena->integer) {
		return;
	}

	client_t *cl;
	cl = &svs.clients[atoi(player)];
	
	playerState_t *ps;
	ps = SV_GameClientNum(cl - svs.clients);

	int team;
	team = *(int*)((void*)ps+gclientOffsets[getVersion()][OFFSET_TEAM]);

	if (team == 0 || team == 1 || team == 2) {

		// find the last arena that has a population greater than 1

		qboolean havePlayers = qfalse;
		int currArenas[MAX_1V1_ARENAS];
		int curr = 0;

		for (int i = 0; i < MAX_1V1_ARENAS; i++) {
			if (sv.Arenas[i].population >= 1) {

				// there are players in arenas
				havePlayers = qtrue;

				currArenas[curr] = i;
				curr++;
			}
		}

		if (!havePlayers) {
			// no players in any arena, so just add us to the first one
			addClientToArena(cl, 0);
			update1v1Location(cl, 2);

		} else {
			// there are arenas, get the highest value from currArenas
			int highest = 0;
			for (int i = 0; i < curr; i++) {
				if (currArenas[i] > highest) {
					highest = currArenas[i];
				}
			}

			if (sv.Arenas[highest].population == 1) {
				// the highest arena has only one player, so add us to it
				addClientToArena(cl, highest);
				update1v1Location(cl, 2);
			} else {
				// the highest arena has more than one player, so add us to the next highest
				addClientToArena(cl, highest+1);
				update1v1Location(cl, 2);
			}
		}

	} else {

		update1v1Location(cl, 1);
		// client is joining spec, force his arena to 99
		if (cl->arena == 99) {
			return;
		}
		client_t* opponent;
		// remove us from the clients arena

		if (sv.Arenas[cl->arena].population == 2) {
			if (sv.Arenas[cl->arena].player1num == cl->playernum) {
				opponent = sv.Arenas[cl->arena].player2num + svs.clients;
				sv.Arenas[cl->arena].player1num = 99;
			} else {
				opponent = sv.Arenas[cl->arena].player1num + svs.clients;
				sv.Arenas[cl->arena].player2num = 99;
			}
			opponent->didWin = qtrue;
			SV_SendServerCommand(opponent, "cchat \"\" \"^5[^3Arena^5] ^8Disconnect: ^3Your opponent ^1disconnected^3, you ^2win ^3by default.\"");
		} else {
			if (sv.Arenas[cl->arena].player1num == cl->playernum) {
				sv.Arenas[cl->arena].player1num = 99;
			} else {
				sv.Arenas[cl->arena].player2num = 99;
			}
		}
		sv.Arenas[cl->arena].population--;
		cl->arena = 99;
		cl->didplay = qfalse;
	}
	update1v1Name(cl);
	
}

/*
===============
SV_onClientDisconnect


===============
*/
void SV_onClientDisconnect(char* cnum)
{
	if (!mod_1v1arena->integer) {
		return;
	}
	// So the client is disconnecting from the server, we now need to remove him from his arena
	client_t* client;
	client_t* opponent;

	client = atoi(cnum) + svs.clients;

	// Client did not have a arena
	if (client->arena == 99) {
		return;
	}

	if (sv.Arenas[client->arena].population == 2) {
		if (sv.Arenas[client->arena].player1num == client->playernum) {
			opponent = sv.Arenas[client->arena].player2num + svs.clients;
			sv.Arenas[client->arena].player1num = 99;
		} else {
			opponent = sv.Arenas[client->arena].player1num + svs.clients;
			sv.Arenas[client->arena].player2num = 99;
		}
		opponent->didWin = qtrue;
		SV_SendServerCommand(opponent, "cchat \"\" \"^5[^3Arena^5] ^8Disconnect: ^3Your opponent ^1disconnected^3, you ^2win ^3by default.\"");
		update1v1Location(opponent, 1);

	} else {
		if (sv.Arenas[client->arena].player1num == client->playernum) {
			sv.Arenas[client->arena].player1num = 99;
		} else {
			sv.Arenas[client->arena].player2num = 99;
		}
	}
	sv.Arenas[client->arena].population--;
	client->arena = 99;
}

/*
===============
SV_onKill1V1


===============
*/
void SV_onKill1V1(char* killer, char* killed, char* wpn)
{
	if (!mod_1v1arena->integer) {
		return;
	}

	client_t* client_killer;
	client_t* client_killed;

	client_killer = &svs.clients[atoi(killer)];
	client_killed = &svs.clients[atoi(killed)];

	if (!sv.Arenas[client_killed->arena].finished) {
		sv.Arenas[client_killed->arena].finished = qtrue;
		sv.finishedArenas++;
	}

	if ((Q_stricmp(killer, "1022") == 0) || ((Q_stricmp(killer, killed) == 0))) {
		if (client_killed->arena != 99 && sv.Arenas[client_killed->arena].population == 2) {
			client_t* client_opponent;
			if (sv.Arenas[client_killed->arena].player1num == client_killed->playernum) {
				client_opponent = sv.Arenas[client_killed->arena].player2num + svs.clients;
			} else {
				client_opponent = sv.Arenas[client_killed->arena].player1num + svs.clients;
			}
			// check if there is a winner already decided
			if (client_opponent->didWin || client_killed->didWin) {
				return;
			}
			client_opponent->didWin = qtrue;
			client_killed->didLose = qtrue;
			client_killed->arenaDeaths++;
			SV_SendServerCommand(client_killed, "cchat \"\" \"^5[^3Arena^5] ^1Suicide: ^3You ^1Loose ^3this round.\"");
			SV_SendServerCommand(client_opponent, "cchat \"\" \"^5[^3Arena^5] ^1Opponent Suicide: ^3You ^2Win ^3this round.\"");
		}	
		updateScores(client_killed);
		checkIfGameFinished();
		return;
	}
	
	// calculate the skill for the clients
	adjustSkillRating(client_killer, client_killed);

	// Update the location string
	update1v1Location(client_killer, 2);

	// Update the scoreboard score
	updateScores(client_killer);
	updateScores(client_killed);

	if (!client_killer->didWin || !client_killed->didWin) {
		client_killer->arenaKills++;
		client_killed->arenaDeaths++;
	}

	client_killer->didWin = qtrue;
	client_killed->didLose = qtrue;

	checkIfGameFinished();
}
