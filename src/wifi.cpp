#include <assert.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"

static bool g_wifi_connected = false;

static void oai_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data) {
  static int s_retry_num = 0;
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < 5) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(LOG_TAG, "retry to connect to the AP");
    }
    ESP_LOGI(LOG_TAG, "connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(LOG_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    g_wifi_connected = true;
  }
}

void oai_wifi_init(void) 
{
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &oai_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &oai_event_handler, NULL));

  ESP_ERROR_CHECK(esp_netif_init());
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

void oai_wifi(void) {

  wifi_config_t wifi_config;
  esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
  if (strlen((const char *)wifi_config.sta.ssid))
  {
    ESP_LOGI(LOG_TAG, "Connecting to last WiFi SSID: %s", wifi_config.sta.ssid);
  } else {
    #if defined(WIFI_SSID) && defined(WIFI_PASSWORD)
      ESP_LOGI(LOG_TAG, "Connecting to WiFi SSID: %s", WIFI_SSID);
      memset(&wifi_config, 0, sizeof(wifi_config));
      strncpy((char *)wifi_config.sta.ssid, (char *)WIFI_SSID,
              sizeof(wifi_config.sta.ssid));
      strncpy((char *)wifi_config.sta.password, (char *)WIFI_PASSWORD,
              sizeof(wifi_config.sta.password));
    #else 
      ESP_LOGI(LOG_TAG, "Please set WIFI_SSID and WIFI_PASSWORD via cmdline, then reboot");
      while (!g_wifi_connected) {
        vTaskDelay(pdMS_TO_TICKS(200));
      }
    #endif
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(34));

  ESP_ERROR_CHECK(esp_wifi_set_config(
      static_cast<wifi_interface_t>(ESP_IF_WIFI_STA), &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_connect());

  // block until we get an IP address
  while (!g_wifi_connected) {
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}
