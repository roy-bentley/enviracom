/**********************************************************************************/
/**************** PVCS HEADER for Enviracom API Source Code ***********************/
/**********************************************************************************/
/*										*/
/* Archive: /Projects/Enviracom API/EnviracomAPI.c $				*/
/* Date: 8/01/03 4:19p $                             				*/
/* Workfile: EnviracomAPI.c $							*/
/* Modtime: 8/01/03 4:12p $							*/
/* Author: Shoglund $								*/
/* Log: /Projects/Enviracom API/EnviracomAPI.c $			
 * 
 * 
 * 11    8/01/03 4:19p Shoglund
 * Added IsSimpleZoning() function- used to determine logic path for
 * SystemSwitch messages.  Changed .Periodic on SPLimits and Deadband to
 * FALSE.  Added special case handling in 60min/90min. RX processing for
 * SystemSwitch to prevent unnecessary querying of SystemSwitch.
 * 
 * 10    7/21/03 5:08p Shoglund
 * Changed MinDataLength to 8 for Setpoint message (1F80) to handle
 * FanSwitch setting.
 * Added 60minute query processing as workaround for T8635's missing
 * 30minute periodic messages.
 * Added processing for scheduled Fan Switch setting (1F80 message).
 * Corrected read of DataLength value in NewEnvrcmRxByte().
 * 
 * 9     6/12/03 3:30p Shoglund
 * V1.06: Fixed header (not 1.07)
 * 
 * 8     6/11/03 11:54a Shoglund
 * Fixed bug in NewEnvrcmRxByte()  validity checking on "inst"
 * that missed the highest valid zone number (i.e. zone = 4 if N_ZONES =
 * 4).
 * 
 * 6     5/16/03 9:21a Shoglund
 * Fixed bug in UnknownSchedule() that was causing lowest zone tstat to
 * show Unknown schedule values.
 * Fixed error in RcvSetpoints(): check against Cool setpoint used 
 * "|=" instead of "!=".
 *
 * 4     8/12/02 2:36p Snichols
 * Rev 1.05 - Fixed bug that did not set variable, inst, correctly after
 * receiving IDLE from the serial adaptor for processing a no response..
 * 
 * 3     7/12/02 12:12p Snichols
 * Fixed writing beyond array size for schedules.
 * 
 * 2     5/10/02 4:48p Snichols
 * Provided a workaround to a bug in the T8635 thermostat that did not
 * publish all of the required schedule messages when a program copy was
 * executed.  The new API implements a 35 second timer whenever a schedule
 * message is received that wasn't asked for.  When the timer expires, it
 * sends a wildcard query for the entire schedule.  Since the entire
 * schedule takes about 30 seconds to send, it can be over a minute from
 * the time the user presses Run Program until the entire schedule is
 * obtained.
 * 
 * 1     4/26/02 8:31a Snichols
 * Rev 1.02 - fixed bug so that it didn't check Setpoint Schedule every 90
 * minutes to see if it had been received.
 * 
 *          (do not change this line it is the end of the log comment)            */
/*                                                                                */

// indicate the name of this source file to the include files
#define _ENVIRACOMAPI_C_


//////////////////////////////////////////////////////////////////////////////////////////////////
// include files
//////////////////////////////////////////////////////////////////////////////////////////////////
#include "EnviracomAPI.h"
//////////////////////////////////////////////////////////////////////////////////////////////////
// Function prototypes
//////////////////////////////////////////////////////////////////////////////////////////////////
void RcvHeatPump(void);
void UnknownHeatPump(void);

void SendTime(void);
void RcvTime(void);

void RcvEquipFault(void);
void UnknownEquipFault(void);

void SendSetpoints(void);
void RcvSetpoints(void);
void UnknownSetpoints(void);

void SendSystemSwitch(void);
void RcvSystemSwitch(void);
void UnknownSystemSwitch(void);

void SendFanSwitch(void);
void RcvFanSwitch(void);
void UnknownFanSwitch(void);

void SendRoomTemp(void);
void RcvRoomTemp(void);
void UnknownRoomTemp(void);

void RcvHumidity(void);
void UnknownHumidity(void);

void SendOutdoorTemp(void);
void RcvOutdoorTemp(void);
void UnknownOutdoorTemp(void);

void SendAirFilterTimer(void);
void RcvAirFilterTimer(void);
void UnknownAirFilterTimer(void);

void RcvLimits(void);
void UnknownLimits(void);

void RcvDeadband(void);
void UnknownDeadband(void);

void SendSchedule(void);
void RcvSchedule(void);
void UnknownSchedule(void);

void RcvZoneMgr(void);
void UnknownZoneMgr(void);

void SendMessage(void);
UNSIGNED_BYTE CharToHex(UNSIGNED_BYTE Chr);
UNSIGNED_BYTE HexToChar(UNSIGNED_BYTE Hex);
UNSIGNED_BYTE CalcChecksum(UNSIGNED_BYTE* pBuffer);

//////////////////////////////////////////////////////////////////////////////////////////////////
// API Data declarations
//////////////////////////////////////////////////////////////////////////////////////////////////

UNSIGNED_WORD QueueFlags[N_ENVIRACOM_BUSSES];	// queue flags for non zoned report/change messages

UNSIGNED_WORD ReceiveFlags[N_ENVIRACOM_BUSSES];	// receive flags for non zoned report messages

UNSIGNED_WORD QueryFlags[N_ENVIRACOM_BUSSES];	// queue flags for non zoned query messages

UNSIGNED_WORD ZoneFlags[N_ENVIRACOM_BUSSES][N_ZONES];	// queue and receive flags for zoned report/change messages

UNSIGNED_WORD QueryZoneFlags[N_ENVIRACOM_BUSSES][N_ZONES];	// queue flags for zoned query messages

#ifdef SCHEDULESIMPLEMENTED
UNSIGNED_BYTE ScheduleFlags[N_ENVIRACOM_BUSSES][N_ZONES][N_SCHEDULE_PERIODS];	// queue flags for schedule change messages
UNSIGNED_BYTE ScheduleQueryFlags[N_ENVIRACOM_BUSSES][N_ZONES][N_SCHEDULE_PERIODS];	// queue flags for schedule query messages
#define CHANGE_MON			0x01	/* Monday schedule change queued flag */
#define CHANGE_TUE			0x02	/* Tuesday schedule change queued flag */
#define CHANGE_WED			0x04	/* Wednesday schedule change queued flag */
#define CHANGE_THU			0x08	/* Thursday schedule change queued flag */
#define CHANGE_FRI			0x10	/* Friday schedule change queued flag */
#define CHANGE_SAT			0x20	/* Saturday schedule change queued flag */
#define CHANGE_SUN			0x40	/* Sunday schedule change queued flag */
#endif

typedef void (*FUNCTION_POINTER)(void);  // pointer to a function

// identifiers for messages in order of priority, highest first
enum
{
	MSG_ZONEMGR,
	MSG_HEATPUMP,
	MSG_TIME,
	MSG_EQUIPFAULT,
	MSG_SETPOINTS,
	MSG_SYSSWITCH,
	MSG_FANSWITCH,
	MSG_ROOMTEMP,
	MSG_HUMIDITY,
	MSG_OUTDOORTEMP,
	MSG_AIRFILTER,
	MSG_LIMITS,
	MSG_DEADBAND,
#ifdef SCHEDULESIMPLEMENTED
	MSG_SCHEDULE,
#endif
	N_MESSAGES		// the number of messages in this table
};

// structure of a message configuration
typedef struct
{
	UNSIGNED_WORD		MessageClassID;		// Message class ID of message
	UNSIGNED_BYTE		Instance;			// Instance of message, or FF = zoned, FE = ALL
	UNSIGNED_BYTE		RxService;			// Receive service = 'R', 'C', or 'Q'
	UNSIGNED_BYTE		MinDataLength;		// the minimum data field size allowed for a received message
	UNSIGNED_WORD		QueueFlagMask;		// mask in QueueFlags or ZoneFlags to queue the message
	UNSIGNED_WORD		ReceivedFlagMask;	// mask in RcvFlags or ZoneFlags to indicate message was received
	UNSIGNED_BYTE		Periodic;			// TRUE if the message is sent periodically, FALSE if not
	FUNCTION_POINTER	pSendFunction;		// pointer to function that sends the message
	FUNCTION_POINTER	pReceiveFunction;	// pointer to function that receives the message
	FUNCTION_POINTER	pUnknownFunction;	// pointer to function that indicates data is unknown
} MESSAGE_CFG;

// queue flags
#define MSK_AIRFILTER	0x0001
#define MSK_HUMIDITY	0x0002
#define MSK_HEATPUMP	0x0004
#define MSK_EQUIPFAULT	0x0008
#define MSK_OUTDOORTEMP	0x0010
#define MSK_TIME		0x0020
#define MSK_ZONEMGR		0x0040
#define QRY_ROOMTEMPZ1	0x2000	/* flag indicating that the zone 1 room temp has been queried */
#define TX_NACK			0x4000	/* flag indicating that an Ack has been sent once */
#define TX_TIME			0x8000  /* flag indicating time change has been sent */

#define RX_ROOMTEMP		0x0001
#define QUE_SETPOINTS	0x0002
#define RX_SETPOINTS	0x0004
#define QUE_FANSWITCH	0x0008
#define RX_FANSWITCH	0x0010
#define QUE_SYSSWITCH	0x0020
#define RX_SYSSWITCH	0x0040
#define RX_LIMITS		0x0080
#define RX_DEADBAND		0x0100
#define TX_SETPOINT		0x8000	// flag indicating setpoint change has been sent */

// table of message configurations, in order of priority, highest first
const MESSAGE_CFG MsgTable[N_MESSAGES] =
{
	// Zone manager
	0x3EE0, 0x00, 'R', 1, 0x00,			MSK_ZONEMGR,	TRUE, NULL, RcvZoneMgr,  UnknownZoneMgr,
#ifdef EXTENSIONS
	// HeatPump was 0x41
	0x3E70, 0xFE, 'R', 4, 0x00,			MSK_HEATPUMP,	TRUE, NULL, RcvHeatPump, UnknownHeatPump,
#else
	0x3E70, 0x41, 'R', 4, 0x00,			MSK_HEATPUMP,	TRUE, NULL, RcvHeatPump, UnknownHeatPump,
#endif
	// Time
	0x313F, 0x00, 'Q', 0, MSK_TIME,		0x00,			TRUE, SendTime, RcvTime, NULL,
	// Equip fault
	0x3100, 0x00, 'R', 4, 0x00,			MSK_EQUIPFAULT, TRUE, NULL, RcvEquipFault, UnknownEquipFault,
	// Setpoints
	0x2330, 0xFF, 'R', 8, QUE_SETPOINTS, RX_SETPOINTS,	TRUE, SendSetpoints, RcvSetpoints, UnknownSetpoints,
	// System Switch
	0x22D0, 0xFF, 'R', 3, QUE_SYSSWITCH, RX_SYSSWITCH,	TRUE, SendSystemSwitch, RcvSystemSwitch, UnknownSystemSwitch,
	// Fan Switch
	0x22C0, 0xFF, 'R', 2, QUE_FANSWITCH, RX_FANSWITCH,	TRUE, SendFanSwitch, RcvFanSwitch, UnknownFanSwitch,
	// Room Temperature
	0x12C0, 0xFF, 'R', 2, 0x00,			RX_ROOMTEMP,	TRUE, SendRoomTemp, RcvRoomTemp, UnknownRoomTemp,
	// Humidity
	0x12A0, 0x00, 'R', 1, 0x00,			MSK_HUMIDITY,	TRUE, NULL, RcvHumidity, UnknownHumidity,
	// Outdoor temperature
	0x1290, 0x00, 'R', 2, MSK_OUTDOORTEMP, MSK_OUTDOORTEMP, TRUE, SendOutdoorTemp, RcvOutdoorTemp, UnknownOutdoorTemp, 
	// Air Filter Timer
	0x10D0, 0x00, 'R', 2, MSK_AIRFILTER, MSK_AIRFILTER, TRUE, SendAirFilterTimer, RcvAirFilterTimer, UnknownAirFilterTimer,
	// Setpoint Limits
	0x1040, 0xFF, 'R', 8, 0x00,			RX_LIMITS,		FALSE, NULL, RcvLimits, UnknownLimits,
	// Setpoint Deadband
	0x0A01, 0xFF, 'R', 8, 0x00,			RX_DEADBAND,	FALSE, NULL, RcvDeadband, UnknownDeadband
#ifdef SCHEDULESIMPLEMENTED
	// Schedules
	,0x1F80, 0xFF, 'R', 8, 0x00,		0x00,			FALSE, SendSchedule, RcvSchedule, UnknownSchedule
#endif
};

/////////////////////////////////////////////////////////////////////////////////////////////
// Enviracom API data declarations
/////////////////////////////////////////////////////////////////////////////////////////////


UNSIGNED_BYTE RxBuffer[N_ENVIRACOM_BUSSES][MESSAGE_SIZE];  // character buffer for bytes
	// received on the serial ports

//UNSIGNED_BYTE QueryZone[N_ENVIRACOM_BUSSES];  // identifies the zone in the next query
	// message
//SIGNED_BYTE QueryMsg[N_ENVIRACOM_BUSSES];  // index into MsgTable that identifies
	// the next query message, Negative number means that all queries have been sent

UNSIGNED_BYTE bus;
SIGNED_BYTE msg;
SIGNED_BYTE inst;
SIGNED_BYTE TxInst;  // the instance of the last transmit message
UNSIGNED_BYTE PrdDay;


SIGNED_BYTE SecondsCnt;	// seconds counter
SIGNED_BYTE NinetySecCnt;	// 90 seconds counter (1.5 minutes)
SIGNED_BYTE SixtyMinBool;    // boolean tracking the 60 minute check

UNSIGNED_BYTE AckIdleTimer[N_ENVIRACOM_BUSSES];  // timer while waiting for both the ack and
	// idle after sending a message to be placed on Enviracom
UNSIGNED_BYTE PendingTxMsg[N_ENVIRACOM_BUSSES];  // index of the sent message that is pending 
UNSIGNED_BYTE PendingTxInst[N_ENVIRACOM_BUSSES];  // instance of the sent message that is pending

#ifdef SCHEDULESIMPLEMENTED
UNSIGNED_BYTE PendingPrdDay[N_ENVIRACOM_BUSSES];	// period and day of the sent message that is
	// pending
SIGNED_BYTE SchedQryZone[N_ENVIRACOM_BUSSES];	// the zone for a schedule query
UNSIGNED_BYTE SchedQryPrdDay[N_ENVIRACOM_BUSSES];  // the period and day for a schedule query
UNSIGNED_BYTE ScheduleWorkaround[N_ENVIRACOM_BUSSES][N_ZONES];  // There is a bug in the T8635
	// when the schedule is copied from 
	// one day to the next in that only some but not all of the schedule messages get sent.  As a 
	// workaround this flag gets set to 1 when a schedule message is received that wasn't queried.
	// This flag is incremented each second if not 0 until it reaches 35 which causes it to send a 
	// wildcard schedule query.  This causes the T8635 to send all
	// of the schedule so that we don't miss anything.
	// This flag is 0 when no wildcard query is pending.  This flag is 35 when a wildcard query is
	// queued but has not yet been sent.  This flag is 36 when the wildcard query is being sent but
	// the bus has not yet gone idle.   This flag is set back to 0 when the bus goes idle.
#define SCHEDULE_WORKAROUND_TIMER 35
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////
// Enviracom API functions
//////////////////////////////////////////////////////////////////////////////////////////////////

// Air Filter Timer
///////////////////

// When the application calls this
// function, a message is queued to be sent on the designated Enviracom bus
// to restart the air filter timer to the value in Envrcm_AirFilterRestart.
void RestartAirFilterTimer(UNSIGNED_BYTE EnviracomBus)
{
	// reset the timer value
	Envrcm_AirFilterRemain[EnviracomBus] = 255;
	// queue the air filter timer change message to restart the timer
	QueueFlags[EnviracomBus] |= MsgTable[MSG_AIRFILTER].QueueFlagMask;
}



// Fan Switch
/////////////////

// When the application
// calls this function, a message is queued to be sent on the specified Enviracom
// bus for the specified zone to change its fan switch position.  The value sent is the value
// in Envrcm_FanSwitch.
void ChangeFanSwitch(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone)
{
	// queue the fan switch settings change message
	ZoneFlags[EnviracomBus][Zone] |= MsgTable[MSG_FANSWITCH].QueueFlagMask;
}


// Outdoor Temperature
////////////////////////

// This function queues a report of the outdoor 
// temperature on each Enviracom bus.  The value sent is the value in
// Envrcm_OutdoorTemperature.
void SetOutdoorTemperature(void)
{
	UNSIGNED_BYTE bus;

	// for each Enviracom bus
	for (bus = 0; bus < N_ENVIRACOM_BUSSES; bus++)
	{
		// queue a report of the outdoor temperature
		QueueFlags[bus] |= MsgTable[MSG_OUTDOORTEMP].QueueFlagMask;
	}
}


// Setpoints
///////////////////

// This function sends the necessary
// Enviracom messages to change the heat setpoint to the value in 
// Envrcm_HeatSetpoint.  Note that the thermostat's schedule is not changed.  This 
// function automatically implements the following rules:
	// If the value in Envrcm_HeatSetpoint is above or below the allowed setpoint
	//		limits, the value is clamped to the limit.
	// If the system switch = AUTO and the value in Envrcm_HeatSetpoint violates the
	//		deadband between heat and cool setpoints, the value in Envrcm_CoolSetpoint
	//		is adjusted to comply with the deadband. 
	// If the current setpoints come from the schedule, temporary setpoints are created.
	//		The heat setpoint is set to the value in Envrcm_HeatSetpoint.  The cool
	//		setpoint remains the same (unless it needs adjusting because of the 
	//		deadband) but is treated by the thermostat as a temporary setpoint.
	// If the current setpoints are already temporary, the setpoints remain temporary,
	//		but the heat setpoint is changed.  The cool setpoint will also change if
	//		it needs adjusting because of the deadband.
	// If the current setpoints are hold setpoints, the setpoints remain hold setpoints
	//		but the heat setpoint is changed.  The cool setpoint will also change if
	//		it needs adjusting because of the deadband.
// If either Envrcm_HeatSetpoint or Envrcm_CoolSetpoint is changed because of the
// limits or deadband, this function returns a positive number.  Otherwise it returns
// zero.
UNSIGNED_BYTE ChangeHeatSetpoint(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone)
{
	SIGNED_WORD TempWord;
	UNSIGNED_BYTE ChangeFlag;

	ChangeFlag = 0;		// default value
	// if the cool setpoint is known
	if (  (Envrcm_CoolSetpoint[EnviracomBus][Zone] != 0x7FFF)
		// AND if the heat setpoint is known
		&& (Envrcm_HeatSetpoint[EnviracomBus][Zone] != 0x7FFF)
		// AND if the setpoint limits are known
		&& (Envrcm_HeatSetpointMaxLimit[EnviracomBus] != 0x7FFF)
		// AND if the deadband is known)
		&& (Envrcm_Deadband[EnviracomBus] != 0x7FFF)  )
	{
		// if the heat setpoint is below the minimum limit, clamp it to the limit
		if (Envrcm_HeatSetpoint[EnviracomBus][Zone] < 
			Envrcm_HeatSetpointMinLimit[EnviracomBus])
		{
			Envrcm_HeatSetpoint[EnviracomBus][Zone] = 
				Envrcm_HeatSetpointMinLimit[EnviracomBus];
			ChangeFlag = 1;	// indicate a change
		}
		// if the heat setpoint is above the maximum limit, clamp it to the limit
		if (Envrcm_HeatSetpoint[EnviracomBus][Zone] > 
			Envrcm_HeatSetpointMaxLimit[EnviracomBus])
		{
			Envrcm_HeatSetpoint[EnviracomBus][Zone] = 
				Envrcm_HeatSetpointMaxLimit[EnviracomBus];
			ChangeFlag = 1;	// indicate a change		
		}
		// if the autochangeover system switch position is allowed or is not known
		if ((Envrcm_AllowedSystemSwitch[EnviracomBus][Zone] & 0x10) || 
			(Envrcm_SystemSwitch[EnviracomBus][Zone] == 0xFF))
		{
			// if the heat setpoint is above the cooling maximum limit minus deadband
			TempWord = Envrcm_CoolSetpointMaxLimit[EnviracomBus] - 
				Envrcm_Deadband[EnviracomBus];
			if (Envrcm_HeatSetpoint[EnviracomBus][Zone] > TempWord)
			{
				// clamp the heat setpoint to the cool maximum limit minus deadband
				Envrcm_HeatSetpoint[EnviracomBus][Zone] = TempWord;
				// clamp the cool setpoint to the cool maximum limit
				Envrcm_CoolSetpoint[EnviracomBus][Zone] = 
					Envrcm_CoolSetpointMaxLimit[EnviracomBus];
				ChangeFlag = 1;  // indicate a change
			}
			// if the cooling setpoint minus the heating setpoint is less than the deadband
			TempWord = Envrcm_HeatSetpoint[EnviracomBus][Zone] + 
				Envrcm_Deadband[EnviracomBus];
			if (Envrcm_CoolSetpoint[EnviracomBus][Zone] < TempWord)
			{
				// adjust the cooling setpoint to match the deadband
				Envrcm_CoolSetpoint[EnviracomBus][Zone] = TempWord;
				ChangeFlag = 1;	// indicate a change
			}
		}
	}
	// if the setpoint status is scheduled, make it temporary
	if (Envrcm_SetpointStatus[EnviracomBus][Zone] == 0)
	{
		Envrcm_SetpointStatus[EnviracomBus][Zone] = 1;
	}

	// queue the change heat and cool setpoint message
	ZoneFlags[EnviracomBus][Zone] |= MsgTable[MSG_SETPOINTS].QueueFlagMask;
	// return, indicating whether any setpoint changed because of limits or deadband
	return (ChangeFlag);
}

// This function sends the necessary
// Enviracom messages to change the cool setpoint to the value in 
// Envrcm_CoolSetpoint.  Note that the thermostat's schedule is not changed.  This 
// function automatically implements the following rules:
	// If the value in Envrcm_CoolSetpoint is above or below the allowed setpoint
	//		limits, the value is clamped to the limit.
	// If the system switch = AUTO and the value in Envrcm_CoolSetpoint violates the
	//		deadband between heat and cool setpoints, the value in Envrcm_HeatSetpoint
	//		is adjusted to comply with the deadband. 
	// If the current setpoints come from the schedule, temporary setpoints are created.
	//		The cool setpoint is set to the value in Envrcm_CoolSetpoint.  The heat
	//		setpoint remains the same (unless it needs adjusting because of the 
	//		deadband) but is treated by the thermostat as a temporary setpoint.
	// If the current setpoints are already temporary, the setpoints remain temporary,
	//		but the cool setpoint is changed.  The heat setpoint will also change if
	//		it needs adjusting because of the deadband.
	// If the current setpoints are hold setpoints, the setpoints remain hold setpoints
	//		but the cool setpoint is changed.  The heat setpoint will also change if
	//		it needs adjusting because of the deadband.
// If either Envrcm_HeatSetpoint or Envrcm_CoolSetpoint is changed because of the
// limits or deadband, this function returns a positive number.  Otherwise it returns
// zero.
UNSIGNED_BYTE ChangeCoolSetpoint(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone)
{
	SIGNED_WORD TempWord;
	UNSIGNED_BYTE ChangeFlag;

	ChangeFlag = 0;		// default value
	// if the cool setpoint is known
	if (  (Envrcm_CoolSetpoint[EnviracomBus][Zone] != 0x7FFF)
		// AND if the heat setpoint is known
		&& (Envrcm_HeatSetpoint[EnviracomBus][Zone] != 0x7FFF)
		// AND if the setpoint limits are known
		&& (Envrcm_CoolSetpointMaxLimit[EnviracomBus] != 0x7FFF)
		// AND if the deadband is known)
		&& (Envrcm_Deadband[EnviracomBus] != 0x7FFF)  )
	{
		// if the cool setpoint is above the maximum limit, clamp it to the limit
		if (Envrcm_CoolSetpoint[EnviracomBus][Zone] > 
			Envrcm_CoolSetpointMaxLimit[EnviracomBus])
		{
			Envrcm_CoolSetpoint[EnviracomBus][Zone] = 
				Envrcm_CoolSetpointMaxLimit[EnviracomBus];
			ChangeFlag = 1;	// indicate a change
		}
		// if the cool setpoint is below the minimum limit, clamp it to the limit
		if (Envrcm_CoolSetpoint[EnviracomBus][Zone] < 
			Envrcm_CoolSetpointMinLimit[EnviracomBus])
		{
			Envrcm_CoolSetpoint[EnviracomBus][Zone] = 
				Envrcm_CoolSetpointMinLimit[EnviracomBus];
			ChangeFlag = 1;	// indicate a change		
		}
		// if the autochangeover system switch position is allowed or is not known
		if ((Envrcm_AllowedSystemSwitch[EnviracomBus][Zone] & 0x10) || 
			(Envrcm_SystemSwitch[EnviracomBus][Zone] == 0xFF))
		{
			// if the cool setpoint is below the heating minimum limit plus deadband
			TempWord = Envrcm_HeatSetpointMinLimit[EnviracomBus] + 
				Envrcm_Deadband[EnviracomBus];
			if (Envrcm_CoolSetpoint[EnviracomBus][Zone] < TempWord)
			{
				// clamp the cool setpoint to the heat minimum limit plus deadband
				Envrcm_CoolSetpoint[EnviracomBus][Zone] = TempWord;
				// clamp the heat setpoint to the heat minimum limit
				Envrcm_HeatSetpoint[EnviracomBus][Zone] = 
					Envrcm_HeatSetpointMinLimit[EnviracomBus];
				ChangeFlag = 1;  // indicate a change
			}
			// if the cooling setpoint minus the heating setpoint is less than the deadband
			TempWord = Envrcm_CoolSetpoint[EnviracomBus][Zone] - 
				Envrcm_Deadband[EnviracomBus];
			if (Envrcm_HeatSetpoint[EnviracomBus][Zone] > TempWord)
			{
				// adjust the heating setpoint to match the deadband
				Envrcm_HeatSetpoint[EnviracomBus][Zone] = TempWord;
				ChangeFlag = 1;	// indicate a change
			}
		}
	}
	// if the setpoint status is scheduled, make it temporary
	if (Envrcm_SetpointStatus[EnviracomBus][Zone] == 0)
	{
		Envrcm_SetpointStatus[EnviracomBus][Zone] = 1;
	}

	// queue the change heat and cool setpoint message
	ZoneFlags[EnviracomBus][Zone] |= MsgTable[MSG_SETPOINTS].QueueFlagMask;
	// return, indicating whether any setpoint changed because of limits or deadband
	return (ChangeFlag);
}

// Note:  When the application changes both the heat and the cool setpoint, because of the
// deadband, the order that ChangeHeatSetpoint() and ChangeCoolSetpoint() are called may
// be important.  If the heat setpoint is more important to the application than the cool
// setpoint, ChangeHeatSetpoint() should be called last.  Similarly, if the cool setpoint is
// more important than the heat setpoint, ChangeCoolSetpoint() should be called last.

// This function creates hold setpoints from the
// current setpoints, regardless of whether the current setpoints are scheduled,
// temporary, or hold setpoints.  If specific values of heat and/or cool hold setpoints
// are desired, the application should first change the values in Envrcm_HeatSetpoint
// and/or Envrcm_CoolSetpoint, then call ChangeHeatSetpoint() and/or 
// ChangeCoolSetpoint() before calling this function.
void HoldSetpoints(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone)
{
	// set up to hold the setpoints
	Envrcm_SetpointStatus[EnviracomBus][Zone] = 2;
	// queue the change heat and cool setpoint message
	ZoneFlags[EnviracomBus][Zone] |= MsgTable[MSG_SETPOINTS].QueueFlagMask;
}
  
// Calling this function has the same effect as 
// pressing the 'Run Program' key on the thermostat.  Any temporary or hold setpoints
// are erased and the thermostat gets its setpoints from the schedule.  When this
// function is called, until the thermostat reports its new setpoints, the
// values in Envrcm_CoolSetpoint and Envrcm_HeatSetpoint will be 7FFFh.
void RunProgram(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone)
{
	// set up to run the schedule
	Envrcm_SetpointStatus[EnviracomBus][Zone] = 0;
	// change the heat and cool setpoints to 'unknown'
	Envrcm_HeatSetpoint[EnviracomBus][Zone] = 0x7FFF;
	Envrcm_CoolSetpoint[EnviracomBus][Zone] = 0x7FFF;
	// queue the change heat and cool setpoint message
	ZoneFlags[EnviracomBus][Zone] |= MsgTable[MSG_SETPOINTS].QueueFlagMask;
}

// Note:  When the application makes back to back calls of ChangeHeatSetpoint(), 
// ChangeCoolSetpoint(), and/or HoldSetpoints() for the same zone, only one
// Enviracom message with all of the changes is sent (the Override Heat-Cool Setpoint message).
// This gives a simultaneous change of the heat setpoint, the cool setpoint, and the setpoint 
// status.



// System Switch
/////////////////

// This function causes an Enviracom message to
// be sent to change the system switch position for the zone to the value
// in Envrcm_SystemSwitch.
void ChangeSystemSwitch(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone)
{
	// queue the change system switch message
	ZoneFlags[EnviracomBus][Zone] |= MsgTable[MSG_SYSSWITCH].QueueFlagMask;
}



// Schedule
//////////////////////

#ifdef SCHEDULESIMPLEMENTED
// This function causes an
// Enviracom message to be sent to change the schedule for the Period/Day/Zone/Bus to the 
// value in Envrcm_ScheduleStartTime, Envrcm_ScheduleHeatSetpoint, and 
// Envrcm_ScheduleCoolSetpoint.
UNSIGNED_BYTE ChangeSchedule(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone, UNSIGNED_BYTE PeriodDay)
{
	UNSIGNED_BYTE Day;
	UNSIGNED_BYTE DayMask;
	UNSIGNED_BYTE Period;
	UNSIGNED_BYTE ChangeFlag;
	SIGNED_WORD TempWord;

	Day = PeriodDay & 0x0F;
	Period = PeriodDay / 0x10;

	// assume setpoints don't need adjusting
	ChangeFlag = 0;
	// if the setpoint limits are known
	if (Envrcm_HeatSetpointMaxLimit[EnviracomBus] != 0x7FFF)
	{
		// if the heat setpoint is below the minimum limit, clamp it to the limit
		if (Envrcm_ScheduleHeatSetpoint[EnviracomBus][Zone][Day][Period] < 
			Envrcm_HeatSetpointMinLimit[EnviracomBus])
		{
			Envrcm_ScheduleHeatSetpoint[EnviracomBus][Zone][Day][Period] = 
				Envrcm_HeatSetpointMinLimit[EnviracomBus];
			ChangeFlag = 1;
		}
		// if the heat setpoint is above the maximum limit, clamp it to the limit
		if (Envrcm_ScheduleHeatSetpoint[EnviracomBus][Zone][Day][Period] > 
			Envrcm_HeatSetpointMaxLimit[EnviracomBus])
		{
			Envrcm_ScheduleHeatSetpoint[EnviracomBus][Zone][Day][Period] = 
				Envrcm_HeatSetpointMaxLimit[EnviracomBus];
			ChangeFlag = 1;
		}
		// if the cool setpoint is below the minimum limit, clamp it to the limit
		if (Envrcm_ScheduleCoolSetpoint[EnviracomBus][Zone][Day][Period] < 
			Envrcm_CoolSetpointMinLimit[EnviracomBus])
		{
			Envrcm_ScheduleCoolSetpoint[EnviracomBus][Zone][Day][Period] = 
				Envrcm_CoolSetpointMinLimit[EnviracomBus];
			ChangeFlag = 1;
		}
		// if the cool setpoint is above the maximum limit, clamp it to the limit
		if (Envrcm_ScheduleCoolSetpoint[EnviracomBus][Zone][Day][Period] > 
			Envrcm_CoolSetpointMaxLimit[EnviracomBus])
		{
			Envrcm_ScheduleCoolSetpoint[EnviracomBus][Zone][Day][Period] = 
				Envrcm_CoolSetpointMaxLimit[EnviracomBus];
			ChangeFlag = 1;
		}
		// if the deadband is known)
		if (Envrcm_Deadband[EnviracomBus] != 0x7FFF)
		{
			// if the autochangeover system switch position is allowed or is not known
			if ((Envrcm_AllowedSystemSwitch[EnviracomBus][Zone] & 0x10) || 
				(Envrcm_SystemSwitch[EnviracomBus][Zone] == 0xFF))
			{
				// if the cool setpoint is below the heating minimum limit plus deadband
				TempWord = Envrcm_HeatSetpointMinLimit[EnviracomBus] + 
					Envrcm_Deadband[EnviracomBus];
				if (Envrcm_ScheduleCoolSetpoint[EnviracomBus][Zone][Day][Period] < TempWord)
				{
					// clamp the cool setpoint to the heat minimum limit plus deadband
					Envrcm_ScheduleCoolSetpoint[EnviracomBus][Zone][Day][Period] = TempWord;
					// clamp the heat setpoint to the heat minimum limit
					Envrcm_ScheduleHeatSetpoint[EnviracomBus][Zone][Day][Period] = 
						Envrcm_HeatSetpointMinLimit[EnviracomBus];
					ChangeFlag = 1;  // indicate a change
				}
				// if the cooling setpoint minus the heating setpoint is less than the deadband
				TempWord = Envrcm_ScheduleCoolSetpoint[EnviracomBus][Zone][Day][Period] - 
					Envrcm_Deadband[EnviracomBus];
				if (Envrcm_ScheduleHeatSetpoint[EnviracomBus][Zone][Day][Period] > TempWord)
				{
					// adjust the heating setpoint to match the deadband
					Envrcm_ScheduleHeatSetpoint[EnviracomBus][Zone][Day][Period] = TempWord;
					ChangeFlag = 1;	// indicate a change
				}
			}
		}
	}


	// determine the mask based on the day
	DayMask = 0x01;
	for (Day = PeriodDay & 0x0F; Day > 0; Day--)
	{
		DayMask *= 2;
	}
	// queue the schedule change request
	ScheduleFlags[EnviracomBus][Zone][PeriodDay / 16] |= DayMask;
	return ChangeFlag;
}
#endif



// Time
////////////////

// When the application calls this function, a message is queued to send
// on all Enviracom busses to change the clock time to the value in Envrcm_Seconds, 
// Envircm_Minutes, and Envrcm_HourDay.  The application should call this function only
// when App_TimeNotify is called, and whenever the time jumps, for example when the daylight
// savings time changeover occurs.
void ChangeTime(void)
{
	UNSIGNED_BYTE bus;

	// for each Enviracom bus
	for (bus = 0; bus < N_ENVIRACOM_BUSSES; bus++)
	{
		// queue a change of time
		QueueFlags[bus] |= MsgTable[MSG_TIME].QueueFlagMask;
	}
}



/////////////////////////////////////////////////////////////////////////////////////////////
// Functions
/////////////////////////////////////////////////////////////////////////////////////////////

// Heat Pump
void RcvHeatPump(void)
{
	SIGNED_BYTE val;
#ifdef EXTENSIONS
	SIGNED_WORD fan;
	SIGNED_WORD gas;
#endif


#ifdef EXTENSIONS
	if (inst == 0x41) {
#endif
	    // indicate that a heat pump is present
	    Envrcm_HeatPumpPresent[bus] = 1;
	    // save the fault information
	    if ( (RxBuffer[bus][16] == 'F') && (RxBuffer[bus][17] == 'F') ) {
		val = 1;	// heat pump fault
	    } else {
		val = -1;	// no heat pump fault
	    }	
	    // if the fault information changed
	    if (Envrcm_HeatPumpFault[bus] != val) {
		// save the new value
		Envrcm_HeatPumpFault[bus] = val;
		// notify the application that it changed
		App_HeatPumpFaultNotify(bus);
	    }
#ifdef EXTENSIONS
	}

	// Added by srp
	if (inst == 0x10) {
	    fan = ((CharToHex(RxBuffer[bus][16]) * 0x10) + CharToHex(RxBuffer[bus][17]));
	    if (Envrcm_HeatPumpFan[bus] != fan) {
		Envrcm_HeatPumpFan[bus] = fan;
		App_HeatPumpFanNotify(bus);
	    }
	}
	if (inst == 0x11) {
	    gas = ((CharToHex(RxBuffer[bus][16]) * 0x10) + CharToHex(RxBuffer[bus][17]));
	    if (Envrcm_HeatPumpStage[bus] != gas) {
		Envrcm_HeatPumpStage[bus] = gas;
		App_HeatPumpStageNotify(bus);
	    }
	}
#endif

}

void UnknownHeatPump(void)
{
	// if the heat pump values are not already unknown
	if ( (Envrcm_HeatPumpFault[bus] >= 0) ||
		(Envrcm_HeatPumpPresent[bus] >= 0) )
	{
		// set the values to unknown
		Envrcm_HeatPumpFault[bus] = -1;
		Envrcm_HeatPumpPresent[bus] = -1;
		// notify the application that the values changed
		App_HeatPumpFaultNotify(bus);
	}
}




// Time
/////////
void SendTime(void)
{
	// the data length is 8
	TxBuffer[bus][14] = '8';
	TxBuffer[bus][0] = 42;
	// first data byte = Priority.  Only 'day of week' is used
	TxBuffer[bus][16] = '0';
	TxBuffer[bus][17] = '8';
	// second data byte = Second
	TxBuffer[bus][19] = HexToChar(Envrcm_Seconds / 16);
	TxBuffer[bus][20] = HexToChar(Envrcm_Seconds & 0x0F);
	// third data byte = Minute
	TxBuffer[bus][22] = HexToChar(Envrcm_Minutes / 16);
	TxBuffer[bus][23] = HexToChar(Envrcm_Minutes & 0x0F);
	// fourth data byte = HourDay
	TxBuffer[bus][25] = HexToChar(Envrcm_HourDay / 16);
	TxBuffer[bus][26] = HexToChar(Envrcm_HourDay & 0x0F);
	// fifth, sixth, seventh, and eighth data byte = 0
	TxBuffer[bus][28] = '0';
	TxBuffer[bus][29] = '0';
	TxBuffer[bus][31] = '0';
	TxBuffer[bus][32] = '0';
	TxBuffer[bus][34] = '0';
	TxBuffer[bus][35] = '0';
	TxBuffer[bus][37] = '0';
	TxBuffer[bus][38] = '0';
}

void RcvTime(void)
{
	// notify the application to send the time
	App_TimeNotify();
}




// Equipment fault
/////////////////////
void RcvEquipFault(void)
{
	SIGNED_BYTE HtFault;
	SIGNED_BYTE ClFault;

	// get the heating fault information
	if (RxBuffer[bus][29] & 0x01)
	{
		HtFault = 1;
	}
	else
	{
		HtFault = -1;
	}
	// get the cooling fault information
	if (RxBuffer[bus][29] & 0x02)
	{
		ClFault = 1;
	}
	else
	{
		ClFault = -1;
	}
	// if any information changed
	if ((HtFault != Envrcm_HeatFault[bus]) || (ClFault != Envrcm_CoolFault[bus]))
	{
		// save the new information
		Envrcm_HeatFault[bus] = HtFault;
		Envrcm_CoolFault[bus] = ClFault;
		// notify the application that the data changed
		App_EquipFaultNotify(bus);
	}
}

void UnknownEquipFault(void)
{
	// if the equipment fault values are not already unknown
	if ( (Envrcm_HeatFault[bus] != 0) ||
		(Envrcm_CoolFault[bus] != 0) )
	{
		// set the values to unknown
		Envrcm_HeatFault[bus] = 0;
		Envrcm_CoolFault[bus] = 0;
		// notify the application that the values changed
		App_EquipFaultNotify(bus);
	}
}





// Setpoints
///////////////
void SendSetpoints(void)
{
	// put in the HeatOvrd field
	TxBuffer[bus][16] = HexToChar(Envrcm_HeatSetpoint[bus][inst] / 0x1000);
	TxBuffer[bus][17] = HexToChar((Envrcm_HeatSetpoint[bus][inst] & 0x0F00) / 0x0100);
	TxBuffer[bus][19] = HexToChar((Envrcm_HeatSetpoint[bus][inst] & 0x00F0) / 0x0010);
	TxBuffer[bus][20] = HexToChar(Envrcm_HeatSetpoint[bus][inst] & 0x000F);
	// put in the Status field
	TxBuffer[bus][22] = HexToChar(Envrcm_SetpointStatus[bus][inst] / 0x10);
	TxBuffer[bus][23] = HexToChar(Envrcm_SetpointStatus[bus][inst] & 0x000F);
	// put in the CoolOvrd field
	TxBuffer[bus][25] = HexToChar(Envrcm_CoolSetpoint[bus][inst] / 0x1000);
	TxBuffer[bus][26] = HexToChar((Envrcm_CoolSetpoint[bus][inst] & 0x0F00) / 0x0100);
	TxBuffer[bus][28] = HexToChar((Envrcm_CoolSetpoint[bus][inst] & 0x00F0) / 0x0010);
	TxBuffer[bus][29] = HexToChar(Envrcm_CoolSetpoint[bus][inst] & 0x000F);
	// the remaining bytes are zero
	TxBuffer[bus][31] = '0';
	TxBuffer[bus][32] = '0';
	TxBuffer[bus][34] = '0';
	TxBuffer[bus][35] = '0';
	TxBuffer[bus][37] = '0';
	TxBuffer[bus][38] = '0';
}

void RcvSetpoints(void)
{
	SIGNED_WORD Heat;
	SIGNED_WORD Cool;
	UNSIGNED_BYTE Status;
#ifdef  EXTENSIONS
	UNSIGNED_BYTE Recovery;
#endif

	// get the heat setpoint
	Heat = (((((CharToHex(RxBuffer[bus][16]) * 0x10) + CharToHex(RxBuffer[bus][17])) * 0x10) + 
		CharToHex(RxBuffer[bus][19])) * 0x10) + CharToHex(RxBuffer[bus][20]);
	// get the cool setpoint
	Cool = (((((CharToHex(RxBuffer[bus][25]) * 0x10) + CharToHex(RxBuffer[bus][26])) * 0x10) + 
		CharToHex(RxBuffer[bus][28])) * 0x10) + CharToHex(RxBuffer[bus][29]);
	// get the status
	Status = (CharToHex(RxBuffer[bus][22]) * 0x10) + CharToHex(RxBuffer[bus][23]);

#ifdef  EXTENSIONS
	Recovery = CharToHex(RxBuffer[bus][31]);

	if (Status == 0 && Recovery != 0) {
	    Status = Recovery + 2; /* max value of Status */
	}
#endif

	// 
	// if any of the data changed
	if ( (Envrcm_HeatSetpoint[bus][inst] != Heat) || 
		(Envrcm_CoolSetpoint[bus][inst] != Cool) ||
		(Envrcm_SetpointStatus[bus][inst] != Status) )
	{
		// save the new values
		Envrcm_HeatSetpoint[bus][inst] = Heat;
		Envrcm_CoolSetpoint[bus][inst] = Cool;
		Envrcm_SetpointStatus[bus][inst] = Status;
		// notify the application that the values changed
		App_SetpointsNotify(bus, inst);
	}
	// A bug in the T8635 thermostat prevents it from becoming the master at power up if it doesn't know the
	// time, even if it receives a time change request.  A workaround is to send a setpoint change message
	// followed by the time change message.
	// if we havn't sent a setpoint change message
	if (!(ZoneFlags[bus][inst] & TX_SETPOINT))
	{
		// if we haven't sent a time change message
		if (!(QueueFlags[bus] & TX_TIME)) 
		{
			// if the setpoints are scheduled
			if (Envrcm_SetpointStatus[bus][inst] == 0)
			{
				// send a setpoint change message
				RunProgram(bus, inst);
				// indicate that we've sent a setpoint change message
				ZoneFlags[bus][inst] |= TX_SETPOINT;
			}
		}
	}
	else  // we have sent a setpoint change message
	{
		// if we haven't sent a time change message
		if (!(QueueFlags[bus] & TX_TIME)) 
		{
			// send a time change message
			App_TimeNotify();
			// indicate that we've sent a time change message
			QueueFlags[bus] |= TX_TIME;
		}
	}
}

void UnknownSetpoints(void)
{
	// if the setpoint values are not already unknown
	if ( (Envrcm_HeatSetpoint[bus][inst] != 0x7FFF) ||
		(Envrcm_CoolSetpoint[bus][inst] != 0x7FFF) ||
		(Envrcm_SetpointStatus[bus][inst] != 0) )
	{
		// set the values to unknown
		Envrcm_HeatSetpoint[bus][inst] = 0x7FFF;
		Envrcm_CoolSetpoint[bus][inst] = 0x7FFF;
		Envrcm_SetpointStatus[bus][inst] = 0;
		// notify the application that the values changed
		App_SetpointsNotify(bus, inst);
	}
}




// System Switch
////////////////////
void SendSystemSwitch(void)
{
	// when simple zoning is used, the system switch is global and must be sent as zone 0
	if (IsSimpleZoning(bus))
	{
		// change the instance to 0
		TxBuffer[bus][9] = '0';
		PendingTxInst[bus] = 0;
	}
	// fill in the SysSwitch field
	TxBuffer[bus][16] = HexToChar(Envrcm_SystemSwitch[bus][inst] / 0x10);
	TxBuffer[bus][17] = HexToChar(Envrcm_SystemSwitch[bus][inst] & 0x0F);
	// the remaining bytes are zero
	TxBuffer[bus][19] = '0';
	TxBuffer[bus][20] = '0';
	TxBuffer[bus][22] = '0';
	TxBuffer[bus][23] = '0';
}

void RcvSystemSwitch(void)
{
	UNSIGNED_BYTE Pos;
	UNSIGNED_WORD Allowed;
	UNSIGNED_BYTE Zone;

    // get the system switch position and mode
    Pos = (CharToHex(RxBuffer[bus][16]) * 0x10) + CharToHex(RxBuffer[bus][17]);

	// get the allowed positions
	Allowed = (((((CharToHex(RxBuffer[bus][19]) * 0x10) + CharToHex(RxBuffer[bus][20])) * 0x10) + 
		CharToHex(RxBuffer[bus][22])) * 0x10) + CharToHex(RxBuffer[bus][23]);
	// when simple zoning is used, the system switch is global
	if ((IsSimpleZoning(bus)) && (inst == 0))
	{
		// for each possible zone
		for (Zone = 0; Zone < N_ZONES; Zone++)
		{
			// if the zone is present
			if (Envrcm_RoomTemperature[bus][Zone] != 0x7FFF)
			{
				// if the data changed
				if ( (Envrcm_SystemSwitch[bus][Zone] != Pos) ||
					(Envrcm_AllowedSystemSwitch[bus][Zone] != Allowed) )
				{
					// save the new data
					Envrcm_SystemSwitch[bus][Zone] = Pos;
					Envrcm_AllowedSystemSwitch[bus][Zone] = Allowed;
					// notify the application that the data changed
					App_SysSwitchNotify(bus, Zone);
				}
			}
		}
	}
	// else, either non zoned or zone manager.  If the data changed
	else if ( (Envrcm_SystemSwitch[bus][inst] != Pos) ||
		(Envrcm_AllowedSystemSwitch[bus][inst] != Allowed) )
	{
		// save the new data
		Envrcm_SystemSwitch[bus][inst] = Pos;
		Envrcm_AllowedSystemSwitch[bus][inst] = Allowed;
		// notify the application that the data changed
		App_SysSwitchNotify(bus, inst);
	}
}

void UnknownSystemSwitch(void)
{
	UNSIGNED_BYTE Zone;

	// when simple zoning is used, the system switch is global
	if (IsSimpleZoning(bus))
	{
		if (inst == 0)
		{
			for (Zone = 0; Zone < N_ZONES; Zone++)
			{
				// if the system switch values are not already unknown
				if ( (Envrcm_SystemSwitch[bus][Zone] != 0xFF) ||
					(Envrcm_AllowedSystemSwitch[bus][Zone] != 0) )
				{
					// set the values to unknown
					Envrcm_SystemSwitch[bus][Zone] = 0xFF;
					Envrcm_AllowedSystemSwitch[bus][Zone] = 0;
					// notify the application that the values changed
					App_SysSwitchNotify(bus, Zone);
				}
			}
		}
	}
	// else not simple zoning.  If the system switch values are not already unknown
	else if ( (Envrcm_SystemSwitch[bus][inst] != 0xFF) ||
		(Envrcm_AllowedSystemSwitch[bus][inst] != 0) )
	{
		// set the values to unknown
		Envrcm_SystemSwitch[bus][inst] = 0xFF;
		Envrcm_AllowedSystemSwitch[bus][inst] = 0;
		// notify the application that the values changed
		App_SysSwitchNotify(bus, inst);
	}
}




// Fan Switch
////////////////////
void SendFanSwitch(void)
{
	// fill in the FanSwitch field
	if (Envrcm_FanSwitch[bus][inst] > 0)
	{
		TxBuffer[bus][16] = 'C';
		TxBuffer[bus][17] = '8';
	}
	else
	{
		TxBuffer[bus][16] = '0';
		TxBuffer[bus][17] = '0';
	}
	// the Flags field = 0
	TxBuffer[bus][19] = '0';
	TxBuffer[bus][20] = '0';
}

void RcvFanSwitch(void)
{
	UNSIGNED_BYTE Pos;

	// get the fan switch position
	Pos = (CharToHex(RxBuffer[bus][16]) * 0x10) + CharToHex(RxBuffer[bus][17]);
	// if it changed
	if (Envrcm_FanSwitch[bus][inst] != Pos)
	{
		// save the new value
		Envrcm_FanSwitch[bus][inst] = Pos;
		// notify the application that it changed
		App_FanSwitchNotify(bus, inst);
	}
}

void UnknownFanSwitch(void)
{
	// if the Fan Switch value is not already unknown
    if (Envrcm_FanSwitch[bus][inst] != 0xFF)
	{
		// set the value to unknown
		Envrcm_FanSwitch[bus][inst] = 0xFF;
		// notify the application that the values changed
		App_FanSwitchNotify(bus, inst);
	}
}





// Room Temperature
/////////////////////
void SendRoomTemp(void)
{
	// if the message is a query
	if (TxBuffer[bus][11] == 'Q')	// if the service is 'query'
	{
		// if the zone is 0/1
		if (inst == 0)
		{
			// if we haven't sent a zone 1 query yet
			if (!(QueryFlags[bus] & QRY_ROOMTEMPZ1))
			{
				// change the zone to zone 1
				TxBuffer[bus][9] = '1';
				// set the flag
				QueryFlags[bus] |= QRY_ROOMTEMPZ1;
				// requeue the query
				QueryZoneFlags[bus][inst] |= MsgTable[MSG_ROOMTEMP].ReceivedFlagMask;
			}
		}
	}
}

void RcvRoomTemp(void)
{
	SIGNED_WORD Temp;
	UNSIGNED_BYTE Units;
	UNSIGNED_BYTE i;

	// get the room temperature
	Temp = (CharToHex(RxBuffer[bus][16]) * 0x10) + CharToHex(RxBuffer[bus][17]);
	// get the units
	Units = CharToHex(RxBuffer[bus][20]) & 0x01;
	// if a sensor fault occurred
	if (CharToHex(RxBuffer[bus][19]) & 0x80)
	{
		// indicate that a fault occurred
		Temp = 0x8000;
	}
	// if the temperature is unknown
	if (Temp == 0x80)
	{
		// indicate that it is unknown
		Temp = 0x7FFF;
	}
	// if Celsius units are used
	else if (Units)
	{
		// convert the room temperature 
		Temp *= 50;
	}
	else  // Fahrenheit units
	{
		Temp = (Temp - 32) * 500 / 9;
	}
	// if the value was unknown
	if (Envrcm_RoomTemperature[bus][inst] == 0x7FFF)
	{
		// queue all queries for the zone
		for (i = 0; i < N_MESSAGES; i++)
		{
			// ignore if the message is room temperature
			if (i != MSG_ROOMTEMP)
			{
				// if the message is zoned
				if (MsgTable[i].Instance == 0xFF)
				{
#ifdef SCHEDULESIMPLEMENTED
					// if the message is schedule
					if (i == MSG_SCHEDULE)
					{
						// queue all queries for the zone
						ScheduleWorkaround[bus][inst] = SCHEDULE_WORKAROUND_TIMER;	// queue a wild card query for the zone
						// the following used in place of the lines above if
						// more bus idles between responses are desired
//						ScheduleQueryFlags[bus][inst][0] = 0xFF;
//						ScheduleQueryFlags[bus][inst][1] = 0xFF;
//						ScheduleQueryFlags[bus][inst][2] = 0xFF;
//						ScheduleQueryFlags[bus][inst][3] = 0xFF;
					}
					else
#endif
					{
						// queue the query
						QueryZoneFlags[bus][inst] |= MsgTable[i].ReceivedFlagMask;
					}
				}
			}
		}
	}
	// if the zone is not 0
	if (RxBuffer[bus][9] != '0')
	{
		// indicate that it is a zoned system
		Envrcm_ZonedSystem[bus] = inst + 1;
	}
	// if anything changed
	if ( (Envrcm_RoomTemperature[bus][inst] != Temp) ||
		(Envrcm_RoomTempUnits[bus][inst] != Units) )
	{
		// save the new values
		Envrcm_RoomTemperature[bus][inst] = Temp;
		Envrcm_RoomTempUnits[bus][inst] = Units;
		// notify the application that it has changed
		App_RoomTempNotify(bus, inst);
	}
}

void UnknownRoomTemp(void)
{
	// if this is the zone that was received last 
	if ((inst + 1) == Envrcm_ZonedSystem[bus])
	{
		// indicate that it is no longer a zoned system
		Envrcm_ZonedSystem[bus] = 0;
	}
	// if the room temperature values are not already unknown
	if (Envrcm_RoomTemperature[bus][inst] != 0x7FFF)
	{
		// set the values to unknown
		Envrcm_RoomTempUnits[bus][inst] = 0;
		Envrcm_RoomTemperature[bus][inst] = 0x7FFF;
		// notify the application that the values changed
		App_RoomTempNotify(bus, inst);
	}
}







// Humidity
////////////////
void RcvHumidity(void)
{
	UNSIGNED_BYTE Val;

	// get the humidity
	Val = (CharToHex(RxBuffer[bus][16]) * 0x10) + CharToHex(RxBuffer[bus][17]);
	// if it changed
	if (Envrcm_Humidity[bus] != Val)
	{
		// save the new value
		Envrcm_Humidity[bus] = Val;
		// notify the application that it has changed
		App_HumidityNotify(bus);
	}
}

void UnknownHumidity(void)
{
	// if the humidity value is not already unknown
	if (Envrcm_Humidity[bus] != 0xF2)
	{
		// set the values to unknown
		Envrcm_Humidity[bus] = 0xF2;
		// notify the application that the value changed
		App_HumidityNotify(bus);
	}
}

void QueryHumidity(void)
{
	// if the value is unknown
	if (Envrcm_Humidity[bus] == 0xF2)
	{
		// send the query
		SendMessage();
	}
}




// Outdoor Temperature
////////////////////////
void SendOutdoorTemp(void)
{
	// if the service is not query
	if (TxBuffer[bus][11] != 'Q')
		// the service is Report
		TxBuffer[bus][11] = 'R';

	// fill in the ODTemp field
	TxBuffer[bus][16] = HexToChar((UNSIGNED_WORD)(Envrcm_OutdoorTemperature) / 0x1000);
	TxBuffer[bus][17] = HexToChar(((UNSIGNED_WORD)(Envrcm_OutdoorTemperature) & 0x0F00) / 
		0x0100);
	TxBuffer[bus][19] = HexToChar(((UNSIGNED_WORD)(Envrcm_OutdoorTemperature) & 0x00F0) / 
		0x0010);
	TxBuffer[bus][20] = HexToChar((UNSIGNED_WORD)(Envrcm_OutdoorTemperature) & 0x000F);
}

void RcvOutdoorTemp(void)
{
	UNSIGNED_BYTE i;
#ifdef EXTENSIONS
	SIGNED_WORD Val;


	// save the new outdoor temperature
	Val = (SIGNED_WORD)((((((CharToHex(RxBuffer[bus][16]) * 0x10) + 
		CharToHex(RxBuffer[bus][17])) * 0x10) + CharToHex(RxBuffer[bus][19])) * 0x10) + 
		CharToHex(RxBuffer[bus][20]));
#else
	Envrcm_OutdoorTemperature = (SIGNED_WORD)((((((CharToHex(RxBuffer[bus][16]) * 0x10) + 
		CharToHex(RxBuffer[bus][17])) * 0x10) + CharToHex(RxBuffer[bus][19])) * 0x10) + 
		CharToHex(RxBuffer[bus][20]));

#endif
	// queue a report on every other Enviracom bus
	for (i = 0; i < N_ENVIRACOM_BUSSES; i++)
	{
		// if this is not the same bus that received the report
		if (i != bus)
		{
			// queue a report
			QueueFlags[i] |= MsgTable[MSG_OUTDOORTEMP].QueueFlagMask;
		}
	}
#ifdef EXTENSIONS
	if (Envrcm_OutdoorTemperature != Val)
	{
		// save the new value
		Envrcm_OutdoorTemperature = Val;
		// notify the application that it has changed
		App_OutdoorTempNotify(bus);
	}
#endif
}

void UnknownOutdoorTemp(void)
{
	// set the value to unknown
	Envrcm_OutdoorTemperature = 0x7FFF;
}






// Air Filter Timer
////////////////////////
void SendAirFilterTimer(void)
{
	// fill in the Remain field
	TxBuffer[bus][16] = HexToChar(Envrcm_AirFilterRemain[bus] / 0x10);
	TxBuffer[bus][17] = HexToChar(Envrcm_AirFilterRemain[bus] & 0x0F);
	// fill in the Duration field
	TxBuffer[bus][19] = HexToChar(Envrcm_AirFilterRestart[bus] / 0x10);
	TxBuffer[bus][20] = HexToChar(Envrcm_AirFilterRestart[bus] & 0x0F);
}

void RcvAirFilterTimer(void)
{
	UNSIGNED_BYTE Remain;
	UNSIGNED_BYTE Restart;

	// get the days remaining
	Remain = (CharToHex(RxBuffer[bus][16]) * 0x10) + CharToHex(RxBuffer[bus][17]);
	// get the restart value
	Restart = (CharToHex(RxBuffer[bus][19]) * 0x10) + CharToHex(RxBuffer[bus][20]);
	// if any value changed
	if ((Envrcm_AirFilterRemain[bus] != Remain) || (Envrcm_AirFilterRestart[bus] != Restart))
	{
		// save the new values
		Envrcm_AirFilterRemain[bus] = Remain;
		Envrcm_AirFilterRestart[bus] = Restart;
		// notify the application that the data changed
		App_AirFilterNotify(bus);
	}
}

void UnknownAirFilterTimer(void)
{
	// if the timer values are not already unknown
	if ( (Envrcm_AirFilterRemain[bus] != 255) ||
		(Envrcm_AirFilterRestart[bus] != 255) )
	{
		// set the values to unknown
		Envrcm_AirFilterRemain[bus] = 255;
		Envrcm_AirFilterRestart[bus] = 255;
		// notify the application that the values changed
		App_AirFilterNotify(bus);
	}
}






// Temperature Limits
////////////////////////
void RcvLimits(void)
{
	// save HeatUpper
	Envrcm_HeatSetpointMaxLimit[bus] = (((((CharToHex(RxBuffer[bus][16]) * 0x10) + 
		CharToHex(RxBuffer[bus][17])) * 0x10) + CharToHex(RxBuffer[bus][19])) * 0x10) + 
		CharToHex(RxBuffer[bus][20]);
	// save HeatLower
	Envrcm_HeatSetpointMinLimit[bus] = (((((CharToHex(RxBuffer[bus][22]) * 0x10) + 
		CharToHex(RxBuffer[bus][23])) * 0x10) + CharToHex(RxBuffer[bus][25])) * 0x10) + 
		CharToHex(RxBuffer[bus][26]);
	// save CoolUpper
	Envrcm_CoolSetpointMaxLimit[bus] = (((((CharToHex(RxBuffer[bus][28]) * 0x10) + 
		CharToHex(RxBuffer[bus][29])) * 0x10) + CharToHex(RxBuffer[bus][31])) * 0x10) + 
		CharToHex(RxBuffer[bus][32]);
	// save CoolLower
	Envrcm_CoolSetpointMinLimit[bus] = (((((CharToHex(RxBuffer[bus][34]) * 0x10) + 
		CharToHex(RxBuffer[bus][35])) * 0x10) + CharToHex(RxBuffer[bus][37])) * 0x10) + 
		CharToHex(RxBuffer[bus][38]);
	// Because the limits are zoned, but should be identical, we will only use
	// zone 0/1 as the zone. Indicate that the zone 0/1 limits have been received
	ZoneFlags[bus][0] |= MsgTable[MSG_LIMITS].ReceivedFlagMask;
}

void UnknownLimits(void)
{
	// Because the limits are zoned, but should be identical, we will only use zone 0/1
	// as the one that indicates the value is unknown
	if (inst == 0)
	{
		// set the values to unknown
		Envrcm_HeatSetpointMinLimit[bus] = 0x7FFF;
		Envrcm_HeatSetpointMaxLimit[bus] = 0x7FFF;
		Envrcm_CoolSetpointMinLimit[bus] = 0x7FFF;
		Envrcm_CoolSetpointMaxLimit[bus] = 0x7FFF;
	}
}







// Deadband
/////////////////
void RcvDeadband(void)
{
	// if the deadband in the message is valid
	if (RxBuffer[bus][29] == 'F')
	{
		// save the deadband
		Envrcm_Deadband[bus] = CharToHex(RxBuffer[bus][17]) * 500 / 9;
	}
	// Because the deadband is zoned, but should be identical, we will only use
	// zone 0/1 as the zone. Indicate that the zone 0/1 deadband has been received
	ZoneFlags[bus][0] |= MsgTable[MSG_DEADBAND].ReceivedFlagMask;
}

void UnknownDeadband(void)
{
	// Because the limits are zoned, but should be identical, we will only use zone 0/1
	// as the one that indicates the value is unknown
	if (inst == 0)
	{
		// set the value to unknown
		Envrcm_Deadband[bus] = 0x7FFF;
	}
}






// Zone Manager
/////////////////////

void RcvZoneMgr(void)
{
	// if a zone manager is not present
	if (Envrcm_ZoneManager[bus] <= 0)
	{
		// indicate that a zone manager is present
		Envrcm_ZoneManager[bus] = 1;
		// notify the application that it has changed
		App_ZoneMgrNotify(bus);
	}
}

void UnknownZoneMgr(void)
{
	// if a zone manager is present or unknown
	if (Envrcm_ZoneManager[bus] >= 0)
	{
		// indicate that a zone manager is not present
		Envrcm_ZoneManager[bus] = -1;
		// notify the application that it has changed
		App_ZoneMgrNotify(bus);
	}
}



#ifdef SCHEDULESIMPLEMENTED
// Schedule
///////////////

void SendSchedule(void)
{
	UNSIGNED_BYTE day;
	UNSIGNED_BYTE period;

	// if the message is a schedule query
	if (TxBuffer[bus][11] == 'Q')	// if the service is 'query'
	{
		// data length is 1
		TxBuffer[bus][0] = 21;
		TxBuffer[bus][14] = '1';
	}
	day = PendingPrdDay[bus] & 0x0F;
	period = PendingPrdDay[bus] / 0x10;
	// if we are sending a wildcard query 
	if (ScheduleWorkaround[bus][inst] == SCHEDULE_WORKAROUND_TIMER + 1)
	{
		// fill in the DayPrd field
		TxBuffer[bus][16] = HexToChar(0);
		TxBuffer[bus][17] = HexToChar(0);
	}
	else
	{
		// fill in the DayPrd field
		TxBuffer[bus][16] = HexToChar(period + 1);
		TxBuffer[bus][17] = HexToChar(day + 1);
	}
	// fill in the TimeFlgs field
	TxBuffer[bus][19] = 'C';
	TxBuffer[bus][20] = HexToChar(Envrcm_ScheduleStartTime[bus][inst][day][period] / 0x0100);
	TxBuffer[bus][22] = 
		HexToChar((Envrcm_ScheduleStartTime[bus][inst][day][period] & 0x00F0) / 0x0010);
	TxBuffer[bus][23] = HexToChar(Envrcm_ScheduleStartTime[bus][inst][day][period] & 0x000F);
	// fill in the SpHeat field
	TxBuffer[bus][25] = 
		HexToChar(Envrcm_ScheduleHeatSetpoint[bus][inst][day][period] / 0x1000);
	TxBuffer[bus][26] = 
		HexToChar((Envrcm_ScheduleHeatSetpoint[bus][inst][day][period] & 0x0F00) / 0x0100);
	TxBuffer[bus][28] = 
		HexToChar((Envrcm_ScheduleHeatSetpoint[bus][inst][day][period] & 0x00F0) / 0x0010);
	TxBuffer[bus][29] = 
		HexToChar(Envrcm_ScheduleHeatSetpoint[bus][inst][day][period] & 0x000F);
	// fill in the SpCool field
	TxBuffer[bus][31] = 
		HexToChar(Envrcm_ScheduleCoolSetpoint[bus][inst][day][period] / 0x1000);
	TxBuffer[bus][32] = 
		HexToChar((Envrcm_ScheduleCoolSetpoint[bus][inst][day][period] & 0x0F00) / 0x0100);
	TxBuffer[bus][34] = 
		HexToChar((Envrcm_ScheduleCoolSetpoint[bus][inst][day][period] & 0x00F0) / 0x0010);
	TxBuffer[bus][35] = 
		HexToChar(Envrcm_ScheduleCoolSetpoint[bus][inst][day][period] & 0x000F);
	// Misc field includes Fan Switch setting, other parts are zero
	TxBuffer[bus][37] = '0';    // high nibble is always zero
    switch (Envrcm_ScheduleFanSwitch[bus][inst][day][period] & 0xFF)
    {
        case 255:   // unknown - set to "not scheduled"
	        TxBuffer[bus][38] = '0';
            break;
        case 0:     // AUTO
	        TxBuffer[bus][38] = '4';
            break;
        case 200:   // ON
        default:
	        TxBuffer[bus][38] = 'C';
            break;
    }
}

void RcvSchedule(void)
{
	UNSIGNED_BYTE period;
	UNSIGNED_BYTE day;
	UNSIGNED_WORD Time;
	SIGNED_WORD SpHeat;
	SIGNED_WORD SpCool;
	UNSIGNED_BYTE SpFan;

	// get the day and period
	period = CharToHex(RxBuffer[bus][16]) - 1;
	day = CharToHex(RxBuffer[bus][17]) - 1;
	// if valid day and period
	if ((day < 7) && (period < 4))
	{
	        // Why is bit 19 sometimes 8? (on singular request replies, and on general last (0 0) reply)
		// Maybe an indicator that this is the last schedule reply?
		// srp@tworoads.net, Wed Jul 23 12:05:13 PDT 2008


		// get the time
		Time = ((((CharToHex(RxBuffer[bus][20]) & 0x07) * 0x10) + CharToHex(RxBuffer[bus][22])) *
			0x10) + CharToHex(RxBuffer[bus][23]);
		// get the heat setpoint
		SpHeat = (((((CharToHex(RxBuffer[bus][25]) * 0x10) + CharToHex(RxBuffer[bus][26])) *
			0x10) + CharToHex(RxBuffer[bus][28])) * 0x10) + CharToHex(RxBuffer[bus][29]);
		// get the cool setpoint
		SpCool = (((((CharToHex(RxBuffer[bus][31]) * 0x10) + CharToHex(RxBuffer[bus][32])) *
			0x10) + CharToHex(RxBuffer[bus][34])) * 0x10) + CharToHex(RxBuffer[bus][35]);

		// get the fan switch setting
	        switch(CharToHex(RxBuffer[bus][38]) & 0x0C)
		{
		    case 0x04:
		    	SpFan = 0;  // AUTO
			break;
		    case 0x0C:
			SpFan = 200;    // ON
			break;
		    default:
			// no change - may be unknown but may just be "Not scheduled".
			break;
		}

		// if this report is in response to a special query
		if (PendingPrdDay[bus] == Envrcm_NextSchedPeriodDay[bus])
		{
			// indicate that the special query is done
			Envrcm_NextSchedPeriodDay[bus] = 0xFF;
		}

		// if any value changed
		if ((Envrcm_ScheduleStartTime[bus][inst][day][period] != Time) ||
			(Envrcm_ScheduleHeatSetpoint[bus][inst][day][period] != SpHeat) ||
			(Envrcm_ScheduleCoolSetpoint[bus][inst][day][period] != SpCool) ||
		        (Envrcm_ScheduleFanSwitch[bus][inst][day][period] != SpFan))
		{
			// save the new values
			Envrcm_ScheduleStartTime[bus][inst][day][period] = Time;
			Envrcm_ScheduleHeatSetpoint[bus][inst][day][period] = SpHeat;
			Envrcm_ScheduleCoolSetpoint[bus][inst][day][period] = SpCool;
			Envrcm_ScheduleFanSwitch[bus][inst][day][period] = SpFan;
			// notify the application that the value changed
			App_ScheduleNotify(bus, inst, (period * 0x10) + day);
		}
	}
}

void UnknownSchedule(void)
{
	UNSIGNED_BYTE day;
	UNSIGNED_BYTE period;

	if (PendingPrdDay[bus] == 0xFF)
		return;
	day = PendingPrdDay[bus] & 0x0F;
	period = PendingPrdDay[bus] / 0x10;

	// if a special query was pending it is done
	Envrcm_NextSchedPeriodDay[bus] = 0xFF;

    // if the values are not already unknown
	if ((Envrcm_ScheduleStartTime[bus][inst][day][period] != 0x0800) ||
		(Envrcm_ScheduleHeatSetpoint[bus][inst][day][period] != 0x7F00) ||
		(Envrcm_ScheduleCoolSetpoint[bus][inst][day][period] != 0x7F00) ||
        (Envrcm_ScheduleFanSwitch[bus][inst][day][period] != 255))
	{
		// then set them to unknown
		Envrcm_ScheduleStartTime[bus][inst][day][period] = 0x0800;
		Envrcm_ScheduleHeatSetpoint[bus][inst][day][period] = 0x7F00;
		Envrcm_ScheduleCoolSetpoint[bus][inst][day][period] = 0x7F00;
        Envrcm_ScheduleFanSwitch[bus][inst][day][period] = 255;
		// notify the application that the values changed
		App_ScheduleNotify(bus, inst, (period * 0x10) + day);
	}
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////////////
// One Second Processing
//////////////////////////////////////////////////////////////////////////////////////////////////

// This function must be called by the application once every second.
// This function performs all API timing, checks for and processes received Enviracom 
// messages from the Enviracom Serial Adapter, and sends the next queued message to the 
// Enviracom Serial Adapter when the previous message completes.
void Envrcm1Sec(void)
{
#ifdef SCHEDULESIMPLEMENTED
	UNSIGNED_BYTE period;
	UNSIGNED_BYTE day;
	UNSIGNED_BYTE msk;
	UNSIGNED_BYTE i;
#endif

	// increment the seconds counter
	SecondsCnt++;
	// if 90 seconds has passed
	if (SecondsCnt >= 90)
	{
		// reset the seconds counter and increment the ninety seconds counter
		SecondsCnt = 0;
		NinetySecCnt++;
#ifdef SCHEDULESIMPLEMENTED
		// if 6 minutes has passed
		if ((NinetySecCnt & 0x03) == 0x00)
		{
			// in order to keep the schedule information fresh, every 6 minutes 
			// another schedule query message is queued.

			// for each bus
			for (bus = 0; bus < N_ENVIRACOM_BUSSES; bus++)
			{
				// get the period and day of the next schedule query
				period = SchedQryPrdDay[bus] / 0x10;
				day = SchedQryPrdDay[bus] & 0x0F;
				msk = 0x01;
				for (i = day; i > 0; i--)
				{
					msk *= 2;
				}
				// queue the next schedule query
				ScheduleQueryFlags[bus][SchedQryZone[bus]][period] |= msk;
				// increment the day
				day++;
				// if overflow
				if (day >= 7)
				{
					// reset the day and increment the period
					day = 0;
					period++;
					// if overflow
					if (period >= 4)
					{
						// reset the period and increment the zone
						period = 0;
						SchedQryZone[bus]++;
						// if overflow
						if (SchedQryZone[bus] >= N_ZONES)
						{
							// reset the zone
							SchedQryZone[bus] = 0;
						}
					}
				}
				// save the period and day
				SchedQryPrdDay[bus] = (period * 0x10) + day;
			}
		}
#endif
		// if 90 minutes has passed
		if (NinetySecCnt >= 60)
		{
			// 90 minutes is the highest interval, restart the ninety seconds counter
			NinetySecCnt = 0;
            SixtyMinBool = TRUE; // also re-enable the 60 minute query timer

			////////////////////////////////////////////////////////////////////////////////
			// Enviracom reports are expected at least every 30 minutes.  We wait 90
			// minutes to be sure.  If we haven't received the information,
			// then the data is unknown.

			// for each Enviracom bus
			for (bus = 0; bus < N_ENVIRACOM_BUSSES; bus++)
			{
				// for each message
				for (msg = 0; msg < N_MESSAGES; msg++)
				{
					// if the message has a receive flag
					if (MsgTable[msg].ReceivedFlagMask != 0)
					{
						// if the message should be reported periodically
						if (MsgTable[msg].Periodic)
						{
							// if the message is not zoned
							if (MsgTable[msg].Instance != 0xFF)
							{
								// if the message was not received
								if (!(ReceiveFlags[bus] & MsgTable[msg].ReceivedFlagMask))
								{
									// if there is a function to indicate that the value is unknown
									if (MsgTable[msg].pUnknownFunction != NULL)
									{
										// indicate that the value is unknown
										(*MsgTable[msg].pUnknownFunction)();
									}
								}
								// clear the flag for next time
								ReceiveFlags[bus] &= ~MsgTable[msg].ReceivedFlagMask;
							}
							else  // the message is zoned
							{
								// for each zone
								for (inst = 0; inst < N_ZONES; inst++)
								{
									// if the message was not received
									if (!(ZoneFlags[bus][inst] & MsgTable[msg].ReceivedFlagMask))
									{
                                        // Special case - for the System Switch message (22D0), in
                                        // a simple zoning system, the message is global.  What that
                                        // means is that anytime you send the message you set the zone
                                        // to zero, and anytime you receive it, all zones are set to that value.
                                        // So call the UNKNOWN function if:
                                        // 1. Message is NOT 22D0 (system switch)  OR
                                        // 2. (Msg is 22D0) and is NOT simple zoning  OR
                                        // 3. (Msg is 22D0) is IS simple zoning AND zone 0/1.
                                        if ((MsgTable[msg].MessageClassID != 0x22D0) ||
 	                                        (!IsSimpleZoning(bus)) ||
                                            (inst == 0))
                                        {
										    // if there is a function to indicate that the value is unknown
										    if (MsgTable[msg].pUnknownFunction != NULL)
										    {
											    // indicate that the value is unknown
											    (*MsgTable[msg].pUnknownFunction)();
                                            }
										}
									}
									// clear the flag for next time
									ZoneFlags[bus][inst] &= ~MsgTable[msg].ReceivedFlagMask;
								}
							}
						}
					}
				}
			}
		}

        // else if 60 minutes has passed, send a query for messages that haven't responded
        // This is kind of a second chance for the tstat.
        // It's supposed to send data every 30 min but doesn't always.
        // It probably will always respond to queries though.
		else if ((NinetySecCnt >= 40) && SixtyMinBool) //  (40*90sec = 60 min)
		{
            SixtyMinBool = FALSE;   // disable this now so we don't do this every time now.
                                    // (gets re-enabled at 90 minutes)
			////////////////////////////////////////////////////////////////////////////////
			// Enviracom reports are expected at least every 30 minutes.  If we haven't 
            // received the information in 60 minutes, query the tstat for that information.
            // If we don't get it in the 90 minute timeframe (code section above) then
            // put up "UNKNOWN" since it's not responding.

			// for each Enviracom bus
			for (bus = 0; bus < N_ENVIRACOM_BUSSES; bus++)
			{
				// for each message
				for (msg = 0; msg < N_MESSAGES; msg++)
				{
					// if the message has a receive flag
					if (MsgTable[msg].ReceivedFlagMask != 0)
					{
						// if the message should be reported periodically
						if (MsgTable[msg].Periodic)
						{
							// if the message is not zoned
							if (MsgTable[msg].Instance != 0xFF)
							{
								// if the message was not received
								if (!(ReceiveFlags[bus] & MsgTable[msg].ReceivedFlagMask))
								{
                                    // set to query the tstat for this information!
                                    // Note - this doesn't do anything when QueueFlagMask
                                    // equals zero, like for SetpointLimits and Deadband msgs.
	                                QueryZoneFlags[bus][inst] |= MsgTable[msg].ReceivedFlagMask;
								}
							}
							else  // the message is zoned
							{
								// for each zone
								for (inst = 0; inst < N_ZONES; inst++)
								{
									// if the message was not received
									if (!(ZoneFlags[bus][inst] & MsgTable[msg].ReceivedFlagMask))
									{
                                        // Special case - for the System Switch message (22D0), in
                                        // a simple zoning system, the message is global.  What that
                                        // means is that anytime you send the message you set the zone
                                        // to zero, and anytime you receive it, all zones are set to that value.
                                        // So send the query if:
                                        // 1. Message is NOT 22D0 (system switch)  OR
                                        // 2. (Msg is 22D0) and is NOT simple zoning  OR
                                        // 3. (Msg is 22D0) is IS simple zoning AND zone 0/1.
                                        if ((MsgTable[msg].MessageClassID != 0x22D0) ||
 	                                        (!IsSimpleZoning(bus)) ||
                                            (inst == 0))
                                        {
                                            // set to query the tstat for this information!
                                            // Note - this doesn't do anything when QueueFlagMask
                                            // equals zero, like for SetpointLimits and Deadband msgs.
	                                        QueryZoneFlags[bus][inst] |= MsgTable[msg].ReceivedFlagMask;
                                        }
									}
								}
							}
						}
					}
				}
			}
        }
	}


	// determine what is the next message to send
	/////////////////////////////////////////////

	// if the 5 second initial delay has occurred
	if (SecondsCnt >= 0)
	{
		// for each Enviracom bus
		for (bus = 0; bus < N_ENVIRACOM_BUSSES; bus++)
		{
#ifdef SCHEDULESIMPLEMENTED
			// for each zone
			for (inst = 0; inst < N_ZONES; inst++)
			{
				// if we are timing from the last schedule report
				if ((ScheduleWorkaround[bus][inst] > 0) &&
					(ScheduleWorkaround[bus][inst] < SCHEDULE_WORKAROUND_TIMER) )
				{
					// increment the timer
					ScheduleWorkaround[bus][inst]++;
				}
			}
#endif
			// if a transmit message has been sent but not ack'ed and not gone idle
			if (TxBuffer[bus][0] > 0)
			{
				// increment the Ack/Idle Timer
				AckIdleTimer[bus]++;
				// if we timed out
				if (AckIdleTimer[bus] >= 60)
				{
					// restart the timer
					AckIdleTimer[bus] = 0;
					// if it was the Ack timer that timed out
					if (TxBuffer[bus][0] > 1)
					{
						// resend the message
						msg = PendingTxMsg[bus];
						inst = PendingTxInst[bus];
						SendMessage();
					}
					// else it was the idle timer that timed out
					else
					{
						// kill the pending message
						TxBuffer[bus][0] = 0;
#ifdef SCHEDULESIMPLEMENTED
						// if a special schedule query was pending it is done
						Envrcm_NextSchedPeriodDay[bus] = 0xFF;
#endif
					}
				}
			}
			// if no sent message is in process
			if (TxBuffer[bus][0] == 0)
			{
				// send the next message in order of priority
				/////////////////////////////////////////////

#ifdef SCHEDULESIMPLEMENTED
				// assume the next sent message will not be a schedule
				PendingPrdDay[bus] = 0xFF;
#endif
				// assume the next sent message will be a Change Request
				TxBuffer[bus][11] = 'C';
				// for each message
				for (msg = 0; msg < N_MESSAGES; msg++)
				{
					// if the message is not zoned
					if (MsgTable[msg].Instance != 0xFF)
					{
						inst = MsgTable[msg].Instance;
						// if the message is queued
						if (QueueFlags[bus] & MsgTable[msg].QueueFlagMask)
						{
							// dequeue the message
							QueueFlags[bus] &= ~MsgTable[msg].QueueFlagMask;
							// send the message
							SendMessage();
							// all done with this bus, go check the next one
							goto NextBus;
						}
					}
					else
					{
						// for each zone
						for (inst = N_ZONES - 1; inst >= 0; inst--)
						{
#ifdef SCHEDULESIMPLEMENTED
							// if this is a schedule message
							if (msg == MSG_SCHEDULE)
							{
								// for each period
								for (period = 0; period < N_SCHEDULE_PERIODS; period++)
								{
									msk = 0x01;
									
									// for each day
									for (day = 0; day < 7; day++)
									{
										// if a change is queued
										if (ScheduleFlags[bus][inst][period] & msk)
										{
											PendingPrdDay[bus] = (period * 0x10) + day;
											// dequeue the message
											ScheduleFlags[bus][inst][period] &= ~msk;
											// send the message
											SendMessage();
											// all done with this bus, go check the next one
											goto NextBus;
										}
										// set up the mask for the next day
										msk *= 2;
									}
								}
							}
							else
#endif
							// if the message is queued
							if (ZoneFlags[bus][inst] & MsgTable[msg].QueueFlagMask)
							{
								// dequeue the message
								ZoneFlags[bus][inst] &= ~MsgTable[msg].QueueFlagMask;
								// send the message

								SendMessage();
								// all done with this bus, go check the next one
								goto NextBus;
							}
						}
					}
				}

				// if we get to here, there was no message queued.  Send a query if needed
				//////////////////////////////////////////////////////////////////////////

				// put the query service into the transmit buffer
				TxBuffer[bus][11] = 'Q';
				// for each message
				for (msg = 0; msg < N_MESSAGES; msg++)
				{
					// if the message is not zoned
					if (MsgTable[msg].Instance != 0xFF)
					{
						inst = MsgTable[msg].Instance;

						// if the query message is queued
						if (QueryFlags[bus] & MsgTable[msg].ReceivedFlagMask)
						{
							// dequeue the message
							QueryFlags[bus] &= ~MsgTable[msg].ReceivedFlagMask;
							// send the message
							SendMessage();
							// all done with this bus, go check the next one
							goto NextBus;
						}
					}
					else
					{
#ifdef SCHEDULESIMPLEMENTED
						// if this is a schedule message
						if (msg == MSG_SCHEDULE)
						{
							// if a special schedule query is queued
							if (Envrcm_NextSchedPeriodDay[bus] != 0xFF)
							{
								// send the schedule query
								inst = Envrcm_NextSchedZone[bus];
								PendingPrdDay[bus] = Envrcm_NextSchedPeriodDay[bus];
								SendMessage();
								// all done with this bus, go check the next one
								goto NextBus;
							}
						}
#endif
						// for each zone
						for (inst = N_ZONES - 1; inst >= 0; inst--)
						{
#ifdef SCHEDULESIMPLEMENTED
							// if this is a schedule message
							if (msg == MSG_SCHEDULE)
							{
								// for each period
								for (period = 0; period < N_SCHEDULE_PERIODS; period++)
								{
									msk = 0x01;
									// for each day
									for (day = 0; day < 7; day++)
									{
										// if a query is queued
										if (ScheduleQueryFlags[bus][inst][period] & msk)
										{
											PendingPrdDay[bus] = (period * 0x10) + day;
											// dequeue the message
											ScheduleQueryFlags[bus][inst][period] &= ~msk;
											// send the message
											SendMessage();
											// all done with this bus, go check the next one
											goto NextBus;
										}
										// set up the mask for the next day
										msk *= 2;
									}
								}
								// if we need to send a wildcard schedule query to workaround the
								// T8635 copy schedule bug
								if ( ScheduleWorkaround[bus][inst] == SCHEDULE_WORKAROUND_TIMER )
								{
									// indicate that we are sending the wildcard query
									ScheduleWorkaround[bus][inst] = SCHEDULE_WORKAROUND_TIMER + 1;
									// set up so that we are not waiting for a response
									PendingPrdDay[bus] = 0xFF;
									// send the message
									SendMessage();
									// all done with this bus, go check the next one
									goto NextBus;
								}
							}
							else
#endif
							// if the query message is queued
							if (QueryZoneFlags[bus][inst] & MsgTable[msg].ReceivedFlagMask)
							{
								// dequeue the message
								QueryZoneFlags[bus][inst] &= ~MsgTable[msg].ReceivedFlagMask;
								// send the message
								SendMessage();
								// all done with this bus, go check the next one
								goto NextBus;
							}
						}
					}
				}

				// if we made it to here, there was no query queued.  
				///////////////////////////////////////////////////////////////////////////
			}
NextBus:;
		}
	}
}




// This function must be called by the application at startup to
// initialize the API variables.  Once this function returns, the application can begin
// making periodic calls to Envrcm1Sec().
void EnvrcmInit(void)
{
#ifdef SCHEDULESIMPLEMENTED
	UNSIGNED_BYTE period;
	UNSIGNED_BYTE day;
#endif
	// Because devices should not transmit for 5 seconds after startup, the seconds counter
	// is initialized to -5 to perform this function
	SecondsCnt = -5;
	NinetySecCnt = 0;
    SixtyMinBool = TRUE; // enable the 60min query mechanism

	Envrcm_OutdoorTemperature = 0x7FFF;

	// for each Enviracom bus
	for (bus = 0; bus < N_ENVIRACOM_BUSSES; bus++)
	{
		QueueFlags[bus] = 0;
		ReceiveFlags[bus] = 0;
		QueryFlags[bus] = 0;
		// queue a query for every non zoned message
		for (msg = 0; msg < N_MESSAGES; msg++)
		{
			if (MsgTable[msg].Instance != 0xFF)
			{
				QueryFlags[bus] |= MsgTable[msg].ReceivedFlagMask;
			}
		}
		RxBuffer[bus][0] = 0;
		TxBuffer[bus][0] = 0;
		AckIdleTimer[bus] = 0;
		PendingTxMsg[bus] = 0xFF;
		PendingTxInst[bus] = 0xFF;

		// initialize global variables
		Envrcm_AirFilterRemain[bus] = 255;
		Envrcm_AirFilterRestart[bus] = 255;

		Envrcm_Humidity[bus] = 0xF2;

#ifdef EXTENSIONS
		// Added by srp
		Envrcm_HeatPumpFan[bus] = 0;
		Envrcm_HeatPumpStage[bus] = 0;
#endif
		Envrcm_HeatPumpFault[bus] = 0;
		Envrcm_HeatPumpPresent[bus] = 0;

		Envrcm_HeatFault[bus] = 0;
		Envrcm_CoolFault[bus] = 0;

		Envrcm_HeatSetpointMinLimit[bus] = 0x7FFF;
		Envrcm_HeatSetpointMaxLimit[bus] = 0x7FFF;
		Envrcm_CoolSetpointMinLimit[bus] = 0x7FFF;
		Envrcm_CoolSetpointMaxLimit[bus] = 0x7FFF;
		Envrcm_Deadband[bus] = 0x7FFF;
		Envrcm_ZoneManager[bus] = 0;
		Envrcm_ZonedSystem[bus] = 0;

#ifdef SCHEDULESIMPLEMENTED
		Envrcm_NextSchedPeriodDay[bus] = 0xFF;
		Envrcm_NextSchedZone[bus] = 0xFF;
		PendingPrdDay[bus] = 0xFF;
		SchedQryZone[bus] = 0;
		SchedQryPrdDay[bus] = 0;
#endif

		// for each zone
		for (inst = 0; inst < N_ZONES; inst++)
		{
			ZoneFlags[bus][inst] = 0;
			QueryZoneFlags[bus][inst] = 0;
			// queue the room temperature query
			QueryZoneFlags[bus][inst] |= MsgTable[MSG_ROOMTEMP].ReceivedFlagMask;
			
			// initialize global variables
			Envrcm_RoomTempUnits[bus][inst] = 0;
			Envrcm_RoomTemperature[bus][inst] = 0x7fff;

			Envrcm_FanSwitch[bus][inst] = 255;

			Envrcm_HeatSetpoint[bus][inst] = 0x7FFF;
			Envrcm_CoolSetpoint[bus][inst] = 0x7FFF;
			Envrcm_SetpointStatus[bus][inst] = 0;

			Envrcm_SystemSwitch[bus][inst] = 0xFF;
			Envrcm_AllowedSystemSwitch[bus][inst] = 0;

#ifdef SCHEDULESIMPLEMENTED
			// for each schedule period
			for (period = 0; period < N_SCHEDULE_PERIODS; period++)
			{
				ScheduleFlags[bus][inst][period] = 0;
				ScheduleQueryFlags[bus][inst][period] = 0;
				// for each day of the week
				for (day = 0; day < N_SCHEDULE_DAYS; day++)
				{
					Envrcm_ScheduleStartTime[bus][inst][day][period] = 0x800;
					Envrcm_ScheduleHeatSetpoint[bus][inst][day][period] = 0x7F00;
					Envrcm_ScheduleCoolSetpoint[bus][inst][day][period] = 0x7F00;
                    Envrcm_ScheduleFanSwitch[bus][inst][day][period] = 255;
				}
			}
			ScheduleWorkaround[bus][inst] = 0;
#endif
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////////////
// Send Message
/////////////////////////////////////////////////////////////////////////////////////////////

UNSIGNED_BYTE CharToHex(UNSIGNED_BYTE Chr)
{
	// if the character is numeric
	if ( (Chr >= '0') && (Chr <= '9') )
	{
		return (Chr - '0');
	}
	else
	{
		return (Chr - ('A' - 10) );
	}
}

UNSIGNED_BYTE HexToChar(UNSIGNED_BYTE Hex)
{
	if (Hex <= 9)
	{
		return (Hex + '0');
	}
	else
	{
		return (Hex - (10 -'A'));
	}
}


UNSIGNED_BYTE CalcChecksum(UNSIGNED_BYTE* pBuffer)
{
	UNSIGNED_BYTE Chksum;
	UNSIGNED_BYTE i;

	// process the prioity character
	switch(pBuffer[1])
	{
	case 'H':
		Chksum = 0x80;
		break;

	case 'M':
		Chksum = 0x40;
		break;

	default:
		Chksum = 0;
		break;
	}

	// process the message class id
	Chksum ^= (CharToHex(pBuffer[3]) * 16) + CharToHex(pBuffer[4]);	// high byte
	Chksum ^= (CharToHex(pBuffer[5]) * 16) + CharToHex(pBuffer[6]);	// low byte

	// process the instance number
	Chksum ^= (CharToHex(pBuffer[8]) * 16) + CharToHex(pBuffer[9]);

	// process the service
	switch(pBuffer[11])
	{
	case 'C':
		Chksum ^= 0x80;
		break;

	case 'R':
		Chksum ^= 0x40;
		break;

	default:
		break;
	}

	// process the data bytes
	for (i = 13; i <= pBuffer[0]-3; i+=3)
	{
		Chksum ^= (CharToHex(pBuffer[i]) * 16) + CharToHex(pBuffer[i+1]);
	}

	return Chksum;
}

// this function sends the message in TxBuffer to the appropriate Enviracom Serial Interface
void SendMessage(void)
{
	UNSIGNED_BYTE Chksum;

	// identify the message as pending
	PendingTxMsg[bus] = msg;
	PendingTxInst[bus] = inst;
	// put space characters into the message
	TxBuffer[bus][2] = 0x20;
	TxBuffer[bus][7] = 0x20;
	TxBuffer[bus][10] = 0x20;
	TxBuffer[bus][12] = 0x20;
	TxBuffer[bus][15] = 0x20;
	TxBuffer[bus][18] = 0x20;
	TxBuffer[bus][21] = 0x20;
	TxBuffer[bus][24] = 0x20;
	TxBuffer[bus][27] = 0x20;
	TxBuffer[bus][30] = 0x20;
	TxBuffer[bus][33] = 0x20;
	TxBuffer[bus][36] = 0x20;
	TxBuffer[bus][39] = 0x20;

	// put the number of characters into the message
	if (TxBuffer[bus][11] == 'Q')	// if the service is 'query'
	{
		// data length = 0
		TxBuffer[bus][0] = 18;
		TxBuffer[bus][14] = '0';
	}
	else
	{
		TxBuffer[bus][0] = (3 * MsgTable[msg].MinDataLength) + 18;
		// fill in the low nibble of # of data bytes
		TxBuffer[bus][14] = HexToChar(MsgTable[msg].MinDataLength);
	}
	// all messages are sent with medium priority
	TxBuffer[bus][1] = 'M';
	// put message class ID into the message
	TxBuffer[bus][3] = HexToChar(MsgTable[msg].MessageClassID / 0x1000);
	TxBuffer[bus][4] = HexToChar( (MsgTable[msg].MessageClassID & 0x0F00) / 0x0100);
	TxBuffer[bus][5] = HexToChar( (MsgTable[msg].MessageClassID & 0x00F0) / 0x0010);
	TxBuffer[bus][6] = HexToChar(MsgTable[msg].MessageClassID & 0x000F);
	// put the instance into the message
	// if the message is zoned
	if (MsgTable[msg].Instance == 0xFF)
	{
		// the high byte of the instance field should be zero
		TxBuffer[bus][8] = '0';
		// if the zone is zone 1
		if (inst == 0)
		{
			// if the system is not zoned
			if (Envrcm_ZonedSystem[bus] == 0)
			{
				// the instance field should be 0
				TxBuffer[bus][9] = '0';
			}
			else  // system is zoned
			{
				// the instance field should be one
				TxBuffer[bus][9] = '1';
			}
		}
		else // the zone is not 0
		{
			// the instance field should be the zone
			TxBuffer[bus][9] = HexToChar(inst + 1);
		}
	}
	else
	{
		// the instance field is obtained from the table
		TxBuffer[bus][8] = HexToChar(MsgTable[msg].Instance / 0x10);
		TxBuffer[bus][9] = HexToChar(MsgTable[msg].Instance & 0x0F);
	}
	// high nibble of # data bytes always = 0
	TxBuffer[bus][13] = '0';
	// fill in the data field.  NOTE:  for query messages, the data field gets filled
	// in but are not sent because the data field length is zero.
	// If there is a function to fill in the data field
	if (MsgTable[msg].pSendFunction != NULL)
		// call the function to fill in the data field
		(*MsgTable[msg].pSendFunction)();
	// insert the message checksum
	Chksum = CalcChecksum(TxBuffer[bus]);
	TxBuffer[bus][TxBuffer[bus][0] - 2] = HexToChar(Chksum / 0x10);
	TxBuffer[bus][TxBuffer[bus][0] - 1] = HexToChar(Chksum & 0x0F);
	// add the carriage return and linefeed characters
	TxBuffer[bus][TxBuffer[bus][0]] = '\r';
	// Reset the Ack/Idle Timer
	AckIdleTimer[bus] = 0;
	// notify the application to send the message in Tx Buffer
	App_EnvrcmSendMsg(bus);
}

// The application calls
// this function whenever a new byte has been received from an Enviracom Serial Adaptor.
// NextByte is the byte that has been received.  EnviracomBus identifies the Enviracom bus
// that the byte was received from. 
void NewEnvrcmRxByte(UNSIGNED_BYTE NextByte, UNSIGNED_BYTE EnviracomBus)
{
	UNSIGNED_WORD MsgClass;
	UNSIGNED_BYTE DataLength;
#ifdef SCHEDULESIMPLEMENTED
	UNSIGNED_BYTE msk;
	UNSIGNED_BYTE period;
	UNSIGNED_BYTE day;
#endif

	bus = EnviracomBus;
	// if the byte is the first byte of a message
	if ( (NextByte == 'H') || (NextByte == 'M') || (NextByte == 'L') ||
		(NextByte == '[') )
	{
		// delete any previous bytes that were received
		RxBuffer[bus][0] = 0;
	}
	else if (RxBuffer[bus][0] == 0)  // the byte would be invalid as the first byte in
		// a message
	{
		return;
	}
	// put the byte in the receive buffer
	RxBuffer[bus][++RxBuffer[bus][0]] = NextByte;
	// if the buffer is full
	if (RxBuffer[bus][0] > MESSAGE_SIZE)
	{
		// set up to not write past the end of the buffer
		RxBuffer[bus][0] = MESSAGE_SIZE;
	}
	// if the byte is the end of an Ack/Nack/Idle
	if (RxBuffer[bus][1] == '[')
	{
		if (NextByte == ']')
		{
			switch (RxBuffer[bus][2])
			{
			case 'A':  // ACK
				// if we were waiting for an Ack
				if (TxBuffer[bus][0] > 1)
				{
					// now wait for the bus to go idle
					TxBuffer[bus][0] = 1;
					// restart the timer
					AckIdleTimer[bus] = 0;
					QueryFlags[bus] &= ~TX_NACK;
				}
				break;

			case 'N':  // NACK
				// if we were waiting for an Ack
				if (TxBuffer[bus][0] > 1)
				{
					// if we weren't been nacked before
					if (!(QueryFlags[bus] & TX_NACK))
					{
						// indicate we've nacked once
						QueryFlags[bus] |= TX_NACK;

						// resend the message
						msg = PendingTxMsg[bus];
						inst = PendingTxInst[bus];
						SendMessage();
					}
					else
					{
						// indicate that the Tx buffer is free
						TxBuffer[bus][0] = 0;
						QueryFlags[bus] &= ~TX_NACK;
#ifdef SCHEDULESIMPLEMENTED
						for (inst = 0; inst < N_ZONES; inst++)
						{
							if (ScheduleWorkaround[bus][inst] >= (SCHEDULE_WORKAROUND_TIMER + 1) )
							{
								// indicate that the wildcard schedule query is not active
								ScheduleWorkaround[bus][inst] = 0;
							}
						}
#endif
					}
				}
				break;

			case 'I':  // IDLE
				// if we were waiting for an Idle
				if (TxBuffer[bus][0] == 1)
				{
					msg = PendingTxMsg[bus];
					inst = PendingTxInst[bus];
					// our expected response did not arrive, indicate that the data is unknown
					if (MsgTable[msg].pUnknownFunction != NULL)
					{

						(*MsgTable[msg].pUnknownFunction)();
					}
					// indicate that the Tx buffer is free
					TxBuffer[bus][0] = 0;
				}
#ifdef SCHEDULESIMPLEMENTED
				for (inst = 0; inst < N_ZONES; inst++)
				{
					if (ScheduleWorkaround[bus][inst] >= (SCHEDULE_WORKAROUND_TIMER + 1))
					{
						// indicate that the wildcard schedule query is not active
						ScheduleWorkaround[bus][inst] = 0;
					}
				}
#endif
				break;

			case 'R':  // Reset
				// if we were waiting for an Ack
				if (TxBuffer[bus][0] > 1)
				{
					// resend the message
					msg = PendingTxMsg[bus];
					inst = PendingTxInst[bus];
					SendMessage();
				}
				// if we were waiting for an Idle
				else if (TxBuffer[bus][0] == 1)
				{
					// our expected response did not arrive, indicate that the data is unknown
					if (MsgTable[msg].pUnknownFunction != NULL)
					{
						(*MsgTable[msg].pUnknownFunction)();
					}
					// indicate that the Tx buffer is free
					TxBuffer[bus][0] = 0;
#ifdef SCHEDULESIMPLEMENTED
					for (inst = 0; inst < N_ZONES; inst++)
					{
						if (ScheduleWorkaround[bus][inst] >= (SCHEDULE_WORKAROUND_TIMER + 1))
						{
							// indicate that the wildcard schedule query is not active
							ScheduleWorkaround[bus][inst] = 0;
						}
					}
#endif
				}
				break;


			default:
				break;
			}
			RxBuffer[bus][0] = 0;
		}
	}
	// if we've received the end of a message
	else if (NextByte == '\r')
	{
		// if the checksum matches
		if ( CalcChecksum(RxBuffer[bus]) == 
			(CharToHex(RxBuffer[bus][RxBuffer[bus][0] - 2]) * 0x10) +
				CharToHex(RxBuffer[bus][RxBuffer[bus][0] - 1]) )
		{
			// find a match in the message table
			for (msg = 0; msg < N_MESSAGES; msg++)
			{
				// if the service matches
				if (RxBuffer[bus][11] == MsgTable[msg].RxService)
				{
					// get the message class
					MsgClass = (((((CharToHex(RxBuffer[bus][3]) * 0x10) + 
						CharToHex(RxBuffer[bus][4])) * 0x10) + CharToHex(RxBuffer[bus][5])) *
						0x10) + CharToHex(RxBuffer[bus][6]);
					// if the message class matches
					if (MsgClass == MsgTable[msg].MessageClassID)
					{
						// get the data length
						DataLength = (CharToHex(RxBuffer[bus][13]) * 0x10) + 
							CharToHex(RxBuffer[bus][14]);

						// if the data length matches
						if (DataLength >= MsgTable[msg].MinDataLength)
						{
							// get the instance
							inst = (CharToHex(RxBuffer[bus][8]) * 16) + 
								CharToHex(RxBuffer[bus][9]);
							// if any instance will do
							if ((MsgTable[msg].Instance == 0xFE) || 
								// or if the message is not zoned and the instance matches
								((MsgTable[msg].Instance != 0xFF) && 
								(inst == MsgTable[msg].Instance)) ||
								// or if the message is zoned and the zone is valid

                                // Since we haven't decremented "inst" (zone #) yet, the value will
                                // be (for zoned systems) 1 (or 0) to N_ZONES, so we need to include
                                // the value N_ZONES as a valid zone.  Once we do the decrement,
                                // the comparison will be < N_ZONES since it'll now be zero-based.
								((MsgTable[msg].Instance == 0xFF) && (inst >= 0) &&
									(inst <= N_ZONES)) )
							{
								// if the instance is zoned
								if (MsgTable[msg].Instance == 0xFF)
								{
									// if the zone is not zero
									if (inst != 0)
									{
										// now decrement the instance to match the array index
										inst--;
									}
								}
#ifdef SCHEDULESIMPLEMENTED
								// get the period and day
								if (msg == MSG_SCHEDULE)
								{
									PrdDay = ((CharToHex(RxBuffer[bus][16]) * 0x10) + 
										CharToHex(RxBuffer[bus][17])) - 0x11;
								}
								else
#endif
								{
									PrdDay = 0xFF;  // not used
								}
								// indicate that the message was received
								if (MsgTable[msg].Instance == 0xFF)
								{
									ZoneFlags[bus][inst] |= MsgTable[msg].ReceivedFlagMask;
								}
								else
								{
									ReceiveFlags[bus] |= MsgTable[msg].ReceivedFlagMask;
								}
								// if this is not a response to a previously sent message
								if ((msg != PendingTxMsg[bus]) || (inst != PendingTxInst[bus]) || 
#ifdef SCHEDULESIMPLEMENTED
									(PrdDay != PendingPrdDay[bus]) ||
#endif
									(TxBuffer[bus][0] != 1))
								{
									// if the message has a queued change request, we don't want
									// to overwrite the change.  So insead we will ignore the
									// received message.

									// non zoned case
									if (MsgTable[msg].Instance != 0xFF)
									{
										// if change queued
										if (QueueFlags[bus] & MsgTable[msg].QueueFlagMask)
										{
											goto SkipMsg;
										}
									}
									else // zoned case
									{
#ifdef SCHEDULESIMPLEMENTED
										// if a schedule change
										if (msg == MSG_SCHEDULE)
										{
											// if we get to here, we have received a schedule 
											// message that wasn't queried.  This indicates a
											// change to the schedule that was voluntarily sent
											// by the thermostat.  There is a bug in the T8635
											// when the schedule is copied from one day to the
											// next in that only some but not all of the schedule 
											// messages get sent.  When we get to here, 
											// it is an indication
											// that this event might have happened.  As a workaround
											// we will set a flag that will cause a wildcard schedule
											// query to be sent.  This causes the T8635 to send all
											// of the schedule so that we don't miss anything.
											if (ScheduleWorkaround[bus][inst] == 0)
											{
												ScheduleWorkaround[bus][inst] = 1;
											}
											period = PrdDay / 0x10;
											day = PrdDay & 0x0F;
											for (msk = 0x01; day > 0; day--)
											{
												msk *= 2;
											}
											if (ScheduleFlags[bus][inst][period] & msk)
											{
												goto SkipMsg;
											}
										}
										else  // not a schedule change
#endif
										{
											// if change queued
											if (ZoneFlags[bus][inst] & MsgTable[bus].QueueFlagMask)
											{
												goto SkipMsg;
											}
										}
									}
								}
								else  // response to a sent message
								{
									// indicate that the message is no longer pending
									TxBuffer[bus][0] = 0;
								}
								// if there is a function to process the received message
								if (MsgTable[msg].pReceiveFunction != NULL)
								{
									// process the received message
									(*MsgTable[msg].pReceiveFunction)();
								}
	SkipMsg:;
							}
						}
					}
				}
			}
		}
		// clear the receive buffer
		RxBuffer[bus][0] = 0;
	}
}


UNSIGNED_BYTE IsSimpleZoning(UNSIGNED_BYTE EnviracomBus)
{
	if ((Envrcm_ZoneManager[EnviracomBus] <= 0) && (Envrcm_ZonedSystem[EnviracomBus] > 0))
        return(TRUE);
    else
        return(FALSE);
}

