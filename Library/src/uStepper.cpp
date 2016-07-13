/********************************************************************************************
* 	 	File: 		uStepper.cpp 															*
*		Version:    0.3.0                                             						*
*      	date: 		May 27th, 2016	                                    					*
*      	Author: 	Thomas Hørring Olsen                                   					*
*                                                   										*	
*********************************************************************************************
*			            uStepper class 					   									*
* 																							*
*	This file contains the implementation of the class methods, incorporated in the  		*
*	uStepper arduino library. The library is used by instantiating an uStepper object 		*
*	by calling either of the two overloaded constructors: 									*
*																							*
*		example:																			*
*																							*
*		uStepper stepper; 																	*
*																							*
*		OR 																					*
*																							*
*		uStepper stepper(500, 2000);														*
*																							*
*	The first instantiation above creates a uStepper object with default acceleration 		*
*	and maximum speed (1000 steps/s^2 and 1000steps/s respectively).						*
*	The second instantiation overwrites the default settings of acceleration and 			*
*	maximum speed (in this case 500 steps/s^2 and 2000 steps/s, respectively);				*
*																							*
*	after instantiation of the object, the object setup function should be called within 	*
*	arduino's setup function:																*
*																							*
*		example:																			*
*																							*
*		uStepper stepper;																	*
*																							*
*		void setup()																		*
*		{																					*
*			stepper.setup();																*
*		} 																					*
*																							*
*		void loop()																			*
*		{																					*
*																							*
*		}																					*
*																							*
*	After this, the library is ready to control the motor!									*
*																							*
*********************************************************************************************
*								TO DO:														*
*	- Implement Doxygen comments															*
*	- Review comments																		*
*	- Implement a function to read the actual speed of the motor, using the encoder			*
*																							*
*********************************************************************************************
*	(C) 2016																				*
*																							*
*	ON Development IVS																		*
*	www.on-development.com 																	*
*	administration@on-development.com 														*
*																							*
*	The code contained in this file is released under the following open source license:	*
*																							*
*			Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International			*
* 																							*
* 	The code in this file is provided without warranty of any kind - use at own risk!		*
* 	neither ON Development IVS nor the author, can be held responsible for any damage		*
* 	caused by the use of the code contained in this file ! 									*
*                                                                                           *
********************************************************************************************/
/**	\file uStepper.cpp
*	\brief Class implementations for the uStepper library
*	
*	This file contains the implementations of the classes defined in uStepper.h
*	
*	\author Thomas Hørring Olsen (thomas@ustepper.com)
*/
#include <uStepper.h>
#include <Arduino.h>
#include <Wire.h>
#include <util/delay.h>
#include <math.h>

uStepper *pointer;
volatile uint8_t abe = 0;

volatile int32_t stepCnt = 0, control = 0;

i2cMaster I2C;

extern "C" {
	void INT0_vect(void)
	{
		if((PIND & (0x08)))			//CCW
		{
			if(control == 0.0)
			{
				PORTD |= (1 << 7);
			}			
			stepCnt--;
		}
		else						//CW
		{
			if(control == 0.0)
			{
				PORTD &= ~(1 << 7);
			}

			stepCnt++;	
		}

		if(control == 0.0)
		{
			PORTD |= (1 << 4);
			delayMicroseconds(2);
			PORTD &= ~(1 << 4);	
		}

	}

	void TIMER2_COMPA_vect(void)
	{
		//asm volatile("jmp _AccelerationAlgorithm \n\t");	//Execute the acceleration profile algorithm
		static uint8_t i = 0;

		if(i < 2)
		{
			i++;
			return;
		}

		if(control != 0.0)
		{
			PORTD |= (1 << 4);
			delayMicroseconds(2);
			PORTD &= ~(1 << 4);	
			if(control < 0.0)
			{
				control += 1.0;
			}

			else if(control > 0.0)
			{
				control -= 1.0;
			}
			i = 0;
		}
	}
	//Den her ISR ødelægger getangle læsning
	//hvorfor skal vi have loops som en float? det er jo bare et heltal...

	void TIMER1_COMPA_vect(void)
	{
		float angle, error;
		uint8_t data[2], ii=0;
		float deltaAngle, avgSpeed;
		uint16_t i=0;
		static float oldAngle = 0.0, offsetAngle = 0.0, loops = 0.0;
		static float tmpSpeed[16];
		
		sei();

		if(I2C.getStatus() != I2CFREE)
		{
			return;
		}

		//read angle
		I2C.read(ENCODERADDR, ANGLE, 2, data);
		pointer->encoder.currentAngle = (float)((((uint16_t)data[0]) << 8 )| (uint16_t)data[1])*0.087890625;

		offsetAngle = fmod(pointer->encoder.currentAngle - pointer->encoder.encoderOffset + 360.0, 360.0);//hvorfor fmod på det her?
		
		//offset angle according to home position
		deltaAngle = (oldAngle - offsetAngle);
	
		//count number of loops
		if(deltaAngle < -180.0)
		{
			loops -=1.0;
		}
		else if(deltaAngle > 180.0)
		{
			loops +=1.0;
		}

		pointer->encoder.angleMoved =offsetAngle+(360.0*loops);
		oldAngle = offsetAngle;
		cli();
		error = stepCnt;
		sei();
		if((error = (0.1125*error) - pointer->encoder.angleMoved) > 1.0)
		{
			control = error/0.1125;
			//control = 0.0;
			PORTD &= ~(1 << 7);
			pointer->startTimer();	
		}
		else if(error < -1.0)
		{
			control = error/0.1125;
			//control = 0.0;
			PORTD |= (1 << 7);
			pointer->startTimer();	
		}
		else
		{
			control = 0.0;
			pointer->stopTimer();	
		}
			
/*
		tmpSpeed[i] = deltaAngle*ENCODERSPEEDCONSTANT;
		avgSpeed=0.0;
		for(ii=0;ii<15;ii++)
		{
			avgSpeed +=tmpSpeed[ii];
		}
		pointer->encoder.curSpeed = avgSpeed/15.0;
		i+=1;
		if(i>14)
		{
			i=0;
		}*/
/*
			if(loops >= 50000.0)
			{
				pointer->encoder.curSpeed = 0.0;
			}
			else
			{
				pointer->encoder.curSpeed = deltaAngle*ENCODERSPEEDCONSTANT;
			}*/
	}

}

uStepperTemp::uStepperTemp(void)
{

}

float uStepperTemp::getTemp(void)
{
	float T = 0.0;
	float Vout = 0.0;
	float NTC = 0.0;

	Vout = analogRead(TEMP)*0.0048828125;	//0.0048828125 = 5V/1024
	NTC = ((R*5.0)/Vout)-R;							//the current NTC resistance
	NTC = log(NTC);
	T = A + B*NTC + C*NTC*NTC*NTC;    				//Steinhart-Hart equation
	T = 1.0/T;

	return T - 273.15;
}

uStepperEncoder::uStepperEncoder(void)
{
	I2C.begin();
}

float uStepperEncoder::getAngleMoved(void)
{
	return this->angleMoved;
}

float uStepperEncoder::getSpeed(void)
{
	return this->curSpeed;
}

void uStepperEncoder::setup(void)
{
	uint8_t data[2];
	TCCR1A = 0;
	TCNT1 = 0;
	OCR1A = 65535;
	TIFR1 = 0;
	TIMSK1 = (1 << OCIE1A);
	TCCR1B = (1 << WGM12) | (1 << CS10);

	I2C.read(ENCODERADDR, ANGLE, 2, data);
	this->encoderOffset = (float)((((uint16_t)data[0]) << 8 ) | (uint16_t)data[1])*0.087890625;

	this->oldAngle = 0.0;
	this->angleMoved = 0.0;

	sei();

}

void uStepperEncoder::setHome(void)
{
	this->encoderOffset = this->getAngle();

	this->oldAngle = 0.0;
	this->angleMoved = 0.0;
	this->angleMoved = 0.0;

}

float uStepperEncoder::getAngle(void)
{
	
	float angle;
	/*uint8_t data[2];

	I2C.read(ENCODERADDR, ANGLE, 2, data);
	
	angle = (float)((((uint16_t)data[0]) << 8 )| (uint16_t)data[1])*0.087890625;
	
	return angle;*/
	return this->currentAngle;
}

uint16_t uStepperEncoder::getStrength()
{
	uint8_t data[2];

	I2C.read(ENCODERADDR, MAGNITUDE, 2, data);

	return (((uint16_t)data[0]) << 8 )| (uint16_t)data[1];
}

uint8_t uStepperEncoder::getAgc()
{
	uint8_t data;

	I2C.read(ENCODERADDR, MAGNITUDE, 1, &data);

	return data;
}

uint8_t uStepperEncoder::detectMagnet()
{
	uint8_t data;

	I2C.read(ENCODERADDR, AGC, 1, &data);

	data &= 0x38;					//For some reason the encoder returns random values on reserved bits. Therefore we make sure reserved bits are cleared before checking the reply !

	if(data == 0x08)
	{
		return 1;					//magnet too strong
	}

	else if(data == 0x10)
	{
		return 2;					//magnet too weak
	}

	else if(data == 0x20)
	{
		return 0;					//magnet detected and within limits
	}

	return 3;						//Something went horribly wrong !
}

uStepper::uStepper(void)
{
	this->state = STOP;

	this->setMaxAcceleration(1000.0);
	this->setMaxVelocity(1000.0);

	pointer = this;

	pinMode(DIR, OUTPUT);
	pinMode(STEP, OUTPUT);
	pinMode(ENA, OUTPUT);	
}

uStepper::uStepper(float accel, float vel)
{
	this->state = STOP;

	this->setMaxVelocity(vel);
	this->setMaxAcceleration(accel);

	pointer = this;

	pinMode(DIR, OUTPUT);
	pinMode(STEP, OUTPUT);
	pinMode(ENA, OUTPUT);
}

void uStepper::setMaxAcceleration(float accel)
{
	this->acceleration = accel;

	this->stopTimer();			//Stop timer so we dont fuck up stuff !
	this->multiplier = (this->acceleration/(INTFREQ*INTFREQ));	//Recalculate multiplier variable, used by the acceleration algorithm since acceleration has changed!
	if(this->state != STOP)
	{
		if(this->continous == 1)	//If motor was running continously
		{
			this->runContinous(this->direction);	//We should make it run continously again
		}
		else						//If motor still needs to perform some steps
		{
			this->moveSteps(this->totalSteps - this->currentStep + 1, this->direction, this->hold);	//we should make sure the motor gets to execute the remaining steps				
		}
	}
}

float uStepper::getMaxAcceleration(void)
{
	return this->acceleration;
}

void uStepper::setMaxVelocity(float vel)
{
	if(vel < 0.5005)
	{
		this->velocity = 0.5005;			//Limit velocity in order to not overflow delay variable
	}

	else if(vel > 32000.0)
	{
		this->velocity = 32000.0;			//limit velocity in order to not underflow delay variable
	}

	else
	{
		this->velocity = vel;
	}

	this->stopTimer();			//Stop timer so we dont fuck up stuff !
	this->cruiseDelay = (uint16_t)((INTFREQ/this->velocity) - 0.5);	//Calculate cruise delay, so we dont have to recalculate this in the interrupt routine
	
	if(this->state != STOP)		//If motor was running, we should make sure it runs again
	{
		if(this->continous == 1)	//If motor was running continously
		{
			this->runContinous(this->direction);	//We should make it run continously again
		}
		else					//If motor still needs to perform some steps
		{
			this->moveSteps(this->totalSteps - this->currentStep + 1, this->direction, this->hold);	//we should make sure it gets to execute these steps	
		}
	}
}

float uStepper::getMaxVelocity(void)
{
	return this->velocity;
}

void uStepper::runContinous(bool dir)
{
	float curVel;

	this->continous = 1;			//Set continous variable to 1, in order to let the interrupt routine now, that the motor should run continously
	
	this->stopTimer();				//Stop interrupt timer, so we don't fuck up stuff !

	if(state != STOP)										//if the motor is currently running and we want to move the opposite direction, we need to decelerate in order to change direction.
	{
		curVel = INTFREQ/this->exactDelay;								//Use this to calculate current velocity

		if(dir != digitalRead(DIR))							//If motor is currently running the opposite direction as desired
		{
			this->state = INITDECEL;							//We should decelerate the motor to full stop before accelerating the speed in the opposite direction
			this->initialDecelSteps = (uint32_t)(((curVel*curVel))/(2.0*this->acceleration));		//the amount of steps needed to bring the motor to full stop. (S = (V^2 - V0^2)/(2*-a)))
			this->accelSteps = (uint32_t)((this->velocity*this->velocity)/(2.0*this->acceleration));			//Number of steps to bring the motor to max speed (S = (V^2 - V0^2)/(2*a)))
			this->direction = dir;
			this->exactDelay = INTFREQ/sqrt((curVel*curVel) + 2.0*this->acceleration);	//number of interrupts before the first step should be performed.

			if(this->exactDelay >= 65535.5)
			{
				this->delay = 0xFFFF;
			}
			else
			{
				this->delay = (uint16_t)(this->exactDelay - 0.5);		//Truncate the exactDelay variable, since we cant perform fractional steps
			}
		}
		else												//If the motor is currently rotating the same direction as the desired direction
		{
			if(curVel > this->velocity)						//If current velocity is greater than desired velocity
			{
				this->state = INITDECEL;						//We need to decelerate the motor to desired velocity
				this->initialDecelSteps = (uint32_t)(((this->velocity*this->velocity) - (curVel*curVel))/(-2.0*this->acceleration));		//Number of steps to bring the motor down from current speed to max speed (S = (V^2 - V0^2)/(2*-a)))
				this->accelSteps = 0;						//No acceleration phase is needed
			}

			else if(curVel < this->velocity)					//If the current velocity is less than the desired velocity
			{
				this->state = ACCEL;							//Start accelerating
				this->accelSteps = (uint32_t)(((this->velocity*this->velocity) - (curVel*curVel))/(2.0*this->acceleration));	//Number of Steps needed to accelerate from current velocity to full speed
			}

			else											//If motor is currently running at desired speed
			{
				this->state = CRUISE;						//We should just run at cruise speed
			}
		}
	}

	else																						//If motor is currently stopped (state = STOP)
	{
		this->state = ACCEL;																		//Start accelerating
		digitalWrite(DIR, dir);																	//Set the motor direction pin to the desired setting
		this->accelSteps = (velocity*velocity)/(2.0*acceleration);								//Number of steps to bring the motor to max speed (S = (V^2 - V0^2)/(2*a)))
		
		this->exactDelay = INTFREQ/sqrt(2.0*this->acceleration);	//number of interrupts before the first step should be performed.
		
		if(this->exactDelay > 65535.0)
		{
			this->delay = 0xFFFF;
		}
		else
		{
			this->delay = (uint16_t)(this->exactDelay - 0.5);		//Truncate the exactDelay variable, since we cant perform fractional steps
		}
	}
	
	this->startTimer();																			//start timer so we can perform steps
	this->enableMotor();																			//Enable motor
}

void uStepper::moveSteps(uint32_t steps, bool dir, bool holdMode)
{
	float curVel;

	this->stopTimer();					//Stop interrupt timer so we dont fuck stuff up !
	steps--;
	this->direction = dir;				//Set direction variable to the desired direction of rotation for the interrupt routine
	this->hold = holdMode;				//Set the hold variable to desired hold mode (block motor or release motor after end movement) for the interrupt routine
	this->totalSteps = steps;			//Load the desired number of steps into the totalSteps variable for the interrupt routine
	this->continous = 0;				//Set continous variable to 0, since the motor should not run continous

	if(state != STOP)					//if the motor is currently running and we want to move the opposite direction, we need to decelerate in order to change direction.
	{
		curVel = INTFREQ/this->exactDelay;								//Use this to calculate current velocity

		if(dir != digitalRead(DIR))									//If current direction is different from desired direction
		{
			this->state = INITDECEL;									//We should decelerate the motor to full stop
			this->initialDecelSteps = (uint32_t)((curVel*curVel)/(2.0*this->acceleration));		//the amount of steps needed to bring the motor to full stop. (S = (V^2 - V0^2)/(2*-a)))
			this->accelSteps = (uint32_t)((this->velocity * this->velocity)/(2.0*this->acceleration));									//Number of steps to bring the motor to max speed (S = (V^2 - V0^2)/(2*a)))
			this->totalSteps += this->initialDecelSteps;				//Add the steps used for initial deceleration to the totalSteps variable, since we moved this number of steps, passed the initial position, and therefore need to move this amount of steps extra, in the desired direction
			this->exactDelayDecel = (INTFREQ/sqrt(this->velocity*this->velocity + 2*this->acceleration));

			if(this->accelSteps > (this->totalSteps >> 1))			//If we need to accelerate for longer than half of the total steps, we need to start decelerating before we reach max speed
			{
				this->accelSteps = this->decelSteps = (this->totalSteps >> 1);	//Accelerate and decelerate for the same amount of steps (half the total steps)
				this->accelSteps += this->totalSteps - this->accelSteps - this->decelSteps;				//If there are still a step left to perform, due to rounding errors, do this step as an acceleration step	
			}
			else
			{
				this->decelSteps = this->accelSteps;					//If top speed is reached before half the total steps are performed, deceleration period should be same length as acceleration period
				this->cruiseSteps = this->totalSteps - this->accelSteps - this->decelSteps; 			//Perform remaining steps, as cruise steps
			}

			this->exactDelay = INTFREQ/sqrt((curVel*curVel) + 2.0*this->acceleration);	//number of interrupts before the first step should be performed.

			if(this->exactDelay >= 65535.5)
			{
				this->delay = 0xFFFF;
			}
			else
			{
				this->delay = (uint16_t)(this->exactDelay - 0.5);		//Truncate the exactDelay variable, since we cant perform fractional steps
			}
		}
		else							//If the motor is currently rotating the same direction as desired, we dont necessarily need to decelerate
		{
			if(curVel > this->velocity)	//If current velocity is greater than desired velocity
			{
				this->state = INITDECEL;	//We need to decelerate the motor to desired velocity
				this->initialDecelSteps = (uint32_t)(((this->velocity*this->velocity) - (curVel*curVel))/(-2.0*this->acceleration));		//Number of steps to bring the motor down from current speed to max speed (S = (V^2 - V0^2)/(2*-a)))
				this->accelSteps = 0;	//No acceleration phase is needed
				this->decelSteps = (uint32_t)((this->velocity*this->velocity)/(2.0*this->acceleration));	//Number of steps needed to decelerate the motor from top speed to full stop
				this->exactDelayDecel = (INTFREQ/sqrt(this->velocity*this->velocity + 2*this->acceleration));
				this->exactDelay = (INTFREQ/sqrt((curVel*curVel) + 2*this->acceleration));

				if(this->totalSteps <= (this->initialDecelSteps + this->decelSteps))
				{
					this->cruiseSteps = 0;
				}
				else
				{
					this->cruiseSteps = steps - this->initialDecelSteps - this->decelSteps;					//Perform remaining steps as cruise steps
				}

				
			}

			else if(curVel < this->velocity)	//If current velocity is less than desired velocity
			{
				this->state = ACCEL;			//Start accelerating
				this->accelSteps = (uint32_t)(((this->velocity*this->velocity) - (curVel*curVel))/(2.0*this->acceleration));	//Number of Steps needed to accelerate from current velocity to full speed
				this->exactDelayDecel = (INTFREQ/sqrt(this->velocity*this->velocity + 2*this->acceleration));
				if(this->accelSteps > (this->totalSteps >> 1))			//If we need to accelerate for longer than half of the total steps, we need to start decelerating before we reach max speed
				{
					this->accelSteps = this->decelSteps = (this->totalSteps >> 1);	//Accelerate and decelerate for the same amount of steps (half the total steps)
					this->accelSteps += this->totalSteps - this->accelSteps - this->decelSteps;				//If there are still a step left to perform, due to rounding errors, do this step as an acceleration step	
					this->cruiseSteps = 0;
				}
				else
				{
					this->decelSteps = this->accelSteps;					//If top speed is reached before half the total steps are performed, deceleration period should be same length as acceleration period
					this->cruiseSteps = this->totalSteps - this->accelSteps - this->decelSteps; 			//Perform remaining steps, as cruise steps
				}

				this->cruiseSteps = steps - this->accelSteps - this->decelSteps;	//Perform remaining steps as cruise steps
				this->initialDecelSteps = 0;								//No initial deceleration phase needed
			}

			else						//If current velocity is equal to desired velocity
			{
				this->state = CRUISE;	//We are already at desired speed, therefore we start at cruise phase
				this->decelSteps = (uint32_t)((this->velocity*this->velocity)/(2.0*this->acceleration));	//Number of steps needed to decelerate the motor from top speed to full stop
				this->accelSteps = 0;	//No acceleration phase needed
				this->initialDecelSteps = 0;		//No initial deceleration phase needed
				this->exactDelayDecel = (INTFREQ/sqrt(this->velocity*this->velocity + 2*this->acceleration));
				if(this->decelSteps >= this->totalSteps)
				{
					this->cruiseSteps = 0;
				}
				else
				{
					this->cruiseSteps = steps - this->decelSteps;	//Perform remaining steps as cruise steps
				}
			}
		}
	}
	
	else								//If motor is currently at full stop (state = STOP)
	{
		digitalWrite(DIR, dir);			//Set direction pin to desired direction
		this->state = ACCEL;
		this->accelSteps = (uint32_t)((this->velocity * this->velocity)/(2.0*this->acceleration));	//Number of steps to bring the motor to max speed (S = (V^2 - V0^2)/(2*a)))
		this->initialDecelSteps = 0;		//No initial deceleration phase needed

		if(this->accelSteps > (steps >> 1))	//If we need to accelerate for longer than half of the total steps, we need to start decelerating before we reach max speed
		{
			this->cruiseSteps = 0; 		//No cruise phase needed
			this->accelSteps = this->decelSteps = (steps >> 1);				//Accelerate and decelerate for the same amount of steps (half the total steps)
			this->accelSteps += steps - this->accelSteps - this->decelSteps;	//if there are still a step left to perform, due to rounding errors, do this step as an acceleration step	
			this->exactDelayDecel = (INTFREQ/sqrt(2*this->acceleration*this->accelSteps));
		}

		else								
		{
			this->decelSteps = this->accelSteps;	//If top speed is reached before half the total steps are performed, deceleration period should be same length as acceleration period
			this->cruiseSteps = steps - this->accelSteps - this->decelSteps;	//Perform remaining steps as cruise steps
			this->exactDelayDecel = (INTFREQ/sqrt(this->velocity*this->velocity + 2*this->acceleration));
		}
		this->exactDelay = INTFREQ/sqrt(2.0*this->acceleration);	//number of interrupts before the first step should be performed.

		if(this->exactDelay > 65535.0)
		{
			this->delay = 0xFFFF;
		}
		else
		{
			this->delay = (uint16_t)(this->exactDelay - 0.5);		//Truncate the exactDelay variable, since we cant perform fractional steps
		}
	}

	this->startTimer();									//start timer so we can perform steps
	this->enableMotor();									//Enable motor driver
}

void uStepper::hardStop(bool holdMode)
{
	this->stopTimer();			//Stop interrupt timer, since we shouldn't perform more steps
	this->hold = holdMode;

	if(state != STOP)
	{
		this->state = STOP;			//Set current state to STOP

		this->startTimer();
	}

	else
	{
		if(holdMode == SOFT)
		{
			this->disableMotor();
		}
		
		else if (holdMode == HARD)
		{
			this->enableMotor();
		}
	}
}

void uStepper::softStop(bool holdMode)
{
	float curVel;

	this->stopTimer();			//Stop interrupt timer, since we shouldn't perform more steps
	this->hold = holdMode;		

	if(state != STOP)
	{
		curVel = INTFREQ/this->exactDelay;								//Use this to calculate current velocity

		this->decelSteps = (uint32_t)((curVel*curVel)/(2.0*this->acceleration));		//Number of steps to bring the motor down from current speed to max speed (S = (V^2 - V0^2)/(2*-a)))	
		this->accelSteps = this->initialDecelSteps = this->cruiseSteps = 0;	//Reset amount of steps in the different phases	
		this->state = DECEL;

		this->exactDelay = INTFREQ/sqrt(2.0*this->acceleration);	//number of interrupts before the first step should be performed.

		if(this->exactDelay > 65535.0)
		{
			this->delay = 0xFFFF;
		}
		else
		{
			this->delay = (uint16_t)(this->exactDelay - 0.5);		//Truncate the exactDelay variable, since we cant perform fractional steps
		}

		this->startTimer();
	}

	else
	{
		if(holdMode == SOFT)
		{
			this->disableMotor();
		}
		
		else if (holdMode == HARD)
		{
			this->enableMotor();
		}
	}
}

void uStepper::setup(void)
{
	pinMode(2,INPUT);

	EICRA = 0x03;		//int0 generates interrupt on rising edge, int1 generates interrupt on any change
	EIMSK = 0x01;		//enable int0 and int1 interrupt requests
	TCCR2B &= ~((1 << CS20) | (1 << CS21) | (1 << CS22) | (1 << WGM22));
	TCCR2A &= ~((1 << WGM20) | (1 << WGM21));
	TCCR2B |= (1 << CS21);				//Enable timer with prescaler 8. interrupt base frequency ~ 7.8125kHz
	TCCR2A |= (1 << WGM21);				//Switch timer 2 to CTC mode, to adjust interrupt frequency
	OCR2A = 60;							//Change top value to 60 in order to obtain an interrupt frequency of 33.333kHz
	this->encoder.setup();
}

void uStepper::startTimer(void)
{
	TCNT2 = 0;							//Clear counter value, to make sure we get correct timing
	TIFR2 |= (1 << OCF2A);				//Clear compare match interrupt flag, if it is set.
	TIMSK2 |= (1 << OCIE2A);			//Enable compare match interrupt

	sei();
}

void uStepper::stopTimer(void)
{
	TIMSK2 &= ~(1 << OCF2A);			//disable compare match interrupt
}

void uStepper::enableMotor(void)
{
	digitalWrite(ENA, LOW);				//Enable motor driver
}

void uStepper::disableMotor(void)
{
	digitalWrite(ENA, HIGH);			//Disable motor driver
}

bool uStepper::getCurrentDirection(void)
{
	return this->direction;
}

bool uStepper::getMotorState(void)
{
	if(this->state != STOP)
	{
		return 1;		//Motor running
	}

	return 0;			//Motor not running
}

int64_t uStepper::getStepsSinceReset(void)
{
	if(this->direction == CW)
	{
		return this->stepsSinceReset + this->currentStep;
	}
	else
	{
		return this->stepsSinceReset - this->currentStep;
	}
}

void i2cMaster::cmd(uint8_t cmd)
{
	uint16_t i = 0;
	// send command
	TWCR = cmd;
	// wait for command to complete
	while (!(TWCR & (1 << TWINT)));
	
	// save status bits
	status = TWSR & 0xF8;	
}

bool i2cMaster::read(uint8_t slaveAddr, uint8_t regAddr, uint8_t numOfBytes, uint8_t *data)
{
	uint8_t i, buff[numOfBytes];

	TIMSK1 &= ~(1 << OCIE1A);

	I2C.start(slaveAddr, WRITE);

	I2C.writeByte(regAddr);

	I2C.restart(slaveAddr, READ);

	for(i = 0; i < (numOfBytes - 1); i++)
	{
		I2C.readByte(ACK, &data[i]);
	}

	I2C.readByte(NACK, &data[numOfBytes-1]);

	I2C.stop();

	TIMSK1 |= (1 << OCIE1A);
	
	return 1; 
}

bool i2cMaster::write(uint8_t slaveAddr, uint8_t regAddr, uint8_t numOfBytes, uint8_t *data)
{
	uint8_t i;

	TIMSK1 &= ~(1 << OCIE1A);

	I2C.start(slaveAddr, WRITE);
	I2C.writeByte(regAddr);
	
	for(i = 0; i < numOfBytes; i++)
	{
		I2C.writeByte(*(data + i));
	}
	I2C.stop();

	TIMSK1 |= (1 << OCIE1A);

	return 1;
}

bool i2cMaster::readByte(bool ack, uint8_t *data)
{
	if(ack)
	{
		this->cmd((1 << TWINT) | (1 << TWEN) | (1 << TWEA));
	}
	
	else
	{
		this->cmd((1 << TWINT) | (1 << TWEN));
	}

	*data = TWDR;

	return 1;
}

bool i2cMaster::start(uint8_t addr, bool RW)
{
	// send START condition
	this->cmd((1<<TWINT) | (1<<TWSTA) | (1<<TWEN));

	if (this->getStatus() != START && this->getStatus() != REPSTART) 
	{
		return false;
	}

	// send device address and direction
	TWDR = (addr << 1) | RW;
	this->cmd((1 << TWINT) | (1 << TWEN));
	
	if (RW == READ) 
	{
		return this->getStatus() == RXADDRACK;
	} 

	else 
	{
		return this->getStatus() == TXADDRACK;
	}
}

bool i2cMaster::restart(uint8_t addr, bool RW)
{
	return this->start(addr, RW);
}

bool i2cMaster::writeByte(uint8_t data)
{
	TWDR = data;

	this->cmd((1 << TWINT) | (1 << TWEN));

	return this->getStatus() == TXDATAACK;
}

bool i2cMaster::stop(void)
{
	uint16_t i = 0;
	//	issue stop condition
	TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);

	// wait until stop condition is executed and bus released
	while (TWCR & (1 << TWSTO));

	status = I2CFREE;

	return 1;
}

uint8_t i2cMaster::getStatus(void)
{
	return status;
}

void i2cMaster::begin(void)
{
	// set bit rate register to 12 to obtain 400kHz scl frequency (in combination with no prescaling!)
	TWBR = 12;
	// no prescaler
	TWSR &= 0xFC;
}

i2cMaster::i2cMaster(void)
{

}