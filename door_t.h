/*
* door.h -> Door structure type definitions
*
* Created: 7/2/2014
* Modified: 2/24/2017
* Author: DMH
*
*/

#ifndef DOOR_H_
#define DOOR_H_

//#include <stdint.h>
//#include <stdbool.h>

typedef struct {
	volatile bool toggle_mode;
	volatile bool toggled;
	volatile bool is_active;
	volatile uint8_t active_high;
	const uint8_t pin;
	const uint8_t port;
} door_t;

#endif /* DOOR_H_ */