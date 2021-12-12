
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
#define MAXIMUM_RETRY 5

static EventGroupHandle_t wifi_event_group;

#define CONNECTED_BIT BIT0
#define FAIL_BIT BIT1

static const char *TAG = "wifi station";

static int retry_num = 0;


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

static void event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data){
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(wifi_event_group, FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        retry_num = 0;
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        blink();
        
    }
}

void wifi_init(void){
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t conf = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&conf));
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            CONNECTED_BIT | FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 WIFI_SSID, WIFI_PASS);
    } else if (bits & FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 WIFI_SSID, WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(wifi_event_group);
}

void effect(void *pvParameter) {
    
    dig_setup();

    while (1) {
       snake();
    }
    
}
void app_main() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    dig_setup();
    wifi_init();
    //xTaskCreate(&effect, "effect", 2048, NULL, 5, NULL);
}