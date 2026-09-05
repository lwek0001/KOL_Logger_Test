/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_http_server.h"

/* The examples use WiFi configuration that you can set via project configuration menu.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL   CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN       CONFIG_ESP_MAX_STA_CONN

#if CONFIG_ESP_GTK_REKEYING_ENABLE
#define EXAMPLE_GTK_REKEY_INTERVAL CONFIG_ESP_GTK_REKEY_INTERVAL
#else
#define EXAMPLE_GTK_REKEY_INTERVAL 0
#endif

// initialise fake measurements for testing
static float current = 123.0f;

static const char *TAG = "wifi softAP";

// freeRTOS task to simulate changing measurements
static void fake_measurement_task(void *pvParameters)
{
    while (1)
    {
        current -= 1.0f;
        if (current > 200.0f) {
            current = 100.0f;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// data_get_handler to return JSON data for current
// runs whenever someone's browser requests /data (current draw)
static esp_err_t data_get_handler(httpd_req_t *req)
{
    // 100 character buffer to hold response
    char response[100];

    // format response as JSON with current value
    snprintf(response,
             sizeof(response),
             "{\"current\":%.1f}",
             current);
    
    // set response type to JSON 
    httpd_resp_set_type(req, "application/json");
    // send response
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

// runs whenever someone's browser requests / (root) - main page
static esp_err_t root_get_handler(httpd_req_t *req)
{

    // browser will receive this HTML page when it requests /
    const char *html =
        "<!DOCTYPE html>"
        "<html>"

        "<head>"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<title>KOL Logger</title>"
        "<link rel=\"icon\" href=\"data:,\">"

        "<style>"
        "body {"
        "    font-family: Arial, sans-serif;"
        "    text-align: center;"
        "    background-color: #f4f4f4;"
        "    margin: 0;"
        "    padding: 30px;"
        "}"

        ".card {"
        "    background-color: white;"
        "    max-width: 400px;"
        "    margin: auto;"
        "    padding: 25px;"
        "    border-radius: 10px;"
        "}"

        ".value {"
        "    font-size: 32px;"
        "    font-weight: bold;"
        "    margin: 10px;"
        "}"
        "</style>"

        "</head>"

        "<body>"

        "<div class=\"card\">"

        "<h1>KOL Logger</h1>"
        "<p>Status: Running</p>"

        "<h2>Current Draw</h2>"
        "<div class=\"value\" id=\"current\">-- mA</div>"

        "</div>"

        "<script>"

        "async function updateData() {"

        "    const response = await fetch('/data');"
        "    const data = await response.json();"

        "    document.getElementById('current').textContent = "
        "        data.current.toFixed(1) + ' mA';"

        "}"

        "updateData();"

        "setInterval(updateData, 1000);"

        "</script>"

        "</body>"
        "</html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

// when web server requests /, this structure defines how to handle it (use root_get_handler)
static const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler,
    .user_ctx = NULL
};
// when web server requests /data, this structure defines how to handle it (use data_get_handler)
static const httpd_uri_t data_uri = {
    .uri = "/data",
    .method = HTTP_GET,
    .handler = data_get_handler,
    .user_ctx = NULL
};

// start_webserver function to start the HTTP server and register URI handlers
static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;

    // Create default configuration 
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_LOGI(TAG, "Starting HTTP server");

    if (httpd_start(&server, &config) == ESP_OK) {

        // Register / webpage
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &data_uri);

        ESP_LOGI(TAG, "HTTP server started");

        return server;
    }

    ESP_LOGE(TAG, "Failed to start HTTP server");

    return NULL;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
            .authmode = WIFI_AUTH_WPA2_PSK,
#endif
            .pmf_cfg = {
                    .required = true,
            },
#ifdef CONFIG_ESP_WIFI_BSS_MAX_IDLE_SUPPORT
            .bss_max_idle_cfg = {
                .period = WIFI_AP_DEFAULT_MAX_IDLE_PERIOD,
                .protected_keep_alive = 1,
            },
#endif
            .gtk_rekey_interval = EXAMPLE_GTK_REKEY_INTERVAL,
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();

    start_webserver();

    xTaskCreate(
    fake_measurement_task,  // task function
    "fake_measurement",     // task name
    2048,                   // stack size  
    NULL,                   // parameters passed to task 
    5,                      // task priority    
    NULL                    // task handle
    );
}
