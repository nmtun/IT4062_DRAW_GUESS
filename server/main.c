#include <stdio.h>
#include "database.h"
#include "websocket_server.h"

int main()
{
    if (!db_connect("127.0.0.1", 3308, "root", "123456", "draw_guess"))
    {
        printf("❌ Lỗi kết nối database\n");
        return 1;
    }

    start_websocket_server(9000);

    db_close();
    return 0;
}
