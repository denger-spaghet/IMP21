
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "driver/gpio.h"


#define LED_1  12
#define LED_2  13
#define LED_3  14

#define GREEN  25
#define BLUE  26
#define RED  27

#define WIFI_SSID "UPC3409456"
#define WIFI_PASS "nZ48Yuwwwaee"

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;


void color_switch(int current_color){
    switch (current_color){
        case 0:
            ESP_ERROR_CHECK(gpio_set_level(RED, 0));
            ESP_ERROR_CHECK(gpio_set_level(GREEN, 1));
            break;
        case 1:
            ESP_ERROR_CHECK(gpio_set_level(GREEN, 0));
            ESP_ERROR_CHECK(gpio_set_level(BLUE, 1));
            break;
        case 2:
            ESP_ERROR_CHECK(gpio_set_level(BLUE, 0));
            ESP_ERROR_CHECK(gpio_set_level(RED, 1));
            break;
    }
}

void blink() {
    int cnt = 0;

    while(cnt < 5) {
        /* Blink off (output low) */
        ESP_ERROR_CHECK(gpio_set_level(LED_1, cnt & 0x01));
        ESP_ERROR_CHECK(gpio_set_level(LED_2, cnt & 0x01));
        ESP_ERROR_CHECK(gpio_set_level(LED_3, cnt & 0x01));
        vTaskDelay(500 / portTICK_RATE_MS);
        cnt++;
    }
}

void snake() {
    int cnt = 0;

    while(cnt < 5) {
            for (int cnt = 0; cnt < 4; cnt++){
                ESP_ERROR_CHECK(gpio_set_level(cnt + 12, 1));
                vTaskDelay(100 / portTICK_RATE_MS);
                ESP_ERROR_CHECK(gpio_set_level(cnt + 12, 0));
            }
            color_switch(cnt % 3);
            
        cnt ++;
    }
    ESP_ERROR_CHECK(gpio_set_level(RED, 0));
    ESP_ERROR_CHECK(gpio_set_level(GREEN, 0));
    ESP_ERROR_CHECK(gpio_set_level(BLUE, 0));

}

void dig_setup() {
    gpio_pad_select_gpio(LED_1);
    gpio_pad_select_gpio(LED_2);
    gpio_pad_select_gpio(LED_3);

    gpio_pad_select_gpio(RED);
    gpio_pad_select_gpio(GREEN);
    gpio_pad_select_gpio(BLUE);

    /* Set the GPIO as a push/pull output */
    ESP_ERROR_CHECK(gpio_set_direction(LED_1, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_direction(LED_2, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_direction(LED_3, GPIO_MODE_OUTPUT));

    ESP_ERROR_CHECK(gpio_set_direction(RED, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_direction(GREEN, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_direction(BLUE, GPIO_MODE_OUTPUT));
}

void wifi_setup() {
    
}

static esp_err_t event_handler(void *ctx, system_event_t *event){
    switch (event->event_id)
    {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
		xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    
    default:
        break;
    }
    return ESP_OK;
}

void effect(void *pvParameter) {
    
    printf("Main task: waiting for connection to the wifi network... ");
	xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
	printf("connected!\n");

    tcpip_adapter_ip_info_t ip_info;
	ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
	printf("IP Address:  %s\n", ip4addr_ntoa(&ip_info.ip));
	printf("Subnet mask: %s\n", ip4addr_ntoa(&ip_info.netmask));
	printf("Gateway:     %s\n", ip4addr_ntoa(&ip_info.gw));

    dig_setup();
    while (1) {
        blink();
        snake();
    }
    
}
void app_main() {
    ESP_ERROR_CHECK(nvs_flash_init());

    esp_log_level_set("wifi", ESP_LOG_NONE);

    wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

	wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
	printf("Connecting to %s\n", WIFI_SSID);

    xTaskCreate(&effect, "effect", 2048, NULL, 5, NULL);
}