//automatically pick up any incoming call after 1 ring: ATS0=1
//cfun=1,1 (full,rst)
//AT+CREG? if answer is 0,1 then is registered on home network
/*
int checkStatus() { // CHECK STATUS FOR RINGING or IN CALL

    sim800.println(“AT+CPAS”); // phone activity status: 0= ready, 2= unknown, 3= ringing, 4= in call
    delay(100);
    if (sim800.find(“+CPAS: “)) { // decode reply
        char c = sim800.read(); // gives ascii code for status number
        // simReply(); // should give ‘OK’ in Serial Monitor
        return c – 48; // return integer value of ascii code
    }
        else return -1;
}
*/

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
#include "simOK.c"
#include "credentials.h"

//toggleLed
void toggleLed(){
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

void setup(void){
    gpio_pad_select_gpio(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
	
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

    printf("==============================\r\n");
	printf("Welcome to SIM800L control App\r\n");
	printf("==============================\r\n");

    printf("Allowed sim number is %s\r\n",ALLOWED1);
    //presentation blinking
    gpio_set_level(BLINK_GPIO, 1);
    vTaskDelay(500 / portTICK_RATE_MS);
    gpio_set_level(BLINK_GPIO, 1);
    vTaskDelay(500 / portTICK_RATE_MS);
    gpio_set_level(BLINK_GPIO, 1);
    vTaskDelay(500 / portTICK_RATE_MS);
    gpio_set_level(BLINK_GPIO, 1);

    if (simOK(UART_NUM_1)==-1){
        printf("\nSim808 module not found, stop here!");
        foreverRed();
    } 

    //Launching Task for getting MANUAL commands to be sent to SIM800L
    xTaskCreate(&getCommand, "getCommand", 8192, NULL, 5, NULL);
    //Launching Task for blinking led according to signal quality report 
    xTaskCreate(&toggleLed, "toggleLed", 4096, NULL, 5, NULL);
}

void loop(void){
    // Read data from the UART1
    char *line1;
    line1 = read_line(UART_NUM_1);
    char *subline1=parse_line(line1);
    stampa_stringa(subline1);

    if (command_valid){
        printf("\nCommand valid, sending %s",command);
        //uart_write_bytes(UART_NUM_1, command, strlen(command));
        scriviUART(UART_NUM_1, command);
        //verificaComando(UART_NUM_1,command,"OK\r\n");
        command_valid=0;
    }

    vTaskDelay(50 / portTICK_RATE_MS);
    int current_phone_status = checkPhoneActivityStatus();

    if (current_phone_status == 0) {
        printf("No call in progress\n");
        //digitalWrite(red, LOW); // red LED off
    } else {
        if (current_phone_status == 4) { // in call 
            printf("Handling Call....\n");
            //digitalWrite(red, HIGH); // red LED on
            int DTMF = checkDTMF();
            printf("Tone detected equals %d\r\n",DTMF);
        }
    }
}

// Main application
void app_main() {
	setup();
    printf("Entering while loop!!!!!\r\n");
    while(1)
        loop();
    fflush(stdout);
}