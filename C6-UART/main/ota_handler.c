#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "ota_handler.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "wifi_ota.h"
#include "c6_uart.h"
#include "c6_modes.h"

static const char *TAG = "OTA";
extern volatile uint8_t last_received_cmd;

// Prototype for the cross-module ACK polling handler
bool wait_for_ack(uint32_t timeout_ms);

/* Simple HTML upload form */
static const char *upload_html =
    "<html><body>"
    "<h1>Truck Gauge Update</h1>"
    "<div style='padding:10px; border:2px solid #444; display:inline-block; margin-bottom:20px;'>"
    "<b>OTA Mode:</b> %s"
    "</div><br><br>"
    "<input type=\"file\" id=\"file_input\" accept=\".bin\">"
    "<button onclick=\"upload()\">Upload Firmware</button><br><br>"
    "<hr>"
    "<h3>System Actions</h3>"
    "<button onclick=\"factoryReset()\" style=\"background-color:#ff4444; color:white;\">Factory Reset (Clear WiFi)</button>"
    "<div id=\"prg\" style=\"margin-top:20px;\"></div>"
    "<script>"
    "function upload() {"
    "  var file = document.getElementById('file_input').files[0];"
    "  if(!file) { alert('Select a .bin file first!'); return; }"
    "  var xhr = new XMLHttpRequest();"
    "  xhr.open('POST', '/update', true);"
    "  xhr.setRequestHeader('Content-Type', 'application/octet-stream');"
    "  xhr.onload = function() {"
    "    if(xhr.status === 200) {"
    "      document.getElementById('prg').innerHTML = 'Update Complete!';"
    "    } else { document.getElementById('prg').innerHTML = 'Error: ' + xhr.responseText; }"
    "  };"
    "  xhr.upload.onprogress = function(e) {"
    "    var p = Math.floor((e.loaded / e.total) * 100);"
    "    document.getElementById('prg').innerHTML = 'Uploading: ' + p + '%%';"
    "  };"
    "  xhr.send(file);"
    "}"
    "function factoryReset() {"
    "  if(confirm(\"Are you sure you want to clear all WiFi settings and reboot?\")) {"
    "    var xhr = new XMLHttpRequest();"
    "    xhr.open('POST', '/reset', true);"
    "    xhr.onload = function() {"
    "      document.getElementById('prg').innerHTML = xhr.responseText;"
    "      alert('Device is resetting. Connect to the Setup AP to re-provision.');"
    "    };"
    "    xhr.send();"
    "  }"
    "}"
    "</script>"
    "</body></html>";

static esp_err_t ota_get_handler(httpd_req_t *req)
{
    ota_target_t target = wifi_ota_get_target();
    const char *mode_str = (target == OTA_TARGET_C6)
                               ? "C6 Firmware Update"
                               : "P4 Firmware Update";

    char page_buf[MAX_PAYLOAD];
    snprintf(page_buf, sizeof(page_buf), upload_html, mode_str);

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, page_buf, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t ota_post_handler(httpd_req_t *req)
{
    ota_target_t target = wifi_ota_get_target();
    ESP_LOGI(TAG, "OTA POST handler invoked (target=%s)",
             (target == OTA_TARGET_C6) ? "C6" : "P4");

    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *update_partition = NULL;

    ESP_LOGI(TAG, "HTTP content_len = %d", req->content_len);

    if (target == OTA_TARGET_C6)
    {
        update_partition = esp_ota_get_next_update_partition(NULL);
        if (!update_partition)
        {
            ESP_LOGE(TAG, "Passive OTA partition not found");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Partition not found");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Update partition: %s @ 0x%lx, len=%d",
                 update_partition->label,
                 (unsigned long)update_partition->address,
                 req->content_len);

        esp_err_t err = esp_ota_begin(update_partition, req->content_len, &ota_handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Target Begin failed");
            return ESP_FAIL;
        }
    }
    else
    {
        ESP_LOGI(TAG, "P4 OTA: streaming firmware to P4");

        // 1. Clear stale state cache before initiating handshake
        last_received_cmd = 0; 

        // 2. Notify P4 of the absolute total size to allow optimal dynamic storage prep
        uint32_t total_len = req->content_len;
        uart_send_frame(CMD_C6_UPLOAD_BEGIN, (uint8_t *)&total_len, sizeof(total_len));
        
        // 3. Wait for P4 to complete initialization and partition tracking tasks
        ESP_LOGI(TAG, "Waiting for P4 initializing OTA session...");
        if (!wait_for_ack(5000)) // 3 seconds is generous for initialization
        {
            ESP_LOGE(TAG, "P4 failed to initialize session");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "P4 Initialization Timeout");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "P4 initialized. Starting data stream.");
    }

    // Allocate buffer on the heap or rely on standard allocation bounds
    static char buf[MAX_PAYLOAD];
    int remaining = req->content_len;
    int total_streamed = 0;

    while (remaining > 0)
    {
        int to_read = MIN(remaining, MAX_PAYLOAD);
        int recv_len = httpd_req_recv(req, buf, to_read);

        if (recv_len <= 0)
        {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
            {
                ESP_LOGW(TAG, "OTA recv timeout, retrying...");
                continue;
            }
            ESP_LOGE(TAG, "OTA receive failed (%d)", recv_len);
            return ESP_FAIL;
        }

        if (target == OTA_TARGET_C6)
        {
            esp_err_t err = esp_ota_write(ota_handle, buf, recv_len);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
                return ESP_FAIL;
            }
        }
        else
        {
            // Reset command latch state before sending the data block
            last_received_cmd = 0;

            // STREAM TO P4 (C6 → P4 over UART in optimized 4KB chunks)
            uart_send_firmware_chunk((uint8_t *)buf, recv_len);
            
            total_streamed += recv_len;
        }

        remaining -= recv_len;
    }

    // -------------------------------
    // END OF OTA
    // -------------------------------
    if (target == OTA_TARGET_C6)
    {
        ESP_LOGI(TAG, "All data received, calling esp_ota_end");
        esp_err_t err = esp_ota_end(ota_handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA End failed");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Calling esp_ota_set_boot_partition(%s)", update_partition->label);
        err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)", esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "OTA update complete, rebooting...");
        uart_send_frame(CMD_C6_UPLOAD_END, NULL, 0);
        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_sendstr(req, "OK");

        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }
    else
    {
        // P4 OTA COMPLETE
        if (total_streamed != req->content_len)
        {
            ESP_LOGE(TAG, "P4 OTA ERROR: streamed %d of %d bytes — aborting",
                     total_streamed, req->content_len);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                "P4 OTA truncated");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "P4 OTA stream complete. Informing P4...");
        uart_send_frame(CMD_C6_UPLOAD_END, NULL, 0);        // bracket upload
        uart_send_frame(CMD_STREAM_FIRMWARE_DONE, NULL, 0); // trigger finalize

        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_sendstr(req, "OK");
    }

    return ESP_OK;
}

static esp_err_t reset_handler(httpd_req_t *req)
{
    ESP_LOGW(TAG, "Factory reset requested via web...");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_sendstr(req, "Provisioning cleared. Rebooting to Setup Mode...");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    nvs_flash_erase();
    esp_restart();
    return ESP_OK;
}

esp_err_t register_ota_handler(httpd_handle_t server)
{
    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = ota_get_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &index_uri);

    httpd_uri_t ota_get_uri = {
        .uri = "/update",
        .method = HTTP_GET,
        .handler = ota_get_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &ota_get_uri);

    httpd_uri_t ota_uri = {
        .uri = "/update",
        .method = HTTP_POST,
        .handler = ota_post_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &ota_uri);

    httpd_uri_t reset_uri = {
        .uri = "/reset",
        .method = HTTP_POST,
        .handler = reset_handler,
        .user_ctx = NULL};
    return httpd_register_uri_handler(server, &reset_uri);
}