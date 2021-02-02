#! /usr/pkg/bin/perl -p

%source = ( "3EE0", "ZoneMngr",
	   "3E70", "HeatPump",
	   "313F", "TimeRequ",
	   "3110", "Unknown1",
	   "3100", "EqpFault",
	   "2330", "SetPoint",
	   "22D0", "SysSwitc",
	   "22C0", "FanSwitc",
	   "12C0", "RoomTemp",
	   "12A0", "Humidity",
	   "1290", "OutdTemp",
	   "1210", "UnknownA",
	   "120F", "UnknownB",
	   "11C0", "Unknown2",
	   "11B0", "Unknown3",
	   "11A0", "Unknown4",
	   "1180", "Unknown5",
	   "1170", "Unknown6",
	   "1150", "Unknown7",
	   "10F0", "Unknown8",
	   "10D0", "AirFiltr",
	   "1040", "SetPoint",
	   "0A03", "Unknown9",
	   "0A01", "DeadBand",
	   "1F80", "Schedule"
);

%type = ( "R", "Reply ",
	  "Q", "Query ",
	  "C", "Change"
	  );


s/^([\d\.]+\s)(\w)\s+(\w+)\s(\d+)\s(\w)/$1 $2 $source{$3} Zone: $4 $type{$5}/g;


