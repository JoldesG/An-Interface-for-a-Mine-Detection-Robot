/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

//c libraries
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//real time operating sistem libraries
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_rom_gpio.h"

#include "sdkconfig.h"
#include "hal/gpio_types.h"
#include "driver/gpio.h"//gpio driver
#include "driver/uart.h"//uart driver

#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "sys/param.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_http_server.h"

// #define SSID "Joldes"
// #define PASS "joldes425"

#define SSID "AndroidAP9502"
#define PASS "george1999"

#define BLINK_LED 14
#define CHECK_PIN 4

static const int RX_BUF_SIZE = 2024;

#define TXD_PIN (GPIO_NUM_1)
#define RXD_PIN (GPIO_NUM_3)

#define TXD_PIN2 (GPIO_NUM_17)//for UART2
#define RXD_PIN2 (GPIO_NUM_16)//for UART2

void init(void);//initializing UART0
void init2(void);//initializing UART2
static void rx_task(void *arg);
static void getstring(char *str);
static void from_DMM_to_DD(void);
static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void wifi_connection();
esp_err_t client_post_handler(esp_http_client_event_handle_t evt);
static void client_post_function();
esp_err_t client_get_handler(esp_http_client_event_handle_t evt);
static void client_get_function();
static void client_get_function_local();

char coor1[] = "Latitude:";
char coor2[] = "Longitude:";
char buff1[13];
char buff2[14];
char rez_lat[10];
char rez_lng[10];

void app_main(void)
{
    nvs_flash_init();
    wifi_connection();
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    printf("WIFI was initiated ... \n\n");
    // vTaskDelay(3000 / portTICK_PERIOD_MS);
    // client_post_function();
    client_get_function();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    client_get_function_local();
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    init();
    init2();
    //xTaskCreate(rx_task,"uart_rx_task",1024*2,NULL,configMAX_PRIORITIES,NULL);
    
    gpio_reset_pin(BLINK_LED);//reset the state of the pin
    gpio_set_direction(BLINK_LED,GPIO_MODE_OUTPUT);//set the pin on the output mode

    gpio_reset_pin(CHECK_PIN);//reset the state of the pin
    gpio_set_direction(CHECK_PIN,GPIO_MODE_INPUT);//set the pin on the input mode

    while(1)
    {
        if(gpio_get_level(CHECK_PIN)==1)//constantlu interogate the state of the file
        {
            gpio_set_level(BLINK_LED,1);
            xTaskCreate(rx_task,"uart_rx_task",1024*2,NULL,configMAX_PRIORITIES,NULL);

            // vTaskDelay(3000 / portTICK_PERIOD_MS);
            client_post_function();//post the coordinates into the database
        }
        else
        {
            gpio_set_level(BLINK_LED,0);
        }
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        // vTaskDelay(1000 / portTICK_PERIOD_MS);
        /*
        vTaskDelay realizeaza o intarziere de un anumit numar de clock cycle-uri
        de aceea 1000 este impartit la perioada unui clock cycle in ms
        astfel se realizeaza o intarziere de un anumit nr de ms
        de 1000 de ms in cazul nostru adica 1s
        */
    }
    
}

void init(void)//initiate the uart responsible for code uploading
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,//setting the baud rate
        .data_bits = UART_DATA_8_BITS,//configuring the UART for 8 bits of data
        .parity = UART_PARITY_DISABLE,//disabling the parity bit
        .stop_bits = UART_STOP_BITS_1,//configuring the UART with 1 stop bit
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,//micro UART hardware to control disable
        .source_clk = UART_SCLK_APB,//the clock at aroung 80MHz
    };

    uart_driver_install(UART_NUM_0,RX_BUF_SIZE*2,0,0,NULL,0);
    uart_param_config(UART_NUM_0,&uart_config);
    uart_set_pin(UART_NUM_0,TXD_PIN,RXD_PIN,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);
}

void init2(void)//initiate the uart that will take the coordinates
{
    const uart_config_t uart_config_2 = {
        .baud_rate = 9600,//setting the baud rate
        .data_bits = UART_DATA_8_BITS,//configuring the UART for 8 bits of data
        .parity = UART_PARITY_DISABLE,//disabling the parity bit
        .stop_bits = UART_STOP_BITS_1,//configuring the UART with 1 stop bit
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,//micro UART hardware to control disable
        .source_clk = UART_SCLK_APB,//the clock at aroung 80MHz
    };

    uart_driver_install(UART_NUM_2,RX_BUF_SIZE*2,0,0,NULL,0);
    uart_param_config(UART_NUM_2,&uart_config_2);
    uart_set_pin(UART_NUM_2,TXD_PIN2,RXD_PIN2,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);
}

static void rx_task(void *arg)
{
    const char *str = "$GPGGA";//the string after which we are going to extract the information
    char *p = NULL;
    char str1[100];
    int j = 0;

    uint8_t* data = (uint8_t*)malloc(RX_BUF_SIZE+1);
    char *str2 = (char*)malloc(sizeof(str1));
    while(1)
    {
        const int rxbytes = uart_read_bytes(UART_NUM_2,data,RX_BUF_SIZE,1000/portTICK_RATE_MS);//extract the data
        if(rxbytes>0)
        {
            p = strstr((const char*)data,str);//copy the data after the character copid in 'str'
            if(p)
            {
                getstring(p);//function to extract the coordinates

                for(int i = 0; p[i] != '\n'; i++)
                {
                    str2[j] = p[i];
                    j++;
                }
                str2[j+1] = '\n';
                //const int len = j+2;
                data[rxbytes] = 0;
                //uart_write_bytes(UART_NUM_0,(const char*)str2,len);

                from_DMM_to_DD();//changing the format

                uart_write_bytes(UART_NUM_0,(const char*)coor1,9);
                uart_write_bytes(UART_NUM_0,(const char*)rez_lat,10);
                uart_write_bytes(UART_NUM_0,(const char*)coor2,10);
                uart_write_bytes(UART_NUM_0,(const char*)rez_lng,10);
                break;
            }
        }
    vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    free(data);
    free(str2);
    //vTaskSuspend(NULL);
    vTaskDelete(NULL);
}

static void getstring(char *str)
{
    int cnt = 0;
    int i = 0;
    int j = 0;

    while(cnt < 2)
    {
        if(str[i] == ',')
        {
            cnt++;
        }
        i++;
    }
    for(j=0;j<12;j++)//extracting the latitude
    {
        buff1[j] = str[i+j];
    }
    buff1[12] = '\n';

    while(cnt < 4)
    {
        if(str[i] == ',')
        {
            cnt++;
        }
        i++;
    }
    for(j=0;j<13;j++)//extracting the longitude
    {
        buff2[j] = str[i+j];
    }
    buff2[13] = '\n';
}

static void from_DMM_to_DD(void)
{
    float deg = 0;
    int min = 0;

    //latitude
    deg = atoi(buff1);
    min = deg;
    min /= 100;//final rezult int
    deg = atof(buff1);
    deg = deg - min*100;//final rezult float
    deg = min + deg/60;//final rezult
    sprintf(rez_lat,"%f",deg);
    rez_lat[9] = '\n';

    //longitude
    deg = atoi(buff2);
    min = deg;
    min /= 100;//final rezult int
    deg = atof(buff2);
    deg = deg - min*100;//final rezult float
    deg = min + deg/60;//final rezult
    sprintf(rez_lng,"%f",deg);
    rez_lng[9] = '\n';
}

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
        case WIFI_EVENT_STA_START:
        printf("WiFi connecting ... \n");
        break;
        case WIFI_EVENT_STA_CONNECTED:
        printf("WiFi connected ... \n");
        break;
        case WIFI_EVENT_STA_DISCONNECTED:
        printf("WiFi lost connection ... \n");
        break;
        case IP_EVENT_STA_GOT_IP:
        printf("WiFi got IP ... \n\n");
        break;
        default:
        break;
    }
}

void wifi_connection()
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_configuration = {
        .sta = {//passing the name of the network and the password
            .ssid = SSID,
            .password = PASS
        }
    };
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    esp_wifi_start();//start the wifi connection
    esp_wifi_connect();
}

esp_err_t client_post_handler(esp_http_client_event_handle_t evt)
{
    switch (evt -> event_id)
    {
        case HTTP_EVENT_ON_DATA:
        printf("HTTP_EVENT_ON_DATA: %.*s\n", evt->data_len, (char *)evt->data);
        break;
        default:
        break;
    }
    return ESP_OK;
}

static void client_post_function()
{
    esp_http_client_config_t config_post = {//choosing the post method and passing the page thet needs to be accessed
        // .url = "http://192.168.0.139:8085/post",
        .url = "http://192.168.100.175:8085/post",
        .method = HTTP_METHOD_POST,
        .event_handler = client_post_handler
    };
    
    esp_http_client_handle_t client_post = esp_http_client_init(&config_post);

    // char *post_data = "{\"Latitude\":46.757828,\"Longitude\":23.389917}";
    char *post_data = (char*)malloc(45);
    char first[] = "{\"Latitude\":";
    char second[] = ",\"Longitude\":";
    //formating the json string
    for(int i = 0; i < 12; i++)
    {
        post_data[i] = first[i];
    }
    for(int i = 0; i < 9; i++)
    {
        post_data[i+12] = rez_lat[i];
    }
    for(int i = 0; i < 13; i++)
    {
        post_data[i+21] = second[i];
    }
    for(int i = 0; i < 9; i++)
    {
        post_data[i+34] = rez_lng[i];
    }
    post_data[43] = '}';
    post_data[44] = '\0';

    esp_http_client_set_post_field(client_post, post_data, strlen(post_data));//post the data on the specified page
    esp_http_client_set_header(client_post, "Content-Type", "application/json");

    printf("1 ...........\n");
    esp_http_client_perform(client_post);
    printf("2 ...........\n");
    esp_http_client_cleanup(client_post);
}

esp_err_t client_get_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        printf("HTTP_EVENT_ON_DATA: %.*s\n", evt->data_len, (char *)evt->data);
        break;

    default:
        break;
    }
    return ESP_OK;
}

static void client_get_function()
{
    esp_http_client_config_t config_get = {//choosing the get method and specified the page that needs to be accessed
        // .url = "http://192.168.0.139:8085/get",
        .url = "http://192.168.100.175:8085/get",
        .method = HTTP_METHOD_GET,
        .event_handler = client_get_handler};
        
    esp_http_client_handle_t client_get = esp_http_client_init(&config_get);

    printf("1 ...........\n");
    esp_http_client_perform(client_get);
    printf("2 ...........\n\n");
    esp_http_client_cleanup(client_get);
}

static void client_get_function_local()
{
    esp_http_client_config_t config_get = {//choosing the get method and specified the page that needs to be accessed
        // .url = "http://192.168.0.139:8085/get",
        .url = "http://192.168.100.175:8085/get",
        .method = HTTP_METHOD_GET,
        .event_handler = client_get_handler};
        
    esp_http_client_handle_t client_get = esp_http_client_init(&config_get);

    printf("1 ...........\n");
    esp_http_client_perform(client_get);
    printf("2 ...........\n\n");
    esp_http_client_cleanup(client_get);
}