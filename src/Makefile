# ============================
#  Detect Operating System
# ============================

UNAME_S := $(shell uname -s)

ifeq ($(OS), Windows_NT)
    PLATFORM = Windows
    EXE = .exe
    RM = del /F
else
    ifeq ($(UNAME_S), Linux)
        PLATFORM = Linux
    endif
    ifeq ($(UNAME_S), Darwin)
        PLATFORM = macOS
    endif
    EXE =
    RM = rm -f
endif

# ============================
#  Compiler settings
# ============================

CC     = gcc
CFLAGS = -Wall -Wextra -g

# ============================
#  MySQL Configuration
# ============================

# Tìm MySQL include directory
# Nếu mysql.h nằm trong thư mục con mysql/, lấy thư mục cha làm include path
MYSQL_H_PATH := $(shell find /usr/local /opt/homebrew -name "mysql.h" 2>/dev/null | head -1)
MYSQL_H_DIR := $(shell echo $(MYSQL_H_PATH) | xargs dirname)
MYSQL_H_PARENT := $(shell echo $(MYSQL_H_DIR) | xargs dirname)
# Kiểm tra nếu thư mục chứa mysql.h có tên là "mysql", thì dùng thư mục cha
MYSQL_INCLUDE := $(shell if [ "$(shell basename $(MYSQL_H_DIR))" = "mysql" ]; then echo $(MYSQL_H_PARENT); else echo $(MYSQL_H_DIR); fi)

# Tìm MySQL lib directory
MYSQL_LIB_DIR := $(shell find /usr/local /opt/homebrew -name "libmysqlclient.*" 2>/dev/null | head -1 | xargs dirname)

# Tìm zstd lib directory (ưu tiên homebrew Cellar và opt)
ZSTD_LIB_DIR := $(shell find /opt/homebrew/Cellar/zstd/*/lib /opt/homebrew/opt/zstd/lib /opt/homebrew/lib /usr/local/lib -name "libzstd.dylib" 2>/dev/null | head -1 | xargs dirname)

# Tìm OpenSSL lib directory (cần cho MySQL)
SSL_LIB_DIR := $(shell find /opt/homebrew/opt/openssl*/lib /usr/local/opt/openssl*/lib -type d 2>/dev/null | head -1)

# Tìm OpenSSL include directory (cần cho auth.c)
SSL_INCLUDE := $(shell find /opt/homebrew/opt/openssl*/include /usr/local/opt/openssl*/include -name "openssl" -type d 2>/dev/null | head -1 | xargs dirname)

# Thêm flags nếu tìm thấy
ifneq ($(MYSQL_INCLUDE),)
    CFLAGS += -I$(MYSQL_INCLUDE)
endif

ifneq ($(MYSQL_LIB_DIR),)
    LDFLAGS += -L$(MYSQL_LIB_DIR)
endif

ifneq ($(ZSTD_LIB_DIR),)
    LDFLAGS += -L$(ZSTD_LIB_DIR)
endif

ifneq ($(SSL_LIB_DIR),)
    LDFLAGS += -L$(SSL_LIB_DIR)
endif

ifneq ($(SSL_INCLUDE),)
    CFLAGS += -I$(SSL_INCLUDE)
endif

# Thêm đường dẫn Homebrew mặc định cho macOS
ifeq ($(PLATFORM),macOS)
    # Thêm đường dẫn zstd và openssl vào trước LDFLAGS
    ifneq ($(ZSTD_LIB_DIR),)
        LDFLAGS := -L$(ZSTD_LIB_DIR) $(LDFLAGS)
    endif
    ifneq ($(SSL_LIB_DIR),)
        LDFLAGS := -L$(SSL_LIB_DIR) $(LDFLAGS)
    endif
    # Thêm đường dẫn mặc định
    LDFLAGS := -L/opt/homebrew/lib $(LDFLAGS)
endif

# Thêm các thư viện cần thiết
LDFLAGS += -lmysqlclient -lzstd -lssl -lcrypto -lz

# Nếu có mysql_config, dùng nó (đáng tin cậy nhất)
MYSQL_CONFIG := $(shell which mysql_config 2>/dev/null || find /usr/local/mysql*/bin /opt/homebrew/bin -name "mysql_config" 2>/dev/null | head -1)
ifneq ($(MYSQL_CONFIG),)
    MYSQL_CFLAGS_RAW := $(shell $(MYSQL_CONFIG) --cflags)
    # Điều chỉnh include path: nếu kết thúc bằng /mysql, bỏ phần /mysql đi
    # Ví dụ: -I/opt/homebrew/opt/mysql-client/include/mysql -> -I/opt/homebrew/opt/mysql-client/include
    MYSQL_CFLAGS := $(shell echo "$(MYSQL_CFLAGS_RAW)" | sed 's|-I\([^ ]*/\)mysql |-I\1|g' | sed 's|-I\([^ ]*/\)mysql$$|-I\1|g')
    MYSQL_LDFLAGS := $(shell $(MYSQL_CONFIG) --libs)
    CFLAGS += $(MYSQL_CFLAGS)
    # Thêm MYSQL_LDFLAGS vào LDFLAGS thay vì ghi đè
    LDFLAGS += $(MYSQL_LDFLAGS)
else
    # Nếu không có mysql_config, dùng thư viện đã tìm được
    LDFLAGS += -lmysqlclient -lzstd -lssl -lcrypto -lz
endif

# ============================
#  Project Structure
# ============================

SRC_DIR = server
HEADER_DIR = include
OBJ_DIR = build

TARGET = main$(EXE)

# Source files
SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/server.c $(SRC_DIR)/database.c $(SRC_DIR)/auth.c \
       $(SRC_DIR)/protocol.c $(SRC_DIR)/protocol_core.c $(SRC_DIR)/protocol_auth.c $(SRC_DIR)/protocol_room.c \
       $(SRC_DIR)/room.c $(SRC_DIR)/drawing.c

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Test room
TEST_ROOM = test_room$(EXE)
TEST_ROOM_SRC = test_room.c
TEST_ROOM_OBJS = $(OBJ_DIR)/room.o

# ============================
#  Build rules
# ============================

all: $(TARGET) test_client room_test test_room
	@echo "Built for: $(PLATFORM)"
	@echo "Executables: $(TARGET), test_client, room_test, $(TEST_ROOM)"

# Test client
# Tắt flag sinh ra .dSYM và .yml cho target này (không dùng -g, không sinh debug info)
test_client: test_client.c
	@echo "Building test client..."
	$(CC) -Wall -Wextra -O2 -I$(HEADER_DIR) -Icommon test_client.c -o test_client
	@echo "Test client built: ./test_client"

# Test room protocol
room_test: room_test.c
	@echo "Building room protocol test..."
	$(CC) -Wall -Wextra -O2 -I$(HEADER_DIR) -Icommon room_test.c -o room_test -lpthread
	@echo "Room test built: ./room_test"
	@echo "Run with: ./room_test"

# Test room module
test_room: $(TEST_ROOM_OBJS) $(TEST_ROOM_SRC)
	@echo "Building room module test..."
	$(CC) $(CFLAGS) -I$(HEADER_DIR) $(TEST_ROOM_SRC) $(TEST_ROOM_OBJS) -o $(TEST_ROOM)
	@echo "Test room built: ./$(TEST_ROOM)"
	@echo "Run with: ./$(TEST_ROOM)"

# Test drawing module
test_drawing: $(OBJ_DIR)/drawing.o test_drawing.c
	@echo "Building drawing module test..."
	$(CC) $(CFLAGS) -I$(HEADER_DIR) test_drawing.c $(OBJ_DIR)/drawing.o -o test_drawing
	@echo "Test drawing built: ./test_drawing"
	@echo "Run with: ./test_drawing"

$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)..."
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Build successful!"

# Luật build object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -I$(HEADER_DIR) -Icommon -c $< -o $@

# Create build directory
$(OBJ_DIR):
	@echo "Creating build directory..."
	mkdir -p $(OBJ_DIR)

# ============================
#  Clean rules
# ============================

clean:
	@echo "Cleaning build artifacts..."
	$(RM) $(OBJ_DIR)/*.o
	$(RM) $(TARGET)
	$(RM) test_client
	$(RM) room_test
	$(RM) $(TEST_ROOM)
	$(RM) test_drawing
	@echo "Clean complete!"

# ============================
#  Docker rules
# ============================

docker-up:
	@echo "Starting Docker containers..."
	docker compose up -d

docker-down:
	@echo "Stopping Docker containers..."
	docker compose down

docker-recreate:
	@echo "Recreating Docker containers..."
	docker compose down -v
	docker compose up -d

# ============================
#  Dependency Installation
# ============================

install-deps:
ifeq ($(UNAME_S),Darwin)
	@echo "Installing dependencies for macOS..."
	brew install mysql-client cjson zstd
	@echo "Dependencies installed successfully!"
else
	@echo "Installing dependencies for Linux..."
	sudo apt update
	sudo apt install -y build-essential gcc libmysqlclient-dev libcjson-dev mysql-client-core-8.0 libzstd-dev libssl-dev zlib1g-dev
	@echo "Dependencies installed successfully!"
endif

# ============================
#  Debug & Info
# ============================

# debug-mysql:
# 	@echo "=== MySQL Configuration Debug ==="
# 	@echo "MYSQL_CONFIG:   $(MYSQL_CONFIG)"
# 	@echo "MYSQL_INCLUDE:  $(MYSQL_INCLUDE)"
# 	@echo "MYSQL_LIB_DIR:  $(MYSQL_LIB_DIR)"
# 	@echo "ZSTD_LIB_DIR:   $(ZSTD_LIB_DIR)"
# 	@echo "SSL_LIB_DIR:    $(SSL_LIB_DIR)"
# 	@echo ""
# 	@echo "=== Compiler Flags ==="
# 	@echo "CFLAGS:  $(CFLAGS)"
# 	@echo "LDFLAGS: $(LDFLAGS)"
# 	@echo ""
# 	@echo "=== Platform Info ==="
# 	@echo "Platform: $(PLATFORM)"
# 	@echo "System:   $(UNAME_S)"

# info:
# 	@echo "=== Project Information ==="
# 	@echo "Target:      $(TARGET)"
# 	@echo "Source Dir:  $(SRC_DIR)"
# 	@echo "Header Dir:  $(HEADER_DIR)"
# 	@echo "Build Dir:   $(OBJ_DIR)"
# 	@echo ""
# 	@echo "=== Source Files ==="
# 	@echo "$(SRCS)"
# 	@echo ""
# 	@echo "=== Object Files ==="
# 	@echo "$(OBJS)"

# ============================
#  Run & Test
# ============================

# run: $(TARGET)
# 	@echo "Running $(TARGET)..."
# 	./$(TARGET)

# rebuild: clean all

# ============================
#  Phony Targets
# ============================

.PHONY: all clean docker-up docker-down docker-recreate install-deps debug-mysql info run rebuild