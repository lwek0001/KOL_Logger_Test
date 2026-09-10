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

// configuration macros for WiFi settings, defined in project configuration
#define ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID // preprocessor macro for WiFi SSID from project configuration, associate ESP_WIFI_SSID with CONFIG_ESP_WIFI_SSID
#define ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define ESP_WIFI_CHANNEL   CONFIG_ESP_WIFI_CHANNEL
#define MAX_STA_CONN       CONFIG_ESP_MAX_STA_CONN

#if CONFIG_ESP_GTK_REKEYING_ENABLE
#define GTK_REKEY_INTERVAL CONFIG_ESP_GTK_REKEY_INTERVAL
#else
#define GTK_REKEY_INTERVAL 0
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
        if (current < 0.0f) {
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

/* wifi_event_handler function to handle WiFi events
 Just log Wi-Fi events (station connected/disconnected) to the console */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        // // event_data is a pointer to a wifi_event_ap_staconnected_t structure
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        // event_data is a pointer to a wifi_event_ap_stadisconnected_t structure
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

// wifi_init_softap function to initialize WiFi in softAP mode
void wifi_init_softap(void)
{
    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create the default event loop that running in background
    // The default event loop is used to handle Wi-Fi events and IP events
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // Create default WIFI AP netif
    esp_netif_create_default_wifi_ap();
    // Initialize Wi-Fi with default configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // call wifi_event_handler() when Wi-Fi events occur 
    /*  Wi-Fi event occurs
       ↓
        ESP event loop
       ↓
        wifi_event_handler()*/
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    // configure wifi settings for softAP mode 
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ESP_WIFI_SSID,
            .ssid_len = strlen(ESP_WIFI_SSID),
            .channel = ESP_WIFI_CHANNEL,
            .password = ESP_WIFI_PASS,
            .max_connection = MAX_STA_CONN,
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
            .gtk_rekey_interval = GTK_REKEY_INTERVAL,
        },
    };
    if (strlen(ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP)); // operate ESP32 as an Access Point (AP)
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());  // start the Wi-Fi driver (ESP32 is now operating as an Access Point)

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             ESP_WIFI_SSID, ESP_WIFI_PASS, ESP_WIFI_CHANNEL);
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
