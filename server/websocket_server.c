#include "websocket_server.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#include <libwebsockets.h>

// Callback xử lý tin nhắn từ client
static int
callback_drawguess(struct lws *wsi, enum lws_callback_reasons reason,
                   void *user, void *in, size_t len)
{
    (void)user;
    (void)len;
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
            const cJSON *u = cJSON_GetObjectItem(json, "username");
            const cJSON *p = cJSON_GetObjectItem(json, "password");
            const char *username = u ? u->valuestring : NULL;
            const char *password = p ? p->valuestring : NULL;

            int user_id = db_login_user(username, password);

            cJSON *response = cJSON_CreateObject();
            cJSON_AddStringToObject(response, "type", "login_result");
            cJSON_AddBoolToObject(response, "success", user_id > 0);
            cJSON_AddNumberToObject(response, "userId", user_id);

            char *resp_str = cJSON_PrintUnformatted(response);
            if (resp_str)
            {
                size_t n = strlen(resp_str);
                unsigned char *buf = malloc(LWS_PRE + n);
                if (buf)
                {
                    memcpy(&buf[LWS_PRE], resp_str, n);
                    lws_write(wsi, &buf[LWS_PRE], (int)n, LWS_WRITE_TEXT);
                    free(buf);
                }
                free(resp_str);
            }

            cJSON_Delete(response);
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
int start_websocket_server(int port)
{
    struct lws_protocols protocols[] = {
        { .name = "drawguess-protocol",
          .callback = callback_drawguess,
          .per_session_data_size = 0,
          .rx_buffer_size = 4096,
          .id = 0 },
        { .name = NULL, .callback = NULL, .per_session_data_size = 0, .rx_buffer_size = 0, .id = 0 }
    };

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = port;
    info.protocols = protocols;

    struct lws_context *context = lws_create_context(&info);
    if (!context)
    {
        printf("❌ Không tạo được WebSocket context.\n");
        return -1;
    }

    printf("✅ WebSocket server đang chạy tại ws://localhost:%d\n", port);

    while (1)
        lws_service(context, 100);

    lws_context_destroy(context);
    return 0;
}
