#pragma once
//debugger mode of operation
#define OPMODE int
#define OPMODE_NONE (OPMODE)0
#define OPMODE_TIMINGS (OPMODE)1	//time in method (will set entry + exit BP) + hitcount
#define OPMODE_FIELDS (OPMODE)2		//field dump (will set entry BP)
#define OPMODE_STATS (OPMODE)4		//newobj/newarr/box/unbox are monitored to get stats about object allocation and boxing
