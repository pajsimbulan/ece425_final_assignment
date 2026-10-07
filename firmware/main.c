/**
 * @file main.c
 *
 * @brief Main source code for the Bluetooth Controlled Music Player (ECE 425 Final Project).
 *
 * The board stores ten short songs and plays them on the buzzer (PC4) by generating
 * square waves at each note's frequency with Timer0, like the sound generation from Lab 2.
 * A song is selected by sending a single character from a phone through the HM-10 BLE
 * module, connected to UART5 (PE4 = RX, PE5 = TX) at 9600 baud. The receive interrupt
 * stores the character, so a new command can also stop or switch songs mid play.
 *
 * Commands (write the character to the HM-10 FFE1 characteristic):
 *   '1' Happy Birthday        '6' Fur Elise
 *   '2' Twinkle Twinkle       '7' When the Saints Go Marching In
 *   '3' Ode to Joy            '8' Row Row Row Your Boat
 *   '4' Mary Had a Little Lamb'9' Frere Jacques
 *   '5' Jingle Bells          '0' The Entertainer
 *   's' stop the current song
 *
 * NOTE: this assumes the default 16 MHz clock (the PLL is never turned on). Timer0
 * makes 1 us ticks out of 16 MHz and the 9600 baud divisors are figured from 16 MHz.
 *
 * @author Paul Simbulan and Kailai Huang
 */

#include "TM4C123GH6PM.h"

//default 4th octave note frequencies (same values as Lab 2)
#define noteC      261.6f
#define noteCSharp 277.2f
#define noteD      293.7f
#define noteEb     311.1f
#define noteE      329.6f
#define noteF      349.2f
#define noteFSharp 370.0f
#define noteG      392.0f
#define noteGSharp 415.3f
#define noteA      440.0f
#define noteBb     466.2f
#define noteB      493.9f
#define REST       0.0f    //silence

//note lengths in milliseconds, change these to speed up or slow down every song at once
#define EIGHTH   200
#define QUARTER  400
#define DOTTEDQ  600
#define HALF     800
#define WHOLE    1600

//one note of a song: which pitch, which octave, and for how long
typedef struct {
	float pitch;   //noteC, noteD, ... or REST
	int octave;    //4 is the middle octave, like in Lab 2
	int duration;  //in milliseconds
} Note;

//counts how many notes are in a song array
#define NUM_NOTES(x) (sizeof(x)/sizeof(x[0]))

//'1' Happy Birthday (same melody we played in Lab 2)
const Note happyBirthday[] = {
	{noteC,4,EIGHTH}, {noteC,4,EIGHTH}, {noteD,4,QUARTER}, {noteC,4,QUARTER}, {noteF,4,QUARTER}, {noteE,4,HALF},
	{noteC,4,EIGHTH}, {noteC,4,EIGHTH}, {noteD,4,QUARTER}, {noteC,4,QUARTER}, {noteG,4,QUARTER}, {noteF,4,HALF},
	{noteC,4,EIGHTH}, {noteC,4,EIGHTH}, {noteC,5,QUARTER}, {noteA,4,QUARTER}, {noteF,4,QUARTER}, {noteE,4,QUARTER}, {noteD,4,HALF},
	{noteBb,4,EIGHTH}, {noteBb,4,EIGHTH}, {noteA,4,QUARTER}, {noteF,4,QUARTER}, {noteG,4,QUARTER}, {noteF,4,HALF},
};

//'2' Twinkle Twinkle Little Star
const Note twinkle[] = {
	{noteC,4,QUARTER}, {noteC,4,QUARTER}, {noteG,4,QUARTER}, {noteG,4,QUARTER}, {noteA,4,QUARTER}, {noteA,4,QUARTER}, {noteG,4,HALF},
	{noteF,4,QUARTER}, {noteF,4,QUARTER}, {noteE,4,QUARTER}, {noteE,4,QUARTER}, {noteD,4,QUARTER}, {noteD,4,QUARTER}, {noteC,4,HALF},
};

//'3' Ode to Joy (Beethoven)
const Note odeToJoy[] = {
	{noteE,4,QUARTER}, {noteE,4,QUARTER}, {noteF,4,QUARTER}, {noteG,4,QUARTER},
	{noteG,4,QUARTER}, {noteF,4,QUARTER}, {noteE,4,QUARTER}, {noteD,4,QUARTER},
	{noteC,4,QUARTER}, {noteC,4,QUARTER}, {noteD,4,QUARTER}, {noteE,4,QUARTER},
	{noteE,4,DOTTEDQ}, {noteD,4,EIGHTH}, {noteD,4,HALF},
	{noteE,4,QUARTER}, {noteE,4,QUARTER}, {noteF,4,QUARTER}, {noteG,4,QUARTER},
	{noteG,4,QUARTER}, {noteF,4,QUARTER}, {noteE,4,QUARTER}, {noteD,4,QUARTER},
	{noteC,4,QUARTER}, {noteC,4,QUARTER}, {noteD,4,QUARTER}, {noteE,4,QUARTER},
	{noteD,4,DOTTEDQ}, {noteC,4,EIGHTH}, {noteC,4,HALF},
};

//'4' Mary Had a Little Lamb
const Note maryLamb[] = {
	{noteE,4,QUARTER}, {noteD,4,QUARTER}, {noteC,4,QUARTER}, {noteD,4,QUARTER},
	{noteE,4,QUARTER}, {noteE,4,QUARTER}, {noteE,4,HALF},
	{noteD,4,QUARTER}, {noteD,4,QUARTER}, {noteD,4,HALF},
	{noteE,4,QUARTER}, {noteG,4,QUARTER}, {noteG,4,HALF},
	{noteE,4,QUARTER}, {noteD,4,QUARTER}, {noteC,4,QUARTER}, {noteD,4,QUARTER},
	{noteE,4,QUARTER}, {noteE,4,QUARTER}, {noteE,4,QUARTER}, {noteE,4,QUARTER},
	{noteD,4,QUARTER}, {noteD,4,QUARTER}, {noteE,4,QUARTER}, {noteD,4,QUARTER},
	{noteC,4,WHOLE},
};

//'5' Jingle Bells (chorus)
const Note jingleBells[] = {
	{noteE,4,QUARTER}, {noteE,4,QUARTER}, {noteE,4,HALF},
	{noteE,4,QUARTER}, {noteE,4,QUARTER}, {noteE,4,HALF},
	{noteE,4,QUARTER}, {noteG,4,QUARTER}, {noteC,4,QUARTER}, {noteD,4,QUARTER}, {noteE,4,WHOLE},
	{noteF,4,QUARTER}, {noteF,4,QUARTER}, {noteF,4,QUARTER}, {noteF,4,EIGHTH}, {noteF,4,EIGHTH},
	{noteF,4,QUARTER}, {noteE,4,QUARTER}, {noteE,4,QUARTER}, {noteE,4,EIGHTH}, {noteE,4,EIGHTH},
	{noteE,4,QUARTER}, {noteD,4,QUARTER}, {noteD,4,QUARTER}, {noteE,4,QUARTER}, {noteD,4,HALF}, {noteG,4,HALF},
	{noteE,4,QUARTER}, {noteE,4,QUARTER}, {noteE,4,HALF},
	{noteE,4,QUARTER}, {noteE,4,QUARTER}, {noteE,4,HALF},
	{noteE,4,QUARTER}, {noteG,4,QUARTER}, {noteC,4,QUARTER}, {noteD,4,QUARTER}, {noteE,4,WHOLE},
	{noteF,4,QUARTER}, {noteF,4,QUARTER}, {noteF,4,QUARTER}, {noteF,4,EIGHTH}, {noteF,4,EIGHTH},
	{noteF,4,QUARTER}, {noteE,4,QUARTER}, {noteE,4,QUARTER}, {noteE,4,EIGHTH}, {noteE,4,EIGHTH},
	{noteG,4,QUARTER}, {noteG,4,QUARTER}, {noteF,4,QUARTER}, {noteD,4,QUARTER}, {noteC,4,WHOLE},
};

//'6' Fur Elise (Beethoven), the famous opening phrase
const Note furElise[] = {
	{noteE,5,EIGHTH}, {noteEb,5,EIGHTH}, {noteE,5,EIGHTH}, {noteEb,5,EIGHTH},
	{noteE,5,EIGHTH}, {noteB,4,EIGHTH}, {noteD,5,EIGHTH}, {noteC,5,EIGHTH},
	{noteA,4,QUARTER}, {REST,4,EIGHTH}, {noteC,4,EIGHTH}, {noteE,4,EIGHTH}, {noteA,4,EIGHTH},
	{noteB,4,QUARTER}, {REST,4,EIGHTH}, {noteE,4,EIGHTH}, {noteGSharp,4,EIGHTH}, {noteB,4,EIGHTH},
	{noteC,5,QUARTER}, {REST,4,EIGHTH}, {noteE,4,EIGHTH},
	{noteE,5,EIGHTH}, {noteEb,5,EIGHTH}, {noteE,5,EIGHTH}, {noteEb,5,EIGHTH},
	{noteE,5,EIGHTH}, {noteB,4,EIGHTH}, {noteD,5,EIGHTH}, {noteC,5,EIGHTH},
	{noteA,4,QUARTER}, {REST,4,EIGHTH}, {noteC,4,EIGHTH}, {noteE,4,EIGHTH}, {noteA,4,EIGHTH},
	{noteB,4,QUARTER}, {REST,4,EIGHTH}, {noteE,4,EIGHTH}, {noteC,5,EIGHTH}, {noteB,4,EIGHTH},
	{noteA,4,HALF},
};

//'7' When the Saints Go Marching In
const Note saints[] = {
	{noteC,4,QUARTER}, {noteE,4,QUARTER}, {noteF,4,QUARTER}, {noteG,4,HALF}, {REST,4,QUARTER},
	{noteC,4,QUARTER}, {noteE,4,QUARTER}, {noteF,4,QUARTER}, {noteG,4,HALF}, {REST,4,QUARTER},
	{noteC,4,QUARTER}, {noteE,4,QUARTER}, {noteF,4,QUARTER}, {noteG,4,QUARTER},
	{noteE,4,QUARTER}, {noteC,4,QUARTER}, {noteE,4,QUARTER}, {noteD,4,HALF}, {REST,4,QUARTER},
	{noteE,4,QUARTER}, {noteE,4,QUARTER}, {noteD,4,QUARTER}, {noteC,4,HALF},
	{noteC,4,QUARTER}, {noteE,4,QUARTER}, {noteG,4,QUARTER}, {noteG,4,QUARTER}, {noteG,4,QUARTER}, {noteF,4,HALF}, {REST,4,QUARTER},
	{noteE,4,QUARTER}, {noteF,4,QUARTER}, {noteG,4,QUARTER}, {noteE,4,QUARTER},
	{noteC,4,QUARTER}, {noteD,4,QUARTER}, {noteC,4,HALF},
};

//'8' Row Row Row Your Boat
const Note rowYourBoat[] = {
	{noteC,4,QUARTER}, {noteC,4,QUARTER}, {noteC,4,EIGHTH}, {noteD,4,EIGHTH}, {noteE,4,QUARTER},
	{noteE,4,EIGHTH}, {noteD,4,EIGHTH}, {noteE,4,EIGHTH}, {noteF,4,EIGHTH}, {noteG,4,HALF},
	{noteC,5,EIGHTH}, {noteC,5,EIGHTH}, {noteC,5,EIGHTH},
	{noteG,4,EIGHTH}, {noteG,4,EIGHTH}, {noteG,4,EIGHTH},
	{noteE,4,EIGHTH}, {noteE,4,EIGHTH}, {noteE,4,EIGHTH},
	{noteC,4,EIGHTH}, {noteC,4,EIGHTH}, {noteC,4,EIGHTH},
	{noteG,4,EIGHTH}, {noteF,4,EIGHTH}, {noteE,4,EIGHTH}, {noteD,4,EIGHTH}, {noteC,4,HALF},
};

//'9' Frere Jacques
const Note frereJacques[] = {
	{noteC,4,QUARTER}, {noteD,4,QUARTER}, {noteE,4,QUARTER}, {noteC,4,QUARTER},
	{noteC,4,QUARTER}, {noteD,4,QUARTER}, {noteE,4,QUARTER}, {noteC,4,QUARTER},
	{noteE,4,QUARTER}, {noteF,4,QUARTER}, {noteG,4,HALF},
	{noteE,4,QUARTER}, {noteF,4,QUARTER}, {noteG,4,HALF},
	{noteG,4,EIGHTH}, {noteA,4,EIGHTH}, {noteG,4,EIGHTH}, {noteF,4,EIGHTH}, {noteE,4,QUARTER}, {noteC,4,QUARTER},
	{noteG,4,EIGHTH}, {noteA,4,EIGHTH}, {noteG,4,EIGHTH}, {noteF,4,EIGHTH}, {noteE,4,QUARTER}, {noteC,4,QUARTER},
	{noteC,4,QUARTER}, {noteG,3,QUARTER}, {noteC,4,HALF},
	{noteC,4,QUARTER}, {noteG,3,QUARTER}, {noteC,4,HALF},
};

//'0' The Entertainer (Scott Joplin), main theme
const Note entertainer[] = {
	{noteD,4,EIGHTH}, {noteEb,4,EIGHTH}, {noteE,4,EIGHTH}, {noteC,5,QUARTER},
	{noteE,4,EIGHTH}, {noteC,5,QUARTER}, {noteE,4,EIGHTH}, {noteC,5,HALF}, {REST,4,EIGHTH},
	{noteC,5,EIGHTH}, {noteD,5,EIGHTH}, {noteEb,5,EIGHTH}, {noteE,5,EIGHTH},
	{noteC,5,EIGHTH}, {noteD,5,EIGHTH}, {noteE,5,QUARTER},
	{noteB,4,EIGHTH}, {noteD,5,QUARTER}, {noteC,5,HALF},
};

/**
 * 'command' is shared between the UART5 interrupt handler (which writes it) and
 * main (which reads it), so it has to be volatile or the compiler could keep
 * using an old copy of it inside the while loop. 0 means no command waiting.
 */
volatile char command = 0;

//microsecond delay using Timer0, same as Lab 2 (Timer0 makes a 1 us tick at 16 MHz)
void delayUs(int ttime) {
	TIMER0->ICR = 0x1;
	for(int i=0; i<ttime; i++) {
		while((TIMER0->RIS & 0x1) == 0); // wait for TimerA timeout flag
		TIMER0->ICR = 0x1;
	}
}

//millisecond delay built out of the microsecond delay
void delayMs(int n) {
	for(int i=0; i<n; i++) {
		delayUs(1000);
	}
}

/**
 * Plays one note on the buzzer (PC4) as a square wave, like Lab 2, but with a
 * duration in milliseconds so every note lasts the right amount of time no
 * matter how high or low it is.
 */
void playNote(float note, int octave, int duration)
{
	float freq = note;

	//a REST means silence, keep the buzzer off for the whole duration
	if(note == REST) {
		GPIOC->DATA &= ~0x10;
		delayMs(duration);
		return;
	}

	//shift the 4th octave base frequency down or up to the requested octave
	if(octave < 4) {
		for(int i=4; i> octave; i--) freq = (freq/2);
	}
	if(octave > 4) {
		for(int i=4; i< octave; i++) freq = (freq*2);
	}

	float periodSeconds = (1/freq);
	float periodMicroSeconds = periodSeconds*1000000.0f;
	float halfPeriodNote = periodMicroSeconds/2;

	//sound the tone for 90% of the duration and stay quiet for the last 10%,
	//that little gap makes repeated notes (like E E E) sound separate
	int toneTime = (duration*9)/10;
	int cycles = (int)((toneTime*1000.0f)/periodMicroSeconds);

	for(int j=0; j<cycles; j++) {
		GPIOC->DATA |= 0x10;
		delayUs(halfPeriodNote);
		GPIOC->DATA &= ~0x10;
		delayUs(halfPeriodNote);
	}
	delayMs(duration - toneTime);
}

/**
 * Plays a whole song. Before every note it checks if a new bluetooth command
 * came in, and if one did it returns right away, so we can stop or switch songs
 * in the middle instead of waiting for the song to finish.
 */
void playSong(const Note song[], int length)
{
	for(int i=0; i<length; i++) {
		if(command != 0) return; //new command arrived, stop this song
		playNote(song[i].pitch, song[i].octave, song[i].duration);
	}
}

/**
 * Runs every time the HM-10 sends us a character (same idea as the Lab 6 handler).
 *
 * IMPORTANT: we clear ALL of the UART interrupt flags at the end, not just the RX
 * one. The UART shares a single interrupt line for many sources (receive, receive
 * timeout, overrun error, framing error, ...). If one of those extra flags gets set
 * (an overrun is easy to cause over Bluetooth) and we never clear it, the line stays
 * asserted and this handler fires over and over, so main() never gets to run and no
 * song plays. This is the same trap Lab 3 warns about: if you do not clear the flag
 * you end up stuck in an infinite loop in the ISR.
 */
void UART5_Handler(void) {
	// if the receive interrupt fired, read the character (reading DR clears RX)
	if (UART5->MIS & (1 << 4)) {
		char ch = UART5->DR;
		if(ch != '\r' && ch != '\n') command = ch;
	}
	// clear every UART interrupt source so the handler can't get stuck re-firing
	UART5->ICR = 0x7F0;
}

int main(void) {

	//PORT C Configuration (buzzer on PC4, same pin as Lab 2)
	SYSCTL->RCGCGPIO |= 0x04;  //GPIOC clock enabled
	GPIOC->AMSEL &= ~0x10;  //disable analog function
	GPIOC->DIR |= 0x10;  //SET Port C 4th bit (PC4) as output pin
	GPIOC->DEN |= 0x10;  //SET PC4 as digital pin

	//Configuring Timer0 (makes the 1 us ticks that delayUs counts)
	SYSCTL->RCGCTIMER |= 1;  //Drive clock on Timer0
	TIMER0->CTL = 0; //Temporarily Disable Timer0 for configuration
	TIMER0->CFG = 0x04; //Set Timer0 for 16 bit operation
	TIMER0->TAMR = 0x02; //set Timer0 as periodic down counter
	TIMER0->TAILR = 16-1; // load Timer0 register for 16 ticks = 1 us at 16 MHz
	TIMER0->ICR = 0x1;  //Clears Timer0 timeout flag
	TIMER0->CTL |= 0x01; //re-enable Timer0, done with configuration

	//Configuring UART5 for the HM-10 bluetooth module
	SYSCTL->RCGCUART |= (1<<5); //Enable clock to UART5 module.
	SYSCTL->RCGCGPIO |= 0x10; //Enable clock to GPIO Port E

	//Initialize PE4 and PE5 for UART module 5. PE4 is the receive (RX) pin that
	//listens to the HM-10 TXD pin, and PE5 is the transmit (TX) pin wired to the
	//HM-10 RXD pin.
	GPIOE->AFSEL |= (1<<4) | (1<<5);
	GPIOE->PCTL &= ~0x00FF0000; //clear the PE4 and PE5 mux fields first
	GPIOE->PCTL |= (1<<16) | (1<<20); //select U5RX and U5TX as the alternate function
	GPIOE->DEN |= (1<<4) | (1<<5);
	GPIOE->AMSEL &= ~((1<<4) | (1<<5));

	//Disable UART5 module during configuration by clearing the first bit of the
	//UART control register.
	UART5->CTL &= ~(1<<0);

	//Set the UART5 baud rate to 9600 (the HM-10 default) for a 16 MHz clock:
	//16000000 / (16 * 9600) = 104.1667, so IBRD = 104 and FBRD = round(0.1667*64) = 11.
	UART5->IBRD = 104;
	UART5->FBRD = 11;

	//Use the system clock for the UART5 module.
	UART5->CC = 0;

	//8 bits of data, no parity, 1 stop bit.
	UART5->LCRH = 0x60;

	//Enable the UART5 module and its RX and TX (bits 0, 8, and 9).
	UART5->CTL |= (1<<0) | (1<<8) | (1<<9);

	//Give the UART a moment to settle, then clear out any stale byte so a leftover
	//value does not fire a bogus interrupt the instant we enable it (matches Lab 6).
	delayMs(50);
	UART5->DR = 0;

	//Clear any pending flags, THEN enable the RX interrupt.
	UART5->ICR = 0x7F0;
	UART5->IM |= (1<<4);

	//Set priority to 3 and enable the UART5 interrupt (IRQ 61) in the NVIC. IRQ 61 is
	//past 31 so its enable bit is in ISER[1] (covers 32-63), and its priority byte is
	//IPR[61] (IPR is the byte array indexed by interrupt number in this CMSIS header).
	NVIC->IPR[61] = 3 << 5;          // Priority 3
	NVIC->ISER[1] |= 1 << (61-32);   // Enable IRQ 61

	//quick two note chirp on startup so we know the board reset and the buzzer works 
	playNote(noteA, 4, 120);
	playNote(noteE, 5, 160);

	while(1) {

		if(command != 0) {
			char ch = command; //grab the most recent command from the interrupt
			command = 0; //mark it as handled so playSong does not instantly stop

			switch(ch) {
				case '1': playSong(happyBirthday, NUM_NOTES(happyBirthday)); break;
				case '2': playSong(twinkle,       NUM_NOTES(twinkle));       break;
				case '3': playSong(odeToJoy,      NUM_NOTES(odeToJoy));      break;
				case '4': playSong(maryLamb,      NUM_NOTES(maryLamb));      break;
				case '5': playSong(jingleBells,   NUM_NOTES(jingleBells));   break;
				case '6': playSong(furElise,      NUM_NOTES(furElise));      break;
				case '7': playSong(saints,        NUM_NOTES(saints));        break;
				case '8': playSong(rowYourBoat,   NUM_NOTES(rowYourBoat));   break;
				case '9': playSong(frereJacques,  NUM_NOTES(frereJacques));  break;
				case '0': playSong(entertainer,   NUM_NOTES(entertainer));   break;
				case 's': break; //stop, play nothing
				default: break; //not a command we know, ignore it
			}

			GPIOC->DATA &= ~0x10; //make sure the buzzer ends up off
		}

	}//end of while

}//end of main