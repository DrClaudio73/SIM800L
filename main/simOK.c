#ifndef UART_NUM_1
#include <stdio.h>
#include "string.h"
#include "driver/gpio.h"
#endif
#include "credentials.h"
#include "esp_log.h"
static const char *TAG = "SIM808App";
// max buffer length
#define LINE_MAX	123
#define MAX_ATTEMPTS 10
#define NUM_LINES_TO_FLUSH 4

int quality=0;
int command_valid=0;
int pending_DTMF=-1;
char command[LINE_MAX];
char parametro_feedback[LINE_MAX];

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
		int num_read = uart_read_bytes(uart_controller, (unsigned char *)ptr, 1, 45/portTICK_RATE_MS);//portMAX_DELAY);
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


int checkDTMF(char* line) {
    //char *line1;
    char* token;

    token = strtok(line, "+DTMF: ");
    return(atoi(token));

    /*for(int k=0;k<10;k++){
        line1 = read_line(UART_NUM_1);
        if (strncmp("+DTMF: ",line1,strlen("+DTMF: "))==0){
            token = strtok(line1, "+DTMF: ");
            //sprintf(parametro_feedback,"%s",token);
            return(atoi(token));
        }
    }
    return -1; //only different feedback*/
}

char* parse_line(char* line){
    static char subline[2048];
    char* token;
    char* token2;
	
    if (strncmp("SMS Ready\r\n",line,11)==0){
        sprintf(subline,"%s","\nSMS Ready Received!!!\n");
        printf(line);
        printf("\n");
        return subline;
    }

    if (strncmp("+CSQ: ",line,6)==0){
        //printf("\r\nSignal Quality Report!!!\r\n");
        token = strtok(line, "+CSQ: ");
        //printf("token: %s",token);
        token2 = strtok(token, ",");
        sprintf(parametro_feedback,"%s",token2);
        //printf("token2: %s",token2);
        quality= atoi(token2);
        sprintf(subline,"%s%s","Signal Quality Report!!! Level=",parametro_feedback);
        printf(line);
        printf("\n");
        return subline;
    }

    if (strncmp("+CPAS: ",line,strlen("+CPAS: "))==0){
        token = strtok(line, "+CPAS: ");
        //printf("token: %s",token);
        sprintf(parametro_feedback,"%s",token);
        sprintf(subline,"%s%s","Phone Activity Status!!! Level=",parametro_feedback);
        printf(line);
        printf("\n");
        return subline;
    }

    if (strncmp("+DTMF: ",line,strlen("+DTMF: "))==0){
        pending_DTMF=checkDTMF(line);
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
				ESP_LOGE(TAG,"\n*****************************Command buffer full!\n*****************************");
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
    int deltaAttempts=0;
    int k=0;
    if (!skip){
        scriviUART(UART_NUM_1, text);
        scriviUART(UART_NUM_1, "\r");
        printf("\nSending command: -- %s\n", text);
    }

    //printf("condizione: %s",condition);
    //printf("\n");
    if (strncmp("SMS Ready",condition,strlen("SMS Ready"))==0){
        deltaAttempts=20;
        //printf("deltaattempts=20");
    } else {
        deltaAttempts=0;
        //printf("deltaattempts=0");
    }

    while ((!SIM808Ready)&&(k<MAX_ATTEMPTS+deltaAttempts)) {
        k++;
        /*if (!skip){
            scriviUART(UART_NUM_1, text);
            scriviUART(UART_NUM_1, "\r");
            printf("\nSending command: -- %s %d\n", text,k);
        }*/
        skip=0;
        vTaskDelay(450 / portTICK_RATE_MS); 
        line = read_line(UART_NUM_1);
        char *subline=parse_line(line);
        stampa_stringa(subline);
        /*
        for (int i=0; i<strlen(line);i++) {
            printf("%d",line[i]);
        }*/
        if (strncmp(condition,line,strlen(condition))==0){
            //printf("\r\nCondition OK !!!\r\n");
            SIM808Ready=1;
            int svuotaUART=NUM_LINES_TO_FLUSH;
            while (svuotaUART-->=0){
                line = read_line(UART_NUM_1);
                char *subline=parse_line(line);
                stampa_stringa(subline);
            }
        }
        if (strncmp("\r\n",line,2)==0){
            skip=1;
            //printf("skipping cr+lf........\n");
        }
        if (strncmp("\n",line,1)==0){
            skip=1;
            //printf("skipping lf........\n");
        }
        if (strncmp(text,line,strlen(text))==0){
            skip=1;
            //printf("skipping echo........\n");
        }
    }
    
    if (k==MAX_ATTEMPTS+deltaAttempts){
        ESP_LOGE(TAG,"\r\nmax_ATTEMPTS %d\r\n",k);
        return(-1); //ERROR
    } else {
        return 0; //OK
    }
}

void reset_module(uart_port_t uart_controller){
    ESP_LOGW(TAG,"Resetting module....\n");
    if (verificaComando(UART_NUM_1,"AT+CFUN=1,1","SMS Ready\r\n")==0){
        ESP_LOGI(TAG,"\nSIM got RESET command !!!\n");
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
    ESP_LOGI(TAG,"\n***************************************************************\nGeneral Settings and Checking for sim808 module and SIM card...\n***************************************************************");
    reset_module(uart_controller); //DA RIPRISTINARE
    // time to startup 3 sec
    for (int i = 0; i < 6; i++) {
    //presentation blinking
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(250 / portTICK_RATE_MS);
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(250 / portTICK_RATE_MS);
    }

    ESP_LOGW(TAG,"\n********Checking if SIM808 responds....");
    if (verificaComando(UART_NUM_1,"AT","OK\r\n")==0){
        ESP_LOGI(TAG,"\nSIM808 module found !!!");

        vTaskDelay(1000 / portTICK_RATE_MS); // wait for sim808 to settle a bit

        ESP_LOGW(TAG,"\n********Disabling Echo....");
        if (verificaComando(UART_NUM_1,"ATE 0","OK\r\n")==0){
            ESP_LOGI(TAG,"\n********Echo disabled !!!");
        } else {
            ESP_LOGE(TAG,"\n********Error in disabling echo !!!");
        }

        /**/
        ESP_LOGW(TAG,"\n********Checking if SIM card inserted...");
        if (verificaComando(UART_NUM_1,"AT+CSMINS?","+CSMINS: 0,1\r\n")==0){
            ESP_LOGI(TAG,"\n********SIM card is inserted!!!");
        } else {
            ESP_LOGE(TAG,"\n********No SIM card found, stop here!\n********"); // continue if SIM card found
            return -1; //ERROR
        }

        //ESP_LOGW(TAG,"\n********Allow some time for SIM to register on the network...\n********");

        ESP_LOGW(TAG,"\n********Checking battery level....");
        if (verificaComando(UART_NUM_1,"AT+CBC","OK\r\n")==0){
            ESP_LOGI(TAG,"\n********Answer from module relvant to battery level !!!");
        } else {
            ESP_LOGE(TAG,"\n********Error in answer from module relvant to battery level !!!");
        }

        ESP_LOGW(TAG,"\n********Setting speaker volume to 80 [0-100]....");
        if (verificaComando(UART_NUM_1,"AT+CLVL=80","OK\r\n")==0){
            ESP_LOGI(TAG,"\n********Speaker Volume set !!!");
        } else {
            ESP_LOGE(TAG,"\n********Error in setting speaker volume !!!");
        }

        ESP_LOGW(TAG,"\n********Setting ringer volume to 80 [0-100]....");
        if (verificaComando(UART_NUM_1,"AT+CRSL=80","OK\r\n")==0){
            ESP_LOGI(TAG,"\n********Ringer Volume set !!!");
        } else {
            ESP_LOGE(TAG,"\n********Error in setting ringer volume !!!");
        }

        ESP_LOGW(TAG,"\n********Setting mic to gain level 10 [0-15]....");
        if (verificaComando(UART_NUM_1,"AT+CMIC=0,10","OK\r\n")==0){
            ESP_LOGI(TAG,"\n********Mic  gain level set !!!");
        } else {
            ESP_LOGE(TAG,"\n********Error in setting mic gain level !!!");
        }

        // ring tone AT+CALS=5,1 to switch on tone 5 5,0 to switch off
        //sim800.println(“AT+CALS=19”); // set alert/ring tone
        //simReply();

        //printf("\nEnabling automatic answer call after 2 ring(s)....\n");
        ESP_LOGW(TAG,"\n********Disabling automatic answer call after some ring(s)....");
        if (verificaComando(UART_NUM_1,"ATS0=0","OK\r\n")==0){
            ESP_LOGI(TAG,"\n********Module will not automaticallly answer !!!");
        } else {
            ESP_LOGE(TAG,"\n********Error in disabling auto answer mode !!!");
        }/**/

        ESP_LOGW(TAG,"\n********Enabling DTMF recognition....");
        if (verificaComando(UART_NUM_1,"AT+DDET=1,1000,0,0","OK\r\n")==0){
            ESP_LOGI(TAG,"\n********DTMF recognition is on !!!");
        } else {
            ESP_LOGE(TAG,"\n********Error in setting DDET mode !!!");
        }

        ESP_LOGW(TAG,"\n********Setting SMS text mode....");
        if (verificaComando(UART_NUM_1,"AT+CMGF=1","OK\r\n")==0){
            ESP_LOGI(TAG,"\n********SMS text Mode enabled !!!");
        } else {
            ESP_LOGE(TAG,"\n********Not possible to enable SMS text Mode !!!");            
        }

        /*printf("\nEnabling DTMF recognition....\n");
        if (verificaComando(UART_NUM_1,"AT+DDET=1","OK\r\n")==0){
            printf("\nDTMF recognition is on !!!\n");
        }*/

        ESP_LOGW(TAG,"\n********Checking Signal Quality....");
        if (verificaComando(UART_NUM_1,"AT+CSQ","+CSQ")==0){
            ESP_LOGI(TAG,"\n********Quality report gotten !!! Quality level=%d\n",quality);
        } else {
            ESP_LOGE(TAG,"\n********No valid quality report gotten !!!\n");
        }

        return 0; //OK
    } else {
        ESP_LOGE(TAG,"\n********NO ANSWER FROM MODULE: Stuck here!!!!!\n********");
        return -1; //ERROR
    }
}

int checkPhoneActivityStatus() { // CHECK STATUS FOR RINGING or IN CALL
    // phone activity status: 0= ready, 2= unknown, 3= ringing, 4= in call
    if (verificaComando(UART_NUM_1,"AT+CPAS","+CPAS: ")==0) { // decode reply
        //char c = sim800.read(); // gives ascii code for status number
        return atoi(parametro_feedback); // return integer value of ascii code
    }
    else return -1;
}

int checkCallingNumber(){
    char *line1;
    char* token;
    char* token2;
    ESP_LOGI(TAG,"checkCallingNumber()\n");
    for(int k=0;k<10;k++){
        line1 = read_line(UART_NUM_1);
        //printf("line read is:%s",line1);
        //+CLIP: "+3912345678",145,"",0,"ccccccc",0
        if (strncmp("+CLIP: ",line1,strlen("+CLIP: "))==0){
            token = strtok(line1, "CLIP: ");
            token = strtok(NULL, "CLIP: ");
            //printf("token: %s",token);
            token2 = strtok(token, "\"");
            sprintf(parametro_feedback,"%s",token2);
            //printf("token2: %s",token2);
            if (strncmp(ALLOWED1,token2,sizeof(ALLOWED1))==0){
                return(0); //OK: Valid Caller
            } else {
                return(-1); //Not valid Caller
            }
        }
    }
    return -1; //Only different feedback
}