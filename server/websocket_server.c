#include "websocket_server.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

// Callback xử lý tin nhắn từ client
static int callback_drawguess(struct lws *wsi, enum lws_callback_reasons reason,
                              void *user, void *in, size_t len)
{
    switch (reason)
    {
    case LWS_CALLBACK_RECEIVE:
    {
        char *msg = (char *)in;
        printf("Nhận từ client: %s\n", msg);

        cJSON *json = cJSON_Parse(msg);
        if (!json)
            break;

        const cJSON *typeObj = cJSON_GetObjectItem(json, "type");
        if (!typeObj)
        {
            cJSON_Delete(json);
            break;
        }

        const char *type = typeObj->valuestring;

        // Xử lý login
        if (strcmp(type, "login") == 0)
        {
            const char *username = cJSON_GetObjectItem(json, "username")->valuestring;
            const char *password = cJSON_GetObjectItem(json, "password")->valuestring;

            int user_id = db_login_user(username, password);

            cJSON *response = cJSON_CreateObject();
            cJSON_AddStringToObject(response, "type", "login_result");
            cJSON_AddBoolToObject(response, "success", user_id > 0);
            cJSON_AddNumberToObject(response, "userId", user_id);

            char *resp_str = cJSON_PrintUnformatted(response);
            unsigned char buf[LWS_PRE + 512];
            size_t n = strlen(resp_str);
            memcpy(&buf[LWS_PRE], resp_str, n);
            lws_write(wsi, &buf[LWS_PRE], n, LWS_WRITE_TEXT);

            cJSON_Delete(response);
            free(resp_str);
        }

        cJSON_Delete(json);
        break;
    }
    default:
        break;
    }

    return 0;
}

// Hàm khởi chạy WebSocket server
void start_websocket_server(int port)
{
    struct lws_protocols protocols[] = {
        {"drawguess-protocol", callback_drawguess, 0, 4096},
        {NULL, NULL, 0, 0}};

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = port;
    info.protocols = protocols;

    struct lws_context *context = lws_create_context(&info);
    if (!context)
    {
        printf("❌ Không tạo được WebSocket context.\n");
        return;
    }

    printf("✅ WebSocket server đang chạy tại ws://localhost:%d\n", port);

    while (1)
        lws_service(context, 100);

    lws_context_destroy(context);
}
