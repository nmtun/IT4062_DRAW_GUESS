#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include <libwebsockets.h>

/**
 * Khởi chạy WebSocket server
 * @param port Cổng server sẽ lắng nghe
 */
void start_websocket_server(int port);

#endif
