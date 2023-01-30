/*
 * qvmStructs.h
 *
 *  Created on: 28 feb. 2017
 *      Author: pedro
 */

#ifndef CODE_SERVER_QVMSTRUCTS_H_
#define CODE_SERVER_QVMSTRUCTS_H_

enum weaponOffsetStructure
{
	OFFSET_BASE,
	OFFSET_CLIPS,
	OFFSET_BULLETS,
	OFFSET_DAMAGES,
	OFFSET_BLEED,
	OFFSET_KNOCKBACK,
	OFFSET_RELOADSPEED,
	OFFSET_FIRETIME,
	OFFSET_NORECOIL,
	OFFSET_WPFLAG,
	SIZE,
	OFFSET_WP_MAX
};

enum playerStats
{
	OFFSET_PS_STAMINA,
	OFFSET_PS_HEALTH,
	OFFSET_PS_RECOIL,
	OFFSET_PS_MAX
};

enum gclientOffsetStructure
{
	OFFSET_NOGEARCHANGE,
	OFFSET_TEAM,
	OFFSET_GL_MAX
};

typedef enum
{
	vunk,
	v42023,
	v43,
	v431,
	v432,
	v433,
	v434,
	vMAX,
}urtVersion;

extern const char versionString[vMAX][10];
extern char (*weaponString)[15];
extern char itemString[UT_ITEM_MAX][10];
extern char teamstring[4][5];
extern char hitlocationstring[HL_MAX][15];

extern int ut_weapon_xor;
extern int gclientOffsets[vMAX][OFFSET_GL_MAX];
extern int playerStatsOffsets[vMAX][OFFSET_PS_MAX];



#define MAX_NETNAME 36
typedef struct
{
	int	connected;
	usercmd_t	cmd;
	qboolean unknow1;
	qboolean unknow2;
	char		netname[MAX_NETNAME];
}clientPersistant_t;

typedef struct gentity_s
{
	int unknow[171];
	int health;
}gentity_t;

typedef struct gclient_s
{
	playerState_t ps;
	clientPersistant_t pers;
}gclient_t;


#endif /* CODE_SERVER_QVMSTRUCTS_H_ */
