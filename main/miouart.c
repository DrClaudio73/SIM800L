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

    ESP_LOGW(TAG,"==============================");
	ESP_LOGW(TAG,"Welcome to SIM800L control App");
	ESP_LOGW(TAG,"==============================\r\n");

    ESP_LOGI(TAG,"Allowed sim number is %s\r\n",ALLOWED1);
    //presentation blinking
    gpio_set_level(BLINK_GPIO, 1);
    vTaskDelay(500 / portTICK_RATE_MS);
    gpio_set_level(BLINK_GPIO, 1);
    vTaskDelay(500 / portTICK_RATE_MS);
    gpio_set_level(BLINK_GPIO, 1);
    vTaskDelay(500 / portTICK_RATE_MS);
    gpio_set_level(BLINK_GPIO, 1);

    if (simOK(UART_NUM_1)==-1){
        ESP_LOGE(TAG,"\nSim808 module not found, stop here!");
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
        ESP_LOGW(TAG,"\n********Command valid, sending %s",command);
        //uart_write_bytes(UART_NUM_1, command, strlen(command));
        scriviUART(UART_NUM_1, command);
        //verificaComando(UART_NUM_1,command,"OK\r\n");
        command_valid=0;
    }

    vTaskDelay(50 / portTICK_RATE_MS);
    int current_phone_status = checkPhoneActivityStatus();

    if (current_phone_status == 0) {
        ESP_LOGI(TAG,"No call in progress");
        //digitalWrite(red, LOW); // red LED off
    } else if (current_phone_status == -1){
        ESP_LOGE(TAG,"Not possible to determine Phone Activity Status!!!");
    } else if (current_phone_status == 3){ //RINGING
        if (checkCallingNumber()==0){
            ESP_LOGI(TAG,"\n********Valid calling number: %s. Answering call.....",parametro_feedback);;
                if (verificaComando(UART_NUM_1,"ATA","OK\r\n")==0){
                    ESP_LOGI(TAG,"\n********Answered call!!!");
                    if (verificaComando(UART_NUM_1,"AT+VTS=\"{1,1},{0,2},{6,1}\"","OK\r\n")==0){
                        ESP_LOGI(TAG,"\n********Answer tones sent!!!");
                        vTaskDelay(500 / portTICK_RATE_MS);
                    } else {
                        ESP_LOGE(TAG,"\n********ERROR in sending answering tones!!!");
                    }
                } else {
                    ESP_LOGE(TAG,"\n********Error in answering allowed call!!!");
                }
        } else {
            ESP_LOGE(TAG,"\n********Calling number %s NOT valid!!! Answering and hanging right after.",parametro_feedback);
            if (verificaComando(UART_NUM_1,"ATA","OK\r\n")==0){
                    ESP_LOGI(TAG,"\n********Answered NOT allowed call!!!");
                    vTaskDelay(2000 / portTICK_RATE_MS);
                    if (verificaComando(UART_NUM_1,"ATH","OK\r\n")==0){
                        ESP_LOGI(TAG,"\n********Hanged call!!!");
                        vTaskDelay(1000 / portTICK_RATE_MS);
                    } else {
                        ESP_LOGE(TAG,"\n********ERROR in hanging NOT allowed call!!!");
                    }
                } else {
                    ESP_LOGE(TAG,"\n********Error in answering NOT allowed call!!!");
                }
        }
    } else if (current_phone_status == 4) { // in call 
            ESP_LOGW(TAG,"Handling Call....");
            //digitalWrite(red, HIGH); // red LED on
            //int DTMF = checkDTMF();
            if (pending_DTMF>=0){
                int DTMF=pending_DTMF;
                pending_DTMF=-1;
                ESP_LOGW(TAG,"Tone detected equals %d. Replying with the same tone.",DTMF);;
                char replyTone[30];
                sprintf(replyTone,"AT+VTS=\"{%d,2}\"",DTMF);
                vTaskDelay(2000 / portTICK_RATE_MS);
                if (verificaComando(UART_NUM_1,replyTone,"OK\r\n")==0){
                            ESP_LOGI(TAG,"\nReply tone sent!!!");
                        } else {
                            ESP_LOGE(TAG,"\nERROR in sending reply tone!!!");
                        }
            } else 
            {
                printf("\nnop\n");
            }
    }
}

// Main application
void app_main() {
	setup();
    ESP_LOGE(TAG,"Entering while loop!!!!!\r\n");
    while(1)
        loop();
    fflush(stdout);
}