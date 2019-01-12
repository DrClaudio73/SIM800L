#include <stdio.h>
// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// UART driver
#include "driver/uart.h"

// Error library
#include "esp_err.h"

#include "string.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#define BLINK_GPIO CONFIG_BLINK_GPIO

// max buffer length
#define LINE_MAX	50
#define MAX_ATTEMPTS 15
char command[LINE_MAX];
int command_valid=0;
int quality=0;

// read a line from the UART controller
char* read_line(uart_port_t uart_controller) {
	static char line[2048];
	char *ptr = line;
	//printf("\nread_line on UART: %d\n", (int) uart_controller);
    while(1) {
		int num_read = uart_read_bytes(uart_controller, (unsigned char *)ptr, 1, 100/portTICK_RATE_MS);//portMAX_DELAY);
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
            ptr[0]=10;
            ptr++;
            ptr[0]=0;
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

void stampa_stringa(char* line, int numline){
     	//printf("Rcv%d: %s", numline,line);
        if(strlen(line)>1){
            printf("%s",line);
        }
        numline++;
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

//toggleLed
void toggleLed(){
    gpio_pad_select_gpio(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    uint32_t livello_led=0;
    while(1){
        if (quality > 0){
            /* Blink output led */
            gpio_set_level(BLINK_GPIO, livello_led);
            livello_led=1-livello_led;
            vTaskDelay(20*(32 - quality) / portTICK_RATE_MS);
        } else {
            gpio_set_level(BLINK_GPIO, livello_led);
            livello_led=1-livello_led;
            vTaskDelay(3000 / portTICK_RATE_MS);
        }
    }
}

void scriviUART(uart_port_t uart_controller, char* text){
    uart_write_bytes(uart_controller, text, strlen(text));    
    //printf("%s ---- %d",text,strlen(text));
    return;
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
            printf("\nSending command: -- %s", text);
        }
        vTaskDelay(550 / portTICK_RATE_MS); 
        line = read_line(UART_NUM_1);
        if (strncmp(condition,line,4)==0){
            //printf("\r\nCondition OK !!!\r\n");
            SIM808Ready=1;
        }
        if (strncmp("\r\n",line,2)==0){
            skip=1;
            //printf("skipping........");
        }
    }
    return 0; //OK
}

// Main application
void app_main() {
	printf("==============================\r\n");
	printf("Welcome to SIM800L control App\r\n");
	printf("==============================\r\n");
	
	// configure the UART1 controller
	uart_config_t uart_config1 = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    /*
    uart_config_t uart_config2 = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };*/
    uart_param_config(UART_NUM_1, &uart_config1);
    uart_set_pin(UART_NUM_1, 22, 23, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE); //TX , RX
    uart_driver_install(UART_NUM_1, 1024, 0, 0, NULL, 0);
    //uint8_t *data1 = (uint8_t *) malloc(1024);

	/* configure the UART2 controller 
    uart_param_config(UART_NUM_2, &uart_config2);
    uart_set_pin(UART_NUM_2, 18, 19, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_2, 1024, 0, 0, NULL, 0);*/
    //uint8_t *data2 = (uint8_t *) malloc(1024);
    char *line1;
    printf("\nVerifying SIM808 responds.....\n");
    if(verificaComando(UART_NUM_1,"AT\r\n","OK\r\n")==0){
        printf("\r\nSIM808 Ready !!!\r\n");
    }

    printf("\nDisabling Echo....\n");
    if (verificaComando(UART_NUM_1,"ATE 0\r\n","OK\r\n")==0){
        printf("\nEcho disabled !!!\n");
    }

    printf("\nSetting SMS text mode....\n");
    if (verificaComando(UART_NUM_1,"AT+CMGF=1\r\n","OK\r\n")==0){
        printf("\nText Mode enabled !!!\n");
    }

    printf("\nEnabling DTMF recognition....\n");
    if (verificaComando(UART_NUM_1,"AT+DDET=1\r\n","OK\r\n")==0){
        printf("\nDTMF recognition is on !!!\n");
    }

    //Launching Task for getting MANUAL commands to be sent to SIM800L
    xTaskCreate(&getCommand, "getCommand", 8192, NULL, 5, NULL);
    //Launching Task for blinking led according to signal quality report 
    xTaskCreate(&toggleLed, "toggleLed", 4096, NULL, 5, NULL);
    
    printf("Entering while loop!!!!!\r\n");

    while (1) {
		// Read 4 lines data from the UART1
        line1 = read_line(UART_NUM_1);
        char *subline1=parse_line(line1);
        stampa_stringa(subline1,1);

        if (command_valid){
            printf("\nCommand valid, sending %s",command);
            //uart_write_bytes(UART_NUM_1, command, strlen(command));
            scriviUART(UART_NUM_1, command);
            command_valid=0;
        }

        vTaskDelay(50 / portTICK_RATE_MS);

		/* Read data from the UART2
        char *line2 = read_line(UART_NUM_2);
		// Write data back to the UART
		printf("I2: %s", line2);
        char* subline2=parse_line(line2,k);
     	printf("O2: %s", subline2);
        uart_write_bytes(UART_NUM_2,subline2, strlen(subline2));
        vTaskDelay(1000 / portTICK_RATE_MS);*/
        }
	fflush(stdout);
}