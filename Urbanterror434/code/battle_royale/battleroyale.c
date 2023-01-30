#include "../server/server.h"

/*
===============
SV_onSurvivorWinner

Announce the winner of the current round
===============
*/

void SV_onSurvivorWinner(char* playerid) {
	if (mod_battleroyale->integer) {
		client_t* cl;
		cl = &svs.clients[atoi(playerid)];

		// Send the message
		SV_SendServerCommand(NULL, "print \"^5%s ^3has ^2WON ^3this round!\n\"", cl->name);
	}

}

/*
===============
SV_BRSurvivorRoundStart

Give a brief description on how to play Battle Royale
===============
*/

void SV_BRSurvivorRoundStart(void) {
	if (!mod_battleroyale->integer) {
		return;
	}
	SV_SendServerCommand(NULL, "cp \"^6~> ^5Battle Royale ^6<~\n\n\n^4Guns ^3can be found around the map\n^3Each ^2Zone ^3contains different gun tiers\"");

	// Reset the previous collected weapons
	client_t* cl;
	int i;
	for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
		cl->lastPistol = NULL;
		cl->lastSecondary = NULL;
		cl->lastRifle = NULL;
		cl->lastSniper = NULL;
	}
}


/*
===============
SV_onItem

Item pickup event, handles removing the item and
giving the player the corresponding reward
===============
*/

void SV_onItem(char* playerid, char* itemname)
{
	if (!mod_battleroyale->integer) {
		return;
	}

	// Reward lists
	const char* zone1[] = {"deagle", "beretta", "glock", "colt", "magnum"};
	const char* zone2[] = {"spas12", "benelli", "mp5k", "ump45", "mac11", "p90"};
	const char* zone3[] = {"lr", "g36", "ak103", "m4", "negev"};
	const char* zone4[] = {"sr8", "psg1", "frf1"};


	// Get the client
	client_t* cl;
	playerState_t* ps;

	cl = &svs.clients[atoi(playerid)];
	ps = SV_GameClientNum(cl - svs.clients);


	/*
	Notes:
	- The random weapon is not truly random as I make sure no weapon is given twice in a row.
	  for the normal player it will feel more random this way and stops stupid repetition.

	- Removing the previous weapon helps minimizing / removing the strange behaviour when players
	  recieve a negev whilst holding a primary, secondary and pistol at the same time.
	  (weapons not being given properly etc)

	- A NULL check before removing the weapon is not required I think, it only raises a "Weapon not found"
	  message in the server console.
	*/


	// zone1 (ut_item_laser):
	if (!Q_stricmp(itemname, "ut_item_laser")) {

		// remove the item from the clients inventory
		Cmd_ExecuteString(va("removeitem %i laser", ps->clientNum));

		// get a random weapon
		int i = rand() % (sizeof(zone1) / sizeof(zone1[0]));

		// make sure the last weapon is not the new weapon, always give a different one
		while (zone1[i] == cl->lastPistol) {
			i = rand() % (sizeof(zone1) / sizeof(zone1[0]));
		}

		// remove our previous pistol
		Cmd_ExecuteString(va("rw %i %s", ps->clientNum, cl->lastPistol));

		// set our new weapon in the clientstate
		cl->lastPistol = zone1[i];
		
		// give the player the weapon
		Cmd_ExecuteString(va("giveweapon %i %s", ps->clientNum, zone1[i]));

		// let the player know what reward he has received
		SV_SendServerCommand(cl, "cchat \"\" \"^2Zone 1 ^4Random-Pistol ^7= ^5%s\"", zone1[i]);
	}

	// zone2 (ut_item_silencer):
	if (!Q_stricmp(itemname, "ut_item_silencer")) {
		// remove the item from the clients inventory
		Cmd_ExecuteString(va("ri %i silencer", ps->clientNum));

		// get a random weapon
		int i = rand() % (sizeof(zone2) / sizeof(zone2[0]));

		// make sure the last weapon is not the new weapon, always give a different one
		while (zone2[i] == cl->lastSecondary) {
			i = rand() % (sizeof(zone2) / sizeof(zone2[0]));
		}

		// remove our previous secondary
		Cmd_ExecuteString(va("rw %i %s", ps->clientNum, cl->lastSecondary));

		// set our new weapon in the clientstate
		cl->lastSecondary = zone2[i];

		// give the player the weapon
		Cmd_ExecuteString(va("gw %i %s", ps->clientNum, zone2[i]));

		// let the player know what reward he has received
		SV_SendServerCommand(cl, "cchat \"\" \"^2Zone 2 ^4Random-Secondary ^7= ^5%s\"", zone2[i]);
	}

	// zone3 (ut_item_medkit):
	if (!Q_stricmp(itemname, "ut_item_medkit")) {
		// remove the item from the clients inventory
		Cmd_ExecuteString(va("ri %i medkit", ps->clientNum));

		// get a random weapon
		int i = rand() % (sizeof(zone3) / sizeof(zone3[0]));

		// make sure the last weapon is not the new weapon, always give a different one
		while (zone3[i] == cl->lastRifle) {
			i = rand() % (sizeof(zone3) / sizeof(zone3[0]));
		}

		// remove our previous rifle
		Cmd_ExecuteString(va("rw %i %s", ps->clientNum, cl->lastRifle));

		// set our new weapon in the clientstate
		cl->lastRifle = zone3[i];
		
		// give the player the weapon
		Cmd_ExecuteString(va("gw %i %s", ps->clientNum, zone3[i]));

		// let the player know what reward he has received
		SV_SendServerCommand(cl, "cchat \"\" \"^2Zone 3 ^4Random-Rifle ^7= ^5%s\"", zone3[i]);
	}

	// zone4 (ut_item_nvg):
	if (!Q_stricmp(itemname, "ut_item_nvg")) {
		// remove the item from the clients inventory
		Cmd_ExecuteString(va("ri %i nvg", ps->clientNum));

		// get a random weapon
		int i = rand() % (sizeof(zone4) / sizeof(zone4[0]));

		// make sure the last weapon is not the new weapon, always give a different one
		while (zone4[i] == cl->lastSniper) {
			i = rand() % (sizeof(zone4) / sizeof(zone4[0]));
		}

		// remove our previous sniper
		Cmd_ExecuteString(va("rw %i %s", ps->clientNum, cl->lastSniper));

		// set our new weapon in the clientstate
		cl->lastSniper = zone4[i];

		// give the player the weapon
		Cmd_ExecuteString(va("gw %i %s", ps->clientNum, zone4[i]));

		// let the player know what reward he has received
		SV_SendServerCommand(cl, "cchat \"\" \"^2Zone 4 ^4Random-Sniper ^7= ^5%s\"", zone4[i]);
	}

	// Health packs:
	if (!Q_stricmp(itemname, "ut_weapon_grenade_flash")) {
		// remove the item from the clients inventory
		Cmd_ExecuteString(va("rw %i flash", ps->clientNum));

		// give the player player 50 health
		Cmd_ExecuteString(va("addhealth %i 50", ps->clientNum));

		// let the player know what reward he has received
		SV_SendServerCommand(cl, "cchat \"\" \"^2+50 ^5Health!\"");
	}
}
