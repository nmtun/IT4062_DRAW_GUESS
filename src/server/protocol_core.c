#include "../include/protocol.h"
#include "../common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

/**
 * Parse message từ buffer nhận được
 */
int protocol_parse_message(const uint8_t* buffer, size_t buffer_len, message_t* msg_out) {
    if (!buffer || !msg_out || buffer_len < 3) {
        return -1;
    }

    // Đọc type (1 byte)
    msg_out->type = buffer[0];

    // Đọc length (2 bytes, network byte order)
    uint16_t length_network;
    memcpy(&length_network, buffer + 1, 2);
    msg_out->length = ntohs(length_network);

    // Kiểm tra độ dài hợp lệ
    if (msg_out->length > buffer_len - 3) {
        fprintf(stderr, "Lỗi: Payload length (%d) vượt quá buffer còn lại (%zu)\n", 
                msg_out->length, buffer_len - 3);
        return -1;
    }

    // Cấp phát bộ nhớ cho payload
    if (msg_out->length > 0) {
        msg_out->payload = (uint8_t*)malloc(msg_out->length);
        if (!msg_out->payload) {
            fprintf(stderr, "Lỗi: Không thể cấp phát bộ nhớ cho payload\n");
            return -1;
        }
        memcpy(msg_out->payload, buffer + 3, msg_out->length);
    } else {
        msg_out->payload = NULL;
    }

    return 0;
}

/**
 * Tạo message theo format protocol
 */
int protocol_create_message(uint8_t type, const uint8_t* payload, uint16_t payload_len, uint8_t* buffer_out) {
    if (!buffer_out) {
        return -1;
    }

    // Type (1 byte)
    buffer_out[0] = type;

    // Length (2 bytes, network byte order)
    uint16_t length_network = htons(payload_len);
    memcpy(buffer_out + 1, &length_network, 2);

    // Payload
    if (payload && payload_len > 0) {
        memcpy(buffer_out + 3, payload, payload_len);
    }

    return 3 + payload_len;
}

/**
 * Gửi message đến client
 */
int protocol_send_message(int client_fd, uint8_t type, const uint8_t* payload, uint16_t payload_len) {
    uint8_t buffer[BUFFER_SIZE];
    
    if (3 + payload_len > BUFFER_SIZE) {
        fprintf(stderr, "Lỗi: Message quá lớn (%d bytes)\n", 3 + payload_len);
        return -1;
    }

    int msg_len = protocol_create_message(type, payload, payload_len, buffer);
    if (msg_len < 0) {
        return -1;
    }

    ssize_t sent = send(client_fd, buffer, msg_len, 0);
    if (sent < 0) {
        perror("send() failed");
        return -1;
    }

    if (sent != msg_len) {
        fprintf(stderr, "Cảnh báo: Chỉ gửi được %zd/%d bytes\n", sent, msg_len);
        return -1;
    }

    return 0;
}
