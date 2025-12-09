//
// Created by ivoglent on 12/9/2025.
//

#pragma once
#include "cJSON.h"
#include "mcp_server.h"

inline PropertyList ParseToolParameters(cJSON* parameters) {
    PropertyList pList;

    cJSON* param = nullptr;
    cJSON_ArrayForEach(param, parameters) {
        std::string name = cJSON_GetObjectItem(param, "name")->valuestring;
        std::string type = cJSON_GetObjectItem(param, "type")->valuestring;

        bool required =
            cJSON_GetObjectItem(param, "required") &&
            cJSON_IsTrue(cJSON_GetObjectItem(param, "required"));

        if (type == "string") {
            if (required)
                pList.AddProperty(Property(name, kPropertyTypeString));
            else
                pList.AddProperty(Property(name, kPropertyTypeString, std::string("")));
        }
        else if (type == "integer") {
            if (required)
                pList.AddProperty(Property(name, kPropertyTypeInteger));
            else
                pList.AddProperty(Property(name, kPropertyTypeInteger, 0));
        }
        else if (type == "boolean") {
            if (required)
                pList.AddProperty(Property(name, kPropertyTypeBoolean));
            else
                pList.AddProperty(Property(name, kPropertyTypeBoolean, false));
        }
    }

    return pList;
}
