/*****************************************************************************
* lab2b.h for lab2b of ECE 153a at UCSB
* Date of the Last Update:  October 23,2014
*****************************************************************************/

#ifndef lab2b_h
#define lab2b_h

enum lab2bSignals {
	ENCODER_UP = Q_USER_SIG,
	ENCODER_DOWN,
	ENCODER_CLICK,
	BUT_1,
	BUT_2,
	BUT_3,
	BUT_4,
	BUT_5,
	WAITED
};


extern struct Lab2BTag AO_Lab2B;


void Lab2B_ctor(void);


#endif
