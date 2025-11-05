# Phát hiện hệ điều hành
UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)

CC := gcc
SRCDIR := server
SRCS := $(SRCDIR)/main.c $(SRCDIR)/database.c $(SRCDIR)/websocket_server.c
TARGET := socket_server

# ============ CẤU HÌNH CHO macOS ============
ifeq ($(UNAME_S),Darwin)
    MYSQL_CFLAGS := -I/opt/homebrew/opt/mysql-client/include
    LWS_CFLAGS := -I/opt/homebrew/opt/libwebsockets/include
    CJSON_CFLAGS := -I/opt/homebrew/opt/cjson/include
    OPENSSL_CFLAGS := -I/opt/homebrew/opt/openssl@3/include
    
    MYSQL_LDFLAGS := -L/opt/homebrew/opt/mysql-client/lib -lmysqlclient
    LWS_LDFLAGS := -L/opt/homebrew/opt/libwebsockets/lib -lwebsockets
    CJSON_LDFLAGS := -L/opt/homebrew/opt/cjson/lib -lcjson
    OPENSSL_LDFLAGS := -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto
    
    CFLAGS := -Wall -Wextra -O2 -g $(MYSQL_CFLAGS) $(LWS_CFLAGS) $(CJSON_CFLAGS) $(OPENSSL_CFLAGS)
    LDFLAGS := $(MYSQL_LDFLAGS) $(LWS_LDFLAGS) $(CJSON_LDFLAGS) $(OPENSSL_LDFLAGS)

# ============ CẤU HÌNH CHO Windows/Linux (MẶC ĐỊNH) ============
else
    CFLAGS := -Wall -Wextra -O2 -g
    LDFLAGS := -lwebsockets -lmysqlclient -lcjson
endif

.PHONY: all clean run docker-up docker-recreate install-deps

all: $(TARGET)

$(TARGET): $(SRCS)
	mkdir -p $(dir $(TARGET))
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(SRCDIR)/*.o

run: $(TARGET)
ifeq ($(UNAME_S),Darwin)
	chmod +x $(TARGET)
endif
	./$(TARGET)

docker-up:
	docker compose up -d

docker-recreate:
	docker compose down -v
	docker compose up -d

install-deps:
ifeq ($(UNAME_S),Darwin)
	@echo "Installing dependencies for macOS..."
	brew install mysql-client libwebsockets cjson openssl@3
else
	@echo "Installing dependencies for Linux/Windows..."
	sudo apt update
	sudo apt install -y build-essential gcc libwebsockets-dev libmysqlclient-dev libcjson-dev mysql-client-core-8.0
endif