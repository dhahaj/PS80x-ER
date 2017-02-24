/************************************************************************/
/************************************************************************/
/*  main.c - Revision 5.2.1                                             */
/*	  --Modification to the EExER Build to allow for dual operation		*/
/*																		*/
/*  Created: 1/23/2013 11:51 AM											*/
/*  Modified: 2/24/2017													*/
/*  Author: DMH                                                         */
/************************************************************************/
/************************************************************************/

#ifdef EExER
#undef EExER
#define EExER 1
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/wdt.h>
#include <util/atomic.h>
#include <avr/interrupt.h>
#include <avr/portpins.h>
#include <avr/sfr_defs.h>
#include "main.h"
#include "timers.h"
#include "interrupts.h"

// Display which build is being compiled in the console
#ifdef EExER
#if (!EExER)
#pragma message("Building the standard ER firmware.")
#else
#pragma message("Building the EExER firmware.")
#endif
#else
#error "EExER is not defined! Add it to the compiler symbols."
#endif

// Initialize the doors
door_t door1 = {false, false, false, ACTIVE_HIGH, DR1_OUT};			
door_t door2 = {false, false, false, ACTIVE_HIGH, DR2_OUT};

void avr_init(void);
void init_door(const door_t *dr);
	
dipswitch_t dip_switch = {DEPENDENT, 0x00, false};

// Main Loop
int main(void)
{
	avr_init();

	//#if (!EExER)
	volatile bool dr1BtnReleased=true, dr2BtnReleased=true; // Not needed for std ER builds.
	//#endif

	/**
	**	MAIN LOOP
	**/
	for(;;)
	{

	//	#if (EExER) /* EExER  */
		if( buttonPressed(&INPUT_PIN_PORT, DR2_BUTTON) || buttonPressed(&INPUT_PIN_PORT, DR1_BUTTON) ) dependentRetract();
	//	#else /* ER */

		/**********************/
		/**	DEPENDENT MODE	**/
		/**********************/
		if(dip_switch.mode==DEPENDENT) {
			if( buttonPressed(&INPUT_PIN_PORT, DR2_BUTTON) || buttonPressed(&INPUT_PIN_PORT, DR1_BUTTON)) dependentRetract();
		}

		/**********************/
		/**	INDEPENDENT MODE **/
		/**********************/
		else {

			/**** DOOR 1 ****/
			if( buttonPressed(&INPUT_PIN_PORT, DR1_BUTTON) && !door1.is_active ) /*	Monitor the Door 1 input */
			{
				if( door1.toggle_mode && dr1BtnReleased) /* Toggle if in toggle mode && button has been released */
				{
					dr1BtnReleased = false; /* Change the previous button state off */
					door1.toggled = !door1.toggled;
					activateDoor1(door1.toggled);
				}

				else if(!door1.is_active && !door1.toggle_mode) /* Activate door if not in toggle mode */
				{
					door1.is_active = true;
					activateDoor1(true);
				}
			} else dr1BtnReleased=true;

			/**** DOOR 2 ****/
			if( buttonPressed(&INPUT_PIN_PORT, DR2_BUTTON) && !door2.is_active) /* Monitor the Door 2 input */
			{
				if(door2.toggle_mode && dr2BtnReleased) /* Toggle if in toggle mode && button has been released */
				{
					dr2BtnReleased = false; /* Change the previous button state off */
					door2.toggled = !door2.toggled;
					activateDoor2(door2.toggled);
				}
				/* Activate door if not in toggle mode */
				else if(dr2BtnReleased)
				{
					door2.is_active = true;
					activateDoor2(true);
				}
			} else dr2BtnReleased=true;
		}

		/* Check for any changed DIP settings */
		if( dip_switch.prev_values != (INPUT_PIN_PORT & INPUT_MASK) )
		readDip();
		//#endif /* EExER */
		wdt_reset();
	}
	return 0;
}

// Initializes the I/O Ports, Configures the Interrupts and Timers
void avr_init(void)
{
	OUTPUT_DDR = (BIT(DR1_OUT)|BIT(DR2_OUT)); // Initialize output port
	RELAY_DDR |= BIT(RELAY1_PIN) | BIT(RELAY2_PIN);
	DS_DDR = ( BIT(DS1_PIN) | BIT(DS2_PIN) );	// PC0 & PC1 set as outputs for DS switches

	wdt_reset();
	wdt_enable(WDTO_2S); // Enable Watchdog Timer @ 2 second time-out

	sbi(PCICR, PCIE2); // Enable Pin Change Interrupt 2
	PCMSK2 = BIT(PCINT16)|BIT(PCINT17)|BIT(PCINT20)|BIT(PCINT23)|BIT(PCINT22);

	TCCR0A |= BIT(CS01)|BIT(CS00); // Initialize timer0 with a prescaler of 64
	sbi(TIMSK0, TOIE0); 	// enable timer 0 overflow interrupt

	TCCR1B |= (1 << WGM12 ); // Configure timer 1 for CTC mode
	TIMSK1 |= BIT(OCIE1A); // Enable Output Compare Interrupt Channel A

	sei(); // Turn on interrupts

	OCR1A = 1562; // Set CTC compare value to 0.2Hz at 1 MHz AVR clock , with a prescaler of 64
	TCCR1B |= ((1 << CS10 ) | (1 << CS11 )); // Start timer at F_CPU /64

//	#if (!EExER)
	readDip();
	//#endif // EExER
	init_door(&door1);
	init_door(&door2);
}

/**
* Initializes the door outputs
*
* \param Door Provides a reference to the door
*/
void init_door(const door_t *dr)
{
	bit_write(!(*dr).active_high, OUTPUT_PORT, BIT((*dr).pin));
}

/**
* \brief Sets or clears a doors' output
*
* \param door The Door to act upon
* \param activate The value which should be written to the port output
*
* \return void
*/
void doorPinWrite(const door_t *door, bool activate)
{
	uint8_t mode, outputMode;
	outputMode = (*door).active_high; // Fetch the configured output mode of the door (Active High/Low)
	mode = (activate) ? bit_get(outputMode, 0) : !bit_get(outputMode, 0); // Compare the configured value and passed parameter to obtain a masked port value
	bit_write(mode, OUTPUT_PORT, BIT((*door).pin)); // write the masked port value to the outputs
}

//Read the DIP switches from the Input Port. Note EExER builds has no need for this method currently.
void readDip(void)
{
	dip_switch.prev_values = INPUT_PIN_PORT & INPUT_MASK; // Read the port
	dip_switch.mode = bit_is_set(dip_switch.prev_values, MODE_PIN) ? DEPENDENT : INDEPENDENT;
	door1.toggle_mode = bit_is_set(dip_switch.prev_values, TOGGLE1_PIN) ? false : true;
	door2.toggle_mode = bit_is_set(dip_switch.prev_values,TOGGLE2_PIN) ? false : true;
	door1.active_high = (bit_get(INPUT_PIN_PORT, ACTIVE_MODE1_PIN)) ? ACTIVE_HIGH : ACTIVE_LOW;
	door2.active_high = (bit_get(INPUT_PIN_PORT, ACTIVE_MODE2_PIN)) ? ACTIVE_HIGH : ACTIVE_LOW;
}

/**
* \brief Method for door 1 independent operation
*
* \param activate: Should Door 1 be activated? If so, we activate the door output for the amount of time set by the timing selection switch, and also monitor
*	the other door input in the case that it was pressed while we were inside this timing loop.
*/
void activateDoor1(boolean activate)
{
	if(!activate) /* Turn door 1 output OFF */
	{
		doorPinWrite(&door1, false);
		cbi(RELAY_PORT, RELAY1_PIN);
		} else /* Turn door 1 output ON */
		{
			doorPinWrite(&door1, true);
			bool dr2BtnPressed = false;
			bool btn_toggled = bit_is_clear(INPUT_PIN_PORT, DR2_BUTTON); /* This variable is used to avoid toggling the door2 output if the button is being held */

			door_timer(true); /* reset the timer */

			/* Monitor the door2 input while we wait for the signal delay to expire */
			while( (door_timer(false) ) <= SIG_DELAY)
			{
				if( !door2.is_active && !dr2BtnPressed && bit_is_clear(INPUT_PIN_PORT, DR2_BUTTON) )
				dr2BtnPressed = true;
				wdt_reset();
			}

			sbi(RELAY_PORT, RELAY1_PIN); /* Activate relay 1 */

			if(dr2BtnPressed && !btn_toggled) /* Activate Door 2 if button was pressed and isn't active */
			{
				if(door2.toggle_mode && !door2.toggled) /* See if door 2 toggle mode is selected */ {
					bool b = !doorActive(&door2); /* Read the pin state */
					activateDoor2(b);
					door2.toggled = b;
					} else if(!door2.toggle_mode) {
					bool active = door2.is_active;
					if(!active)
					activateDoor2(door2.is_active);
				}
			}
		}
		wdt_reset();
	}

/**
* \brief Method for door 2 independent operation
*
* \param activate Should Door 2 be activated? If so, we activate the door output for the amount of time set by the timing selection switch, and also monitor
*	the other door input in the case that it was pressed while we were inside this timing loop.
*/
void activateDoor2(boolean activate)
{
	/* Turn OFF door 2 output */
	if(activate==false) {
		doorPinWrite(&door2, false);
		cbi(RELAY_PORT, RELAY2_PIN);
	}
	/* Turn ON door 2 output */
	else {
		doorPinWrite(&door2, true);
		bool btn_pressed = false;

		/* This variable is used to avoid toggling door1 output if the button is being held */
		bool btn_toggled = bit_is_clear(INPUT_PIN_PORT, DR1_BUTTON);

		/* reset the timer */
		door_timer(true);

		/* Monitor input 1 while waiting for the signal delay to expire */
		while((door_timer(false)) <= SIG_DELAY) {
			if(bit_is_clear(INPUT_PIN_PORT, DR1_BUTTON) && !door1.is_active && !btn_pressed) {
				btn_pressed = true;
			}
			wdt_reset();
		}

		/* Activate relay 2 */
		sbi(RELAY_PORT,RELAY2_PIN);

		/* Activate door 2 if the button was pressed and isn't active */
		if(btn_pressed && !btn_toggled)
		{
			if(door1.toggle_mode && !door1.toggled)
			{
				bool active = !doorActive(&door1);
				activateDoor1(active);
				door1.toggled = active;
			}

			/* Not in toggle mode */
			else if(!door1.toggle_mode)
			{
				bool active = !door1.is_active;
				door1.is_active = active;
				activateDoor1(active);
			}
		}
	}
	wdt_reset(); // Reset the watchdog timer
}

/**
* Method for handling dependent operation
*/
void dependentRetract(void)
{
	/**	In Toggle Mode **/
	if(door1.toggle_mode)
	{
		if(door1.toggled) // Outputs currently toggled, so toggle them off
		{
			doorPinWrite(&door1,false); // Door 1 off
			doorPinWrite(&door2,false); // Door 2 off
			cbi(RELAY_PORT, RELAY1_PIN); // Turn off door signal outputs
			cbi(RELAY_PORT, RELAY2_PIN);
			door1.toggled = false;
			door2.toggled = false;
			} else /* Doors not toggled, toggle them now */ {
			doorPinWrite(&door1, true); // Door 1 Active
			door1.toggled = true;
			_delay_ms(DIFF_DELAY); // Differential Delay
			//wdt_reset(); // Reset the watchdog timer
			doorPinWrite(&door2, true); // Door 2 Active
			door2.toggled = true;
			_delay_ms(SIG_DELAY);	// Door Signal Delay
			sbi(RELAY_PORT, RELAY1_PIN);	// Turn on relays now
			sbi(RELAY_PORT, RELAY2_PIN);
		}

		// Maintain while buttons are held
		while(bit_is_clear(INPUT_PIN_PORT, DR1_BUTTON) || bit_is_clear(INPUT_PIN_PORT, DR2_BUTTON))
		{
			wdt_reset();
		}
		} else /** Not in Toggle Mode **/ {
		dip_switch.retracting = true;

		#if (!EExER)
		doorPinWrite(&door1, true); // Door 1 Active
		_delay_ms(DIFF_DELAY); // Differential Delay
		doorPinWrite(&door2, true); // Door 2 Active
		_delay_ms(SIG_DELAY);	// Door Signal Delay
		sbi(RELAY_PORT, RELAY1_PIN);	// Activate door signal outputs
		sbi(RELAY_PORT, RELAY2_PIN);
		#else
		sbi(RELAY_PORT, RELAY1_PIN);	// Activate door signal outputs
		_delay_ms(EExER_DELAY);
		doorPinWrite(&door1, true); // Door 1 Active
		_delay_ms(DIFF_DELAY); // Differential Delay
		doorPinWrite(&door2, true); // Door 2 Active
		_delay_ms(SIG_DELAY);	// Door Signal Delay
		sbi(RELAY_PORT, RELAY2_PIN);
		#endif
		door_timer(true); // Reset the Door timer
		unsigned long t;
		while( door_timer(false) <= ((t=getTime(DR1_OUT))*1000) ) // Hold Time Delay, value set from DS1
		{
			wdt_reset(); // Reset the watchdog timer
			if( door1.toggle_mode==true )
			break;
			if( bit_is_clear(INPUT_PIN_PORT, DR1_BUTTON) || bit_is_clear(INPUT_PIN_PORT, DR2_BUTTON) )	// Maintain active outputs when button is held
			door_timer(true); // reset the door timer
			if(dip_switch.mode==INDEPENDENT || door1.toggle_mode==true) /* If output or toggle mode changed, break loop */
			break;
		}
		doorPinWrite(&door1, false); // Door outputs off
		doorPinWrite(&door2, false);
		cbi(RELAY_PORT, RELAY1_PIN);	// Deactivate door signal outputs
		cbi(RELAY_PORT, RELAY2_PIN);
		dip_switch.retracting = false;
	}
}

/**
* \brief Checks and debounces the door input buttons
*
* \param PIN The pin to check. Input pins are pullup enabled.
* \param BUTTON_BIT A port mask to isolate the pin to be checked.
*
* \return bool True if button pressed, false otherwise.
*/
bool buttonPressed(volatile ui8 *pin, ui8 buttonBit)
{
	if ( bit_is_clear(*pin, buttonBit) )
	{
		_delay_ms(DEBOUNCE_TIME);
		if (bit_is_clear(*pin, buttonBit) ) 
			return true;
	}
	return false;
}

/**
* \brief Reads a door port value and compares it with the door output mode
*
* \param door The door to reference
*
* \return bool Returns the boolean comparison of the doors' configured settings and the actual port value.
*/
bool doorActive(const door_t *door)
{
	// Read the port
	uint8_t portValue = 0x01 & (DOOR_PIN_PORT>>(*door).pin); /* Read the state of output pin */
	return (portValue == (*door).active_high);
}
