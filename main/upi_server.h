// HTML page for user input
static const char* index_html = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<title>UPI Payment Registration</title>"
"<meta name='viewport' content='width=device-width, initial-scale=1'>"
"<style>"
"body { font-family: Arial; margin: 0; padding: 20px; background: #f0f0f0; }"
".container { max-width: 500px; margin: auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
"h2 { text-align: center; color: #333; }"
"label { display: block; margin-top: 10px; font-weight: bold; }"
"input { width: 100%; padding: 10px; margin-top: 5px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }"
"button { width: 100%; padding: 12px; margin-top: 20px; background: #4CAF50; color: white; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }"
"button:hover { background: #45a049; }"
".result { margin-top: 20px; padding: 15px; background: #e7f3fe; border-left: 4px solid #2196F3; border-radius: 4px; display: none; }"
".error { background: #ffe7e7; border-left-color: #f44336; }"
".token { font-family: monospace; background: #fff; padding: 10px; border-radius: 4px; word-break: break-all; }"
"</style>"
"</head>"
"<body>"
"<div class='container'>"
"<h2>UPI Payment Registration</h2>"
"<form id='registrationForm'>"
"<label>Name:</label>"
"<input type='text' id='name' placeholder='Enter your name' required>"
"<label>UPI ID:</label>"
"<input type='text' id='upi_id' placeholder='example@okaxis' required>"
"<button type='submit'>Generate Token</button>"
"</form>"
"<div id='result' class='result'></div>"
"</div>"

"<script>"
"document.getElementById('registrationForm').onsubmit = async function(e) {"
"    e.preventDefault();"
"    const name = document.getElementById('name').value;"
"    const upi_id = document.getElementById('upi_id').value;"
"    const resultDiv = document.getElementById('result');"
"    "
"    resultDiv.style.display = 'block';"
"    resultDiv.innerHTML = 'Processing...';"
"    resultDiv.className = 'result';"
"    "
"    try {"
"        const response = await fetch('/register', {"
"            method: 'POST',"
"            headers: {'Content-Type': 'application/x-www-form-urlencoded'}," 
"            body: 'name=' + encodeURIComponent(name) + '&upi_id=' + encodeURIComponent(upi_id)"
"        });"
"        "
"        const data = await response.json();"
"        "
"        if (data.success) {"
"            resultDiv.innerHTML = '<strong>Success!</strong><br><br>' +"
"                '<strong>Merchant ID:</strong> ' + data.merchant_id + '<br>' +"
"                '<strong>Name:</strong> ' + data.name + '<br>' +"
"                '<strong>UPI ID:</strong> ' + data.upi_id + '<br>' +"
"                '<strong>ESP Token:</strong><br>' +"
"                '<div class=\"token\">' + data.esp_token + '</div><br>' +"
"                '<em>' + data.message + '</em>';"
"            resultDiv.style.background = '#e7f3fe';"
"        } else {"
"            resultDiv.innerHTML = '<strong>Error!</strong><br>' + data.message;"
"            resultDiv.className = 'result error';"
"        }"
"    } catch(error) {"
"        resultDiv.innerHTML = '<strong>Error!</strong><br>' + error.message;"
"        resultDiv.className = 'result error';"
"    }"
"};"
"</script>"
"</body>"
"</html>";

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html, strlen(index_html));
    return ESP_OK;
}

static esp_err_t register_post_handler(httpd_req_t *req)
{
    char content[512] = {0};
    char name[100] = {0};
    char upi_id[100] = {0};
    int ret;
    
    ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
        return ESP_FAIL;
    }
    content[ret] = '\0';
    
    // Parse form data (basic extraction)
    char *name_ptr = strstr(content, "name=");
    char *upi_ptr = strstr(content, "upi_id=");
    
    if (name_ptr && upi_ptr) {
        name_ptr += 5;
        upi_ptr += 7;
        
        char *end = strchr(name_ptr, '&');
        if (end) {
            int len = end - name_ptr;
            strncpy(name, name_ptr, len);
            name[len] = '\0';
        }
        
        char *end2 = strchr(upi_ptr, '&');
        if (!end2) end2 = upi_ptr + strlen(upi_ptr);
        int len = end2 - upi_ptr;
        strncpy(upi_id, upi_ptr, len);
        upi_id[len] = '\0';
    }
    
    char *response = call_upi_api(name, upi_id);
    
    if (response) {
        cJSON *root = cJSON_Parse(response);
        if (root) {
            cJSON *success = cJSON_GetObjectItem(root, "success");
            if (success && (cJSON_IsTrue(success) || (cJSON_IsBool(success) && success->valueint))) {
                
                cJSON *token_json = cJSON_GetObjectItem(root, "esp_token");
                cJSON *merchant_id_json = cJSON_GetObjectItem(root, "merchant_id");
                
                if (cJSON_IsString(token_json) && token_json->valuestring) {
                    
                    const char *new_token = token_json->valuestring;
                    const char *new_mid = merchant_id_json ? merchant_id_json->valuestring : "";
                    ESP_LOGI(TAG, "Clearing old data and saving new merchant details...");
                    save_merchant_data_to_nvs(new_token, new_mid, name, upi_id);
                    if (qr_task_queue) {
                        qr_task_data_t qr_data;
                        memset(&qr_data, 0, sizeof(qr_data));
                        
                        strncpy(qr_data.token, new_token, sizeof(qr_data.token) - 1);
                        strncpy(qr_data.merchant_id, new_mid, sizeof(qr_data.merchant_id) - 1);
                        strncpy(qr_data.merchant_name, name, sizeof(qr_data.merchant_name) - 1);
                        strncpy(qr_data.merchant_upi, upi_id, sizeof(qr_data.merchant_upi) - 1);
                        
                        xQueueSend(qr_task_queue, &qr_data, portMAX_DELAY);
                        ESP_LOGI(TAG, "QR Task Queued with new token.");
                    }
                }
            }
            cJSON_Delete(root);
        }
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, strlen(response));
        free(response);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "API call failed");
    }
    
    return ESP_OK;
}

void start_web_server(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    config.stack_size = 10240; 
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root_uri);
        
        httpd_uri_t register_uri = {
            .uri = "/register",
            .method = HTTP_POST,
            .handler = register_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &register_uri);
        
        ESP_LOGI(TAG, "Web server started with 10KB stack on port: %d", config.server_port);
    } else {
        ESP_LOGE(TAG, "Failed to start web server");
    }
}