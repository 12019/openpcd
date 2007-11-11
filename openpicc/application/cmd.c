#include <AT91SAM7.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <USB-CDC.h>
#include <board.h>
#include <string.h>

#include "env.h"
#include "cmd.h"
#include "openpicc.h"
#include "led.h"
#include "da.h"

xQueueHandle xCmdQueue;
xTaskHandle xCmdTask;
xTaskHandle xCmdRecvUsbTask;

/* Whether to require a colon (':') be typed before long commands, similar to e.g. vi
 * This makes a distinction between long commands and short commands: long commands may be
 * longer than one character and/or accept optional parameters, short commands are only one
 * character with no arguments (typically some toggling command). Short commands execute
 * immediately when the character is pressed, long commands must be completed with return.
 * */
static const portBASE_TYPE USE_COLON_FOR_LONG_COMMANDS = 0;
/* When not USE_COLON_FOR_LONG_COMMANDS then short commands will be recognized by including
 * their character in the string SHORT_COMMANDS
 * */
static const char *SHORT_COMMANDS = "c+-l?h";
/* Note that the long/short command distinction only applies to the USB serial console
 * */

/**********************************************************************/
void DumpUIntToUSB(unsigned int data)
{
    int i=0;
    unsigned char buffer[10],*p=&buffer[sizeof(buffer)];

    do {
        *--p='0'+(unsigned char)(data%10);
        data/=10;
        i++;
    } while(data);

    while(i--)
        vUSBSendByte(*p++);
}
/**********************************************************************/

void DumpStringToUSB(char* text)
{
    unsigned char data;

    if(text)
        while((data=*text++)!=0)
            vUSBSendByte(data);
}
/**********************************************************************/

static inline unsigned char HexChar(unsigned char nibble)
{
   return nibble + ((nibble<0x0A) ? '0':('A'-0xA));
}

void DumpBufferToUSB(char* buffer, int len)
{
    int i;

    for(i=0; i<len; i++) {
	vUSBSendByte(HexChar( *buffer  >>   4));
	vUSBSendByte(HexChar( *buffer++ & 0xf));
    }
}
/**********************************************************************/

/*
 * Convert a string to an integer. Ignores leading spaces.
 * Optionally returns a pointer to the end of the number in the string */
int atoiEx(const char * nptr, char * * eptr)
{
	portBASE_TYPE sign = 1, i=0;
	int curval = 0;
	while(nptr[i] == ' ' && nptr[i] != 0) i++;
	if(nptr[i] == 0) goto out;
	if(nptr[i] == '-') {sign *= -1; i++; }
	else if(nptr[i] == '+') { i++; } 
	while(nptr[i] != 0 && nptr[i] >= '0' && nptr[i] <= '9')
		curval = curval * 10 + (nptr[i++] - '0');
	
	out:
	if(eptr != NULL) *eptr = (char*)nptr+i;
	return sign * curval;
}

static const AT91PS_SPI spi = AT91C_BASE_SPI;
#define SPI_MAX_XFER_LEN 33

extern volatile unsigned portLONG ulCriticalNesting;
void prvExecCommand(u_int32_t cmd, portCHAR *args) {
	static int led = 0;
	portCHAR cByte = cmd & 0xff;
	portLONG j;
	int i,h,m,s;
	if(cByte>='A' && cByte<='Z')
	    cByte-=('A'-'a');
	
	DumpStringToUSB("Got command ");
	DumpUIntToUSB(cmd);
	DumpStringToUSB(" with args ");
	DumpStringToUSB(args);
	DumpStringToUSB("\n\r");
	    
	i=0;
	// Note: Commands have been uppercased when this code is called
	    switch(cmd)
	    {
	    	case '4':
	    	case '3':
	    	case '2':
	    	case '1':
		    env.e.mode=cmd-'0';
		    DumpStringToUSB(" * Switched to transmit mode at power level ");
		    DumpUIntToUSB(env.e.mode);
		    DumpStringToUSB("\n\r");
	    	    break;
		case 'S':
		    env_store();
		    DumpStringToUSB(" * Stored environment variables\n\r");
		    break;
		case '0':
		    DumpStringToUSB(" * switched to RX mode\n\r");		    
		    break;
		case 'TEST':
		    DumpStringToUSB("Testing critical sections\r\n");
		    j=ulCriticalNesting;
		    DumpStringToUSB("Nesting is now ");
		    DumpUIntToUSB(j);
		    DumpStringToUSB("\n\r");
		    taskENTER_CRITICAL();
		    for(i=0; i<1000; i++) {;}
		    j=ulCriticalNesting;
		    taskEXIT_CRITICAL();
		    DumpStringToUSB("Nesting was  ");
		    DumpUIntToUSB(j);
		    DumpStringToUSB("\n\r");
		    j=ulCriticalNesting;
		    DumpStringToUSB("Nesting is now ");
		    DumpUIntToUSB(j);
		    DumpStringToUSB("\n\r");
		    break;
		case 'I':
		    i=atoiEx(args, &args);
		    if(i!=0) {
			env.e.reader_id = i;
			DumpStringToUSB("Reader ID set to ");
			DumpUIntToUSB(env.e.reader_id);
			DumpStringToUSB("\n\r");
		    }
		    break;
		case 'C':
		    DumpStringToUSB(
			" *****************************************************\n\r"
			" * Current configuration:                            *\n\r"
			" *****************************************************\n\r"
			" *\n\r");
		    DumpStringToUSB(" * Uptime is ");
		    s=xTaskGetTickCount()/1000;
		    h=s/3600;
		    s%=3600;
		    m=s/60;
		    s%=60;
		    DumpUIntToUSB(h);
		    DumpStringToUSB("h:");
		    DumpUIntToUSB(m);
		    DumpStringToUSB("m:");
		    DumpUIntToUSB(s);
		    DumpStringToUSB("s");
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * The reader id is ");
		    DumpUIntToUSB(env.e.reader_id);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * The mode is ");
		    DumpUIntToUSB(env.e.mode);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * The transmit interval is ");
		    DumpUIntToUSB(env.e.speed);
		    DumpStringToUSB("00ms\n\r");
		    DumpStringToUSB(" * The comparator threshold is ");
		    DumpUIntToUSB(da_get_value());
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(
			" *\n\r"
			" *****************************************************\n\r"
			);
		    break;
		case '+':
		case '-':
		    if(cmd == '+')
		    {
			if(da_get_value() < 255)
			    da_comp_carr(da_get_value()+1);
		    }
		    else
			if(da_get_value() > 0)
			    da_comp_carr(da_get_value()-1);;
		    			
		    DumpStringToUSB(" * Comparator threshold set to ");
		    DumpUIntToUSB(da_get_value());		    
		    DumpStringToUSB("\n\r");
		    break;
		case 'L':
		    led = (led+1)%4;
		    vLedSetRed( (led&1) );
		    vLedSetGreen( led&2 );
		    DumpStringToUSB(" * LEDs set to ");
		    vUSBSendByte( (char)led + '0' );
		    DumpStringToUSB("\n\r");
		    break;
		case 'H':	
		case '?':
		    DumpStringToUSB(
			" *****************************************************\n\r"
			" * OpenPICC USB terminal                             *\n\r"
			" * (C) 2007 Milosch Meriac <meriac@openbeacon.de>    *\n\r"
			" * (C) 2007 Henryk Plötz <henryk@ploetzli.ch>        *\n\r"
			" *****************************************************\n\r"
			" *\n\r"
			" * s    - store transmitter settings\n\r"
			" * test - test critical sections\n\r"
			" * c    - print configuration\n\r"
			" * 0    - receive only mode\n\r"
			" * 1..4 - automatic transmit at selected power levels\n\r"
			" * +,-  - decrease/increase comparator threshold\n\r"
			" * l    - cycle LEDs\n\r"
			" * ?,h  - display this help screen\n\r"
			" *\n\r"
			" *****************************************************\n\r"
			);
		    break;
	    }
    
}

// A task to execute commands
void vCmdCode(void *pvParameters) {
	(void) pvParameters;
	u_int32_t cmd;
	portBASE_TYPE i, j=0;
	
	for(;;) {
		cmd_type next_command;
		cmd = j = 0;
		 if( xQueueReceive(xCmdQueue, &next_command, ( portTickType ) 100 ) ) {
			DumpStringToUSB("Command received:");
			DumpStringToUSB(next_command.command);
			DumpStringToUSB("\n\r");
			while(next_command.command[j] == ' ' && next_command.command[j] != 0) {
				j++;
			}
			for(i=0;i<4;i++) {
				portCHAR cByte = next_command.command[i+j];
				if(next_command.command[i+j] == 0 || next_command.command[i+j] == ' ')
					break;
				if(cByte>='a' && cByte<='z') {
					cmd = (cmd<<8) | (cByte+('A'-'a'));
				} else cmd = (cmd<<8) | cByte;
			}
			while(next_command.command[i+j] == ' ' && next_command.command[i+j] != 0) {
				i++;
			}
			prvExecCommand(cmd, next_command.command+i+j);
		 } else {
		 }
	}
}



// A task to read commands from USB
void vCmdRecvUsbCode(void *pvParameters) {
	portBASE_TYPE len=0;
	portBASE_TYPE short_command=1, submit_it=0;
	cmd_type next_command = { source: SRC_USB, command: ""};
	(void) pvParameters;
    
	for( ;; ) {
		if(vUSBRecvByte(&next_command.command[len], 1, 100)) {
			if(USE_COLON_FOR_LONG_COMMANDS) {
				if(len == 0 && next_command.command[len] == ':')
					short_command = 0;
			} else {
				if(strchr(SHORT_COMMANDS, next_command.command[len]) == NULL)
					short_command = 0;
			}
			next_command.command[len+1] = 0;
			DumpStringToUSB(next_command.command + len);
			if(next_command.command[len] == '\n' || next_command.command[len] == '\r') {
				next_command.command[len] = 0;
				submit_it = 1;
			}
			if(short_command==1) {
				submit_it = 1;
			}
			if(submit_it) {
				if(len > 0 || short_command) {
			    		if( xQueueSend(xCmdQueue, &next_command, 0) != pdTRUE) {
			    			DumpStringToUSB("Queue full, command can't be processed.\n");
			    		}
				}
		    		len=0;
		    		submit_it=0;
		    		short_command=1;
			} else if( len>0 || next_command.command[len] != ':') len++;
			if(len >= MAX_CMD_LEN-1) {
				DumpStringToUSB("ERROR: Command too long. Ignored.");
				len=0;
			}
	    	}
    	}
}

portBASE_TYPE vCmdInit(void) {
	/* FIXME Maybe modify to use pointers? */
	xCmdQueue = xQueueCreate( 10, sizeof(cmd_type) );
	if(xCmdQueue == 0) {
		return 0;
	}
	
	if(xTaskCreate(vCmdCode, (signed portCHAR *)"CMD", TASK_CMD_STACK, NULL, 
		TASK_CMD_PRIORITY, &xCmdTask) != pdPASS) {
		return 0;
	}
	
	if(xTaskCreate(vCmdRecvUsbCode, (signed portCHAR *)"CMDUSB", TASK_CMD_STACK, NULL, 
		TASK_CMD_PRIORITY, &xCmdRecvUsbTask) != pdPASS) {
		return 0;
	}
	
	return 1;
}
