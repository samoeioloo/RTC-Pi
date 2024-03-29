/*
 * BinClock.c
 * Jarrod Olivier
 * Modified by Keegan Crankshaw
 * Further Modified By: Mark Njoroge
 *
 *
 * OLXSAM001 MSNSFI001
 * 21/08/2021
*/

#include <signal.h> //for catching signals
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <stdio.h> //For printf functions
#include <stdlib.h> // For system functions

#include "BinClock.h"
#include "CurrentTime.h"



//Global variables
int hours, mins, secs;
int toggle=0; // used for toggling LED
long lastInterruptTime = 0; //Used for button debounce
int RTC; //Holds the RTC instance

int HH,MM,SS;


// Clean up function to avoid damaging used pins
void CleanUp(int sig){
	printf("Cleaning up\n");

	//Set LED to low then input mode
	digitalWrite(LED, LOW);
	//Logic here
	pinMode(LED, INPUT);

	for (int j=0; j < sizeof(BTNS)/sizeof(BTNS[0]); j++) {
		pinMode(BTNS[j],INPUT);
	}

	exit(0);

}

void initGPIO(void){
	/*
	 * Sets GPIO using wiringPi pins. see pinout.xyz for specific wiringPi pins
	 * You can also use "gpio readall" in the command line to get the pins
	 * Note: wiringPi does not use GPIO or board pin numbers (unless specifically set to that mode)
	 */
	printf("Setting up\n");
	wiringPiSetup(); //This is the default mode. If you want to change pinouts, be aware


	RTC = wiringPiI2CSetup(RTCAddr); //Set up the RTC

	//Set up the LED, sets pin 3 to OUTPUT
	pinMode(LED, OUTPUT);
	toggle = 0;
	pinMode(3, OUTPUT);
	digitalWrite(LED, LOW);

	printf("LED and RTC done\n");

	//Set up the Buttons
	for(int j=0; j < sizeof(BTNS)/sizeof(BTNS[0]); j++){
		pinMode(BTNS[j], INPUT);
		pullUpDnControl(BTNS[j], PUD_UP);
	}

	//Attach interrupts to Buttons
	wiringPiISR(BTNS[0], INT_EDGE_RISING, hourInc);
	wiringPiISR(BTNS[1], INT_EDGE_RISING, minInc);




	printf("BTNS done\n");
	printf("Setup done\n");
}

int main(void){
	signal(SIGINT,CleanUp);
	initGPIO();

	//Set random time (3:04PM)
	//You can comment this file out later
	//wiringPiI2CWriteReg8(RTC, HOUR_REGISTER, 0x13+TIMEZONE); // write hour
	//wiringPiI2CWriteReg8(RTC, MIN_REGISTER, 0x4);
	//wiringPiI2CWriteReg8(RTC, SEC_REGISTER, 0x00);

	//int led_status = 0; // off initially
	toggleTime();
	// initialise hour, min and seconds values
	// Repeat this until we shut down
	for (;;){

		//Toggle Seconds LED
		toggle = ~toggle;
		//printf("Toggle: %d\n", toggle);

		digitalWrite(3, toggle);
		//Fetch the time from the RTC
		int hour_val = wiringPiI2CReadReg8(RTC, HOUR_REGISTER); // read hour
		int min_val = wiringPiI2CReadReg8(RTC, MIN_REGISTER); // needs to be converted
		int sec_val = wiringPiI2CReadReg8(RTC, SEC_REGISTER);

		// convert returned hex values to dec
		hours = hexCompensation(hour_val);
		mins = hexCompensation(min_val);
		secs = hexCompensation(sec_val);
		// Print out the time we have stored on our RTC

		printf("The current time is: %d:%d:%d\n", hours, mins, secs);

		// Toggle LED
		if (toggle==0) {
			//printf("LED off, writing it to high\n");
			digitalWrite(LED, HIGH);
			//toggle = 1;
		}
		else if(toggle==-1)
		{
			digitalWrite(LED, LOW);
			//printf("LED switched off");
			//toggle = 0;

		}
		//printf("Button 1 state : %d\n", digitalRead(BTNS[0]));
		//printf("Button 2 state : %d\n", digitalRead(BTNS[1]));
		/**

			}
		}*/
		//using a delay to make our program "less CPU hungry"
		delay(1000); //milliseconds

		//if (digitalRead(BTNS[1]) == HIGH){
		//	printf("Button 2 Pressed.\n");
		//}

	}
	return 0;
}

/*
 * Changes the hour format to 12 hours
 */
int hFormat(int hours){
	/*formats to 12h*/
	if (hours >= 24){
		hours = 0;
	}
	else if (hours > 12){
		hours -= 12;
	}
	return (int)hours;
}


/*
 * hexCompensation
 * This function may not be necessary if you use bit-shifting rather than decimal checking for writing out time values
 * Convert HEX or BCD value to DEC where 0x45 == 0d45
 */
int hexCompensation(int units){

	int unitsU = units%0x10;

	if (units >= 0x50){
		units = 50 + unitsU;
	}
	else if (units >= 0x40){
		units = 40 + unitsU;
	}
	else if (units >= 0x30){
		units = 30 + unitsU;
	}
	else if (units >= 0x20){
		units = 20 + unitsU;
	}
	else if (units >= 0x10){
		units = 10 + unitsU;
	}
	return units;
}


/*
 * decCompensation
 * This function "undoes" hexCompensation in order to write the correct base 16 value through I2C
 */
int decCompensation(int units){
	int unitsU = units%10;

	if (units >= 50){
		units = 0x50 + unitsU;
	}
	else if (units >= 40){
		units = 0x40 + unitsU;
	}
	else if (units >= 30){
		units = 0x30 + unitsU;
	}
	else if (units >= 20){
		units = 0x20 + unitsU;
	}
	else if (units >= 10){
		units = 0x10 + unitsU;
	}
	return units;
}


/*
 * hourInc
 * Fetch the hour value off the RTC, increase it by 1, and write back
 * Be sure to cater for there only being 23 hours in a day
 * Software Debouncing should be used
 */
void hourInc(void){
	//Debounce
	long interruptTime = millis();

	if (interruptTime - lastInterruptTime>200){
		printf("Interrupt 1 triggered, %x\n", hours);
		//Fetch RTC Time
		hours = hexCompensation(wiringPiI2CReadReg8(RTC, HOUR_REGISTER));		//Increase minutes by 1, ensuring not to overflow
		printf("Hours: %d\n", hours);
		//Increase hours by 1, ensuring not to overflow
		//Write hours back to the RTC
		if(hours<=23)
		{
			hours+=1;
		}
		else
		{
			printf("midnight");
			hours=0;
		}
		//Write minutes back to the RTC
		wiringPiI2CWriteReg8(RTC, HOUR_REGISTER, decCompensation(hours));
	}
	lastInterruptTime = interruptTime;
}

/*
 * minInc
 * Fetch the minute value off the RTC, increase it by 1, and write back
 * Be sure to cater for there only being 60 minutes in an hour
 * Software Debouncing should be used
 */
void minInc(void){
	long interruptTime = millis();

	if (interruptTime - lastInterruptTime>200){
		printf("Interrupt 2 triggered, %x\n", mins);
		//Fetch RTC Time
		mins = hexCompensation(wiringPiI2CReadReg8(RTC, MIN_REGISTER));
		printf("mins: %d\n", mins);
		//Increase minutes by 1, ensuring not to overflow
		if(mins<=59)
		{
			mins+=1;
		}
		else
		{
			hours +=1;
			mins=0;
		}
		//Write minutes back to the RTC
		wiringPiI2CWriteReg8(RTC, MIN_REGISTER, decCompensation(mins));
		//minutes = ;
	}
	lastInterruptTime = interruptTime;
}

//This interrupt will fetch current time from another script and write it to the clock registers
//This functions will toggle a flag that is checked in main
void toggleTime(void){
	long interruptTime = millis();

	if (interruptTime - lastInterruptTime>200){
		HH = getHours();
		MM = getMins();
		SS = getSecs();

		HH = hFormat(HH);
		HH = decCompensation(HH);
		wiringPiI2CWriteReg8(RTC, HOUR_REGISTER, HH);

		MM = decCompensation(MM);
		wiringPiI2CWriteReg8(RTC, MIN_REGISTER, MM);

		SS = decCompensation(SS);
		wiringPiI2CWriteReg8(RTC, SEC_REGISTER, 0b10000000+SS);

	}
	lastInterruptTime = interruptTime;
}
