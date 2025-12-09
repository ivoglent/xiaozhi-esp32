//
// Created by ivoglent on 12/9/2025.
//

#pragma once

#include <string>
#include "esp_http_client.h"

class HttpProtocol {
public:
    static std::string httpGet(const std::string& url);
    static std::string httpPost(const std::string& url, const std::string& body,
                                const std::string& contentType = "application/json");

private:
    static esp_err_t _httpEventHandler(esp_http_client_event_t *evt);
};
