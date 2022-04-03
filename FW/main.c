/*
 * turnsig2.c
 *
 * Created: 11.02.2019 15:49:34
 * Author : Adam Borowski
 */ 
#include <stdio.h>
#include <stdlib.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#define MAX_PWM 199
#define SEQ_DELAY 50
#define BLINKING_TOUT	20

typedef enum {
	STATE_INIT,
	STATE_POWERING_UP,
	STATE_FULL_ON,
	STATE_FADING_OUT,
	STATE_FULL_OFF,
	STATE_FADING_IN,
} eSysState;

typedef struct {
	uint8_t ena;
	uint8_t len;
	uint8_t ptr;
	uint8_t *seq;
} sequence;

eSysState state = STATE_INIT;
volatile uint8_t delay = 0;
volatile uint8_t blinking = 0;
volatile sequence posSeq  = { 0, 0, 0 };
volatile sequence stopSeq = { 0, 0, 0 };

static const uint8_t pwrUpDelaySeq[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t pwrUpBlinkSeq[] = {  0, 0, MAX_PWM, 0, 0, MAX_PWM, 0, 0, MAX_PWM, 0, 0, 0, 0, 0, 0 };
static const uint8_t fadeInSeq[] = { 0, 0, 1, 2, 4, 8, 12, 18, 24, 32, 40, 50, 60, 72, 84, 98, 112, 128, 144, 162, 180, MAX_PWM };

uint8_t getBlinkDet(void) {
	return 0;
}

void inline setPwmStop(uint8_t pwm) {
	OCR0A = pwm;
}

void inline setPwmPos(uint8_t pwm) {
	OCR0B = pwm;
}

void configureIO(void) {
	DDRB = (1 << DDB1) | (1 << DDB0);
	PORTB &= ~((1 << DDB1) | (1 << DDB0));
	PCMSK = (1 << PCINT2);
	PCICR = (1 << PCIE0);
}

void configureTimer(void) {
	TCCR0A = (1 << WGM01) | (0 << WGM00);
	TCCR0B = (1 << WGM03) | (1 << WGM02);
	ICR0 = MAX_PWM;
	TCNT0 = 0;
	OCR0A = 0;
	OCR0B = 0;
	TIMSK0 = (1 << TOIE0);
	TCCR0B |= (1 << CS01) | (1 << CS00);
}

ISR(PCINT0_vect) {
	blinking = BLINKING_TOUT;
}

ISR(TIM0_OVF_vect) {
	++delay;
	if (OCR0A == 0) {
		TCCR0A &= ~(1 << COM0A1);
		} else {
		TCCR0A |= (1 << COM0A1);
	}
	if (OCR0B == 0) {
		TCCR0A &= ~(1 << COM0B1);
		} else {
		TCCR0A |= (1 << COM0B1);
	}

	if (delay == SEQ_DELAY) {
		delay = 0;

		if (posSeq.ena) {
			if (posSeq.ptr < posSeq.len) {
				setPwmPos(posSeq.seq[posSeq.ptr]);
				++posSeq.ptr;
			} else {
				posSeq.ena = 0;
			}
		}

		if (stopSeq.ena) {
			if (stopSeq.ptr < stopSeq.len) {
				setPwmStop(stopSeq.seq[stopSeq.ptr]);
				++stopSeq.ptr;
			} else {
				stopSeq.ena = 0;
			}
		}
		if (blinking) {
			--blinking;
		}
	}
}

uint8_t isSeqRunning(void) {
	return (posSeq.ena || stopSeq.ena);
}

void setPosSeq(uint8_t* seq, uint8_t len) {
	posSeq.ena = 0;
	posSeq.ptr = 0;
	posSeq.len = len;
	posSeq.seq = seq;
}

void setStopSeq(uint8_t* seq, uint8_t len) {
	stopSeq.ena = 0;
	stopSeq.ptr = 0;
	stopSeq.len = len;
	stopSeq.seq = seq;
}

void runSeq(void) {
	posSeq.ena = 1;
	stopSeq.ena = 1;	
}

void abortSeq(void) {
	posSeq.ena = 0;
	stopSeq.ena = 0;
}


int main(int argc, char** argv)
{
	CCP = 0xD8;
	CLKPSR = 0x00;
	configureIO();
	configureTimer();

	setPwmStop(0);
	setPwmPos(0);
	
	sei();

  while (1) 
  {
		switch(state) {
			case STATE_INIT:
				setPosSeq((uint8_t*)pwrUpDelaySeq, sizeof(pwrUpDelaySeq));
				runSeq();
				while(isSeqRunning()) {}
				setPosSeq((uint8_t*)pwrUpBlinkSeq, sizeof(pwrUpBlinkSeq));
				runSeq();
				while(isSeqRunning()) {}
				state = STATE_POWERING_UP;
				break;

			case STATE_POWERING_UP:
				setPosSeq((uint8_t*)fadeInSeq, sizeof(fadeInSeq));
				setStopSeq((uint8_t*)fadeInSeq, sizeof(fadeInSeq));
				runSeq();
				while(isSeqRunning()) {}
				state = STATE_FULL_ON;
				break;

			case STATE_FULL_ON:
				if (blinking) {
					state = STATE_FADING_OUT;
				}
				break;

			case STATE_FADING_OUT:
				setPwmStop(0);
				setPwmPos(0);
				state = STATE_FULL_OFF;
				break;

			case STATE_FULL_OFF:
				if (!blinking) {
					state = STATE_FADING_IN;
				}
				break;

			case STATE_FADING_IN:
				setPosSeq((uint8_t*)fadeInSeq, sizeof(fadeInSeq));
				setStopSeq((uint8_t*)fadeInSeq, sizeof(fadeInSeq));
				runSeq();
				state = STATE_FULL_ON;
				while(isSeqRunning()) {
					if (blinking) {
						abortSeq();
						state = STATE_FADING_OUT;
						break;
					}
				}
				break;
		};
  }
  return (EXIT_SUCCESS);
}
