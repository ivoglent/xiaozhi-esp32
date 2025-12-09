//
// Created by ivoglent on 12/9/2025.
//

#include "http_protocol.h"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "HttpProtocol";

static std::string responseBuffer;

esp_err_t HttpProtocol::_httpEventHandler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (evt->data && evt->data_len > 0) {
            responseBuffer.append((char*)evt->data, evt->data_len);
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}


std::string HttpProtocol::httpGet(const std::string& url) {
    responseBuffer.clear();

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.method = HTTP_METHOD_GET;
    config.event_handler = _httpEventHandler;
    config.timeout_ms = 10000;  // 10 seconds

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return "";
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP GET Error: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return "";
    }

    esp_http_client_cleanup(client);
    return responseBuffer;
}


std::string HttpProtocol::httpPost(const std::string& url, const std::string& body,
                                   const std::string& contentType) {
    responseBuffer.clear();

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.method = HTTP_METHOD_POST;
    config.event_handler = _httpEventHandler;
    config.timeout_ms = 10000;  // 10 seconds

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return "";
    }

    esp_http_client_set_header(client, "Content-Type", contentType.c_str());
    esp_http_client_set_post_field(client, body.c_str(), body.length());

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP POST Error: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return "";
    }

    esp_http_client_cleanup(client);
    return responseBuffer;
}
