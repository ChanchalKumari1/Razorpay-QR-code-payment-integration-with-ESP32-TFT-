
static esp_err_t image_http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_HEADER:
        if (evt->data && strstr((const char *)evt->data, "Content-Length:"))
        {
            qr_image_size = atoi((const char *)evt->data + 15);
            ESP_LOGI("IMG_DL", "Expected image size: %d bytes", qr_image_size);

            // Limit image size to prevent large downloads
            if (qr_image_size > MAX_QR_IMAGE_SIZE)
            {
                ESP_LOGW("IMG_DL", "Image too large (%d bytes), limiting to %d", qr_image_size, MAX_QR_IMAGE_SIZE);
                qr_image_size = MAX_QR_IMAGE_SIZE;
            }

            if (qr_image_size > 0 && qr_image_size < 100000)
            {
                if (qr_image_buffer)
                {
                    free(qr_image_buffer);
                    qr_image_buffer = NULL;
                }
                qr_image_buffer = (uint8_t *)malloc(qr_image_size);
                if (qr_image_buffer)
                {
                    image_total_len = 0;
                }
                else
                {
                    ESP_LOGE("IMG_DL", "Failed to allocate buffer");
                }
            }
        }
        break;

    case HTTP_EVENT_ON_DATA:
        if (evt->data && evt->data_len > 0)
        {
            if (!qr_image_buffer)
            {
                qr_image_size = 32768;
                qr_image_buffer = (uint8_t *)malloc(qr_image_size);
                if (!qr_image_buffer)
                {
                    ESP_LOGE("IMG_DL", "Failed to allocate initial buffer");
                    break;
                }
                image_total_len = 0;
            }

            if (image_total_len + evt->data_len > qr_image_size)
            {
                int new_size = qr_image_size * 2;
                uint8_t *new_buffer = (uint8_t *)realloc(qr_image_buffer, new_size);
                if (new_buffer)
                {
                    qr_image_buffer = new_buffer;
                    qr_image_size = new_size;
                    ESP_LOGI("IMG_DL", "Buffer grown to %d bytes", new_size);
                }
                else
                {
                    ESP_LOGE("IMG_DL", "Failed to grow buffer");
                    break;
                }
            }

            memcpy(qr_image_buffer + image_total_len, evt->data, evt->data_len);
            image_total_len += evt->data_len;
        }
        break;

    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI("IMG_DL", "Download completed: %d bytes total", image_total_len);
        if (qr_image_buffer && image_total_len > 0)
        {
            // Check for PNG signature
            if (image_total_len > 4 &&
                qr_image_buffer[0] == 0x89 &&
                qr_image_buffer[1] == 'P' &&
                qr_image_buffer[2] == 'N' &&
                qr_image_buffer[3] == 'G')
            {
                ESP_LOGI("IMG_DL", "Valid PNG image received (%d bytes)", image_total_len);

                // Check PNG dimensions (bytes 16-23 contain width and height)
                if (image_total_len > 23)
                {
                    uint32_t width = (qr_image_buffer[16] << 24) | (qr_image_buffer[17] << 16) |
                                     (qr_image_buffer[18] << 8) | qr_image_buffer[19];
                    uint32_t height = (qr_image_buffer[20] << 24) | (qr_image_buffer[21] << 16) |
                                      (qr_image_buffer[22] << 8) | qr_image_buffer[23];
                    ESP_LOGI("IMG_DL", "PNG dimensions: %d x %d", width, height);
                }
            }
            else
            {
                ESP_LOGE("IMG_DL", "Not a valid PNG (first bytes: %02x %02x %02x %02x)",
                         qr_image_buffer[0], qr_image_buffer[1], qr_image_buffer[2], qr_image_buffer[3]);
                if (image_total_len > 100)
                    image_total_len = 100;
                qr_image_buffer[image_total_len] = '\0';
                ESP_LOGE("IMG_DL", "Content: %s", (char *)qr_image_buffer);
            }
        }
        break;
    case HTTP_EVENT_ERROR:
        ESP_LOGE("IMG_DL", "HTTP error occurred");
        break;

    default:
        break;
    }
    return ESP_OK;
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        if (!api_response)
        {
            api_response = (char *)malloc(evt->data_len + 1);
            if (!api_response)
                return ESP_FAIL;
            memcpy(api_response, evt->data, evt->data_len);
            api_response[evt->data_len] = '\0';
        }
        else
        {
            size_t old_len = strlen(api_response);
            api_response = (char *)realloc(api_response, old_len + evt->data_len + 1);
            if (!api_response)
                return ESP_FAIL;
            memcpy(api_response + old_len, evt->data, evt->data_len);
            api_response[old_len + evt->data_len] = '\0';
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

esp_err_t save_merchant_data_to_nvs(const char *token, const char *merchant_id, const char *name, const char *upi_id)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);

    if (err != ESP_OK)
    {
        ESP_LOGE("NVS", "Error opening NVS: %s", esp_err_to_name(err));
        return err;
    }

    // --- CLEAR ALL OLD DATA ---
    err = nvs_erase_all(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to clear old data: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI("NVS", "All previous merchant details cleared.");
    }

    // --- SAVE NEW DATA ---
    if (token)
        nvs_set_str(nvs_handle, "esp_token", token);
    if (merchant_id)
        nvs_set_str(nvs_handle, "merchant_id", merchant_id);
    if (name)
        nvs_set_str(nvs_handle, "merchant_name", name);
    if (upi_id)
        nvs_set_str(nvs_handle, "upi_id", upi_id);

    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    ESP_LOGI("NVS", "New merchant data saved successfully");
    return ESP_OK;
}

char *call_qr_api(const char *token)
{
    if (!token || strlen(token) == 0)
    {
        ESP_LOGE("QR_API", "No token provided");
        return NULL;
    }

    if (api_response)
    {
        free(api_response);
        api_response = NULL;
    }

    char qr_url[256];
    snprintf(qr_url, sizeof(qr_url), "%s?token=%s", QR_API_URL, token);

    ESP_LOGI("QR_API", "Calling QR API: %s", qr_url);

    esp_http_client_config_t config = {
        .url = qr_url,
        .method = HTTP_METHOD_GET,
        .event_handler = http_event_handler,
        .timeout_ms = 15000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        return NULL;
    }

    esp_err_t err = esp_http_client_perform(client);
    char *result = NULL;

    if (err == ESP_OK)
    {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI("QR_API", "HTTP Status = %d", status_code);

        if (status_code == 200 && api_response)
        {
            ESP_LOGI("QR_API", "Raw Response: %.100s...", api_response);

            cJSON *root = cJSON_Parse(api_response);
            if (root)
            {
                cJSON *success = cJSON_GetObjectItem(root, "success");
                if (success && cJSON_IsBool(success) && success->valueint)
                {
                    cJSON *qr_id = cJSON_GetObjectItem(root, "qr_id");
                    cJSON *qr_url_json = cJSON_GetObjectItem(root, "qr_url");
                    cJSON *upi_string = cJSON_GetObjectItem(root, "upi_string");
                    cJSON *name = cJSON_GetObjectItem(root, "name");
                    cJSON *shop_name = cJSON_GetObjectItem(root, "shop_name");
                    cJSON *upi_id = cJSON_GetObjectItem(root, "upi_id");

                    ESP_LOGI("QR_API", "========== QR CODE GENERATED ==========");
                    ESP_LOGI("QR_API", "QR ID: %s", qr_id ? qr_id->valuestring : "N/A");
                    ESP_LOGI("QR_API", "QR URL: %s", qr_url_json ? qr_url_json->valuestring : "N/A");
                    ESP_LOGI("QR_API", "UPI String: %s", upi_string ? upi_string->valuestring : "N/A");

                    result = (char *)malloc(1024);
                    if (result)
                    {
                        snprintf(result, 1024,
                                 "{\"success\":true,\"qr_id\":\"%s\",\"qr_url\":\"%s\","
                                 "\"upi_string\":\"%s\",\"name\":\"%s\",\"shop_name\":\"%s\",\"upi_id\":\"%s\"}",
                                 qr_id ? qr_id->valuestring : "",
                                 qr_url_json ? qr_url_json->valuestring : "",
                                 upi_string ? upi_string->valuestring : "",
                                 name ? name->valuestring : "",
                                 shop_name ? shop_name->valuestring : "",
                                 upi_id ? upi_id->valuestring : "");
                    }
                }
                else
                {
                    cJSON *message = cJSON_GetObjectItem(root, "message");
                    const char *error_msg = message ? message->valuestring : "QR generation failed";
                    ESP_LOGE("QR_API", "QR API error: %s", error_msg);

                    result = (char *)malloc(256);
                    if (result)
                    {
                        snprintf(result, 256, "{\"success\":false,\"message\":\"%s\"}", error_msg);
                    }
                }
                cJSON_Delete(root);
            }
            else
            {
                ESP_LOGE("QR_API", "Failed to parse JSON response");
                result = strdup("{\"success\":false,\"message\":\"Invalid JSON response\"}");
            }

            free(api_response);
            api_response = NULL;
        }
        else
        {
            ESP_LOGE("QR_API", "HTTP Error: %d", status_code);
            result = strdup("{\"success\":false,\"message\":\"HTTP request failed\"}");
        }
    }
    else
    {
        ESP_LOGE("QR_API", "Request failed: %s", esp_err_to_name(err));
        result = strdup("{\"success\":false,\"message\":\"Network error\"}");
    }

    esp_http_client_cleanup(client);
    return result;
}

static char *build_post_data(const char *name, const char *upi_id)
{
    char *post_data = (char *)malloc(512);
    if (!post_data)
        return NULL;

    char encoded_name[256] = {0};
    char *dest = encoded_name;
    for (const char *src = name; *src; src++)
    {
        if (*src == ' ')
        {
            *dest++ = '%';
            *dest++ = '2';
            *dest++ = '0';
        }
        else
        {
            *dest++ = *src;
        }
    }
    *dest = '\0';

    snprintf(post_data, 512,
             "admin_pass=upi_testing&name=%s&shop_name=ESP32_Store&upi_id=%s",
             encoded_name, upi_id);

    return post_data;
}

char *call_upi_api(const char *name, const char *upi_id)
{
    char *post_data = build_post_data(name, upi_id);
    if (!post_data)
    {
        ESP_LOGE("API", "Failed to allocate POST data");
        return NULL;
    }

    ESP_LOGI("API", "Sending POST data: %s", post_data);

    if (api_response)
    {
        free(api_response);
        api_response = NULL;
    }

    esp_http_client_config_t config = {
        .url = API_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .timeout_ms = 15000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        free(post_data);
        return NULL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    char *formatted_response = NULL;

    if (err == ESP_OK)
    {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI("API", "HTTP Status = %d", status_code);

        if (status_code == 200 && api_response)
        {
            ESP_LOGI("API", "Raw Response: %.100s...", api_response);

            cJSON *root = cJSON_Parse(api_response);
            if (root)
            {
                cJSON *success = cJSON_GetObjectItem(root, "success");
                if (success && cJSON_IsBool(success) && success->valueint)
                {
                    cJSON *merchant_id = cJSON_GetObjectItem(root, "merchant_id");
                    cJSON *name_json = cJSON_GetObjectItem(root, "name");
                    cJSON *shop_name = cJSON_GetObjectItem(root, "shop_name");
                    cJSON *upi_id_json = cJSON_GetObjectItem(root, "upi_id");
                    cJSON *esp_token = cJSON_GetObjectItem(root, "esp_token");
                    cJSON *message = cJSON_GetObjectItem(root, "message");

                    formatted_response = (char *)malloc(1024);
                    if (formatted_response)
                    {
                        snprintf(formatted_response, 1024,
                                 "{\"success\":true,\"merchant_id\":\"%s\",\"name\":\"%s\","
                                 "\"shop_name\":\"%s\",\"upi_id\":\"%s\",\"esp_token\":\"%s\","
                                 "\"message\":\"%s\"}",
                                 merchant_id ? merchant_id->valuestring : "",
                                 name_json ? name_json->valuestring : "",
                                 shop_name ? shop_name->valuestring : "",
                                 upi_id_json ? upi_id_json->valuestring : "",
                                 esp_token ? esp_token->valuestring : "",
                                 message ? message->valuestring : "");
                    }
                }
                else
                {
                    cJSON *message = cJSON_GetObjectItem(root, "message");
                    const char *error_msg = message ? message->valuestring : "Registration failed";
                    ESP_LOGE("API", "API error: %s", error_msg);

                    formatted_response = (char *)malloc(256);
                    if (formatted_response)
                    {
                        snprintf(formatted_response, 256, "{\"success\":false,\"message\":\"%s\"}", error_msg);
                    }
                }
                cJSON_Delete(root);
            }
            else
            {
                ESP_LOGE("API", "Failed to parse JSON response");
                formatted_response = strdup("{\"success\":false,\"message\":\"Invalid JSON response\"}");
            }

            free(api_response);
            api_response = NULL;
        }
        else
        {
            ESP_LOGE("API", "HTTP Error: %d", status_code);
            formatted_response = strdup("{\"success\":false,\"message\":\"HTTP request failed\"}");
        }
    }
    else
    {
        ESP_LOGE("API", "HTTP request failed: %s", esp_err_to_name(err));
        formatted_response = strdup("{\"success\":false,\"message\":\"Network error\"}");
    }

    esp_http_client_cleanup(client);
    free(post_data);

    if (!formatted_response)
    {
        formatted_response = strdup("{\"success\":false,\"message\":\"API call failed\"}");
    }

    return formatted_response;
}

static void convert_to_http_url(const char *https_url, char *http_url, size_t size)
{
    if (strstr(https_url, "https://") == https_url)
    {
        snprintf(http_url, size, "http://%s", https_url + 8);
    }
    else
    {
        strncpy(http_url, https_url, size - 1);
        http_url[size - 1] = '\0';
    }
}

void qr_api_task(void *pvParameters) {
    qr_task_data_t data;
    
    while (1) {
        if (xQueueReceive(qr_task_queue, &data, portMAX_DELAY)) {
            ESP_LOGI("QR_TASK", "Processing token: %s", data.token);
            
            reset_png_decoder_state();
            
            if (strcmp(cached_token, data.token) == 0 && cached_qr_buffer && cached_qr_size > 0) {
                ESP_LOGI("QR_TASK", "Using CACHED QR code - Instant display!");
 
                tft_fill_screen(0xFFFF);
                vTaskDelay(pdMS_TO_TICKS(100));
                
                pngle_t *pngle = pngle_new();
                if (pngle) {
                    pngle_set_draw_callback(pngle, on_draw);

                    int consumed = pngle_feed(pngle, cached_qr_buffer, cached_qr_size);
                    
                    if (consumed != cached_qr_size) {
                        ESP_LOGW("QR_TASK", "PNG decode incomplete: consumed %d of %d bytes", 
                                 consumed, cached_qr_size);
                
                        free(cached_qr_buffer);
                        cached_qr_buffer = NULL;
                        cached_qr_size = 0;
                        cached_token[0] = '\0';
                        pngle_destroy(pngle);
                        continue;
                    }
                    pngle_destroy(pngle);
                }
                
                tft_fill_rect(0, 215, TFT_WIDTH, 30, COLOR_WHITE);
                tft_fill_rect(0, 215, TFT_WIDTH, 1, COLOR_BLACK);
                
                vTaskDelay(pdMS_TO_TICKS(500));
                static char current_monitoring_token[64] = {0};
                static TaskHandle_t monitor_task_handle = NULL;
                
                // Check if monitoring task is still running
                if (monitor_task_handle != NULL) {
                    // Task exists, check if it's still alive
                    if (eTaskGetState(monitor_task_handle) == eDeleted) {
                        monitor_task_handle = NULL;
                        current_monitoring_token[0] = '\0';
                    }
                }
                
                if (strcmp(current_monitoring_token, data.token) != 0 || monitor_task_handle == NULL) {
                    strcpy(current_monitoring_token, data.token);
                    start_payment_monitoring_with_handle(data.token, &monitor_task_handle);
                }
                continue; 
            }
            
            tft_fill_screen(0x0000);
            
            char *qr_response = call_qr_api(data.token);
            if (qr_response) {
                cJSON *root = cJSON_Parse(qr_response);
                if (root) {
                    cJSON *qr_url_json = cJSON_GetObjectItem(root, "qr_url");
                    
                    if (qr_url_json && qr_url_json->valuestring) {
                        char http_url[256];
                        convert_to_http_url(qr_url_json->valuestring, http_url, sizeof(http_url));
                        
                        image_total_len = 0;
                        if (qr_image_buffer) { 
                            free(qr_image_buffer); 
                            qr_image_buffer = NULL; 
                        }
                        
                        esp_http_client_config_t img_config = {
                            .url = http_url,
                            .method = HTTP_METHOD_GET,
                            .event_handler = image_http_event_handler,
                            .timeout_ms = 5000,
                        };
                        esp_http_client_handle_t img_client = esp_http_client_init(&img_config);
                        
                        if (img_client) {
                            esp_err_t err = esp_http_client_perform(img_client);
                        
                            if (err == ESP_OK && qr_image_buffer && qr_image_buffer[0] == 0x89) {
                                ESP_LOGI("QR_TASK", "Download complete. Refreshing screen...");
                
                                tft_fill_screen(0xFFFF);
                                vTaskDelay(pdMS_TO_TICKS(200));
                                
                                if (cached_qr_buffer) {
                                    free(cached_qr_buffer);
                                    cached_qr_buffer = NULL;
                                }
                                
                                cached_qr_size = image_total_len;
                                cached_qr_buffer = (uint8_t*)malloc(cached_qr_size);
                                if (cached_qr_buffer) {
                                    memcpy(cached_qr_buffer, qr_image_buffer, cached_qr_size);
                                    strncpy(cached_token, data.token, sizeof(cached_token) - 1);
                                    cached_token[sizeof(cached_token) - 1] = '\0';
                                    ESP_LOGI("QR_TASK", "QR code CACHED for future use (size: %d bytes)", cached_qr_size);
                                } else {
                                    ESP_LOGE("QR_TASK", "Failed to cache QR code - out of memory");
                                }
                                
                                ESP_LOGI("QR_TASK", "Starting PNG Decode...");
                                pngle_t *pngle = pngle_new();
                                if (pngle) {
                                    pngle_set_draw_callback(pngle, on_draw);
                                    int consumed = pngle_feed(pngle, qr_image_buffer, image_total_len);
                                    
                                    if (consumed != image_total_len) {
                                        ESP_LOGW("QR_TASK", "PNG decode partial: %d/%d bytes", consumed, image_total_len);
                                    } else {
                                        ESP_LOGI("QR_TASK", "PNG decode completed successfully");
                                    }
                                    
                                    pngle_destroy(pngle);
                                } else {
                                    ESP_LOGE("QR_TASK", "Failed to create PNGLE context");
                                    tft_fill_screen(0xF800);
                                }
                                
                                tft_fill_rect(0, 215, TFT_WIDTH, 30, COLOR_WHITE);
                                tft_fill_rect(0, 215, TFT_WIDTH, 1, COLOR_BLACK);
                                
                                ESP_LOGI("QR_TASK", "Display finished. Starting monitor...");
                                vTaskDelay(pdMS_TO_TICKS(500));
                                
                                static char current_monitoring_token[64] = {0};
                                static TaskHandle_t monitor_task_handle = NULL;
                                
                                if (monitor_task_handle != NULL) {
                                    if (eTaskGetState(monitor_task_handle) == eDeleted) {
                                        monitor_task_handle = NULL;
                                        current_monitoring_token[0] = '\0';
                                    }
                                }
                                
                                if (strcmp(current_monitoring_token, data.token) != 0 || monitor_task_handle == NULL) {
                                    strcpy(current_monitoring_token, data.token);
                                    start_payment_monitoring_with_handle(data.token, &monitor_task_handle);
                                }
                            } else {
                                ESP_LOGE("QR_TASK", "Invalid PNG or download failed");
                                tft_fill_screen(0xF800);
                                tft_draw_text("QR Download Failed!", 50, 100, COLOR_WHITE, COLOR_RED, 2);
                                vTaskDelay(pdMS_TO_TICKS(3000));
                            }
                            esp_http_client_cleanup(img_client);
                        }
                    }
                    cJSON_Delete(root);
                } else {
                    ESP_LOGE("QR_TASK", "Failed to parse QR API response");
                    tft_fill_screen(0xF800);
                    tft_draw_text("API Error", 100, 120, COLOR_WHITE, COLOR_RED, 2);
                    vTaskDelay(pdMS_TO_TICKS(3000));
                }
                free(qr_response);
            } else {
                ESP_LOGE("QR_TASK", "QR API returned NULL response");
                tft_fill_screen(0xF800);
                tft_draw_text("Network Error", 90, 120, COLOR_WHITE, COLOR_RED, 2);
                vTaskDelay(pdMS_TO_TICKS(3000));
            }
            
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}