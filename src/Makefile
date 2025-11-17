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

SRC_DIR = server
OBJ_DIR = build

TARGET = main$(EXE)

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/server.c
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# ============================
#  Build rules
# ============================

all: $(TARGET)
	@echo "Built for: $(PLATFORM)"

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/server.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directory
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# ============================
#  Clean rules
# ============================

clean:
	$(RM) $(OBJ_DIR)/*.o
	$(RM) $(TARGET)

.PHONY: all clean
