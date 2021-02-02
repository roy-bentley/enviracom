/**********************************************************************************/
/**************** PVCS HEADER for Enviracom API Source Code ***********************/
/**********************************************************************************/
/*										*/
/* Archive: /Projects/Enviracom API/EnviracomAPI.h $				*/
/* Date: 8/01/03 4:15p $							*/
/* Workfile: EnviracomAPI.h $							*/
/* Modtime: 8/01/03 10:07a $							*/
/* Author: Shoglund $								*/
/* Log: /Projects/Enviracom API/EnviracomAPI.h $			
 * 
 * 7     8/01/03 4:15p Shoglund
 * Added function declaration for IsSimpleZoning() - used to determine
 * logic path for SystemSwitch messages.
 * 
 * 6     7/21/03 5:11p Shoglund
 * Checked in correct file (last version came from wrong directory) to
 * correctly
 * add in the Envrcm_ScheduleFanSwitch variable.
 * 
 * 3     5/09/03 4:45p Shoglund
 * 
 * Corrected comment describing Envrcm_HourDay structure, the "Hour" part of the
 * data takes bits 0-4, not 0-3.  
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

// if this file has not already been included
#ifndef _ENVIRACOMAPI_H_
// indicate that this is the first time this file has been included
#define _ENVIRACOMAPI_H_

#ifndef TRUE
#define	TRUE	1
#endif
#ifndef FALSE
#define	FALSE	0
#endif
#ifndef NULL
#define NULL    0
#endif

#undef _ENVRCM_DEF_
// if this file is being included by the Envrcm_API.c source file
#ifdef _ENVIRACOMAPI_C_
// configure to declare the API data
#define _ENVRCM_DEF_
// else not being included by the Envrcm_API.c source file
#else
// configure to reference the API data
#define _ENVRCM_DEF_ extern
#endif

////////////////////////////////////////////////////////////////////////////////////////////////
// Application defined constants
////////////////////////////////////////////////////////////////////////////////////////////////

// Application developers should adjust these define statements to configure this API to their
// application:

#define UNSIGNED_BYTE unsigned char  /* declaration of unsigned 8 bit variable type */
#define SIGNED_BYTE   char           /* declaration of signed 8 bit variable type */
#define UNSIGNED_WORD unsigned int   /* declaration of unsigned 16 bit variable type */
#define SIGNED_WORD   int            /* declaration of signed 16 bit variable type */

#define N_ENVIRACOM_BUSSES	2 /* the maximum number of supported Enviracom buses */
// NOTE:  To avoid conflict when there are multiple furnaces in a home, each furnace needs its 
// own independant Enviracom bus.  If the application needs to communicate with each of them,
// then each Enviracom bus needs its own Enviracom Serial Adaptor connected to its own
// serial port.

#define N_ZONES		3	/* the maximum number of supported zones for each Enviracom bus */
// NOTE:  N_ZONES must be a value between 1 to 9, inclusive. If the application does not
// support a zoned system, NZONES must be set to 1.
//
// NOTE:  Many Enviracom values below are arrays that use a zone number as one of the dimensions
// of the array.  Also many Enviracom functions below pass a zone number as one of the parameters.
// In a non-zoned system, a value of 0 should be used as the zone number.  In a zoned system, a
// value of the zone number minus 1 should be used.  For example, 0 should be used for zone 1, 1
// should be used for zone 2, and so on.  An application can dynamically determine whether a system
// is zoned or not by examining the variable, Envrcm_ZonedSystem, below.

// If the application is not interested in the thermostat schedule(s), it should comment out
// the following define to save code and variable space.
#define SCHEDULESIMPLEMENTED	/* the application can view and change the thermostat schedule(s) */

  
/////////////////////////////////////////////////////////////////////////////////////////////////
// definitions
/////////////////////////////////////////////////////////////////////////////////////////////////
#define N_SCHEDULE_PERIODS	4	/* the number of periods that each thermostat supports in their schedules */
#define PERIOD_WAKE			0	/* constant to identify the Wake period */
#define PERIOD_LEAVE		1	/* constant to identify the Leave period */
#define PERIOD_RETURN		2	/* constant to identify the Return period */
#define PERIOD_SLEEP		3	/* constant to identify the Sleep period */

#define N_SCHEDULE_DAYS	7	/* the number of days that each thermostat supports in their schedules */
#define DAY_MONDAY		0	/* constant to identify Monday */
#define DAY_TUESDAY		1	/* constant to identify Tuesday */
#define DAY_WEDNESDAY	2	/* constant to identify Wednesday */
#define DAY_THRUSDAY	3	/* constant to identify Thursday */
#define DAY_FRIDAY		4	/* constant to identify Friday */
#define DAY_SATURDAY	5	/* constant to identify Saturday */
#define DAY_SUNDAY		6	/* constant to identify Sunday */

#define ZONE1		0		/* constant to identify zone 1 */
#define ZONE2		1		/* constant to identify zone 2 */
#define ZONE3		2		/* constant to identify zone 3 */
#define ZONE4		3		/* constant to identify zone 4 */
#define ZONE5		4		/* constant to identify zone 5 */
#define ZONE6		5		/* constant to identify zone 6 */
#define ZONE7		6		/* constant to identify zone 7 */
#define ZONE8		7		/* constant to identify zone 8 */
#define ZONE9		8		/* constant to identify zone 9 */
// NOTE:  In a non zoned system, use ZONE1 as the zone.  Or use the following:
#define NONZONED	ZONE1	/* constant to use for zone when the application doesn't support zones */


#define MESSAGE_SIZE	45  /* the maximum number of characters in a message */

/////////////////////////////////////////////////////////////////////////////////////////////////
// Data Descriptions
/////////////////////////////////////////////////////////////////////////////////////////////////

// Data items that are READ ONLY can only be read by the application.  Data items that are
// WRITE ONLY can only be written by the application.  Data items that are READ/WRITE can be read
// and written by the application.  See the comments for additional details.

_ENVRCM_DEF_ UNSIGNED_BYTE TxBuffer[N_ENVIRACOM_BUSSES][MESSAGE_SIZE];  // (READ/WRITE) character 
	// buffers for messages
	// to send on each enviracom bus via the serial ports.  For each enviracom bus, the first
	// byte in the buffer contains the number of bytes in the message.  This byte is not sent to the serial
	// port, however the bytes that follow must be sent in order to the serial port.  For 
	// additional information see the function, App_EnvrcmSendMsg.


// Air Filter
///////////////////
_ENVRCM_DEF_ UNSIGNED_BYTE Envrcm_AirFilterRemain[N_ENVIRACOM_BUSSES];  // (READ ONLY) The remaining number of
	// days of fan run time before the filter is dirty, for each Enviracom bus.
		// 0 = DIRTY_OR_DISABLED - The filter is either dirty or disabled depending on 
		//		whether	Envrcm_AirFltrRestart = 0.
		// 255 = Unknown - Unknown but not dirty
	// After initialization, until it can be determined what the actual value is, the 
	// value will = 255.

_ENVRCM_DEF_ UNSIGNED_BYTE Envrcm_AirFilterRestart[N_ENVIRACOM_BUSSES];	// (READ ONLY) The timer restart
	// value for each Enviracom bus, in number of days of fan run time units.
		// 0 = The timing function has been disabled and the filter is not dirty even if 
		//		Envrcm_AirFilterRemain = 0.
		// 255 = Unknown
	// After initialization, until it can be determined what the actual value is, the 
	// value will = 255.  To change this value, the application should first write a new 
	// value to this variable, then call ChangeAirFilterRestart().

void RestartAirFilterTimer(UNSIGNED_BYTE EnviracomBus);  // When the application calls this
	// function, Envrcm_AirFilterRemain is changed to the value in Envrcm_AirFilterRestart and
	// a message is queued to be sent on 
	// the designated Enviracom bus to restart the air filter timer to the value in 
	// Envrcm_AirFilterRestart.

void App_AirFilterNotify(UNSIGNED_BYTE EnviracomBus);  // this function must be written by the
	// application.  It is called by this API whenever an Air Filter Timer message is received
	// on Enviracom whose value differs from what is in Envrcm_AirFilterRemain and/or 
	// Envrcm_AirFilterRestart.  EnviracomBus indicates which Enviracom bus has the value that
	// changed.  The variables Envrcm_AirFilterRemain and Envrcm_AirFilterRestart will contain
	// the new values from the received message.  This function can do whatever the application 
	// requires.



// Humidity
///////////////////
_ENVRCM_DEF_ UNSIGNED_BYTE Envrcm_Humidity[N_ENVIRACOM_BUSSES];  // (READ ONLY) Ambient humidity
	// for each Enviracom bus in percent relative humidity.  Special values are:
		// F0h = shorted sensor
		// F1h = open sensor
		// F2h = not available
		// F3h = out of range high
		// F4h = out of range low
		// F5h = not reliable
		// F6h-FEh = reserved error
		// FFh = non-specified error
	// After initialization, until it can be determined what the actual value is, the 
	// value will = F2h.

void App_HumidityNotify(UNSIGNED_BYTE EnviracomBus);  // this function must be written by the
	// application.  It is called by this API whenever an Ambient Humidity message is received
	// on Enviracom whose value differs from what is in Envrcm_Humidity.  EnviracomBus 
	// indicates which Enviracom bus has the value that changed.    The variable Envrcm_Humidity
	// will contain the new value from the received message.  This function can 
	// do whatever the application requires.


// Heat Pump 
/////////////////////
_ENVRCM_DEF_ SIGNED_BYTE Envrcm_HeatPumpFault[N_ENVIRACOM_BUSSES];  // (READ ONLY) Indicates 
	// the heat pump fault status for each Enviracom bus.  Positive if 
	// there is a heat pump fault.  Negative if no fault. Zero if it is unknown whether there is
	// a heat pump fault or not. After initialization, until it can be determined what the 
	// actual value is, the value will = 0.

#ifdef EXTENSIONS
_ENVRCM_DEF_ SIGNED_WORD Envrcm_HeatPumpFan[N_ENVIRACOM_BUSSES];  // (READ ONLY) Indicates status of
	// the heat pump fan for each Enviracom bus.  Positive if the Fan is on
	// values are 200 if the fan is on. This is experimental.

_ENVRCM_DEF_ SIGNED_WORD Envrcm_HeatPumpStage[N_ENVIRACOM_BUSSES];  // (READ ONLY) Indicates status of
	// the heat pump gas stage for each Enviracom bus.  Positive if the stage is on, 
	// values are 100 for first stage, 200 for second stage. This is experimental.
#endif

_ENVRCM_DEF_ SIGNED_BYTE Envrcm_HeatPumpPresent[N_ENVIRACOM_BUSSES];  // (READ ONLY) Indicates
	// whether a heat pump is present for each Enviracom bus.  Positive if
	// there is a heat pump present.  Negative if no heat pump is present.  Zero if it is unknown 
	// whether a heat pump is present or not.  After initialization, until it can be determined
	// what the actual value is, the value will = 0.
	// If it is determined that no heat pump is present, Envrcm_HeatPumpFault will be set to a
	// negative number.

void App_HeatPumpFaultNotify(UNSIGNED_BYTE EnviracomBus);  // this function must be written by the
	// application.  It is called by this API whenever a message is received on Enviracom whose
	// value differs from what is in Envrcm_HeatPumpFault.  EnviracomBus indicates which 
	// Enviracom bus has the value that changed.  The variable Envrcm_HeatPumpFault will contain
	// the new value in the received message. This function can do whatever the
	// application requires.

#ifdef EXTENSIONS
void App_HeatPumpFanNotify(UNSIGNED_BYTE EnviracomBus);  // this function must be written by the
	// application.  It is called by this API whenever a message is received on Enviracom whose
	// value differs from what is in Envrcm_HeatPumpFan.  EnviracomBus indicates which 
	// Enviracom bus has the value that changed.  The variable Envrcm_HeatPumpFan will contain
	// the new value in the received message. This function can do whatever the
	// application requires.


void App_HeatPumpStageNotify(UNSIGNED_BYTE EnviracomBus);  // this function must be written by the
	// application.  It is called by this API whenever a message is received on Enviracom whose
	// value differs from what is in Envrcm_HeatPumpStage.  EnviracomBus indicates which 
	// Enviracom bus has the value that changed.  The variable Envrcm_HeatPump will contain
	// the new value in the received message. This function can do whatever the
	// application requires.
#endif

// Equipment Fault
/////////////////////////////
_ENVRCM_DEF_ SIGNED_BYTE Envrcm_HeatFault[N_ENVIRACOM_BUSSES];  // (READ ONLY)  Indicates whether
	// the Enviracom discharge air sensor has detected a heating fault, for each Enviracom
	// bus.  Positive if there is a heating
	// fault.  Negative if no heating fault.  Zero if it is unknown whether there is a 
	// heating fault or not.  After initialization, until it can be determined what the 
	// actual value is, the value will be zero.  If the discharge air sensor is not 
	// present, the value will remain zero.

_ENVRCM_DEF_ SIGNED_BYTE Envrcm_CoolFault[N_ENVIRACOM_BUSSES];  // (READ ONLY)  Indicates whether
	// the Enviracom discharge air sensor has detected a cooling fault, for each Enviracom
	// bus.  Positive if there is a cooling
	// fault.  Negative if no cooling fault.  Zero if it is unknown whether there is a 
	// cooling fault or not.  After initialization, until it can be determined what the 
	// actual value is, the value will be zero.  If the discharge air sensor is not 
	// present, the value will remain zero.

void App_EquipFaultNotify(UNSIGNED_BYTE EnviracomBus);  // this function must be written by the
	// application.  It is called by this API whenever a message is received on Enviracom whose
	// value differs from what is in Envrcm_HeatFault or Envrcm_CoolFault.  EnviracomBus 
	// indicates which Enviracom bus has the value that changed.  The variables Envrcm_HeatFault
	// and Envrcm_CoolFault will contain the new values from the received message.  This function 
	// can do whatever the application requires.  



// Room Temperature
/////////////////////////
_ENVRCM_DEF_ UNSIGNED_BYTE Envrcm_RoomTempUnits[N_ENVIRACOM_BUSSES][N_ZONES];  // (READ ONLY)  Bit
	// mapped for each Enviracom bus and each zone as follows:
		// Bit 0:  Units for the temperature that is displayed on the thermostat
		//			0 = Fahrenheit
		//			1 = Celsius
		// All other bits are reserved.
	// After initialization, until it can be determined what the actual values are, all 
	// bits will = 0.

_ENVRCM_DEF_ SIGNED_WORD Envrcm_RoomTemperature[N_ENVIRACOM_BUSSES][N_ZONES];  // (READ ONLY)  The
	// displayed ambient temperature in each zone of each Enviracom bus, in hundreths of deg C
	// units. Special values:
		// 7FFFh = Not Available
		// 8000h = Sensor fault
	// After initialization, until it can be determined what the actual value is, the 
	// value will be 7FFFh.  This array can be used to determine which zones are present in 
	// the system.   If the value is 7FFFh, the zone is not present, otherwise the zone is
	// present.  If all values are 7FFFh, then no thermostats are present in the system.
	// If the system is not zoned, and a non zoned thermostat is present in the system,
	// zone 1 will have a temperature and all others will be 7FFFh.

_ENVRCM_DEF_ UNSIGNED_BYTE Envrcm_ZonedSystem[N_ENVIRACOM_BUSSES];  // Positive if a zoned system is
	// present.  Otherwise 0.

void App_RoomTempNotify(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone);  // this function must
	// be written by the application.  It is called by this API whenever a message is received
	// on Enviracom whose value differs from what is in Envrcm_RoomTemperature or 
	// Envrcm_RoomTempUnits.  EnviracomBus and Zone indicate which Enviracom bus and which
	// zone has the value that changed.  The variables Envrcm_RoomTempUnits and 
	// Envrcm_RoomTemperature will contain the new values from the received message.  This 
	// function can do whatever the application requires.  


// Fan Switch
///////////////
_ENVRCM_DEF_ UNSIGNED_BYTE Envrcm_FanSwitch[N_ENVIRACOM_BUSSES][N_ZONES];  // (READ/WRITE)  The
	// fan switch position in each zone of each Enviracom bus.
		// A value of 0 =			AUTO
		// A value of 1 to 200 =	ON
		// A value of 255 =			Unknown (READ ONLY)
	// After initialization, until it can be determined what the actual is, the value 
	// will = 255.  To change the fan switch position, the application should write a new 
	// value to this variable, then call ChangeFanSwitch().

void ChangeFanSwitch(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone);  // When the application
	// calls this function, a message is queued to be sent on the specified Enviracom
	// bus for the specified zone to change its fan switch position.  The value sent is the value
	// in Envrcm_FanSwitch.

void App_FanSwitchNotify(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone);  // this function must
	// be written by the application.  It is called by this API whenever a message is received
	// on Enviracom whose value differs from what is in Envrcm_FanSwitch.  EnviracomBus and Zone
	// indicate which Enviracom bus and which zone has the value that changed.  The variable
	// Envrcm_FanSwitch will contain the new value from the received message.  This function
	// can do whatever the application requires.  


// Outdoor Temperature
///////////////////////
// The application can either use the outdoor temperature, in which case it gets the 
// outdoor temperature value from the Enviracom system, or it can supply the outdoor 
// temperature, in which case it has gotten the outdoor temperature from some
// other means and it sends the value in an Enviracom message.  If the application uses 
// the outdoor temperature, it simply reads the following variable:

_ENVRCM_DEF_ SIGNED_WORD Envrcm_OutdoorTemperature;  // (READ/WRITE) Outdoor temperature 
	// in hundredths of deg C units.  
	// Special values are: 
		// 7FFFh = not available
		// 8000h-80FFh = shorted sensor
		// 8100h-81FFh = open sensor
		// 8200h-82FFh = not available
		// 8300h-83FFh = out of range high
		// 8400h-84FFh = out of range low
		// 8500h-85FFh = not reliable
	// After initialization, until it can be determined what the actual value is, the 
	// value will = 7FFFh.

// When the outdoor temperature is received on one of the Enviracom busses, it's value is
// saved in Envrcm_OutdoorTemperature and an Outdoor Temperature report with this value is
// automatically queued by this API on each of the other Enviracom busses.  In this way, this
// API automatically propagates the outdoor temperature from one Enviracom bus to multiple 
// Enviracom busses.

// If the application supplies the outdoor temperature, it must write a new value to 
// Envrcm_OutdoorTemperature and periodically call the following function with an interval 
// no shorter than three minutes and no longer than 5 minutes.  If the application does 
// not supply the outdoor temperature, it must not write to Envrcm_OutdoorTemperature 
// and must not call the following function:

void SetOutdoorTemperature(void);  // This function queues a report of the outdoor 
	// temperature on each Enviracom bus.  The value sent is the value in
	// Envrcm_OutdoorTemperature.

#ifdef EXTENSIONS
void App_OutdoorTempNotify(UNSIGNED_BYTE EnviracomBus);  // this function must be written by the
	// application.  It is called by this API whenever an Outdoor Temperature message is received
	// on Enviracom whose value differs from what is in Envrcm_OutdoorTemperature.  EnviracomBus 
	// indicates which Enviracom bus has the value that changed.    The variable Envrcm_OutdoorTemperature
	// will contain the new value from the received message.  This function can 
	// do whatever the application requires.
#endif



// Setpoints
/////////////////
_ENVRCM_DEF_ SIGNED_WORD Envrcm_HeatSetpoint[N_ENVIRACOM_BUSSES][N_ZONES];  // (READ/WRITE)  The current heat
	// setpoint in hundredths of deg C units for each zone.  This value is the setpoint
	// that the thermostat tries to control to regardless of whether it is running off 
	// a schedule, a temporary setpoint, or a hold setpoint.
	// Special values:
		// 7FFF = not available (READ ONLY)
	// After initialization, until it can be determined what the actual value is, the 
	// value will be 7FFFh.
	// To create a temporary setpoint change on the thermostat, the application should 
	// change this value, then call ChangeHeatSetpoint().

_ENVRCM_DEF_ SIGNED_WORD Envrcm_CoolSetpoint[N_ENVIRACOM_BUSSES][N_ZONES];  // (READ/WRITE)  The current cool
	// setpoint in hundredths of deg C units for each zone.  This value is the setpoint
	// that the thermostat tries to control to regardless of whether it is running off 
	// a schedule, a temporary setpoint, or a hold setpoint.
	// Special values:
		// 7FFF = not available (READ ONLY)
	// After initialization, until it can be determined what the actual value is, the 
	// value will be 7FFFh.
	// To create a temporary setpoint change on the thermostat, the application should 
	// change this value, then call ChangeCoolSetpoint().

_ENVRCM_DEF_ UNSIGNED_BYTE Envrcm_SetpointStatus[N_ENVIRACOM_BUSSES][N_ZONES];  // (READ ONLY)  Indicates
	// whether the current setpoints are from the schedule, temporary setpoints, or hold
	// setpoints.  Values are encoded as follows:
		// 0 = Scheduled 
		// 1 = Temporary - the setpoints are used only until the next scheduled change
		// 2 = Hold - the setpoints will be used until the hold is removed 
#ifdef EXTENSIONS
		// From byte 31: 
		// 3 = Unknown (1 on Byte 31)
		// 4 = Recovery (2 on Byte 31)
#endif

	// After initialization, until it can be determined what the actual value is, the 
	// value will be 0.
	// The application should not write this value.  Instead it should call
	// ChangeHeatSetpoint(), ChangeCoolSetpoint(), HoldSetpoints(), and/or RunProgram().

UNSIGNED_BYTE ChangeHeatSetpoint(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone);  // This
	// function sends the necessary Enviracom messages to change the heat setpoint for
	// EnviracomBus/Zone to the value in 
	// Envrcm_HeatSetpoint.  Before calling this function, the application should write
	// the desired value in Envrcm_HeatSetpoint.  Note that the thermostat's schedule is 
	// not changed.  This function automatically implements the following rules:
		// If the value in Envrcm_HeatSetpoint is above or below the allowed setpoint
		//		limits, the value is clamped to the limit.
		// If the AUTO system switch position is allowed and the value in Envrcm_HeatSetpoint violates the
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
	// If either Envrcm_HeatSetpoint or Envrcm_CoolSetpoint are changed because of the
	// limits or deadband, this function returns a positive number.  Otherwise it returns
	// zero.

UNSIGNED_BYTE ChangeCoolSetpoint(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone);  // This
	// function sends the necessary Enviracom messages to change the cool setpoint for 
	// EnviracomBus/Zone to the value in 
	// Envrcm_CoolSetpoint.  Before calling this function, the application should write the
	// deisred value in ENvrcm_CoolSetpoint.  Note that the thermostat's schedule is not 
	// changed.  This 
	// function automatically implements the following rules:
		// If the value in Envrcm_CoolSetpoint is above or below the allowed setpoint
		//		limits, the value is clamped to the limit.
		// If the AUTO system switch position is allowed and the value in Envrcm_CoolSetpoint violates the
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
	// If either Envrcm_HeatSetpoint or Envrcm_CoolSetpoint are changed because of the
	// limits or deadband, this function returns a positive number.  Otherwise it returns
	// zero.

// Note:  When the application changes both the heat and the cool setpoint, because of the
// deadband, the order that ChangeHeatSetpoint() and ChangeCoolSetpoint() are called may
// be important.  If the heat setpoint is more important to the application than the cool
// setpoint, ChangeHeatSetpoint() should be called last.  Similarly, if the cool setpoint is
// more important than the heat setpoint, ChangeCoolSetpoint() should be called last.

void HoldSetpoints(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone);  // This function creates hold setpoints from the
	// current setpoints, regardless of whether the current setpoints are scheduled,
	// temporary, or hold setpoints.  If specific values of heat and/or cool hold setpoints
	// are desired, the application should first change the values in Envrcm_HeatSetpoint
	// and/or Envrcm_CoolSetpoint, then call ChangeHeatSetpoint() and/or 
	// ChangeCoolSetpoint() before calling this function.
  
void RunProgram(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone);  // Calling this function has the same effect as 
	// pressing the 'Run Program' key on the thermostat.  Any temporary or hold setpoints
	// are erased and the thermostat gets its setpoints from the schedule.  When this
	// function is called, until the thermostat reports its new setpoints, the
	// values in Envrcm_CoolSetpoint and Envrcm_HeatSetpoint will be 7FFFH.

// Note:  When the application makes back to back calls of ChangeHeatSetpoint(), 
// ChangeCoolSetpoint(), and/or HoldSetpoints() for the same zone, only one
// Enviracom message with all of the changes is sent (the Override Heat-Cool Setpoint message).
// This gives a simultaneous change of the heat setpoint, the cool setpoint, and the setpoint 
// status. 

void App_SetpointsNotify(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone);  // this function must
	// be written by the application.  It is called by this API whenever a message is received
	// on Enviracom whose value differs from what is in Envrcm_HeatSetpoint, Envrcm_CoolSetpoint,
	// and/or Envrcm_SetpointStatus.  EnviracomBus and Zone indicate which Enviracom bus and 
	// which zone has the value(s) that changed.  The variables Envrcm_HeatSetpoint, 
	// Envrcm_CoolSetpoint, and Envrcm_SetpointStatus will contain the new values from the 
	// received message.  This function can do whatever the application requires.  


// Setpoint Limits
////////////////////
_ENVRCM_DEF_ SIGNED_WORD Envrcm_HeatSetpointMinLimit[N_ENVIRACOM_BUSSES];  // (READ ONLY)
	// The lower heating setpoint limit in hundredths of deg C units.  7FFFh = not available
_ENVRCM_DEF_ SIGNED_WORD Envrcm_HeatSetpointMaxLimit[N_ENVIRACOM_BUSSES];  // (READ ONLY)
	// The upper heating setpoint limit in hundredths of deg C units.  7FFFh = not available
_ENVRCM_DEF_ SIGNED_WORD Envrcm_CoolSetpointMinLimit[N_ENVIRACOM_BUSSES];  // (READ ONLY)
	// The lower cooling setpoint limit in hundredths of deg C units.  7FFFh = not available
_ENVRCM_DEF_ SIGNED_WORD Envrcm_CoolSetpointMaxLimit[N_ENVIRACOM_BUSSES];  // (READ ONLY)
	// The upper cooling setpoint limit in hundredths of deg C units.  7FFFh = not available


// Heat/Cool Setpoint Deadband
///////////////////////////////
_ENVRCM_DEF_ SIGNED_WORD Envrcm_Deadband[N_ENVIRACOM_BUSSES];  // (READ ONLY)
	// The minimum deadband between heat and cool setpoints when the system switch is in 
	// the AUTO position.  Units are hundredths of deg C units.  7FFFh = Unknown.



// System Switch
////////////////////
_ENVRCM_DEF_ UNSIGNED_BYTE Envrcm_SystemSwitch[N_ENVIRACOM_BUSSES][N_ZONES];  // (READ/WRITE) Bit mapped as follows:
	// Bits 0-3:  The system switch position for the zone.  Encoded as follows:
		// 0 = Emergency Heat
		// 1 = Heat
		// 2 = Off
		// 3 = Cool
		// 4 = Auto
	// Bit 4:  The heat/cool mode of the thermostat.  This is useful when the system switch
		// is in the AUTO position. 0 = Heat mode, 1 = Cool mode. 
	// All other bits are unused.
	// A value of FFh in this byte = Unknown.
	// After initialization, until it can be determined what the actual value is, the
	// the value of this byte will be FFh.
	// To change the system switch position on a thermostat, the application should 
	// change this value, then call ChangeSystemSwitch().  The value in the heat/cool mode (bit
	// 4) is ignored when changing the system switch position.
	//
	// NOTE:  When a zone manager is present (see Envrcm_ZoneManager below), there is a system switch position for
	// each zone.  When a zone manager is not present, all zones will have the same system switch position, and
	// the value will be stored as zone 1.
	

_ENVRCM_DEF_ UNSIGNED_BYTE Envrcm_AllowedSystemSwitch[N_ENVIRACOM_BUSSES][N_ZONES];  // (READ ONLY)  A bit map of
	// the allowed system switch positions.  Each bit position represents a specific system 
	// switch setting.  A 1 = the system switch position is allowed.   A 0 = the system switch
	// position is not allowed.  The bit positions are assigned as follows:
		// Bit 0 = Emergency Heat
		// Bit 1 = Heat
		// Bit 2 = Off
		// Bit 3 = Cool
		// Bit 4 = Auto
		// All other bits are unused.
	// After initialization, until it can be determined what the actual value is, the
	// the value of this byte will be 0.

void ChangeSystemSwitch(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone);    // This function
	// causes an Enviracom message to
	// be sent to change the system switch position for the zone to the value
	// in Envrcm_SystemSwitch.

void App_SysSwitchNotify(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone);  // this function must
	// be written by the application.  It is called by this API whenever a message is received
	// on Enviracom whose value differs from what is in Envrcm_SystemSwitch.  EnviracomBus and 
	// Zone indicate which Enviracom bus and which zone has the value that changed.  The variable
	// Envrcm_SystemSwitch and Envrcm_AllowedSystemSwitch will contain the new values from the 
	// received message.  This function can do whatever the application requires.  



// Schedule
//////////////////////

#ifdef SCHEDULESIMPLEMENTED
// The thermostat sends changes to its schedule on Enviracom when they occur, but does not send
// it at startup or periodically.  Therefore this API queries the schedule after startup
// and periodically to provide data integrity.  Since the schedule takes many Enviracom messages
// to complete, the schedule is queried in period/day/zone sections after startup.  These
// sections are spaced several seconds apart on each Enviracom bus to avoid tying up traffic.
// Once all schedules have been obtained, the schedules are repeatedly requeried in sections, but
// spaced 5 minutes apart on each bus.  This will periodically update all the schedules from 9 
// zones slightly more often than once per day.  When fewer zones are present in the system, the
// schedules will be updated proportionally more often.
// The application can determine the schedule in each thermostat on each Enviracom bus by reading
// each of the following three variables.
// The application can change the thermostat schedules by changing the values in any or all of 
// the following three variables, and then then call ChangeSchedule() for each of the 
// bus/period/day/zones that changed.  The changing of the values and the calls to 
// ChangeSchedule() can occur in any order.

_ENVRCM_DEF_ UNSIGNED_WORD Envrcm_ScheduleStartTime[N_ENVIRACOM_BUSSES][N_ZONES][N_SCHEDULE_DAYS][N_SCHEDULE_PERIODS];
	// (READ/WRITE)  The start time of the each schedule period for each day and zone.  Encoded 
	// as minutes from midnight. Values of 0 to 1439 (0 to 59Fh).
	// Special values:
		// 07XXh = not scheduled.  The period does not occur for the day and zone.
		// 08XXh = unknown (READ ONLY)
		// 09XXh = the thermostat did not respond to the query (READ ONLY)
	// After initialization, until it can be determined what the actual value is, the
	// the value will be 0800h.

_ENVRCM_DEF_ SIGNED_WORD Envrcm_ScheduleHeatSetpoint[N_ENVIRACOM_BUSSES][N_ZONES][N_SCHEDULE_DAYS][N_SCHEDULE_PERIODS];
	// (READ/WRITE) The scheduled heat set point for the period / day / zone, in hundredths 
	// deg C units.
	// Special values:
		// 7FXXh = Not applicable (READ ONLY)
	// After initialization, until it can be determined what the actual value is, the
	// the value will be 7F00h.

_ENVRCM_DEF_ SIGNED_WORD Envrcm_ScheduleCoolSetpoint[N_ENVIRACOM_BUSSES][N_ZONES][N_SCHEDULE_DAYS][N_SCHEDULE_PERIODS];
	// (READ/WRITE) The scheduled cool set point for the period / day / zone, in hundredths 
	// deg C units.
	// Special values:
		// 7FXXh = Not applicable (READ ONLY)
	// After initialization, until it can be determined what the actual value is, the
	// the value will be 7F00h.

_ENVRCM_DEF_ UNSIGNED_BYTE Envrcm_ScheduleFanSwitch[N_ENVIRACOM_BUSSES][N_ZONES][N_SCHEDULE_DAYS][N_SCHEDULE_PERIODS];
	// (READ/WRITE) The scheduled Fan switch setting for the period / day / zone. 
		// A value of 0 =			AUTO
		// A value of 1 to 200 =	ON
		// A value of 255 =			Unknown (READ ONLY)
    // NOTE: these values are used for compatability with the main Envrcm_FanSwitch variable.
    // The values used in the messages are actually (from the spec):
    // 00 = Not scheduled or Unknown
    // 04 = AUTO
    // 0C = ON
	// After initialization, until it can be determined what the actual is, the value 
	// will = 255.  To change the fan switch position, the application should write a new 
	// value to this variable, then call ChangeFanSwitch().

// After initialization, it can take a significant period of time before all of the
// schedule(s) are obtained from the thermostat(s).  If the application needs to get
// the data for a specific period/day/zone right away, it can write to the following two 
// variables to configure a special query to send on Enviracom.
  
_ENVRCM_DEF_ UNSIGNED_BYTE Envrcm_NextSchedZone[N_ENVIRACOM_BUSSES];  // (READ/WRITE)  the zone for the
	// next special schedule query that is sent on Enviracom.
	// 0 = zone 1 or non zoned, 1 = zone 2, . . ., 8 = zone 9.
	// After initialization, the value will be undetermined.

_ENVRCM_DEF_ UNSIGNED_BYTE Envrcm_NextSchedPeriodDay[N_ENVIRACOM_BUSSES];  // (READ/WRITE)  the period and day for the
	// next special schedule query that is sent on Enviracom.
	// Encoded as a bit map as follows:
		// Bits 0-3:	Day, 0 = Mon, 1 = Tue, . . ., 6 = Sun.  All other values are not
		//		allowed.
		// Bits 4-8:	Period, 0 = Wake, 1 = Leave, 2 = Return, 3 = Sleep.  All other values 
		//		are not allowed.
	// After the special query is sent on Enviracom, the value of this variable
	// is changed to FF (hex), at which point the application can configure a new special query if 
	// desired. After initialization, the value will be FF (hex).

UNSIGNED_BYTE ChangeSchedule(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone, UNSIGNED_BYTE PeriodDay);
	// This function causes an
	// Enviracom message to be sent to change the schedule for the Period/Day/Zone/Bus to the 
	// value in Envrcm_ScheduleStartTime, Envrcm_ScheduleHeatSetpoint, and 
	// Envrcm_ScheduleCoolSetpoint.  This 
	// function automatically implements the following rules:
		// If the value in Envrcm_ScheduleHeatSetpoint is above or below the allowed setpoint
		//		limits, the value is clamped to the limit.
		// If the value in Envrcm_ScheduleCoolSetpoint is above or below the allowed setpoint
		//		limits, the value is clamped to the limit.
		// If the AUTO system switch position is allowed and the value in Envrcm_ScheduleCoolSetpoint violates the
		//		deadband between heat and cool setpoints, the value in Envrcm_ScheduleHeatSetpoint
		//		is adjusted to comply with the deadband. 
	// If either Envrcm_ScheduleHeatSetpoint or Envrcm_ScheduleCoolSetpoint
	//  are changed because of the 
	// limits or deadband, this function returns a positive number.  Otherwise it returns
	// zero.


void App_ScheduleNotify(UNSIGNED_BYTE EnviracomBus, UNSIGNED_BYTE Zone, UNSIGNED_BYTE PeriodDay);
	// this function must be written by the application.  It is called by this API whenever a 
	// message is received on Enviracom whose value differs from what is in 
	// Envrcm_ScheduleStartTime, Envrcm_ScheduleHeatSetpoint, and/or Envrcm_ScheduleCoolSetpoint.  
	// EnviracomBus, Zone, and PeriodDay indicate which Enviracom bus/zone/period/day has the 
	// value(s) that changed.  The variables Envrcm_ScheduleStartTime, 
	// Envrcm_ScheduleHeatSetpoint, and Envrcm_ScheduleCoolSetpoint will contain the new values
	// from the received message.  This function can do whatever the application requires.  
#endif



// Time
////////////////

// The application is the master clock in the system, meaning that it gets its time from some
// means other than through Enviracom, and it sends its time to all other Enviracom clocks.  The
// application writes its time to the following variables:
_ENVRCM_DEF_ UNSIGNED_BYTE Envrcm_Seconds;	// (WRITE ONLY)  Bit mapped as follows:
	// Bit 7: Daylight Savings - this bit indicates whether daylight savings is in
	//		effect or not.
	//		0 = daylight savings time is not in effect
	//		1 = daylight savings time is in effect
	// Bits 0-6: Seconds - from the top of the minute, values 0 - 59.
_ENVRCM_DEF_ UNSIGNED_BYTE Envrcm_Minutes;	// (WRITE ONLY)  Minutes - from the top of the
	//		hour, values 0 - 59.
_ENVRCM_DEF_ UNSIGNED_BYTE Envrcm_HourDay;  // (WRITE ONLY)  Bit mapped as follows:
	// Bits 0-4:  Hour - from midnight, values 0 - 23.
	// Bits 5-7:  Day of Week - encoded as follows:
	//		0 = Monday, 1 = Tuesday, . . ., 6 = Sunday
	//		7 = Unknown
// NOTE:  The time in Envrcm_Seconds, Envrcm_Minutes, and Envrcm_HourDay is the local time
// displayed on the thermostat(s).  The hour value must be adjusted for daylight savings
// time.  The Daylight Savings bit in Envrcm_Seconds is used only for display.

// NOTE:  The time in Envrcm_Seconds, Envrcm_Minutes, and Envrcm_HourDay is only a snapshot of
// time queued in an Enviracom time message to send out.  It does not require continuous
// updating by the application.

void ChangeTime(void);  // When the application calls this function, a message is queued to send
	// on all Enviracom busses to change the clock time to the value in Envrcm_Seconds, 
	// Envircm_Minutes, and Envrcm_HourDay.  The application should call this function only
	// when App_TimeNotify is called, and whenever the time jumps, for example when the daylight
	// savings time changeover occurs.

void App_TimeNotify(void);  // this function must be written by the application.  It is called by
	// this API whenever a device with an Enviracom clock needs to know what time it is.  The
	// application should write to Envrcm_Seconds, Envrcm_Minutes, and EnvrcmHourDay, and then 
	// call ChangeTime(). 


// Zone Manager
///////////////////////

_ENVRCM_DEF_ SIGNED_BYTE Envrcm_ZoneManager[N_ENVIRACOM_BUSSES];  // (READ ONLY)  Indicates
	// whether a zone manager is present for each Enviracom bus.  A negative number means a zone 
	// manager is not present.  A positive number means a zone manager is present.  A value of
	// zero means it is not known whether or not a zone manager is present. After initialization, 
	// until it can be determined what the actual value is, the
	// the value will be 0.

void App_ZoneMgrNotify(UNSIGNED_BYTE EnviracomBus);  // this function must be written by the 
	// application.  It is called by
	// this API whenever Envrcm_ZoneManager changes.



//////////////////////////////////////////////////////////////////////////////////////////////////
// API Functions
//////////////////////////////////////////////////////////////////////////////////////////////////

void NewEnvrcmRxByte(UNSIGNED_BYTE NextByte, UNSIGNED_BYTE EnviracomBus);  // The application calls
	// this function whenever a new byte has been received from an Enviracom Serial Adaptor.
	// NextByte is the byte that has been received.  EnviracomBus identifies the Enviracom bus
	// that the byte was received from. 
 
void App_EnvrcmSendMsg(UNSIGNED_BYTE EnviracomBus);  // The application
	// must supply this function.  It must take the message in TxBuffer and configure
	// so that it is sent in the background on the serial port for the Enviracom bus designated by 
	// EnviracomBus.  The first byte of the message in TxBuffer contains the number of bytes
	// in the
	// message and must not be sent to the serial port.  The bytes that follow
	// must be sent in order to the serial port.    This function must not wait for the
	// message to be sent on the serial port before returning.  The API will not call this 
	// function again or overwrite the buffer until the Enviracom Serial Adapter has ACK'ed or
	// NACK'ed the previous message or until a timeout period has expired, in which case the 
	// application can discard the previous message. The application must not write over the
	// message in TxBuffer.

// NOTE:  The application is responsible for receiving bytes on the seral port(s) and sending
// byte strings on the serial port(s).  The serial ports must be configured for 19.2 K bits per
// second, 8 data bits, no parity, 1 stop bit, full duplex.


void Envrcm1Sec(void);  // This function must be called by the application once every second.
	// This function performs all API timing, checks for and processes received Enviracom 
	// messages from the Enviracom Serial Adapter, and sends the next queued message to the 
	// Enviracom Serial Adapter when the previous message completes.

void EnvrcmInit(void);  // the application must call this function at startup before calling any 
	// of the other functions.
 

// function to determine if we're currently using Simple Zoning 
UNSIGNED_BYTE IsSimpleZoning(UNSIGNED_BYTE EnviracomBus);


// if this file has been included at least once, the above definitions/references/prototypes 
// are ignored.
#endif
