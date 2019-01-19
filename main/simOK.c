#ifndef UART_NUM_1
#include <stdio.h>
#include "string.h"
#include "driver/gpio.h"
#endif
// max buffer length
#define LINE_MAX	80
#define MAX_ATTEMPTS 15
int quality=0;

int command_valid=0;
char command[LINE_MAX];

void scriviUART(uart_port_t uart_controller, char* text){
    uart_write_bytes(uart_controller, text, strlen(text));    
    //printf("%s ---- %d",text,strlen(text));
    return;
}


// read a line from the UART controller
char* read_line(uart_port_t uart_controller) {
	static char line[2048];
	char *ptr = line;
	//printf("\nread_line on UART: %d\n", (int) uart_controller);
    while(1) {
		int num_read = uart_read_bytes(uart_controller, (unsigned char *)ptr, 1, 500/portTICK_RATE_MS);//portMAX_DELAY);
		if(num_read == 1) {
			// new line found, terminate the string and return 
            //printf("received char: %c", *ptr);
			if(*ptr == '\n') {
				ptr++;
				*ptr = '\0';
				return line;
			}
			// else move to the next char
			ptr++;
		} else {
            //printf("num_read=%d",num_read);
            *ptr=10;
            ptr++;
            *ptr=0;
            return line;
        } 
	}
}

char* parse_line(char* line){
    static char subline[2048];
    char* token;
    char* token2;
	
    if (strncmp("SMS Ready\r\n",line,11)==0){
        sprintf(subline,"%s","\r\nSMS Ready Received!!!\r\n");
        return subline;
    }

    if (strncmp("+CSQ: ",line,6)==0){
        //printf("\r\nSignal Quality Report!!!\r\n");
        token = strtok(line, "+CSQ: ");
        //printf("token: %s",token);
        token2 = strtok(token, ",");
        //printf("token2: %s",token2);
        quality= atoi(token2);
        sprintf(subline,"%s","\r\nSignal Quality Report!!!\r\n");
        return subline;            
    }

    sprintf(subline,"%s",line);
    return subline;
}

void stampa_stringa(char* line){
     	//printf("Rcv%d: %s", numline,line);
        if(strlen(line)>1){
            printf("%s",line);
        }
}

void getCommand(){
	int line_pos = 0;

	while(1) {
		int c = getchar();
		// nothing to read, give to RTOS the control
		if(c < 0) {
			vTaskDelay(10 / portTICK_PERIOD_MS);
			continue;
		}
		//if(c == '\r') continue;
		if(c == '\n') {
			// terminate the string and parse the command
			command[line_pos] = c;
			line_pos++;
            command[line_pos] = 0;
			//parse_command(command);

			// reset the buffer and print the prompt
			line_pos = 0;
            command_valid=1;
            printf("\nYou entered: %s",command);
			//print_prompt();
		}
		else {
			command_valid=0;
            putchar(c);
			command[line_pos] = c;
			line_pos++;
			
			// buffer full!
			if(line_pos == LINE_MAX) {
				printf("\nCommand buffer full!\n");
				// reset the buffer and print the prompt
				line_pos = 0;
				//print_prompt();
			}
		}
	}	
}


int verificaComando(uart_port_t uart_controller, char* text, char* condition){
    int skip=0;
    int SIM808Ready=0;
    char *line;

    int k=0;
    while ((!SIM808Ready)&&(k<MAX_ATTEMPTS)) {
        k++;
        //uart_write_bytes(UART_NUM_1, "AT\r\n", strlen("AT\r\n"));
        if (!skip){
            scriviUART(UART_NUM_1, text);
            scriviUART(UART_NUM_1, "\r");
            printf("\nSending command: -- %s %d\n", text,k);
        }
        skip=0;
        vTaskDelay(550 / portTICK_RATE_MS); 
        line = read_line(UART_NUM_1);
        char *subline=parse_line(line);
        stampa_stringa(subline);
        /*
        for (int i=0; i<strlen(line);i++) {
            printf("%d",line[i]);
        }*/
        if (strncmp(condition,line,strlen(condition))==0){
            printf("\r\nCondition OK !!!\r\n");
            SIM808Ready=1;
        }
        if (strncmp("\r\n",line,2)==0){
            skip=1;
            printf("skipping cr+lf........\n");
        }
        if (strncmp("\n",line,1)==0){
            skip=1;
            printf("skipping lf........\n");
        }
        if (strncmp(text,line,strlen(text))==0){
            skip=1;
            printf("skipping echo........\n");
        }
    }
    
    if (k==MAX_ATTEMPTS){
        printf("\r\nmax_ATTEMPTS %d\r\n",k);
        return(-1); //ERROR
    } else {
        return 0; //OK
    }
}

void reset_sim(uart_port_t uart_controller){
    printf("Checking SIM condition....\n");
    if (verificaComando(UART_NUM_1,"AT+CFUN=1,1","OK\r\n")==0){
        printf("\nSIM got RESET command !!!\n");
    }
}

void foreverRed() {
for (;;) { // blink red LED forever
    gpio_set_level(BLINK_GPIO, 1);
    vTaskDelay(100 / portTICK_RATE_MS);
    gpio_set_level(BLINK_GPIO, 0);
    vTaskDelay(100 / portTICK_RATE_MS);
    }
}


int simOK(uart_port_t uart_controller) { // SIM CHECK OK
    printf("Checking for sim808 module and SIM card..\r\n");
    reset_sim(uart_controller);

    // time to startup 3 sec
    for (int i = 0; i < 6; i++) {
    //presentation blinking
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(250 / portTICK_RATE_MS);
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(250 / portTICK_RATE_MS);
    }

    printf("\nChecking if SIM808 responds....\n");
    if (verificaComando(UART_NUM_1,"AT","OK\r\n")==0){
        printf("\nSIM808 module found !!!\n");

    vTaskDelay(1000 / portTICK_RATE_MS); // wait for sim800 to settle a bit

    printf("\nChecking if SIM card inserted....\n");
    if (verificaComando(UART_NUM_1,"AT+CSMINS?","CSMINS: 0,0")==0){
        printf("\nSIM card is inserted!!!\n");
        return -1;
    } else {
        printf("\nno SIM card found, stop here\n"); // continue if SIM card found
    }

    printf("Allow some time for SIM to register on the network...");

    printf("\nChecking battery level....\n");
    if (verificaComando(UART_NUM_1,"AT+CBC","OK\r\n")==0){
        printf("\nAnswer from module relvant to battery level !!!\n");
    } else {
        printf("\nError in answer from module relvant to battery level !!!\n");
    }

    printf("\nset speaker volume to 80 [0-100]....\n");
    if (verificaComando(UART_NUM_1,"AT+CLVL=80","OK\r\n")==0){
        printf("\nSpeaker Volume set !!!\n");
    } else {
        printf("\nError in setting speaker volume !!!\n");
    }

    printf("\nset ringer volume to 80 [0-100]....\n");
    if (verificaComando(UART_NUM_1,"AT+CRSL=80","OK\r\n")==0){
        printf("\nRinger Volume set !!!\n");
    } else {
        printf("\nError in setting ringer volume !!!\n");
    }

    printf("\nset mic to gain level 10 [0-15]....\n");
    if (verificaComando(UART_NUM_1,"AT+CMIC=0,10","OK\r\n")==0){
        printf("\nMic  gain level set !!!\n");
    } else {
        printf("\nError in setting mic gain level !!!\n");
    }

    // ring tone AT+CALS=5,1 to switch on tone 5 5,0 to switch off
    //sim800.println(“AT+CALS=19”); // set alert/ring tone
    //simReply();

    printf("\nEnabling automatic answer call after 2 ring(s)....\n");
    if (verificaComando(UART_NUM_1,"ATS0=1","OK\r\n")==0){
        printf("\nModule will answer after two (2) ring(s) !!!\n");
    } else {
        printf("\nError in setting auto answer mode !!!\n");
    }

    printf("\nEnabling DTMF recognition....\n");
    if (verificaComando(UART_NUM_1,"AT+DDET=1,1000,0,0","OK\r\n")==0){
        printf("\nDTMF recognition is on !!!\n");
    } else {
        printf("\nError in setting DDET mode !!!\n");
    }
    
    return 0; //OK
    
    } else {
        return -1; //ERROR
    }
}