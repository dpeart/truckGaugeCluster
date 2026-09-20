#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "ota_handler.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_log.h"

static const char *TAG = "OTA";

/* Simple HTML upload form */
static const char* upload_html = 
    "<html><body>"
    "<h1>Truck Gauge Update</h1>"
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
    "  xhr.onload = function() {"
    "    if(xhr.status === 200) {"
    "      document.getElementById('prg').innerHTML = 'Update Complete! Rebooting in 5 seconds...';"
    "      setTimeout(function() { location.reload(); }, 5000);"
    "    } else { document.getElementById('prg').innerHTML = 'Error: ' + xhr.responseText; }"
    "  };"
    "  xhr.upload.onprogress = function(e) {"
    "    var p = Math.floor((e.loaded / e.total) * 100);"
    "    document.getElementById('prg').innerHTML = 'Uploading: ' + p + '%';"
    "  };"
    "  xhr.send(file);"
    "}"
    "function factoryReset() {"
    "  if(confirm('Are you sure you want to clear all WiFi settings and reboot?')) {"
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

static esp_err_t ota_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, upload_html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t ota_post_handler(httpd_req_t *req)
{
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Passive OTA partition not found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Partition not found");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Update partition: %s @ 0x%lx, len=%d",
             update_partition->label,
             (unsigned long)update_partition->address,
             req->content_len);

    esp_err_t err = esp_ota_begin(update_partition, req->content_len, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA Begin failed");
        return ESP_FAIL;
    }

    char buf[1024];
    int remaining = req->content_len;

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));
        if (recv_len <= 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                ESP_LOGW(TAG, "OTA recv timeout, retrying...");
                continue;
            }
            ESP_LOGE(TAG, "OTA receive failed (%d)", recv_len);
            return ESP_FAIL;
        }

        err = esp_ota_write(ota_handle, buf, recv_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
            return ESP_FAIL;
        }
        remaining -= recv_len;
    }

    ESP_LOGI(TAG, "All data received, calling esp_ota_end");
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA End failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Calling esp_ota_set_boot_partition(%s)", update_partition->label);
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA update complete, rebooting...");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_sendstr(req, "OK");

    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
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
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &index_uri);

    httpd_uri_t ota_get_uri = {
        .uri = "/update",
        .method = HTTP_GET,
        .handler = ota_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ota_get_uri);

    httpd_uri_t ota_uri = {
        .uri = "/update",
        .method = HTTP_POST,
        .handler = ota_post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ota_uri);

    httpd_uri_t reset_uri = {
        .uri = "/reset",
        .method = HTTP_POST,
        .handler = reset_handler,
        .user_ctx = NULL
    };
    return httpd_register_uri_handler(server, &reset_uri);
}
