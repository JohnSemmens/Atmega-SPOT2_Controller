/*
SPOT GPS Gen 2 Controller
This controller is designed to activate the SPT GPS Gen 2 at predetermined intervals.
It is intended to operate for many months on battery power.

It uses a state machine to control transitions into different states.

Alternate between pressing the ok button and the custom message button.
This is intended to improve overall reliability. on the basis that if one message function fails, may be the other one will work.

Power is minimized by using a sleep state, and using an 8 second watch dog timer.

Wiring Details
--------------             ------------------------------
| Arduino     |           |      SPOT GPS Gen 2          |
| Nano        |           |                              |
|  D9  Pin xx |----47kR-->|  Power Switch                |
|             |           |                              |
|  D12 Pin xx |<---47kR---| 3Vdc Switched (power sense)  |
|             |           |  TP(unmarked)                |
|             |           |                              |
|  D10 Pin xx |----47kR-->| Ok Message Switch            |
|             |           |                              |
|  D11 Pin xx |----47kR-->| Custom Message Switch        |
|             |           |                              |
|             |           |                              |
|             |           |                              |
|         Gnd |-----------| Gnd                          |
--------------             ------------------------------

V1.0 16/8/2015 John Semmens Initial Version
V1.1 30/8/2015 Near final release.  Adjusting timing constants.
V1.2 3/9/2015 timing adjustment for 4 hours.
V1.3 27/9/2017 timing adjustment for 2 hours.

*/

#include <avr/sleep.h>
#include <avr/wdt.h>

const byte LED = 13;
int counter8s; // global variable as counter.

const byte SpotOnOffButton = 9;
const byte SpotOn3VSense = 12;
const byte SpotOkMsgButton = 10;
const byte SpotCustomMsgButton = 11;

const int kSpotOffTime = 60; // 1720; // x 8 seconds // 830 = 2 hours
							   // 1740 = 4 hours + 3 min
							   // 1720 = 4 hours-3minutes  --- this must be wrong... it must be about 4 hours. 8am 4/9/2015
								// 420 & 38 = 1 hour 7 minutes --- May 2017
								// may be 368 is one hour ???
								//  try 820 = 2 hours 8 minutes -- Sept 2017
								// try 760 = 2hours = 1 hr 56 min - 2018
								// 100 = 21 minute period
								// 80 = 18 to 19 min
								// 60 = 15 min -- 13/4/2019

const int kSpotOnTime = 38;    // x 8 seconds // 38 = about 5 min

enum SpotGPSState_Type
{
	Power_Off,
	Power_On,
	Sending_Ok,
	Power_Off_2,
	Power_On_2,
	Sending_Custom
};

SpotGPSState_Type SpotGPSState;
int q8SecondBlocks;

// watchdog interrupt
ISR(WDT_vect)
{
	wdt_disable();  // disable watchdog
}

void setup() {
	counter8s = 0;
	SpotGPSState = Power_Off;
	q8SecondBlocks = 1;
}

void loop()
{
	EightSeconds();

	// disable ADC
	ADCSRA = 0;

	// clear various "reset" flags
	MCUSR = 0;
	// allow changes, disable reset
	WDTCSR = bit(WDCE) | bit(WDE);
	// set interrupt mode and an interval
	WDTCSR = bit(WDIE) | bit(WDP3) | bit(WDP0);    // set WDIE, and 8 seconds delay
	wdt_reset();  // pat the dog

	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	noInterrupts();           // timed sequence follows
	sleep_enable();

	// turn off brown-out enable in software
	MCUCR = bit(BODS) | bit(BODSE);
	MCUCR = bit(BODS);
	interrupts();             // guarantees next instruction executed
	sleep_cpu();

	// cancel sleep as a precaution
	sleep_disable();
}

void EightSeconds()
{
	// We arrive here every 8 seconds approx
	// Perform the "action()" at intervals of aprroximately 8 seconds x k8SecondBlocks
	// Typical values:
	//          7 = 1 minute
	//        106 = 15 minutes
	//        424 = 1 hour
	//        833 = 2 hours
	//        1000 = 2 hours 22 minutes (measured)
	//       1700 = 4 hours

	counter8s++;
	if (counter8s >= q8SecondBlocks)
	{
		counter8s = 0;
		action();
	}
}

void action() {
	// Process the state machine

	switch (SpotGPSState)
	{
	case Power_Off:
		Spot_Turn_On();
		SpotGPSState = Power_On;
		q8SecondBlocks = 1; // "on" time before moving to next state, about 8 sec
		break;

	case Power_On:
		Spot_Message_OK();
		SpotGPSState = Sending_Ok;
		q8SecondBlocks = kSpotOnTime; // "send ok message" time before turning off. 38 = about 5 min
		break;

	case Sending_Ok:
		Spot_Turn_Off();
		SpotGPSState = Power_Off_2;
		q8SecondBlocks = kSpotOffTime; // "off time" before turning on again. about 1000 = 2.2 hours
		break;

	case Power_Off_2:
		Spot_Turn_On();
		SpotGPSState = Power_On_2;
		q8SecondBlocks = 1; // "on" time before moving to next state, about 8 sec
		break;

	case Power_On_2:
		Spot_Message_Custom();
		SpotGPSState = Sending_Custom;
		q8SecondBlocks = kSpotOnTime; // "send Custom message" time before turning off. 38 = about 5 min
		break;

	case Sending_Custom:
		Spot_Turn_Off();
		SpotGPSState = Power_Off;
		q8SecondBlocks = kSpotOffTime; // "off time" before turning on again. about 1000 = 2.2 hours
		break;

	default: // should not get here.
		Spot_Turn_Off();
		SpotGPSState = Power_Off;
		q8SecondBlocks = kSpotOffTime; // "off time" before turning on again. about 1000 = 2.2 hours
	}
}

void Spot_Turn_On()
{
	// Procedure to simulate pressing the power on button on the spot - to turn it on.
	// it waits for the feedback from the Spot device in the form of a switched 3Volt signal.

	// check if its off by looking for 3V sense signal low
	pinMode(SpotOn3VSense, INPUT);
	if (digitalRead(SpotOn3VSense) == LOW)
	{
		// press the on/off button.
		pinMode(SpotOnOffButton, OUTPUT);
		digitalWrite(SpotOnOffButton, LOW);

		// turn the led on
		pinMode(LED, OUTPUT);
		digitalWrite(LED, HIGH);

		// wait for power sense 3V signal
		pinMode(SpotOn3VSense, INPUT);
		while (digitalRead(SpotOn3VSense) == LOW)
		{
			delay(100);
			wdt_reset();
		}

		// release on/off button.
		digitalWrite(SpotOnOffButton, HIGH);
		pinMode(SpotOnOffButton, INPUT);

		// turn led off
		digitalWrite(LED, LOW);
		pinMode(LED, INPUT);
	}
}

void Spot_Turn_Off()
{
	// Procedure to simulate pressing the power on button on the spot - to turn it off
	// it waits for the feedback from the Spot device in the form of a switched 3Volt signal.

	// check if its on by looking for 3V sense signal
	pinMode(SpotOn3VSense, INPUT);
	if (digitalRead(SpotOn3VSense) == HIGH)
	{
		// only turn it off if its on already

		// press the on/off button.
		pinMode(SpotOnOffButton, OUTPUT);
		digitalWrite(SpotOnOffButton, LOW);

		// LED on
		pinMode(LED, OUTPUT);
		digitalWrite(LED, HIGH);

		// wait for power sense 3V to drop to zero
		pinMode(SpotOn3VSense, INPUT);
		while (digitalRead(SpotOn3VSense) == HIGH)
		{
			delay(100);
			wdt_reset();
		}

		// release on/off button.
		digitalWrite(SpotOnOffButton, HIGH);
		pinMode(SpotOnOffButton, INPUT);

		// turn led off
		digitalWrite(LED, LOW);
		pinMode(LED, INPUT);
	}
}

void Spot_Message_OK()
{
	// Procedure to simulate pressing the "I'm OK" button on the Spot Device.
	// It uses a simple 4 second delay before releasing.

	// press the OK Message button.
	pinMode(SpotOkMsgButton, OUTPUT);
	digitalWrite(SpotOkMsgButton, LOW);

	// LED on
	pinMode(LED, OUTPUT);
	digitalWrite(LED, HIGH);

	// hold down OK button for 4 seconds
	delay(4000);

	// release OK Message button.
	digitalWrite(SpotOkMsgButton, HIGH);
	pinMode(SpotOkMsgButton, INPUT);

	// turn led off
	digitalWrite(LED, LOW);
	pinMode(LED, INPUT);
}


void Spot_Message_Custom()
{
	// Procedure to simulate pressing the "Custom Message" button on the Spot Device.
	// It uses a simple 4 second delay before releasing.

	// press the "Custom" Message button.
	pinMode(SpotCustomMsgButton, OUTPUT);
	digitalWrite(SpotCustomMsgButton, LOW);

	// LED on
	pinMode(LED, OUTPUT);
	digitalWrite(LED, HIGH);

	// hold down OK button for 4 seconds
	delay(4000);

	// release "Custom" Message button.
	digitalWrite(SpotCustomMsgButton, HIGH);
	pinMode(SpotCustomMsgButton, INPUT);

	// turn led off
	digitalWrite(LED, LOW);
	pinMode(LED, INPUT);
}

