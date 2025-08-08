# Compiler and flags
CC = gcc
CFLAGS_COMMON = -fPIC -Wall -Wextra -O2 -Isrc -Isrc/stub
CFLAGS_LIB = $(CFLAGS_COMMON) \
			 -Dmalloc=__forbidden_malloc \
			 -Dcalloc=__forbidden_calloc \
			 -Drealloc=__forbidden_realloc \
			 -Dfree=__forbidden_free
CFLAGS_APP = $(CFLAGS_COMMON)

LDFLAGS_LIB = -shared
LDFLAGS_APP = -L$(BUILD_DIR) -lospager

# Directories
SRC_DIR = src
LIB_SRC_DIR = $(SRC_DIR)/stub
BUILD_DIR = build

# Output targets
LIB_TARGET = $(BUILD_DIR)/libospager.so
APP_TARGET = $(BUILD_DIR)/app

# Source files
LIB_SRC = $(wildcard $(LIB_SRC_DIR)/*.c)
APP_SRC = $(filter-out $(LIB_SRC), $(wildcard $(SRC_DIR)/*.c))

# Object files
LIB_OBJ = $(patsubst $(LIB_SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(LIB_SRC))
APP_OBJ = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(APP_SRC))

# Default target: build app (which depends on the library)
all: test

test: $(APP_TARGET)
	@LD_LIBRARY_PATH=$(BUILD_DIR) $(APP_TARGET)

# Build the shared library
$(LIB_TARGET): $(LIB_OBJ) | $(BUILD_DIR)
	@echo Linking $@
	@$(CC) $(LDFLAGS_LIB) -o $@ $^

# Build the executable
$(APP_TARGET): $(APP_OBJ) $(LIB_TARGET)
	@echo Linking $@
	@$(CC) -o $@ $(APP_OBJ) $(LDFLAGS_APP)

# Compile app source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo Compiling $<
	@$(CC) $(CFLAGS_APP) -c $< -o $@

# Compile library source files to object files
$(BUILD_DIR)/%.o: $(LIB_SRC_DIR)/%.c | $(BUILD_DIR)
	@echo Compiling $<
	@$(CC) $(CFLAGS_LIB) -c $< -o $@

# Ensure build directory exists
$(BUILD_DIR):
	@echo Creating $@
	@mkdir -p $(BUILD_DIR)

# Clean all build artifacts
clean:
	@echo Deleting $@
	@rm -rf $(BUILD_DIR)
