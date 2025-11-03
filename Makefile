CC := gcc
CFLAGS := -Wall -Wextra -O2 -g
LDFLAGS := -lwebsockets -lmysqlclient -lcjson

SRCDIR := server
SRCS := $(SRCDIR)/main.c $(SRCDIR)/database.c $(SRCDIR)/websocket_server.c
TARGET := socket_server

.PHONY: all clean run docker-up docker-recreate install-deps

all: $(TARGET)

$(TARGET): $(SRCS)
	mkdir -p $(dir $(TARGET))
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(SRCDIR)/*.o

run: $(TARGET)
	./$(TARGET)

docker-up:
	docker compose up -d

docker-recreate:
	docker compose down -v
	docker compose up -d

install-deps:
	sudo apt update
	sudo apt install -y build-essential gcc libwebsockets-dev libmysqlclient-dev libcjson-dev mysql-client-core-8.0
