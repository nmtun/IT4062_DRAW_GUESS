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
       $(SRC_DIR)/protocol_drawing.c $(SRC_DIR)/protocol_game.c $(SRC_DIR)/room.c $(SRC_DIR)/drawing.c $(SRC_DIR)/game.c \
       $(SRC_DIR)/protocol_chat.c $(SRC_DIR)/sha256.c

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# ============================
#  Build rules
# ============================

all: $(TARGET)
	@echo "Built for: $(PLATFORM)"
	@echo "Executables: $(TARGET)"

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

.PHONY: all clean docker-up docker-down docker-recreate install-deps debug-mysql info run rebuild