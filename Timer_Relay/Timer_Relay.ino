#include <EEPROM.h>
#include <avr/interrupt.h>

#define SHIFT		7
#define LOAD		8
#define DIO			9

#define RELAY		12
#define BUTTON1		A5		// RESET/SETTING HOLD/SELECT
#define BUTTON2		A4		// TRIGGER/CHANGE
#define LED1		A0
#define LED2		A1

#define DEBOUNCE	200
#define LONG_PRESS	3000
#define SHORT_PRESS	150
#define TIME_OUT	5000

#define STATE_OFF   0
#define STATE_ON	1


unsigned long tick1 = 0;
unsigned long tick2 = 0;
unsigned long b1tick = 0;
unsigned long b2tick = 0;
unsigned long timeout = 0;

uint8_t state = 0;
volatile uint8_t phase = 0;
volatile uint8_t blinkCount = 0;
volatile uint8_t blink = 0;
volatile int selected = -1;

bool button1State = true;
bool button2State = true;
bool changeState = false;

volatile int time1 = 100;
volatile int time2 = 200;
volatile int countdown = 0;
volatile bool phase1 = STATE_OFF;
volatile bool phase2 = STATE_OFF;
volatile bool phase3 = STATE_OFF;

struct numArray { 
    uint8_t chars[4]; 
}; 

uint8_t numbers[] = 
    {// 0    1     2     3     4     5     6     7     8     9     o     n     f     transparent
       0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90, 0xA3, 0xAB, 0x8E, 0xff
    };	
	
	
void setupTimer(){
	
	cli();
	TCCR1A = 0;
    TCCR1B = 0;
    TIMSK1 = 0;
	
    TCCR1B |= (1 << CS11) | (1 << CS10);    // prescale = 64
    TCNT1 = 40535;
    TIMSK1 = (1 << TOIE1);
	sei();
}

void resetTimer(){
	TCNT1 = 40535;
}

ISR (TIMER1_OVF_vect) {
	
	blinkCount++;
	if(blinkCount == 3){
		blinkCount = 0;
		blink = blink == 0x00? 0xff : 0x00;
	}
	
	if(countdown > 0){
		if(phase == 0){
			countdown = countdown - 1;
			if(countdown <= 0){
				countdown = time2;
				phase++;
				setLeds(STATE_OFF, STATE_ON);
				digitalWrite(RELAY, phase2);
			}
		}else if(phase == 1){
			countdown = countdown - 1;
			if(countdown <= 0){
				countdown = 0;
				phase++;
				setLeds(STATE_ON, STATE_ON);
				digitalWrite(RELAY, phase3);
			}
		}
	}
	
    TCNT1 = 40535;
}
	
void load(){
	time1 = EEPROM.read(0) * 1000 + EEPROM.read(1) * 100 + EEPROM.read(2) * 10 + EEPROM.read(3);
	time2 = EEPROM.read(4) * 1000 + EEPROM.read(5) * 100 + EEPROM.read(6) * 10 + EEPROM.read(7);
	phase1 = EEPROM.read(8);
	phase2 = EEPROM.read(9);
	phase3 = EEPROM.read(10);
}

void save(){
	int temp = time1;
	EEPROM.write(0, temp/1000);
	temp = temp % 1000;
	EEPROM.write(1, temp/100);
	temp = temp % 100;
	EEPROM.write(2, temp/10);
	EEPROM.write(3, temp%10);
	
	temp = time2;
	EEPROM.write(4, temp/1000);
	temp = temp % 1000;
	EEPROM.write(5, temp/100);
	temp = temp % 100;
	EEPROM.write(6, temp/10);
	EEPROM.write(7, temp%10);
	
	EEPROM.write(8, phase1);
	EEPROM.write(9, phase2);
	EEPROM.write(10, phase3);
}

void showLed(numArray numarr){
	
	for(int i = 0; i < 4; i++){
		uint8_t temp = numarr.chars[3 - i];
		if(selected == 3 - i){
			temp = temp | blink;
		}
		for(int j = 0; j<8; j++){
			if(temp & 0x80){
				digitalWrite(DIO, HIGH);
			}else{
				digitalWrite(DIO, LOW);
			}
			temp = temp << 1;
			digitalWrite(SHIFT, LOW);
			digitalWrite(SHIFT, HIGH);
		}

		uint8_t pos = 0xf7 >> i;

		for(int j = 0; j<8; j++){
			if(pos & 0x80){
				digitalWrite(DIO, HIGH);
			}else{
				digitalWrite(DIO, LOW);
			}
			pos = pos << 1;
			digitalWrite(SHIFT, LOW);
			digitalWrite(SHIFT, HIGH);
		}
		
		digitalWrite(LOAD, LOW);
		digitalWrite(LOAD, HIGH);
		delay(3);
	}
}

numArray num2Array(int number){
	struct numArray numarr; 
	numarr.chars[0] = numbers[number/1000];
	number = number % 1000;
	numarr.chars[1] = numbers[number/100];
	number = number % 100;
	numarr.chars[2] = numbers[number/10] ^ 0x80;
	numarr.chars[3] = numbers[number%10];
	
	return numarr;
}

numArray state2Array(uint8_t phase, bool _state){
	struct numArray numarr;
	numarr.chars[0] = numbers[phase] ^ 0x80;
	
	if(_state == STATE_OFF){
		numarr.chars[1] = numbers[10];
		numarr.chars[2] = numbers[12];
		numarr.chars[3] = numbers[12];
	}else{
		numarr.chars[1] = numbers[13];
		numarr.chars[2] = numbers[10];
		numarr.chars[3] = numbers[11];
	}
	
	return numarr;
}

void reset(){
	state = 0;
	phase = 0;
	countdown = 0;
	setLeds(STATE_OFF, STATE_OFF);	
}

void trigger(){
	reset();
	countdown = time1;
	setLeds(STATE_ON, STATE_OFF);
	digitalWrite(RELAY, phase1);
	resetTimer();
}

void buttonProcess(){
	
	bool b1 = digitalRead(BUTTON1);
	bool b2 = digitalRead(BUTTON2);
		
	if(b1 != button1State){
		if(b1 == LOW){
			if(state == 0){	
				reset();
			}else {
				if(selected == -1){
					state = state == 5? 1 : state + 1;
					if(state == 1) setLeds(STATE_ON, STATE_OFF);
					else if(state == 2) setLeds(STATE_OFF, STATE_ON);
					else setLeds(STATE_OFF, STATE_OFF);
				}else{
					int temp = 0;
					if(state == 1) temp = time1;
					else if(state == 2) temp = time2;
				
					numArray num;
					num.chars[0] = temp / 1000;
					num.chars[1] = (temp%1000) / 100;
					num.chars[2] = (temp%100) / 10;
					num.chars[3] = temp % 10;
					
					num.chars[selected] = num.chars[selected] == 9? 0: num.chars[selected] + 1;
					temp = num.chars[0]*1000 + num.chars[1]*100 + num.chars[2]*10 + num.chars[3];
					
					if(state == 1) time1 = temp;
					else if(state == 2) time2 = temp;
				}
			}
			changeState = false;
		}
		button1State = b1;
		b1tick = millis();
		timeout = millis();
	}else{
		if(state == 0){
			if(b1 == LOW && millis() -  b1tick >= LONG_PRESS){
				changeState = true;
				state = 1;
				setLeds(STATE_ON, STATE_OFF);
				timeout = millis();
			}
		}
	}
	
	if(b2 != button2State){
		if(b2 == LOW){
			if(state == 0){	
				trigger();
			}else if(state == 1 || state == 2){
				selected = selected == 3? -1 : selected + 1;
				blink = 0x00;
				blinkCount = 0;
			}else if(state == 3){
				phase1 = !phase1;
			}else if(state == 4){
				phase2 = !phase2;
			}else if(state == 5){
				phase3 = !phase3;
			}
			changeState = false;
		}
		button2State = b2;
		b2tick = millis();
		timeout = millis();
	}
	
	if(state != 0 && millis() - timeout > TIME_OUT){
		selected = -1;
		reset();
		save();
	}
}

void setLeds(bool led1, bool led2){
	digitalWrite(LED1, led1);
	digitalWrite(LED2, led2);
}

void ledProcess(){
	if(state == 0){
		showLed(num2Array(countdown));
	}else if(state == 1){	// change phase 1 time
		showLed(num2Array(time1));
	}else if(state == 2){	// change phase 2 time
		showLed(num2Array(time2));
	}else if(state == 3){	// change phase state
		showLed(state2Array(1, phase1));
	}else if(state == 4){	// change phase state
		showLed(state2Array(2, phase2));
	}else if(state == 5){	// change phase state
		showLed(state2Array(3, phase3));
	}
}

void setup() {

	pinMode(BUTTON1, INPUT);
	pinMode(BUTTON2, INPUT);
	pinMode(LOAD, OUTPUT);
	pinMode(SHIFT, OUTPUT);
	pinMode(DIO, OUTPUT);
	pinMode(RELAY, OUTPUT);
	pinMode(LED1, OUTPUT);
	pinMode(LED2, OUTPUT);

	
	digitalWrite(LED1, LOW);
	digitalWrite(LED2, LOW);
	digitalWrite(RELAY, STATE_OFF);

	Serial.begin(9600);
	
	timeout = b1tick = b2tick = tick1 = tick2 = millis();
	load();
	setupTimer();
	setLeds(STATE_OFF, STATE_OFF);
}

void loop() {
	
	ledProcess();
	buttonProcess();
	
}
