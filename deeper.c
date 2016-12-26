/*
 *****************************************************************************
 *
 * Purpose:
 * DEEPER THOUGHT
 *  - This program simulates the appearance of the famous program
 *    deep thought for the PDP-8, on the PiDP-8 kit created by
 *    Oscar Vermeulen -- see his kit at
 *       http://obsolescence.wix.com/obsolescence#!pidp-8
 *    The operation of this program was copied from the programs I've seen
 *    on videos from YouTube
 *
 * This program was created because the "full blown" simH PDP-8 emulator
 * uses 100% of the CPU.  This application only uses 12%
 *
 * NOTE: This program must be run as superuser
 *
 * The file gpio.c and gpio.h were copied from Oscar's PDP-8/simH project
 *
 * Modification Log:
 * Rev.  Date       By                 Description
 * ------------------------------------------------------------------------
 * 1.0   2016.02.23 Norman Davie       Initial release
 * 2.0   2016.02.26 Tim Wells          See details below
 * 2.01  2016.03.09 David C. Eilering  Minor bug fixes related to Link light
 * 2.10  2016.03.12 David C. Eilering  Added Pong mode and minor bug fixes
 * 2.20  2016.03.29 David C. Eilering  Added Text scroller mode
 * 2.21  2016.03.30 David C. Eilering  Added loadMsg function
*  2.xx  2016.12.26 Anthony L. Hill    Added 24x7 time based pattern changes
 *****************************************************************************
 * 	Version 2.0 by Tim Wells
 *
 * 	Added modes by changing the 3 far left brown switches (0=down / 1=up)
 * 		111 = Normal mode with all LEDs flashing (Default / Undefined fallback)
 * 		011 = Sleep Mode (All LEDs off except for the columns on the right side of the panel)
 * 		101 = Dim Mode - Fewer LEDs Blink (Only the Program Counter, Memory Address and Memory Buffer groups)
 * 		110 = Binary Clock (From top to bottom: Hour, Minute, Second, Month, Day)
 * 		001 = Snake Mode (3 LEDs move across a row then down to the next row in the opposite direction)
 * 		000 = Test Mode (All LEDs on steady, except some of the columns of LEDs on the right blink off for 20ms)
 *		010 = Pong Mode (Bouncing Ball)
 *		100 = Text scroller
 *
 * 	Expanded the timing switches from 6 to 12 switches
 * 		The third brown and third white switch groups control the maximum delay (slowest speed)
 * 			Up   = More delay (slower)
 * 			Down = Less delay (faster)
 * 			The value from the switches is multiplied by 50,000us (50ms).
 * 		The second brown and second white switch groups control the variability of the timing
 * 			Up   = More variability
 * 			Down = Less variability (all down results in steady timing)
 * 			The varied amount is subtracted from the max delay, making the LEDs change faster.
 * 		The timing range should be the similar to version 1.0, but with more degrees of change
 * 			The maximum delay switch mask value has 1 added to it to prevent a delay of 0, which crashed the program.
 * 			The delay range (before variability) is 50ms to 3200ms
 *
 * 	Changed the behavior of the LED columns on the right side of the panel
 * 		All LEDs in the left column blink randomly.
 * 			Some of these LEDs are programmed to flash more often than others (see the rand_flag function).
 * 		The left column of LEDs are turned off for 20ms at the end of each cycle.
 *			This gives the left column a short blink even if that LED stays on in the next cycle.
 * 			The 20ms delay is subtracted from the main blink delay to keep the same timing.
 * 		Instead of toggling the execute LED, it blinks for 20ms.
 * 			It is turned on at the beginning of the cycle and turned off before the 20ms delay described above.
 *
 * 	Changed the stop switch so that it must be held for 3 seconds to quit the program
 *
 * 	Added new command sequences:
 * 		Shutdown system - Flip both the Sing Inst and Sing Step switches down and hold the Stop button for 3 seconds
 * 		Reboot system - Flip both the Sing Inst and Sing Step switches down and hold the Start button for 3 seconds
 *
 * 	Misc Notes:
 *		Added console output that shows switch values when the switches change.
 * 		The blink delay is fixed to 1/2 second in Binary Clock mode.
 * 		This should not be run simultaneously with the pidp8 simulator
 *
 *	Installation
 *  	To install run "sudo ./install_deeper.sh" in the deeper directory (also builds)
 * 		The install script enables auto-start and disables auto-start for the pidp8 simulator
 *  	To install without enabling auto-start, add the "--no-autostart" parameter
 *  	To later disable auto-start and restore the pidp8 simulator auto-start, add the "--restore-pidp8" parameter
 * 		To just build run "make" in the deeper directory.
 *
 *	Running Deeper Thought 2
 * 		Stop the pidp8 simulator before running this (sudo /etc/init.d/pidp8 stop)
 *  	To run as a daemon in the background:
 *			sudo /etc/init.d/deeper {start|stop|restart|status}
 *		To run in the terminal window run:
 *			sudo /usr/bin/deeper
 *
 *****************************************************************************
 * 	Version 2.01 by David C. Eilering
 *
 *  Minor bug fixes related to the Link light
 *
 *  When switching from other modes to the Binary Clock mode (6) or the Dim mode (5), the Link light
 *  isn't being reset.  If the Link light is on when the mode is switched, the Link light stays on
 *  when the Binary Clock or Dim mode starts.  A consistent example of this bug can be seen by
 *  switching from the Test mode (0) to the Binary Clock mode or Dim mode.  When switching from Normal
 *  mode the bug only occurs if the Link light happened to be on when the mode is switched.  Resetting
 *  the Link light in the Binary Clock mode and the Dim mode fixes this bug.
 *****************************************************************************
 * 	Version 2.10 by David C. Eilering
 *
 *  Removed random delay in Snake mode
 *  Added Pong mode (2) (written by Norman Davie)
 *  Added feature to Pong mode to allow the switches to control the ball speed
 *****************************************************************************
 * 	Version 2.20 by David C. Eilering
 *
 *  Added Text scroller mode (4)
 *
 *  The Text scroller mode scrolls a text message using the 5 rows of 12 LEDs
 *  (Program Counter, Memory Address, Memory Buffer, Accumulator, and
 *  Multiplier Quotient).  The font used is a 5x5 font (with some letters not
 *  using the full 5 pixel width).  The font includes all symbols from ASCII
 *  32 (space) to ASCII 126 (tilde).  Characters outside of this range are
 *  ignored.  When the message has finished scrolling, it repeats from the
 *  beginning.  The program looks for a text file /home/pdp/scrollText.txt  If
 *  the file is not found, a hard-coded welcome message is displayed.  If the
 *  file is found the text is loaded from all lines into a single long string
 *  of characters.  The characters are run together from one line to the next.
 *  So, if a space is desired between lines, add a space to the end of the
 *  line.  For example:
 *
 *  This is a test.
 *
 *  is the same as (note the spaces at the beginning and end of some lines):
 *
 *  Th
 *  is is
 *  a
 *   te
 *  st.
 *
 *  This allows very specific spacing of the message.  This also allows a
 *  a message to contain a delay by adding a series of spaces in the message.
 *  Since the message repeats, it's likely that 3 or 4 spaces are desired to
 *  clear the display before the message starts again.
 *
 *  As with other modes that allow variable speed, the front panel switches
 *  control the speed of the scrolling.
 *****************************************************************************
 * 	Version 2.21 by David C. Eilering
 *
 *  Added loadMsg function
 *
 *  Putting the file reading code into a function allows the loadMsg function
 *  to be called within the main loop of the program.  This allows the message
 *  to be modified by changing the file while the program is running.
 *
 *  Also changed the hard-coded message to mostly uppercase to improve
 *  readability.  Added the date/time at the end of the message as a way to
 *  demonstrate how messages can be changed dynamically.
 *****************************************************************************
 * 	Version 2.xx by Anthony Hill
 *
 *  Changed switch mode 5 to a 24x7 changing pattern mode
 *
 *****************************************************************************
 */

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

typedef unsigned int    uint32;
typedef signed int      int32;
typedef unsigned short  uint16;
typedef unsigned char   uint8;

extern void *blink(void *ptr);	// the real-time multiplexing process to start up
extern uint32 ledstatus[8];     // bitfields: 8 ledrows of up to 12 LEDs
extern uint32 switchstatus[3];  // bitfields: 3 rows of up to 12 switches

#include <signal.h>
#include <ctype.h>

// 24x7 MODE SETTINGS (101)
#define s_morning     8*60
#define s_afternoon  12*60
#define s_evening    17*60
#define s_night      22*60

#define d_slow     001
#define d_medium   003
#define d_fast     037
#define d_burst    077

#define f_seldom   002
#define f_normal   017
#define f_fast     020

#define o_long     077
#define o_normal   007
#define o_fast     003

struct schedule_settings {

	uint16 start ;
	uint16 pgm_duration ;
	uint16 pgm_frequency ;
	uint16 pgm_delay ;

}   schedule[] = {
	{ 0            , d_slow   , f_seldom , o_long   },
	{ s_morning    , d_medium , f_normal , o_normal },
	{ s_afternoon  , d_fast   , f_fast   , o_fast   },
	{ s_evening    , d_medium , f_normal , o_long   },
	{ s_night      , d_slow   , f_seldom , o_long   }
} ;


// GET / STORE             row   shift  mask value
int programCounter[] 	= {0x00, 0,     07777};
int dataField[] 	= {0x07, 9,     07};
int instField[] 	= {0x07, 6,     07};
int linkLED[]           = {0x07, 5,     01};
int memoryAddress[]     = {0x01, 0,     07777};
int memoryBuffer[]      = {0x02, 0,     07777};
int accumulator[]       = {0x03, 0,     07777};
int multiplierQuotient[]= {0x04, 0,     07777};
int andLED[]            = {0x05, 11,    01};
int tadLED[]            = {0x05, 10,    01};
int iszLED[]            = {0x05, 9,     01};
int dcaLED[]            = {0x05, 8,     01};
int jmsLED[]            = {0x05, 7,     01};
int iotLED[]            = {0x05, 5,     01};
int jmpLED[]            = {0x05, 6,     01};
int oprLED[]            = {0x05, 4,     01};
int fetchLED[]          = {0x05, 3,     01};
int executeLED[]        = {0x05, 2,     01};
int deferLED[]          = {0x05, 1,     01};
int wordCountLED[]      = {0x05, 0,     01};
int currentAddressLED[] = {0x06, 11,    01};
int breakLED[]          = {0x06, 10,    01};
int ionLED[]            = {0x06, 9,     01};
int pauseLED[]          = {0x06, 8,     01};
int runLED[]            = {0x06, 7,     01};
int stepCounter[]       = {0x06, 0,     0177};

// GETSWOTCJ
int singInst[]          = {0x02, 4,     01};
int singStep[]          = {0x02, 5,     01};
int stop[]              = {0x02, 6,     01};
int cont[]              = {0x02, 7,     01};
int exam[]              = {0x02, 8,     01};
int dep[]               = {0x02, 9,     01};
int loadAdd[]           = {0x02, 10,    01};
int start[]             = {0x02, 11,    01};

// GETSWITCHES
int swregister[]        = {0x00, 0,     07777};
int step[]              = {0x01, 6,     077};

// STORE
// 1) clamps the maximum value via an and mask
// 2) shifts the value to the appropriate area of within the uint
// 3) masks out the value that was previously there without effecting other bits
// 4) or's the new value in place
#define STORE(item, value) { ledstatus[item[0]] =  (ledstatus[item[0]] & ~(item[2] << item[1])  ) |  ((value & item[2]) << item[1]); }

// GET
// 1) gets shifts the value to the "normal" range
// 2) masks off bits that are not related to our value
#define GET(item)          ( (ledstatus    [ item[0] ] >> item[1]) & item[2] )
#define GETSWITCH(flip)   !( (switchstatus [ flip[0] ] >> flip[1]) & flip[2] )
#define GETSWITCHES(flip)  ( (switchstatus [ flip[0] ] >> flip[1]) & flip[2] )


int terminate=0;

int opled_delay = 20000;


// Handle CTRL-C
void sig_handler( int signo )
{
  if( signo == SIGINT )
	terminate = 1;
}

// Random flag value with a fixed probability
// Creates a random number between 1 and max_rand
// If the random number is <= max_true, TRUE is returned
// Example:	rand_flag(100, 60) should give you:
//			60% probability true / 40% probability false
int rand_flag( int max_rand, int max_true )
{
	int rand_value;
	rand_value = (rand() % max_rand) + 1;
	if(rand_value <= max_true)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void ClearAllLEDs()	// function added by Norman Davie - used by Pong mode
{
	STORE(programCounter,    0);
	STORE(dataField,         0);
	STORE(instField,         0);
	STORE(linkLED,           0);
	STORE(programCounter,    0);
	STORE(memoryAddress,     0);
	STORE(memoryBuffer,      0);
	STORE(accumulator,       0);
	STORE(multiplierQuotient,0);
	STORE(stepCounter,       0);
	STORE(andLED,            0);
	STORE(tadLED,            0);
	STORE(iszLED,            0);
	STORE(dcaLED,            0);
	STORE(jmsLED,            0);
	STORE(iotLED,            0);
	STORE(jmpLED,            0);
	STORE(oprLED,            0);
	STORE(fetchLED,          0);
	STORE(executeLED,        0);
	STORE(deferLED,          0);
	STORE(wordCountLED,      0);
	STORE(currentAddressLED, 0);
	STORE(breakLED,          0);
	STORE(ionLED,            0);
	STORE(pauseLED,          0);
	STORE(runLED,            0);
}

void loadMsg(char *tmpMsg) {
	char my_line[256];
	char *my_fname = "/home/pdp/scrollText.txt";
	FILE *my_file;
	my_file = fopen(my_fname, "r");
	time_t t = time(NULL);

	strcpy(tmpMsg, "");

	if (my_file == NULL) {
		strcat(tmpMsg, "WELCOME TO THE PiDP-8/I TEXT SCROLLER BY DAVID C. EILERING.   ");
		strcat(tmpMsg, "TO CUSTOMIZE THIS MESSAGE, ENTER THE DESIRED TEXT INTO THE FILE ");
		strcat(tmpMsg, my_fname);
		strcat(tmpMsg, "   ");
		strcat(tmpMsg, "FOR MORE INFORMATION, VISIT https://github.com/VentureKing/Deeper-Thought-2   ");
		strcat(tmpMsg, "THE CURRENT DATE/TIME IS: ");
		strcat(tmpMsg, asctime(localtime(&t)));
		strcat(tmpMsg, "   ");
	}
	else {
		while (fgets(my_line, sizeof(my_line), my_file)) {
			// replace carriage return with null
			for (int i = 255; i > 0; i--) {
				if (my_line[i] == 0x0A) {
					my_line[i] = 0x00;
				}
			}

			strcat(tmpMsg, my_line);
		}
		fclose(my_file);
	}
}


int main( int argc, char *argv[] )
{
  pthread_t     thread1;
  int           iret1;
  unsigned long sleepTime;
  int           deeperThoughtMode = 0;
  int           dontChangeLEDs = 0;
  unsigned long delayAmount;
  unsigned long varietyAmount;
  unsigned long varietyMult;
  unsigned long swRegValue;
  unsigned long swStepValue;
  unsigned long stopPressedTime;
  unsigned long startPressedTime;
  int swIfValue;
  time_t currentTime;
  struct tm *localTime;
  int hour;
  int min;
  int sec;
  int x, y, shift_dir;

  x = 1;
  y = 1;
  shift_dir = 1;

  swRegValue = 0;
  swStepValue = 0;

  int burst_timer=0;
  int off_timer=0;

  char chars[] = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

	int letters[95] = {
		0x00000003, 0x000000B9, 0x0000601B, 0x057D5F55, 0x04D7F595,
		0x0888888D, 0x08225555, 0x00000019, 0x00001172, 0x00000E8A,
		0x0513E455, 0x00008E23, 0x00000882, 0x00008423, 0x00000081,
		0x00888885, 0x074EB975, 0x0847F285, 0x0956B5CD, 0x0556B58D,
		0x0F90843D, 0x04D6B59D, 0x04D6B575, 0x0187A10D, 0x0556B555,
		0x0756B515, 0x00000051, 0x00000A82, 0x00022A23, 0x00014A53,
		0x00008A8B, 0x0106A105, 0x0726B175, 0x0F14A5F5, 0x0556B5FD,
		0x08C63175, 0x074631FD, 0x08D6B5FD, 0x0094A5FD, 0x0ED6B175,
		0x0F9084FD, 0x00023F8B, 0x07C63145, 0x08A884FD, 0x084210FD,
		0x0F8882FD, 0x0FA082FD, 0x07463175, 0x0114A5FD, 0x087A3175,
		0x0D14A5FD, 0x04D6B595, 0x0087E10D, 0x07C2107D, 0x03A2083D,
		0x0FA088FD, 0x08A88A8D, 0x008B820D, 0x08CEB98D, 0x000011FA,
		0x0820820D, 0x00001F8A, 0x00004113, 0x08421085, 0x0000020A,
		0x0F56B54D, 0x064A52FD, 0x05463175, 0x0FCA5265, 0x0B56B575,
		0x0084A5F5, 0x0756B595, 0x0C1084FD, 0x000000E9, 0x00361044,
		0x08A884FD, 0x0002107B, 0x0F07C1F5, 0x0F0421FD, 0x07463175,
		0x032529FD, 0x0FA52935, 0x010422FD, 0x04D6B595, 0x0442527D,
		0x0FA2107D, 0x01B20C1D, 0x07C1F07D, 0x08A88A8D, 0x07D2949D,
		0x08CEB98D, 0x00001B22, 0x000000F9, 0x000004DA, 0x01104115
	};

	char msg[2048] = "";

	int msgLen;
	int charNum;
	int charWidth;
	int currChar;
	int bank1     = 0;
	int bank2     = 0;
	int bank3     = 0;
	int bank4     = 0;
	int bank5     = 0;
	int bankMask  = 0xFFF;
	int letterGap = 1;
	int msgPos    = 0;
	int charPos   = 0;

  // install handler to terminate future thread
  if( signal(SIGINT, sig_handler) == SIG_ERR )
	{
	  fprintf( stderr, "Failed to install SIGINT handler.\n" );
	  exit( EXIT_FAILURE );
	}

  // create thread
  iret1 = pthread_create( &thread1, NULL, blink, &terminate );

  if( iret1 )
	{
	  fprintf( stderr, "Error creating thread, return code %d\n", iret1 );
	  exit( EXIT_FAILURE );
	}

  sleep( 2 );			// allow 2 sec for multiplex to start

  srand(time(NULL));

  // set the status LEDs
  STORE(ionLED,     1);
  STORE(fetchLED,   1);
  STORE(executeLED, 1);
  STORE(runLED,     1);
  STORE(pauseLED,   0);
  STORE(jmpLED,     1);

  while(! terminate)
  {
	// blink the execute LED after every randomization
	//STORE(executeLED, ! GET(executeLED));
	STORE(executeLED, 1);

		// Use DF switches to control mode
		deeperThoughtMode = (GETSWITCHES(step) & 070)>>3;

		// Get IF switches value
		swIfValue = (GETSWITCHES(step) & 07);

	// if we're paused -- don't change the LEDs
	if (! dontChangeLEDs)
	{
	  // Maximum amount to delay between changes
	  // least signifiant address lines control the maximum delay
	  // all "up" -- maximum delay
	  // all "down" -- minimal delay
	  //delayAmount  =  (GETSWITCHES(swregister) & 07) * 400000L;
	  delayAmount  =  ((GETSWITCHES(swregister) & 077)+1) * 50000L;

	  // How much to vary the above timing
	  // the next bank of three address lines control how much
	  // we can shorten the maximum delay
	  // all "up" -- we can shorten to zero seconds
	  // all "down" -- must use maximum time before we change
	  //varietyMult = (GETSWITCHES(swregister) & 070)>>3;
	  varietyMult = (GETSWITCHES(swregister) & 07700)>>6;
	  //varietyAmount = (unsigned long) (((rand() & delayAmount) / 7.0f) * varietyMult);
	  varietyAmount = (unsigned long) (((rand() % delayAmount) / 63.0f) * varietyMult);

	  sleepTime = delayAmount - varietyAmount;

	  // In future revisions, we'll have different randomization sequences
	  switch(deeperThoughtMode)
	  {
		  case 3:	// 011 = Most LEDs Off
			STORE(programCounter,    0);
			STORE(memoryAddress,     0);
			STORE(memoryBuffer,      0);
			STORE(accumulator,       0);
			STORE(multiplierQuotient,0);
			STORE(stepCounter,       0);
			STORE(dataField,         0);
			STORE(instField,         0);
			// Randomly blink first column of operation LEDs
			STORE(andLED, rand_flag(100,20));
			STORE(tadLED, rand_flag(100,2));
			STORE(iszLED, rand_flag(100,5));
			STORE(dcaLED, rand_flag(100,5));
			STORE(jmsLED, rand_flag(100,5));
			STORE(jmpLED, rand_flag(100,15));
			STORE(iotLED, rand_flag(100,10));
			STORE(oprLED, rand_flag(100,10));
			STORE(linkLED, 0);
			STORE(deferLED, 0);
			STORE(wordCountLED, 0);
			STORE(currentAddressLED, 0);
			STORE(breakLED, 0);
			STORE(ionLED,     0);
			STORE(fetchLED,   0);
			break;
		  case 0:	// 000 = ALL LEDS ON
			STORE(programCounter,    65535 & programCounter[2]);
			STORE(memoryAddress,     65535 & memoryAddress[2]);
			STORE(memoryBuffer,      65535 & memoryBuffer[2]);
			STORE(accumulator,       65535 & accumulator[2]);
			STORE(multiplierQuotient,65535 & multiplierQuotient[2]);
			STORE(stepCounter,       65535 & stepCounter[2]);
			STORE(dataField,         65535 & dataField[2]);
			STORE(instField,         65535 & instField[2]);
			STORE(andLED, 1);
			STORE(tadLED, 1);
			STORE(iszLED, 1);
			STORE(dcaLED, 1);
			STORE(jmsLED, 1);
			STORE(jmpLED, 1);
			STORE(iotLED, 1);
			STORE(oprLED, 1);
			STORE(pauseLED, 1);
			STORE(linkLED, 1);
			STORE(deferLED, 1);
			STORE(wordCountLED, 1);
			STORE(currentAddressLED, 1);
			STORE(breakLED, 1);
			STORE(ionLED,     1);
			STORE(fetchLED,   1);
			break;
		  case 6:	// 110 = Binary Clock
			currentTime = time(NULL);
			localTime = localtime(&currentTime);
			hour = localTime->tm_hour;
			min = localTime->tm_min;
			sec = localTime->tm_sec;

			STORE(programCounter,    hour);
			STORE(memoryAddress,     min);
			STORE(memoryBuffer,      sec);
			STORE(accumulator,       (localTime->tm_mon + 1));
			STORE(multiplierQuotient,localTime->tm_mday);
			STORE(stepCounter,       0);
			STORE(dataField,         0);
			STORE(instField,         0);
			//STORE(linkLED, rand_flag(100,20));
			// Reset the Link light (bug fixed in v2.01 by David C. Eilering)
			STORE(linkLED, 0);
			STORE(deferLED, 0);
			STORE(wordCountLED, 0);
			STORE(currentAddressLED, 0);
			STORE(breakLED, 0);
			STORE(ionLED,     1);
			STORE(fetchLED,   1);
			// Randomly blink first column of operation LEDs
			STORE(andLED, rand_flag(100,50));
			STORE(tadLED, rand_flag(100,5));
			STORE(iszLED, rand_flag(100,10));
			STORE(dcaLED, rand_flag(100,10));
			STORE(jmsLED, rand_flag(100,10));
			STORE(jmpLED, rand_flag(100,30));
			STORE(iotLED, rand_flag(100,20));
			STORE(oprLED, rand_flag(100,20));
			// Override Sleep Time to 0.5 second
			sleepTime = 500000;
			break;
			
		case 5:   // 101 = 24x7 mode
			currentTime = time(NULL);
			localTime = localtime(&currentTime);
			int min = localTime->tm_hour*60 + localTime->tm_min;
			int index=0 ;
			while (( min>schedule[index+1].start ) && (index<(sizeof schedule / sizeof schedule[0])-1)) index++ ;
			if ((burst_timer==0) && (off_timer==0) && ( rand_flag(100,schedule[index].pgm_frequency)))
				burst_timer = (rand() & schedule[index].pgm_duration) + 1 ;
			if (burst_timer > 0)
			{
				STORE(programCounter,    rand() & programCounter[2]);
				STORE(memoryAddress,     rand() & memoryAddress[2]);
				STORE(memoryBuffer,      rand() & memoryBuffer[2]);
				STORE(accumulator,       rand() & accumulator[2]);
				if ( burst_timer < 2)
				{
					STORE(multiplierQuotient,rand() & multiplierQuotient[2]);
					STORE(stepCounter,       rand() & stepCounter[2]);
					STORE(dataField,         rand() & dataField[2]);
					STORE(instField,         rand() & instField[2]);
				}
				if (--burst_timer == 0 ) off_timer = rand()&schedule[index].pgm_delay;
			}
			else
			{
				if (off_timer>0) off_timer-- ;
				STORE(programCounter,    0);
				STORE(memoryAddress,     0);
				STORE(memoryBuffer,      0);
				STORE(accumulator,       0);
				STORE(multiplierQuotient,0);
				STORE(stepCounter,       0);
				STORE(dataField,         0);
				STORE(instField,         0);
			}
			STORE(linkLED, rand_flag(100,20));

			// Randomly blink first column of operation LEDs
			STORE(andLED,  rand_flag(100,20));
			STORE(tadLED,  rand_flag(100,2));
			STORE(iszLED,  rand_flag(100,5));
			STORE(dcaLED,  rand_flag(100,5));
			STORE(jmsLED,  rand_flag(100,5));
			STORE(jmpLED,  rand_flag(100,15));
			STORE(iotLED,  rand_flag(100,10));
			STORE(oprLED,  rand_flag(100,10));
			STORE(linkLED, rand_flag(100,20));
			STORE(deferLED,rand_flag(100,2));
			STORE(ionLED,  rand_flag(100,10));
			STORE(fetchLED,rand_flag(100,10));
			STORE(wordCountLED, 0);
			STORE(currentAddressLED, 0);
			STORE(breakLED, 0);
			break ;

		  case 1:	// 001 = Snake
			switch(y)
			{
				case 1:
					STORE(programCounter, 	 x & programCounter[2]);
					STORE(memoryAddress,     0);
					STORE(memoryBuffer,      0);
					STORE(accumulator,       0);
					STORE(multiplierQuotient,0);
					break;
				case 2:
					STORE(programCounter, 	 0);
					STORE(memoryAddress,     x & memoryAddress[2]);
					STORE(memoryBuffer,      0);
					STORE(accumulator,       0);
					STORE(multiplierQuotient,0);
					break;
				case 3:
					STORE(programCounter, 	 0);
					STORE(memoryAddress,     0);
					STORE(memoryBuffer,      x & memoryBuffer[2]);
					STORE(accumulator,       0);
					STORE(multiplierQuotient,0);
					break;
				case 4:
					STORE(programCounter, 	 0);
					STORE(memoryAddress,     0);
					STORE(memoryBuffer,      0);
					STORE(accumulator,       x & accumulator[2]);
					STORE(multiplierQuotient,0);
					break;
				case 5:
					STORE(programCounter, 	 0);
					STORE(memoryAddress,     0);
					STORE(memoryBuffer,      0);
					STORE(accumulator,       0);
					STORE(multiplierQuotient, x & multiplierQuotient[2]);
					break;
				default:
					y = 1;
			}
			if(shift_dir == 1 && x < 14336)
			{
				x = x << 1;
				if(x < 7)
					x += 1;
			}
			else if(shift_dir == 0 && x > 1)
			{
				x = x >> 1;
			}
			else
			{
				shift_dir = !shift_dir;
				y++;
			}


			STORE(stepCounter,       0);
			STORE(dataField,         0);
			STORE(instField,         0);
			STORE(linkLED, 0);
			STORE(deferLED, 0);
			STORE(wordCountLED, 0);
			STORE(currentAddressLED, 0);
			STORE(breakLED, 0);
			STORE(ionLED,     1);
			STORE(fetchLED,   1);
			// Randomly blink first column of operation LEDs
			STORE(andLED, rand_flag(100,50));
			STORE(tadLED, rand_flag(100,10));
			STORE(iszLED, rand_flag(100,20));
			STORE(dcaLED, rand_flag(100,20));
			STORE(jmsLED, rand_flag(100,20));
			STORE(jmpLED, rand_flag(100,60));
			STORE(iotLED, rand_flag(100,40));
			STORE(oprLED, rand_flag(100,40));

			// Don't include random delay in snake mode (added v2.10 by David C. Eilering)
			sleepTime = delayAmount;
			break;

			// break;

			case 2: //010 pong / bouncing ball (by Norman Davie)
			  {
		 		static long xDirection = 1;
	 			static long yDirection = 1;
				static long ball = 1;
		  		static long yBall = 1;

		  		ClearAllLEDs();

		  		// switch directions if we're too far right
		  		if ((ball >> 1) < 1)
		 		{
					xDirection = 1;
		  		}

		  		// switch directions if we're too far left
		  		if ((ball << 1) > (unsigned long) memoryBuffer[2])
			  	{
					xDirection = 0;
				}

		  		if (xDirection)
				{
			 		ball = ball << 1;
		  		}

		  		if (xDirection)
			 		ball = ball << 1;
		  		else
			 		ball = ball >> 1;

		  		yBall = yBall + yDirection;
		  		switch(yBall)
		  		{
		  			case 0:  // too far up
			 			yDirection = 1;
			 			yBall = 1;
			 			break;
		  			case 6: // too far down
			 			yDirection = -1;
			 			yBall = 5;
		  		}

		  		// store the value in the appropriate row
		  		switch(yBall)
		  		{
		  		case 1:
			 			STORE(multiplierQuotient, ball);
			 			break;
		  		case 2:
			 			STORE(accumulator, ball);
			 			break;
		  		case 3:
			 			STORE(memoryBuffer, ball);
			 			break;
		  		case 4:
			 			STORE(memoryAddress, ball);
			 			break;
		  		case 5:
			 			STORE(programCounter, ball);
			 			break;
		  		default:
			 			break;
		  		}
		  		// sleepTime = 50 * 1000;
				// Allow the switches to control the ball speed (added v2.10 by David C. Eilering)
		  		sleepTime = delayAmount;

			  }
			  break;

		  case 4:	// text scroller (by David C. Eilering)
			STORE(stepCounter, 0);
			STORE(dataField,   0);
			STORE(instField,   0);
			STORE(linkLED,     0);

			if (msgPos == 0 && charPos == 0) {
				loadMsg(msg);
				msgLen = strlen(msg);
			}

			// invalid character - skip it
			if (msg[msgPos] < 32 || 126 < msg[msgPos]) {
				msgPos++;
				msgPos = msgPos % msgLen;
				break;
			}

			if (letterGap < 0) {
				letterGap = 1;
			}

			// get letter
			charNum = (strchr(chars, msg[msgPos]) - chars);

			if (charPos == 0) {
				currChar = letters[charNum];
				charWidth = currChar & 0x07;
				currChar = currChar >> 3;
				// currChar = currChar >> (charPos * 5 + 3);
			}
			else {
				currChar = currChar >> 5;
			}

			if (currChar & 1)
				bank1++;

			if (currChar & 2)
				bank2++;

			if (currChar & 4)
				bank3++;

			if (currChar & 8)
				bank4++;

			if (currChar & 16)
				bank5++;

			charPos++;

			if (charPos >= (charWidth + letterGap)) {
				charPos = 0;
				msgPos++;
				msgPos = msgPos % msgLen;
			}

			// display the banks
			STORE(programCounter,    bank1 & programCounter[2]);
			STORE(memoryAddress,     bank2 & memoryAddress[2]);
			STORE(memoryBuffer,      bank3 & memoryBuffer[2]);
			STORE(accumulator,       bank4 & accumulator[2]);
			STORE(multiplierQuotient,bank5 & multiplierQuotient[2]);

			// shift the banks
			bank1 = (bank1 << 1) & bankMask;
			bank2 = (bank2 << 1) & bankMask;
			bank3 = (bank3 << 1) & bankMask;
			bank4 = (bank4 << 1) & bankMask;
			bank5 = (bank5 << 1) & bankMask;

			sleepTime = delayAmount / 2;	// half the delay for text scroll

			break;

			default:
			STORE(programCounter,    rand() & programCounter[2]);
			STORE(memoryAddress,     rand() & memoryAddress[2]);
			STORE(memoryBuffer,      rand() & memoryBuffer[2]);
			STORE(accumulator,       rand() & accumulator[2]);
			STORE(multiplierQuotient,rand() & multiplierQuotient[2]);
			STORE(stepCounter,       rand() & stepCounter[2]);
			STORE(dataField,         rand() & dataField[2]);
			STORE(instField,         rand() & instField[2]);
			STORE(linkLED, rand_flag(100,20));
			STORE(deferLED, 0);
			STORE(wordCountLED, 0);
			STORE(currentAddressLED, 0);
			STORE(breakLED, 0);
			STORE(ionLED,     1);
			STORE(fetchLED,   1);
			// Randomly blink first column of operation LEDs
			STORE(andLED, rand_flag(100,50));
			STORE(tadLED, rand_flag(100,10));
			STORE(iszLED, rand_flag(100,20));
			STORE(dcaLED, rand_flag(100,20));
			STORE(jmsLED, rand_flag(100,20));
			STORE(jmpLED, rand_flag(100,60));
			STORE(iotLED, rand_flag(100,40));
			STORE(oprLED, rand_flag(100,40));
			break;
	  }
	}
	else
	{
		sleepTime = 250 * 1000;
	}

	// Subtract the delay added below
	if(sleepTime > opled_delay)
		sleepTime = sleepTime - opled_delay;
	else
		sleepTime = 0;

	// Output Console when register switches change
	if(swRegValue != GETSWITCHES(swregister))
	{
		swRegValue = GETSWITCHES(swregister);
		printf("Register Switch: Value=%lu  delay=%lu  varietyMult=%lu \n", swRegValue, delayAmount, varietyMult);

	}

	// Output Console when register switches change
	if(swStepValue != GETSWITCHES(step))
	{
		swStepValue = GETSWITCHES(step);
		printf("Step Switch: Value=%lu  Mode=%i  IF Value=%i\n", swStepValue, deeperThoughtMode, swIfValue);

	}

	// Random Delay
	usleep(sleepTime);

	// if the stop switch is held for > 3 seconds, then clean up nicely
	if (GETSWITCH(stop))
	{
		stopPressedTime = (unsigned long)(stopPressedTime + ((sleepTime + opled_delay) / 1000.0f));
		if(stopPressedTime > 3000)
		{
			//if(swIfValue==0)
			if(GETSWITCH(singStep) && GETSWITCH(singInst))
			{
				system("shutdown --poweroff now");
			}
			else
			{
				terminate = 1;
			}
		}
	}
	else
	{
		stopPressedTime = 0;
	}

	// if the start switch is held for > 3 seconds, and both Sing switchs are down, reboot system
	if (GETSWITCH(start))
	{
		startPressedTime = (unsigned long)(startPressedTime + ((sleepTime + opled_delay) / 1000.0f));
		if(startPressedTime > 3000)
		{
			//if(swIfValue==0)
			if(GETSWITCH(singStep) && GETSWITCH(singInst))
			{
				system("reboot");
			}
		}
	}
	else
	{
		startPressedTime = 0;
	}

	// if one of the single step switches is selected, then "pause" and don't change the LED display
	// otherwise "run"
	dontChangeLEDs = GETSWITCH(singStep) || GETSWITCH(singInst);
	STORE(pauseLED, dontChangeLEDs);
	STORE(runLED, ! dontChangeLEDs);

	// Turn operation LEDs off for 10ms to create a fast blink
	STORE(executeLED, 0);
	STORE(andLED, 0);
	STORE(tadLED, 0);
	STORE(iszLED, 0);
	STORE(dcaLED, 0);
	STORE(jmsLED, 0);
	STORE(jmpLED, 0);
	STORE(iotLED, 0);
	STORE(oprLED, 0);
	usleep(opled_delay);
 }


  if( pthread_join(thread1, NULL) )
	printf( "\r\nError joining multiplex thread\r\n" );

  return 0;
}
