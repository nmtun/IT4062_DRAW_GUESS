#include "../include/protocol.h"
#include "../common/protocol.h"
#include <stdio.h>

// Forward declarations cho các handlers từ các module khác
extern int protocol_handle_login(server_t* server, int client_index, const message_t* msg);
extern int protocol_handle_register(server_t* server, int client_index, const message_t* msg);
extern int protocol_handle_room_list_request(server_t* server, int client_index, const message_t* msg);
extern int protocol_handle_create_room(server_t* server, int client_index, const message_t* msg);
extern int protocol_handle_join_room(server_t* server, int client_index, const message_t* msg);
extern int protocol_handle_leave_room(server_t* server, int client_index, const message_t* msg);

/**
 * Xử lý message nhận được từ client
 */
int protocol_handle_message(server_t* server, int client_index, const message_t* msg) {
    if (!server || !msg) {
        return -1;
    }

    switch (msg->type) {
        case MSG_LOGIN_REQUEST:
            return protocol_handle_login(server, client_index, msg);
            
        case MSG_REGISTER_REQUEST:
            return protocol_handle_register(server, client_index, msg);
            
        case MSG_LOGOUT:
            // TODO: Implement logout handler
            printf("Nhận LOGOUT từ client %d\n", client_index);
            return 0;

        case MSG_ROOM_LIST_REQUEST:
            return protocol_handle_room_list_request(server, client_index, msg);

        case MSG_CREATE_ROOM:
            return protocol_handle_create_room(server, client_index, msg);

        case MSG_JOIN_ROOM:
            return protocol_handle_join_room(server, client_index, msg);

        case MSG_LEAVE_ROOM:
            return protocol_handle_leave_room(server, client_index, msg);
            
        default:
            fprintf(stderr, "Unknown message type: 0x%02X từ client %d\n", 
                    msg->type, client_index);
            return -1;
    }
}
