/*
 * game_weapon.c
 *
 *  Created on: 19 feb. 2017
 *      Author: pedro
 */

#include "server.h"

void updateXOR(playerState_t *ps)
{
	ut_weapon_xor = ps->powerups[MAX_POWERUPS-1];
}
int SV_LastWeaponNum(playerState_t * ps)
{
	int i, last = 0;
	for(i = 0; i < MAX_POWERUPS; i++)
	{
		ut_weapon_xor = ps->powerups[(MAX_POWERUPS - 1)];
		if(UT_WEAPON_GETID(ps->powerups[i]) != UT_WP_NONE)
		{
			last++;
		}
	}
	if(last < 0) last = 1;
	return last-1;
}
utItemID_t SV_Char2Item(char *item)
{
	int i;
	for ( i = 0; i < (UT_ITEM_MAX); i++ ) {
		if ( Q_stricmp( itemString[i], item ) == 0) {
			return i;
		}
	}
	return 0;
}
weapon_t SV_Char2Weapon(char weapon[36])
{
	int i;
	for ( i = 0; i < (UT_WP_NUM_WEAPONS + 3); i++ ) {
		if ( Q_stricmp( weaponString[i], weapon ) == 0) {
			return i;
		}
	}
	return 0;
}

int SV_FirstMatchFor(playerState_t *ps, weapon_t weapon)
{
    int i;
    if (weapon == UT_WP_NONE)
        return -1;

    for(i = 0; i < MAX_POWERUPS; i++)
    {
        if(UT_WEAPON_GETID(ps->powerups[i]) == weapon)
        {
            return i;
        }
    }

    return -1;
}

int  generateWeapon(playerState_t *ps, int clips, int mode, int bullet, int weapon)
{
	updateXOR(ps);
	int pweapon = 0;
	UT_WEAPON_SETBULLETS(pweapon, bullet);
	UT_WEAPON_SETCLIPS(pweapon, clips);
	UT_WEAPON_SETMODE(pweapon, mode);
	UT_WEAPON_SETID(pweapon, weapon);

	return pweapon;
}
void SV_GiveBulletsAW(playerState_t *ps, int bulletsCount, int weapon)
{
	updateXOR(ps);
	bulletsCount += UT_WEAPON_GETBULLETS(ps->powerups[weapon]);
	//The max bullets allowed on urbanterror is 255, we should handle it
	if(bulletsCount > 255)
		bulletsCount = 255;
	if(bulletsCount < 0)
		bulletsCount = 0;
	UT_WEAPON_SETBULLETS(ps->powerups[weapon], bulletsCount);
}
void SV_GiveClipsAW(playerState_t *ps, int clipsCount, int weapon)
{
	updateXOR(ps);
	clipsCount += UT_WEAPON_GETCLIPS(ps->powerups[weapon]);
	//The max clips allowed on urbanterror is 255, we should handle it
	if(clipsCount > 255)
		clipsCount = 255;
	if(clipsCount < 0)
		clipsCount = 0;
	UT_WEAPON_SETCLIPS(ps->powerups[weapon], clipsCount);
}
void SV_SetBulletsAW(playerState_t *ps, int bulletsCount, int weapon)
{
	updateXOR(ps);
	//The max bullets allowed on urbanterror is 255, we should handle it
	if(bulletsCount > 255)
		bulletsCount = 255;
	if(bulletsCount < 0)
		bulletsCount = 0;
	UT_WEAPON_SETBULLETS(ps->powerups[weapon], bulletsCount);
}
void SV_SetClipsAW(playerState_t *ps, int clipsCount, int weapon)
{
	updateXOR(ps);
	//The max clips allowed on urbanterror is 255, we should handle it
	if(clipsCount > 255)
		clipsCount = 255;
	if(clipsCount < 0)
		clipsCount = 0;
	UT_WEAPON_SETCLIPS(ps->powerups[weapon], clipsCount);
}

int utPSFirstMath (playerState_t *ps, utItemID_t itemid)
{
    int i;
    for(i = 0; i < MAX_WEAPONS; i++)
    {
        if(UT_ITEM_GETID(ps->ammo[i]) == itemid)
        {
            return i;
        }
    }
    return -1;
}

void utPSGiveItem ( playerState_t *ps, utItemID_t itemid )
{
	int  i,itemSlot = -1;
	//Search for a empty itemslot
	for(i=0;i<MAX_WEAPONS;i++)
	{
		if(UT_ITEM_GETID(ps->ammo[i]) == UT_ITEM_NONE)
		{
			itemSlot = i;
		}
	}
	if(itemSlot == -1)
		return;
	switch (itemid)
	{
		case UT_ITEM_AMMO:
			{
				for (i = 0; i < MAX_POWERUPS; i++) //UT_MAX_WEAPONS
				{
					int  weaponID = UT_WEAPON_GETID( ps->powerups[i] );
					int  ammo;

					if (weaponID == UT_WP_KNIFE || weaponID == UT_WP_NONE)
					{
						continue;
					}

					if ((weaponID == UT_WP_GRENADE_HE) || (weaponID == UT_WP_GRENADE_FLASH) || (weaponID == UT_WP_GRENADE_SMOKE))
					{
						continue;
					}

					ammo  = UT_WEAPON_GETCLIPS(ps->powerups[i]);
					ammo += *(int*)QVM_clips(weaponID);

					UT_WEAPON_SETCLIPS(ps->powerups[i], ammo );
				}
				break;
			}

		default:
			UT_ITEM_SETID( ps->ammo[itemSlot], itemid );
			UT_ITEM_SETFLAGS(ps->ammo[itemSlot], UT_ITEMFLAG_ON);
			break;
	}
}
void utPSRemoveItem ( playerState_t *ps, utItemID_t itemid )
{
	int i;
	for(i = 0; i < MAX_WEAPONS; i++)
	{
		if(UT_ITEM_GETID(ps->ammo[i]) == itemid)
		{
			UT_ITEM_SETID(ps->ammo[i],0);
		}
	}
}

void SV_AutoItemGive(playerState_t *ps)
{
	int i;
	team_t team = *(int*)((void*)ps+gclientOffsets[getVersion()][OFFSET_TEAM]);

	if(team == TEAM_SPECTATOR)
		return;

	if(strlen(Cvar_VariableString(va("%s_giveitem", teamstring[team]))))
	{
		Cmd_TokenizeString(Cvar_VariableString(va("%s_giveitem", teamstring[team])));
		for(i = 0; i < Cmd_Argc(); i++)
		{
			utPSGiveItem(ps, SV_Char2Item(Cmd_Argv(i)));
		}
	}
}
void SV_AutoItemTake(playerState_t *ps)
{
	int i;
	team_t team = *(int*)((void*)ps+gclientOffsets[getVersion()][OFFSET_TEAM]);

	if(team == TEAM_SPECTATOR)
		return;

	if(strlen(Cvar_VariableString(va("%s_takeitem", teamstring[team]))))
	{
		Cmd_TokenizeString(Cvar_VariableString(va("%s_takeitem", teamstring[team])));
		for(i = 0; i < Cmd_Argc(); i++)
		{
			utPSRemoveItem(ps, SV_Char2Item(Cmd_Argv(i)));
		}
	}
}

void SV_GiveWeapon(playerState_t *ps, weapon_t wp)
{
	int i;
	for(i = 0; i < (MAX_POWERUPS - 1); i++)
	{
		if(i == 15)
		{
			Com_Printf("No space\n");
			return;
		}
		if(ps->powerups[i] == ps->powerups[(MAX_POWERUPS - 1)])
		{
                    int mode;
                    mode = Info_ValueForKey(svs.clients[ps->clientNum].userinfo, "weapmodes")[wp] - '0';
                    if(mode > 2)
                        mode = 0;
                    ps->powerups[i] = generateWeapon(ps, *(int*)QVM_clips(wp), mode, *(int*)QVM_bullets(wp),wp);
                    break;
		}
	}
}

void SV_GiveWeaponCB(playerState_t *ps, weapon_t wp, int bullets, int clips)
{
    int i;
    for(i = 0; i < (MAX_POWERUPS - 1); i++)
    {
        if(i == 15)
        {
            Com_Printf("No space\n");
            return;
        }
        if(ps->powerups[i] == ps->powerups[(MAX_POWERUPS - 1)])
        {
            int mode;
            mode = Info_ValueForKey(svs.clients[ps->clientNum].userinfo, "weapmodes")[wp] - '0';
            if(mode > 2)
                mode = 0;
            ps->powerups[i] = generateWeapon(ps, clips, mode, bullets,wp);
            break;
        }
    }
}




void SV_RemoveWeapon(playerState_t *ps, weapon_t wp)
{
	int i;

	updateXOR(ps);
	for(i = 0; i < MAX_POWERUPS; i++)
	{
		if(ps->powerups[i] != ps->powerups[(MAX_POWERUPS - 1)])
		{
			if(UT_WEAPON_GETID(ps->powerups[i]) == wp)
			{
				ps->powerups[i] = ps->powerups[(MAX_POWERUPS - 1)];
			}
		}
	}
	ps->weapon = SV_LastWeaponNum(ps);
}
void SV_AutoRemove(playerState_t *ps)
{
	int i;
	team_t team = *(int*)((void*)ps+gclientOffsets[getVersion()][OFFSET_TEAM]);
	if(team == TEAM_SPECTATOR)
		return;

	if(strlen(Cvar_VariableString(va("%s_takeweapon", teamstring[team]))))
	{
		Cmd_TokenizeString(Cvar_VariableString(va("%s_takeweapon", teamstring[team])));
		for(i = 0; i < Cmd_Argc(); i++)
		{
			SV_RemoveWeapon(ps, SV_Char2Weapon(Cmd_Argv(i)));
		}
	}
}
void SV_AutoGive(playerState_t *ps)
{
	int i;
	team_t team = *(int*)((void*)ps+gclientOffsets[getVersion()][OFFSET_TEAM]);

	if(team == TEAM_SPECTATOR)
		return;

	if(strlen(Cvar_VariableString(va("%s_giveweapon", teamstring[team]))))
	{
		Cmd_TokenizeString(Cvar_VariableString(va("%s_giveweapon", teamstring[team])));
		for(i = 0; i < Cmd_Argc(); i++)
		{
			SV_GiveWeapon(ps, SV_Char2Weapon(Cmd_Argv(i)));
		}
	}
}

void SV_WeaponMod(int cnum)
{
    playerState_t *ps;
    ps = SV_GameClientNum(cnum);

    if(*(int *)((void*)ps+gclientOffsets[getVersion()][OFFSET_NOGEARCHANGE]))
    {
        return;
    }
    SV_AutoRemove(ps);
    SV_AutoGive(ps);
    SV_AutoItemTake(ps);
    SV_AutoItemGive(ps);
    ps->weapon = SV_LastWeaponNum(ps);
}

int overrideQVMData(void)
{
	weapon_t weapon;
	int i;

	if(getVersion()==vunk)
		return 0;

	if (!mod_enableWeaponsCvars->integer) {
        Com_Printf("Warning: mod_enableWeaponsCvars is 0.\n");
	    return 0;
	}

	Com_Printf("Dumping weapon modifications to qvm memory... ");
	for(weapon = UT_WP_KNIFE; weapon < (UT_WP_NUM_WEAPONS + 3); weapon++)
	{
		//Check one by one all cvar that can contains some data for changes weapons
		if(Q_stricmp(Cvar_VariableString(va("%s_clips", weaponString[weapon])), ""))
		{
			*(int*)QVM_clips(weapon) = Cvar_VariableIntegerValue(va("%s_clips", weaponString[weapon]));
		}
		if(Q_stricmp(Cvar_VariableString(va("%s_bullets", weaponString[weapon])), ""))
		{
			*(int*)QVM_bullets(weapon) = Cvar_VariableIntegerValue(va("%s_bullets", weaponString[weapon]));
		}
		if(Q_stricmp(Cvar_VariableString(va("%s_knockback", weaponString[weapon])), ""))
		{
			*(float*)QVM_knockback(weapon) = Cvar_VariableValue(va("%s_knockback", weaponString[weapon]));
		}
		if(Q_stricmp(Cvar_VariableString(va("%s_reloadspeed", weaponString[weapon])), ""))
		{
			*(float*)QVM_reloadSpeed(weapon) = Cvar_VariableValue(va("%s_reloadspeed", weaponString[weapon]));
		}
		if(Q_stricmp(Cvar_VariableString(va("%s_damagesfactor", weaponString[weapon])), ""))
		{
			for(i=0;i<HL_MAX;i++)
			{
				*(float*)QVM_damages(weapon,i)*=Cvar_VariableValue(va("%s_damagesfactor", weaponString[weapon]));
			}
		}

		//Now check strings that have more values than only the name
		for(i=0; i<HL_MAX; i++)
		{
			if(Q_stricmp(Cvar_VariableString(va("%s_damages_%s", weaponString[weapon], hitlocationstring[i])), ""))
			{
				*(float*)QVM_damages(weapon,i) = Cvar_VariableValue(va("%s_damages_%s", weaponString[weapon], hitlocationstring[i]));
			}
			if(Q_stricmp(Cvar_VariableString(va("%s_bleed_%s", weaponString[weapon], hitlocationstring[i])), ""))
			{
				if(Cvar_VariableIntegerValue(va("%s_bleed_%s", weaponString[weapon], hitlocationstring[i])))
					*(float*)QVM_bleed(weapon,i) = 1;
				else
					*(float*)QVM_bleed(weapon,i) = 0;
			}

		}
		for(i=0; i<3; i++)
		{
			if(Q_stricmp(Cvar_VariableString(va("%s_%i_firetime", weaponString[weapon], i)), ""))
			{
				*(int*)QVM_fireTime(weapon,i) = Cvar_VariableIntegerValue(va("%s_%i_firetime", weaponString[weapon], i));
			}
			if(Q_stricmp(Cvar_VariableString(va("%s_%i_norecoil", weaponString[weapon], i)), ""))
			{
				*(int*)QVM_noRecoil(weapon,i) |= UT_WPMODEFLAG_NORECOIL;
			}
		}
	}
	Com_Printf("[OK]\n");
	return 1;
}
