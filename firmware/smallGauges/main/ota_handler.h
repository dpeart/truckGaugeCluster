#pragma once
#include "esp_http_server.h"

esp_err_t register_ota_handler(httpd_handle_t server);
