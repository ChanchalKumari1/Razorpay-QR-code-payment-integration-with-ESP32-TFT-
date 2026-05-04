
void reset_png_decoder_state(void) {
    png_last_y = 0xFFFF;
    png_start_x = 0;
    png_count = 0;
    png_decode_active = false;

    memset(png_line_buffer, 0, sizeof(png_line_buffer));
    ESP_LOGI("PNG", "Decoder state reset");
}

void on_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t rgba[4]) {
    static uint32_t last_y = 0xFFFFFFFF;
    static uint32_t start_x = 0;
    static uint32_t count = 0;
    static uint16_t line_buffer[320]; // Static buffer, no malloc needed

    if (rgba == NULL) {
        if (count > 0 && last_y != 0xFFFFFFFF) {
            tft_draw_image(start_x, last_y, count, 1, line_buffer);
        }
        last_y = 0xFFFFFFFF;
        start_x = 0;
        count = 0;
        ESP_LOGD("PNG", "Decode completed");
        return;
    }

    if (rgba[3] > 200 && rgba[0] < 100 && rgba[1] < 100 && rgba[2] < 100) {
        
        uint32_t target_size = 200; 
        uint32_t offset_x = (320 - target_size) / 2;
        uint32_t offset_y = (240 - target_size) / 2;

        uint32_t img_w = pngle_get_width(pngle);
        uint32_t img_h = pngle_get_height(pngle);
        
        // Calculate scaled coordinates
        uint32_t draw_x = offset_x + ((x * target_size) / img_w);
        uint32_t draw_y = offset_y + ((y * target_size) / img_h);
    
        if (draw_x >= 320 || draw_y >= 240) {
            return;
        }
        if (draw_y != last_y || draw_x != (start_x + count)) {
            // Flush previous line
            if (count > 0 && last_y != 0xFFFFFFFF) {
                tft_draw_image(start_x, last_y, count, 1, line_buffer);
            }
     
            start_x = draw_x;
            last_y = draw_y;
            count = 1;
            line_buffer[0] = 0x0000;
        } else {
            if (count < 320) {
                line_buffer[count] = 0x0000;
                count++;
            }
            
            if (count >= 320) {
                tft_draw_image(start_x, last_y, 320, 1, line_buffer);
                start_x += 320;
                count = 0;
            }
        }
    }

    if (x == 0 && (y % 10 == 0)) {
        taskYIELD();
    }
}

char *call_payment_status_api(const char *token)
{
    if (!token || strlen(token) == 0)
    {
        ESP_LOGE("PAYMENT", "No token provided");
        return NULL;
    }

    if (api_response)
    {
        free(api_response);
        api_response = NULL;
    }

    char status_url[256];
    snprintf(status_url, sizeof(status_url), "%s?token=%s", PAYMENT_STATUS_URL, token);

    ESP_LOGI("PAYMENT", "Checking payment status: %s", status_url);

    esp_http_client_config_t config = {
        .url = status_url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 15000,
        .event_handler = http_event_handler,
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
        ESP_LOGI("PAYMENT", "HTTP Status = %d", status_code);

        if (status_code == 200 && api_response)
        {
            ESP_LOGI("PAYMENT", "Raw Response: %s", api_response);
            result = strdup(api_response);
            free(api_response);
            api_response = NULL;
        }
        else
        {
            ESP_LOGE("PAYMENT", "HTTP Error: %d", status_code);
        }
    }
    else
    {
        ESP_LOGE("PAYMENT", "Request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);

    if (!result)
    {
        result = strdup("{\"status\":\"error\",\"message\":\"API call failed\"}");
    }
    return result;
}

static payment_status_t *parse_payment_response(const char *json_response)
{
    if (!json_response)
        return NULL;

    payment_status_t *payment = (payment_status_t *)calloc(1, sizeof(payment_status_t));
    if (!payment)
        return NULL;

    ESP_LOGI("PAYMENT", "Parsing response: %s", json_response);

    cJSON *root = cJSON_Parse(json_response);
    if (!root)
    {
        ESP_LOGE("PAYMENT", "Failed to parse JSON: %s", json_response);
        strcpy(payment->status, "error");
        strcpy(payment->error_message, "Invalid JSON response");
        return payment;
    }

    cJSON *status = cJSON_GetObjectItem(root, "status");
    if (!status)
    {
        status = cJSON_GetObjectItem(root, "payment_status");
    }

    if (status && status->valuestring)
    {
        strncpy(payment->status, status->valuestring, sizeof(payment->status) - 1);
        ESP_LOGI("PAYMENT", "Payment status: %s", payment->status);

        if (strcmp(payment->status, "success") == 0)
        {
            cJSON *amount = cJSON_GetObjectItem(root, "amount");
            cJSON *payer_name = cJSON_GetObjectItem(root, "payer_name");
            cJSON *payer_upi = cJSON_GetObjectItem(root, "payer_upi");
            cJSON *paid_at = cJSON_GetObjectItem(root, "paid_at");

            if (!amount)
                amount = cJSON_GetObjectItem(root, "transaction_amount");
            if (!payer_name)
                payer_name = cJSON_GetObjectItem(root, "name");
            if (!payer_upi)
                payer_upi = cJSON_GetObjectItem(root, "upi_id");

            if (amount && amount->valuestring)
                strncpy(payment->amount, amount->valuestring, sizeof(payment->amount) - 1);
            else if (amount && cJSON_IsNumber(amount))
                snprintf(payment->amount, sizeof(payment->amount), "%.2f", amount->valuedouble);

            if (payer_name && payer_name->valuestring)
                strncpy(payment->payer_name, payer_name->valuestring, sizeof(payment->payer_name) - 1);
            if (payer_upi && payer_upi->valuestring)
                strncpy(payment->payer_upi, payer_upi->valuestring, sizeof(payment->payer_upi) - 1);
            if (paid_at && paid_at->valuestring)
                strncpy(payment->paid_at, paid_at->valuestring, sizeof(payment->paid_at) - 1);

            ESP_LOGI("PAYMENT", "Payment successful! Amount: ₹%s, Payer: %s",
                     payment->amount, payment->payer_name);
        }
        else if (strcmp(payment->status, "pending") == 0)
        {
            ESP_LOGI("PAYMENT", "Payment still pending...");
            cJSON *message = cJSON_GetObjectItem(root, "message");
            if (message && message->valuestring)
                strncpy(payment->error_message, message->valuestring, sizeof(payment->error_message) - 1);
            else
                strcpy(payment->error_message, "Waiting for payment...");
        }
        else if (strcmp(payment->status, "failed") == 0)
        {
            cJSON *message = cJSON_GetObjectItem(root, "message");
            if (message && message->valuestring)
                strncpy(payment->error_message, message->valuestring, sizeof(payment->error_message) - 1);
            else
                strcpy(payment->error_message, "Payment failed");
            ESP_LOGE("PAYMENT", "Payment failed: %s", payment->error_message);
        }
    }
    else
    {
        cJSON *error = cJSON_GetObjectItem(root, "error");
        if (error && error->valuestring)
        {
            strcpy(payment->status, "error");
            strncpy(payment->error_message, error->valuestring, sizeof(payment->error_message) - 1);
        }
        else
        {
            cJSON *message = cJSON_GetObjectItem(root, "message");
            if (message && message->valuestring)
            {
                strcpy(payment->status, "pending");
                strncpy(payment->error_message, message->valuestring, sizeof(payment->error_message) - 1);
            }
            else
            {
                strcpy(payment->status, "error");
                strcpy(payment->error_message, "Unknown response format");
            }
        }
    }

    cJSON_Delete(root);
    return payment;
}

void check_payment_status_task(void *pvParameters)
{
    char *token = (char *)pvParameters;
    bool payment_successful = false;
    uint8_t check_count = 0;
    const uint8_t max_checks = 20;
    bool first_check = true;

    if (!token || strlen(token) == 0)
    {
        ESP_LOGE("PAYMENT", "No token provided");
        free(token);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI("PAYMENT", "Starting payment monitoring - Checking status every 3 seconds");

    while (!payment_successful && check_count < max_checks)
    {
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        check_count++;

        ESP_LOGI("PAYMENT", "Checking payment status (Attempt %d/%d)...", check_count, max_checks);

        char *status_response = call_payment_status_api(token);
        if (status_response)
        {
            payment_status_t *payment = parse_payment_response(status_response);
            if (payment)
            {
                if (strcmp(payment->status, "success") == 0)
                {
                    ESP_LOGI("PAYMENT", "Payment successful! Amount: ₹%s", payment->amount);
                    
                    tft_fill_rect(20, 220, 280, 20, COLOR_WHITE);
                    
                    char success_text[128];
                    snprintf(success_text, sizeof(success_text), "STATUS: SUCCESS - ₹%s", payment->amount);
                    tft_draw_text(success_text, 20, 222, COLOR_GREEN, COLOR_WHITE, 1);
                    
                    if (strlen(payment->payer_name) > 0) {
                        char payer_text[128];
                        snprintf(payer_text, sizeof(payer_text), "From: %.50s", payment->payer_name); 
                        tft_draw_text(payer_text, 20, 235, COLOR_BLUE, COLOR_WHITE, 1);
                    }
                    
                    payment_successful = true;
                    free(payment);
                    free(status_response);
                    break;
                }
                else if (strcmp(payment->status, "pending") == 0)
                {
                    if (first_check) {
                        ESP_LOGI("PAYMENT", "Payment pending - showing status");
                        
                        tft_fill_rect(20, 220, 280, 20, COLOR_WHITE);
                        tft_draw_text("STATUS: PAYMENT PENDING", 20, 222, COLOR_ORANGE, COLOR_WHITE, 1);
                        
                        first_check = false;
                    } else {
                        ESP_LOGI("PAYMENT", "Payment still pending (Attempt %d/%d)", check_count, max_checks);
                    }
                    
                    free(payment);
                }
                else
                {
                    ESP_LOGE("PAYMENT", "Payment error: %s", payment->error_message);
                    
                    tft_fill_rect(20, 220, 280, 20, COLOR_WHITE);
                    tft_draw_text("STATUS: PAYMENT FAILED", 20, 222, COLOR_RED, COLOR_WHITE, 1);
                    
                    free(payment);
                    break;
                }
            }
            free(status_response);
        }
        else
        {
            if (first_check) {
                tft_fill_rect(20, 220, 280, 20, COLOR_WHITE);
                tft_draw_text("STATUS: NETWORK ERROR", 20, 222, COLOR_RED, COLOR_WHITE, 1);
                first_check = false;
            }
        }
    }

    if (!payment_successful && check_count >= max_checks)
    {
        tft_fill_rect(20, 220, 280, 20, COLOR_WHITE);
        tft_draw_text("STATUS: TIMEOUT", 20, 222, COLOR_RED, COLOR_WHITE, 1);
        ESP_LOGE("PAYMENT", "Payment monitoring timed out after %d checks", max_checks);
        
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI("PAYMENT", "Payment monitoring ended");
    free(token);
    vTaskDelete(NULL);
}

void start_payment_monitoring_with_handle(const char *token, TaskHandle_t *task_handle)
{
    char *task_token = strdup(token);
    if (task_token == NULL) return;

    BaseType_t ret = xTaskCreate(check_payment_status_task, "payment_monitor", 24576, (void*)task_token, 5, task_handle);
    if (ret == pdPASS) {
        ESP_LOGI("PAYMENT", "Monitoring task created for token: %s", task_token);
    } else {
        ESP_LOGE("PAYMENT", "Failed to create monitoring task");
        free(task_token);
    }
}

void start_payment_monitoring(const char *token)
{
    start_payment_monitoring_with_handle(token, NULL);
}