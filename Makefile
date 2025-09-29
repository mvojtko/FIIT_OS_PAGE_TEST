# Compiler and flags
CXX = g++
CC = gcc
CFLAGS_COMMON = -fPIC -Wall -Wextra -O2 -Isrc -Isrc/stub/include
CFLAGS_LIB = $(CFLAGS_COMMON) \
			 -Dmalloc=__forbidden_malloc \
			 -Dcalloc=__forbidden_calloc \
			 -Drealloc=__forbidden_realloc \
			 -Dfree=__forbidden_free
CXXFLAGS_APP = $(CFLAGS_COMMON) -Igtest/googletest/include -pthread

LDFLAGS_LIB = -shared
LDFLAGS_APP = -L$(BUILD_DIR) -lospager $(GTEST_LIB)

GTEST_DIR = gtest
GTEST_LIB = build/lib/libgtest.a

# Directories
SRC_DIR = src
LIB_SRC_DIR = $(SRC_DIR)/stub
BUILD_DIR = build

# Output targets
LIB_TARGET = $(BUILD_DIR)/libospager.so
APP_TARGET = $(BUILD_DIR)/os

# Source files
LIB_SRC = $(wildcard $(LIB_SRC_DIR)/*.c)
APP_SRC = $(filter-out $(LIB_SRC), $(wildcard $(SRC_DIR)/*.cpp))

# Object files
LIB_OBJ = $(patsubst $(LIB_SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(LIB_SRC))
APP_OBJ = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(APP_SRC))

DEBUG ?= -d

# Default target: build app (which depends on the library)
all: test

test: $(APP_TARGET)
	@LD_LIBRARY_PATH=$(BUILD_DIR) $(APP_TARGET) $(DEBUG)

#build gtest libs
$(GTEST_LIB): | $(BUILD_DIR)
	@cd $(GTEST_DIR) && cmake -S . -B ../$(BUILD_DIR) && $(MAKE) -C ../$(BUILD_DIR)

# Build the shared library
$(LIB_TARGET): $(LIB_OBJ) | $(BUILD_DIR)
	@echo Linking $@
	@$(CC) $(LDFLAGS_LIB) -o $@ $^

# Build the executable
$(APP_TARGET): $(APP_OBJ) $(LIB_TARGET) $(GTEST_LIB)
	@echo Linking $@
	@$(CXX) -o $@ $(APP_OBJ) $(LDFLAGS_APP)

# Compile app source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@echo Compiling $<
	@$(CXX) $(CXXFLAGS_APP) -c $< -o $@

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
	@echo Deleting $(BUILD_DIR)
	@rm -rf $(BUILD_DIR)
