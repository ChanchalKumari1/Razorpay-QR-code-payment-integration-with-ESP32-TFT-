#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "nvs.h"
#include "cJSON.h"
#include "pngle.h"
#include "tft_display.h"

static const char *TAG = "UPI_PAYMENT";

// API URL
#define UPI_BASE_URL "https://digitalmonk.biz/upi"
#define API_URL UPI_BASE_URL "/xxx/add_merchant.php"
#define QR_API_URL UPI_BASE_URL "/xxx/get_qr.php"
#define PAYMENT_STATUS_URL UPI_BASE_URL "/xxx/payment_status.php"

// WiFi credentials 
#define EXAMPLE_ESP_WIFI_SSID      "DigitalMonk"
#define EXAMPLE_ESP_WIFI_PASS      "xxxxxxx"
#define EXAMPLE_ESP_MAXIMUM_RETRY  2

// Constants
#define MAX_QR_IMAGE_SIZE 20000

// Global variables for WiFi
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int s_retry_num = 0;

typedef struct {
    char token[64];
    char merchant_id[16];
    char merchant_name[100];
    char merchant_upi[100];
} qr_task_data_t;

typedef struct {
    char status[32];
    char amount[32];
    char payer_name[100];
    char payer_upi[100];
    char paid_at[64];
    char error_message[256];
} payment_status_t;

// Queue handle
static QueueHandle_t qr_task_queue = NULL;

// Global variable for API response
static char *api_response = NULL;
static uint8_t *qr_image_buffer = NULL;
static int qr_image_size = 0;
static int image_total_len = 0;  

char cached_token[64] = {0};
uint8_t *cached_qr_buffer = NULL;
int cached_qr_size = 0;

static uint16_t png_last_y = 0xFFFF;
static uint16_t png_start_x = 0;
static uint16_t png_count = 0;
static uint16_t png_line_buffer[320];
static bool png_decode_active = false;

static void event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data);
void wifi_init_sta(void);

static esp_err_t image_http_event_handler(esp_http_client_event_t *evt);
static esp_err_t http_event_handler(esp_http_client_event_t *evt);
esp_err_t save_merchant_data_to_nvs(const char *token, const char *merchant_id, const char *name, const char *upi_id);
                                          
char* call_upi_api(const char *name, const char *upi_id);
char* call_qr_api(const char *token);
static char *build_post_data(const char *name, const char *upi_id);
void qr_api_task(void *pvParameters);

static void convert_to_http_url(const char *https_url, char *http_url, size_t size);

static esp_err_t root_get_handler(httpd_req_t *req);
static esp_err_t register_post_handler(httpd_req_t *req);
void start_web_server(void);

void check_payment_status_task(void *pvParameters);
char* call_payment_status_api(const char *token);
void start_payment_monitoring(const char *token);

void on_init(pngle_t *pngle, uint32_t w, uint32_t h);
void on_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t rgba[4]);

void reset_png_decoder_state(void);
void start_payment_monitoring_with_handle(const char *token, TaskHandle_t *task_handle);
void on_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t rgba[4]);