#include <Arduino.h>

#include <TimerOne.h>
#include <Wire.h>

int latchPin   = 4; //Pin connected to ST_CP of 74HC595
int myClockPin = 3; //Pin connected to SH_CP of 74HC595
int myDataPin  = 2; //Pin connected to DS    of 74HC595

unsigned int index = 0;
unsigned int counter = 0;

uint8_t draw_state = 0;

char buffer[256];
bool ready = false;

int cnt = 0;

void stepperRotate(float rotation, float rpm);

enum
{
	MODE_CONSTANT = 0,
	MODE_SET_POSITION,
};

enum ACCEL_STATE
{
	INITAL=0,
	RAMP_UP,
	CONSTANT,
	RAMP_DOWN,
};

enum
{
	DIRECTION_FORWARD = 0,
	DIRECTION_BACKWARD,
};
//char modes[] = {MODE_CONSTANT,MODE_CONSTANT,MODE_CONSTANT,MODE_CONSTANT,MODE_CONSTANT,MODE_CONSTANT,MODE_CONSTANT,MODE_CONSTANT};
char modes[] = {MODE_SET_POSITION,MODE_SET_POSITION,MODE_SET_POSITION,MODE_SET_POSITION,MODE_SET_POSITION,MODE_SET_POSITION,MODE_SET_POSITION,MODE_SET_POSITION};
char accelState[] = {INITAL,INITAL,INITAL,INITAL,INITAL,INITAL,INITAL,INITAL};
char directions[] = {DIRECTION_FORWARD,DIRECTION_FORWARD,DIRECTION_FORWARD,DIRECTION_FORWARD,DIRECTION_FORWARD,DIRECTION_FORWARD,DIRECTION_FORWARD,DIRECTION_FORWARD};
long positions[] = {0,0,0,0,0,0,0,0};
long destPositions[] = {0,0,0,0,0,0,0,0};
bool changing[] = {0,0,0,0,0,0,0,0};
bool enabledState[] = {1,0,0,0,0,0,0,0};
unsigned int times[] = {0,0,0,0,0,0,0,0};
unsigned long speeds[] = {1000, 600, 600, 600, 600, 600, 600, 600};

long initalRampSpeed = 10000;

long rampSpeeds[] = {initalRampSpeed,initalRampSpeed,initalRampSpeed,initalRampSpeed,initalRampSpeed,initalRampSpeed,initalRampSpeed,initalRampSpeed};
bool states[] = {1,1,1,1,1,1,1,1};
bool stepModes[8][3] = {{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}};
unsigned long startSpeeds[] = {2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000};

#define TIMER_US 300 // 300mS set timer duration in microseconds

bool stepperPosChanged[] = {false, false, false, false, false, false, false, false};
int stepperPosChangedListeners[] = {1, -1, -1, -1, -1, -1, -1, -1};

/**
 * \brief Setup code. This is only ran once after the Arduino is powered up.
 */
void setup()
{
	pinMode(latchPin,   OUTPUT);
	pinMode(myClockPin, OUTPUT);
	pinMode(myDataPin,  OUTPUT);

	//Serial.begin(115200); // Seems to not work
	Serial.begin(9600);

	Serial.println("Ready");

	Timer1.initialize(TIMER_US);         // Initialise timer 1
	Timer1.attachInterrupt( timerIsr );  // attach the ISR routine here

	// Start the I2C Bus as Slave on address 9
	Wire.begin(9);
	Wire.onReceive(receiveEvent);
}

char receiveEventBuf[128];
int receiveEventindex = 0;

void completeBuffer()
{
	receiveEventBuf[receiveEventindex] = 0;

	receiveEventindex = 0;
}

void receiveEvent(int bytes)
{
	cli();

	int ID = 0;

	while(Wire.available())
	{
		const char incomingByte = Wire.read(); // read the incoming byte

		if(isDigit(incomingByte) || incomingByte == '-') { receiveEventBuf[receiveEventindex++] = incomingByte ;}

		else
		{
			completeBuffer();

			if(incomingByte == 'i') { ID = atoi(receiveEventBuf) ;}

			else if(incomingByte == 'l')
			{
				const int stepper = atoi(receiveEventBuf);

				modes[stepper] = MODE_SET_POSITION;

				const long hPos = positions[stepper];

				destPositions[stepper] = positions[stepper] = 0;

				Serial.print("home "); Serial.println(hPos);
			}
			else if(incomingByte == 'p') { destPositions[ID] = atoi(receiveEventBuf) ;}
		}
	}

	sei();
}

void timerIsr() { stepperRotate() ;}

void p(char * buf, char *fmt, ... )
{
	va_list args;
	va_start (args, fmt );
	vsnprintf(buf, 512, fmt, args);
	va_end (args);
}

bool doCommands(const char * command)
{
	if(!command) { return false ;}

	bool echoCommands = true;

	if(!strcmp(command, "help") || !strcmp(command, "?"))
	{
		Serial.println("Help:");
		Serial.println();
		Serial.println("Set speed: s <stepper> <speed>");
		Serial.println("Set mode: m <stepper> <mode>");
		Serial.println("Set step mode: sm <stepper> <stepMode>");
		Serial.println("Set position: p <stepper> <position>");
		Serial.println("Get position: gp <stepper>");
		Serial.println("Set direction: d <stepper> <direction>");
		Serial.println("Set enabled: e <stepper> <enabled>");
	}
	else if(!strcmp(command, "s"))
	{
		const int stepper = atoi(strtok(NULL, " "));
		const int speed   = atoi(strtok(NULL, " "));

		changing[stepper] = true;
		speeds  [stepper] = speed + 500;
		changing[stepper] = false;

		if(echoCommands)
		{
			Serial.print("Set speed ");
			Serial.print(stepper);
			Serial.print(' ');
			Serial.println(speed);
		}
	}
	else if(!strcmp(command, "m"))
	{
		const int stepper = atoi(strtok(NULL, " "));
		const int mode   = atoi(strtok(NULL, " "));

		if(echoCommands)
		{
			Serial.print("Set mode ");
			Serial.print(stepper); Serial.print(' ');
			Serial.println(mode);
		}

		modes[stepper] = mode;
	}
	else if(!strcmp(command, "sm"))
	{
		const int stepper = atoi(strtok(NULL, " "));
		const int mode   = atoi(strtok(NULL, " "));

		if(echoCommands)
		{
			Serial.print("Set step mode ");
			Serial.print(stepper);
			Serial.print(' ');
			Serial.println(mode);
		}

		switch(mode)
		{
		case 0: stepModes[stepper][0] = 0; stepModes[stepper][1] = 0; stepModes[stepper][2] = 0; break; // Full step
		case 1: stepModes[stepper][0] = 1; stepModes[stepper][1] = 0; stepModes[stepper][2] = 0; break; // Half step
		case 2: stepModes[stepper][0] = 0; stepModes[stepper][1] = 1; stepModes[stepper][2] = 0; break; // 1/4 step
		case 3: stepModes[stepper][0] = 1; stepModes[stepper][1] = 1; stepModes[stepper][2] = 0; break; // 1/8 step
		case 4: stepModes[stepper][0] = 0; stepModes[stepper][1] = 0; stepModes[stepper][2] = 1; break; // 1/16 step
		case 5: stepModes[stepper][0] = 1; stepModes[stepper][1] = 0; stepModes[stepper][2] = 1; break; // 1/32 step
		}
	}
	else if(!strcmp(command, "p"))
	{
		const int stepper = atoi(strtok(NULL, " "));
		const long position   = atoi(strtok(NULL, " "));

		if(echoCommands)
		{
			Serial.print("Set position ");
			Serial.print(stepper); Serial.print(' ');
			Serial.println(position);
		}
		modes[stepper] = MODE_SET_POSITION;
		accelState[stepper] = RAMP_UP;
		destPositions[stepper] = position;
	}
	else if(!strcmp(command, "gp"))
	{
		const int stepper = atoi(strtok(NULL, " "));

		if(echoCommands)
		{
			Serial.print("Get position ");
			Serial.print(stepper); Serial.print(' ');
			Serial.print("Pos: ");
		}

		Serial.println(positions[stepper]);
	}
	else if(!strcmp(command, "d"))
	{
		const int stepper = atoi(strtok(NULL, " "));

		directions[stepper] = atoi(strtok(NULL, " "));

		if(echoCommands)
		{
			Serial.print("Set direction ");
			Serial.print(stepper); Serial.print(' ');
			Serial.println(directions[stepper]);
		}
	}
	else if(!strcmp(command, "e"))
	{
		const int stepper = atoi(strtok(NULL, " "));
		enabledState[stepper] = atoi(strtok(NULL, " "));
		if(echoCommands)
		{
			Serial.print("Set enabled ");
			Serial.print(stepper); Serial.print(' ');
			Serial.println(enabledState[stepper]);
		}
	}

	ready = false;

	return true;
}

void loop()
{
	bool needToSendPos = false;

	for(int i = 0; i < 8; ++i)
	{
		if(!stepperPosChanged[i] || stepperPosChangedListeners[i] == -1) { continue ;}

		if(!needToSendPos)
		{
			Wire.beginTransmission(9); // transmit to device #9

			needToSendPos = true;
		}

		char buf[128];

		p(buf, "%ii%ip", stepperPosChangedListeners[i], positions[i]);

		Wire.write(buf);

		stepperPosChanged[i] = false;
	}

	if(needToSendPos) { Wire.endTransmission() ;}

	if (ready)
	{
		const char * command = strtok(buffer, " \r\n");

		for(;doCommands(command) ;) { command = strtok(NULL, " ") ;}
	}
	else while (Serial.available())
	{
		char c = Serial.read();
		buffer[cnt++] = c;
		if ((c == '\n') || (cnt == sizeof(buffer)-1))
		{
			buffer[cnt] = '\0';
			cnt = 0;
			ready = true;
		}
	}
}

// the heart of the program
void shiftOut(byte myDataOut)
{
	for (int i=7; i>=0; i--)
	{
		PORTD &= ~(1 << myClockPin);

		if ( myDataOut & (1<<i) ) { PORTD |= (1 << myDataPin) ;}
		else { PORTD &= ~(1 << myDataPin) ;}

		PORTD |= (1 << myClockPin);
		PORTD &= ~(1 << myClockPin);
	}

	PORTD &= ~(1 << myClockPin);
}

void stepperRotate()
{
	cli();

	counter += TIMER_US;

	bool newState = false;

	for(int i = 0; i < 8; ++i)
	{
		if(changing[i]) { continue ;}
		//unsigned long diff = counter - times[i];
		unsigned int diff = counter - times[i];
		if(!enabledState[i]) { continue ;}
		//if(diff > speeds[i] || states[i])

		if(positions[i] == destPositions[i])
		{
			accelState[i] = INITAL;

			rampSpeeds[i] = initalRampSpeed;

			continue;
		} // Breaks constant rotation

		switch(accelState[i])
		{
			case INITAL:

				accelState[i] = RAMP_UP;

				goto doContinue;


			case RAMP_UP:

				rampSpeeds[i] -= 10;

				if(rampSpeeds[i] < 0)
				{
					accelState[i] = CONSTANT;

					goto doContinue;
				}

				if(diff <= rampSpeeds[i])
				{
					continue;
				}

				break;

			case CONSTANT:
				break;

			case RAMP_DOWN:
				break;
		}

		doContinue:

		if(diff <= speeds[i]) { continue ;}

		stepperPosChanged[i] = true;

		times[i] = counter;

		switch(modes[i]) // Try to move this code above, or combine it
		{
			case MODE_CONSTANT:
			states[i] = !states[i];
			if(states[i]) { ++positions[i] ;}
			newState = true;
			break;
			case MODE_SET_POSITION:
			states[i] = !states[i];
			if(states[i])
			{
				if(positions[i] == destPositions[i])
				{
					states[i] = 0; // Does it get here?

					break;
				}
				if(positions[i] - destPositions[i] > 0)
				{
					directions[i] = DIRECTION_BACKWARD;

					--positions[i];
				}
				else
				{
					directions[i] = DIRECTION_FORWARD;

					++positions[i];
				}
			}

			newState = true;

			break;
		}
	}

	newState = true;

	if(!newState)
	{
		sei();

		return;
	}

	byte out1 = 0;
	byte out2 = 0;
	byte out3 = 0;
	byte out4 = 0;
	byte out5 = 0;
	byte out6 = 0;

	out6 |= !enabledState[7] << 0; // EN 7
	out6 |= stepModes[7][0] << 1;  // M0 7
	out6 |= stepModes[7][1] << 2;  // M1 7
	out6 |= stepModes[7][2] << 3;  // M2 7
	out6 |= states[7] << 4;        // Spt 7
	out6 |= directions[7] << 5;    // Dir 7

	out1 |= !enabledState[6] << 2; // EN 6
	out1 |= stepModes[6][0] << 3;  // M0 6
	out1 |= stepModes[6][1] << 4;  // M1 6
	out1 |= stepModes[6][2] << 5;  // M2 6
	out1 |= states[6] << 6;        // Stp 6
	out1 |= directions[6] << 7;    //

	out2 |= stepModes[2][2] << 0;  // M2 2
	out2 |= stepModes[2][1] << 1;  // M1 2
	out2 |= stepModes[2][0] << 2;  // M0 2
	out2 |= !enabledState[2] << 3; // EN 2
	out3 |= states[2] << 7;        // Stp 2
	out3 |= directions[2] << 6;    // Dir 2

	out2 |= !enabledState[5] << 4; // EN 5
	out2 |= stepModes[5][0] << 5;  // M0 5
	out2 |= stepModes[5][1] << 6;  // M1 5
	out2 |= stepModes[5][2] << 7;  // M2 5
	out1 |= states[5] << 0;        // Stp 5
	out1 |= directions[5] << 1;    // Dir 5

	out3 |= !enabledState[3] << 5; // EN 3
	out3 |= stepModes[3][0] << 4;  // M0 3
	out3 |= stepModes[3][1] << 3;  // M1 3
	out3 |= stepModes[3][2] << 2;  // M2 3
	out3 |= states[3] << 1;        // Stp 3
	out3 |= directions[3] << 0;    // Dir 3

	out4 |= !enabledState[0] << 7; // EN 0
	out4 |= stepModes[0][0] << 6;  // M0 0
	out4 |= stepModes[0][1] << 5;  // M1 0
	out4 |= stepModes[0][2] << 4;  // M2 0
	out4 |= states[0] << 3;        // Stp 0
	out4 |= directions[0] << 2;    // Dir 0

	out4 |= !enabledState[1] << 1; // EN 1
	out4 |= stepModes[1][0] << 0;  // M0 1
	out5 |= stepModes[1][1] << 7;  // M1 1
	out5 |= stepModes[1][2] << 6;  // M2 1
	out5 |= states[1] << 5;        // Stp 1
	out5 |= directions[1] << 4;    // Dir 1

	out5 |= !enabledState[4] << 3; // EN 4
	out5 |= stepModes[4][0] << 2;  // M0 4
	out5 |= stepModes[4][1] << 1;  // M1 4
	out5 |= stepModes[4][2] << 0;  // M2 4
	out6 |= states[4] << 7;        // Stp 4
	out6 |= directions[4] << 6;    // Dir 4

	PORTD &= ~(1 << latchPin);
	shiftOut(out1);
	shiftOut(out2);
	shiftOut(out3);
	shiftOut(out4);
	shiftOut(out5);
	shiftOut(out6);
	PORTD |= (1 << latchPin);

	sei();
}
