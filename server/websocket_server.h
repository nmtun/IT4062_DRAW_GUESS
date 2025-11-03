#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include <libwebsockets.h>

/**
 * Khởi chạy WebSocket server
 * @param port Cổng server sẽ lắng nghe
 * @return Trả về 0 khi thành công, <0 khi lỗi
 */
int start_websocket_server(int port);

#endif // WEBSOCKET_SERVER_H
