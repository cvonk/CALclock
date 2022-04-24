/**
 * @brief Read calender events from Google Apps script and forward them as messages
 *
 * © Copyright 2016, 2022, Sander and Coert Vonk
 * 
 * This file is part of CALclock.
 * 
 * CALclock is free software: you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 * 
 * CALclock is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along with CALclock. 
 * If not, see <https://www.gnu.org/licenses/>.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 **/

#include <string.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <nvs_flash.h>
#include <esp_netif.h>
#include <esp_tls.h>
#include <esp_http_client.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <lwip/dns.h>
#include <cJSON.h>

#include "https_client_task.h"
#include "../ipc/ipc.h"

static const char * TAG = "https_client_task";
static char * _data = NULL;
static int _data_len = 0;

void
sendToClient(toClientMsgType_t const dataType, char const * const data, ipc_t const * const ipc)
{
    toClientMsg_t msg = {
        .dataType = dataType,
        .data = strdup(data)
    };
    assert(msg.data);
    if (xQueueSendToBack(ipc->toClientQ, &msg, 0) != pdPASS) {
        ESP_LOGE(TAG, "toClientQ full");
        free(msg.data);
    }
}

esp_err_t
_http_event_handle(esp_http_client_event_t *evt)
{
    // gets called twice in a row because of the redirect that GAS uses

    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        // store data ptr and len here, because content_length returns -1
        _data = evt->data;
        _data_len = evt->data_len;
    }
    return ESP_OK;
}

static void
_json2pushId(char const * const serializedJson, char * const pushId, uint const pushId_len)
{
    *pushId = '\0';
    if (serializedJson[0] != '{' || serializedJson[strlen(serializedJson)-1] != '}') {
        ESP_LOGW(TAG, "first/last JSON chr ('%c' '%c'", serializedJson[0], serializedJson[strlen(serializedJson)-1]);
        return;
    }

    cJSON * const jsonRoot = cJSON_Parse(serializedJson);
    if (jsonRoot->type != cJSON_Object) {
        ESP_LOGE(TAG, "JSON root is not an Object");
        return;
    }
    cJSON const *const jsonPushId = cJSON_GetObjectItem(jsonRoot, "pushId");
    if (!jsonPushId || jsonPushId->type != cJSON_String) {
        ESP_LOGW(TAG, "JSON.pushId is missing or not an string");
        return;
    }
    strncpy(pushId, jsonPushId->valuestring, pushId_len);
    cJSON_Delete(jsonRoot);
}

void
https_client_task(void * ipc_void)
{
    ipc_t * const ipc = ipc_void;

    uint const pushId_len = 64;
    char * const pushId = malloc(pushId_len);
    assert(pushId);
    *pushId = '\0';

    while (1) {

        char * url;
        assert(asprintf(&url, "%s?devName=%s&pushId=%s", CONFIG_CALCLOCK_GAS_CALENDAR_URL, ipc->dev.name, pushId) >= 0);
        ESP_LOGI(TAG, "url = \"%s\"", url);

        esp_http_client_config_t config = {
            .url = url,
            .event_handler = _http_event_handle,
            .buffer_size = 2048, // big enough so "Location:" in the header doesn't get split over 2 chunks
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);

        if (esp_http_client_perform(client) == ESP_OK) {
            int const status = esp_http_client_get_status_code(client);
            // content_length returns -1 to indicate the data arrived chunked, instead use the data_len that we stored
            ESP_LOGI(TAG, "status = %d, _data_len = %d", status, _data_len);
            if (status == 200) {
                _data[_data_len] = '\0';
                ESP_LOGI(TAG, "rx \"%s\"", _data);
                sendToDisplay(TO_DISPLAY_MSGTYPE_JSON, _data, ipc);
                _json2pushId(_data, pushId, pushId_len);
            }
        }
        free(url);
        esp_http_client_cleanup(client);

        bool const pushActive = strlen(pushId);
        uint const pushServiceDuration = 60;  // max push notification service duration is 1 hr
        int const minPerPxl = 12 * 60 / CONFIG_CALCLOCK_WS2812_COUNT;

        uint const waitMinutes = pushActive ? pushServiceDuration : minPerPxl;
        toClientMsg_t msg;
        if (xQueueReceive(ipc->toClientQ, &msg, waitMinutes * 60000L / portTICK_PERIOD_MS) == pdPASS) {
            free(msg.data);
        }
    }
}
