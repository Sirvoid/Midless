RAYLIB_PATH = C:/raylib/raylib
COMPILER_PATH ?= C:/mingw64/bin

# PLATFORM_WEB properties
BUILD_WEB_SHELL       ?= shell.html
BUILD_WEB_HEAP_SIZE   ?= 134217728
BUILD_WEB_RESOURCES   ?= TRUE
BUILD_WEB_RESOURCES_PATH  ?= client/bin/textures
BUILD_WEB_RAYLIB_LIB = libs/libraylibweb.a
EMSDK_PATH ?= C:/emsdk

# SERVER_WEB_SUPPORT properties
OPENSSL_INCLUDE_PATH = C:/curl/include
OPENSSL_LIB_PATH = C:/curl/lib

# Compiler
CC := gcc

rwildcard = $(foreach entry,$(wildcard $1*),$(call rwildcard,$(entry)/,$2) $(filter $(subst *,%,$2),$(entry)))
SERVER_ALL_SRC := $(call rwildcard,./server/src/,*.c)
SERVER_DEDICATED_SRC := ./server/src/main.c ./server/src/server.c ./server/src/serverwss.c
SERVER_CORE_SRC := $(filter-out $(SERVER_DEDICATED_SRC),$(SERVER_ALL_SRC))

ifndef PLATFORM
	PLATFORM = PLATFORM_DESKTOP
endif

CDIRECTIVES = -D$(PLATFORM)

ifeq ($(OS),Windows_NT)
	PLATFORM_OS = WINDOWS
else
	UNAMEOS = $(shell uname)
	ifeq ($(UNAMEOS),Linux)
	    PLATFORM_OS = LINUX
	endif
endif
ifneq ($(PLATFORM),PLATFORM_WEB)
	CDIRECTIVES += -DOS_$(PLATFORM_OS)
endif

ifeq ($(BUILD_SERVER), TRUE)
	PROJECT := server$(EXT)
	BUILD_DIR ?= build/server/$(PROJECT)

	DIR_SRC += ./server
	DIR_SRC += ./server/src
	DIR_SRC += ./server/src/world
	DIR_SRC += ./server/src/world/chunk
	DIR_SRC += ./server/src/scripting

	ifeq ($(SERVER_HEADLESS), TRUE)
		CDIRECTIVES += -DSERVER_HEADLESS
	endif
else

	ifeq ($(PLATFORM),PLATFORM_WEB)
		EMSCRIPTEN_PATH    ?= $(EMSDK_PATH)/upstream/emscripten
		CLANG_PATH          = $(EMSDK_PATH)/upstream/bin
		PYTHON_PATH         = $(EMSDK_PATH)/python/3.9.2-nuget_64bit
		NODE_PATH           = $(EMSDK_PATH)/node/14.18.2_64bit/bin
		CC = emcc
		EXT = .html
		CDIRECTIVES += -DPLATFORM_WEB
		export PATH = $(EMSDK_PATH);$(EMSCRIPTEN_PATH);$(CLANG_PATH);$(NODE_PATH);$(PYTHON_PATH):$$(PATH)
	endif

	PROJECT := game$(EXT)
	BUILD_DIR ?= build/client/$(PROJECT)

	DIR_SRC += ./client
	DIR_SRC += ./client/src
	DIR_SRC += ./client/src/chunk
	DIR_SRC += ./client/src/block
	DIR_SRC += ./client/src/entity
	DIR_SRC += ./client/src/gui
	DIR_SRC += ./client/src/networking

	EMBEDDED_SERVER_SRC := $(SERVER_CORE_SRC)

endif

DIR_SRC += ./libs/
DIR_SRC += ./shared

DIR_INC = $(addprefix -I, $(DIR_SRC))

SRC_C += $(wildcard $(addsuffix /*.c, $(DIR_SRC)))
SRC_C += $(EMBEDDED_SERVER_SRC)
ifeq ($(BUILD_SERVER),TRUE)
	SRC_C += $(SERVER_ALL_SRC)
endif
SRC_C := $(sort $(SRC_C))

ifneq ($(SERVER_WEB_SUPPORT),TRUE)
	SRC_C := $(filter-out ./libs/mongoose.c ./libs//mongoose.c,$(SRC_C))
endif

BUILD_FLAVOR := $(if $(filter TRUE,$(BUILD_SERVER)),server,client)-$(PLATFORM)-$(if $(filter TRUE,$(DEBUG)),debug,release)
BUILD_FLAVOR := $(BUILD_FLAVOR)-websocket$(if $(filter TRUE,$(SERVER_WEB_SUPPORT)),1,0)-headless$(if $(filter TRUE,$(SERVER_HEADLESS)),1,0)
OBJ_DIR ?= build/obj/$(BUILD_FLAVOR)
OBJS := $(patsubst ./%.c,$(OBJ_DIR)/%.o,$(SRC_C))

CFLAGS = -Wall -std=c99 -D_DEFAULT_SOURCE -Wno-missing-braces
ifeq ($(DEBUG), TRUE)
	CFLAGS += -g -Og
else
	ifeq ($(PLATFORM),PLATFORM_WEB)
		CFLAGS += -Os
	else ifeq ($(PLATFORM_OS), WINDOWS)
		CFLAGS += -s -Os -Wl,--subsystem,windows
	else ifeq ($(PLATFORM_OS), LINUX)
		CFLAGS += -s -Os
	endif
endif

ifeq ($(PLATFORM),PLATFORM_WEB)
	CFLAGS += -pthread
endif

INCLUDE_PATHS = $(DIR_INC) -I./server/src -I./server/src/world -I./server/src/world/chunk -I./server/src/scripting -I$(RAYLIB_PATH)/src -I$(RAYLIB_PATH)/src/external -I$(RAYLIB_PATH)/src/extras -I./libs
LDFLAGS = -L. -L$(RAYLIB_PATH)/src -L./libs

ifeq ($(SERVER_WEB_SUPPORT), TRUE)
	CDIRECTIVES += -DSERVER_WEB_SUPPORT
endif

ifeq ($(PLATFORM),PLATFORM_WEB)
	LDFLAGS += -s USE_GLFW=3 -s TOTAL_MEMORY=$(BUILD_WEB_HEAP_SIZE) -s FORCE_FILESYSTEM=1
	LDFLAGS += --preload-file $(BUILD_WEB_RESOURCES_PATH)
	ifeq ($(DEBUG),TRUE)
        LDFLAGS += -s ASSERTIONS=1 --profiling
    endif
	LDFLAGS += --shell-file $(BUILD_WEB_SHELL)
	LDLIBS = $(BUILD_WEB_RAYLIB_LIB) -pthread -sPTHREAD_POOL_SIZE=5 -sFORCE_FILESYSTEM -sALLOW_MEMORY_GROWTH -lwebsocket.js -sWEBSOCKET_SUBPROTOCOL:'binary'
	BUILD_DEPENDENCIES += $(BUILD_WEB_RAYLIB_LIB)
else
	BUILD_DEPENDENCIES += $(RAYLIB_PATH)/src/libraylib.a
	ifeq ($(PLATFORM_OS),LINUX)
		LDLIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
		
		ifeq ($(SERVER_WEB_SUPPORT), TRUE)
			CFLAGS += -I$(OPENSSL_INCLUDE_PATH) -L$(OPENSSL_LIB_PATH) -DMG_ENABLE_OPENSSL=1 -lssl -lcrypto
		endif
		
	else ifeq ($(PLATFORM_OS),WINDOWS)
		LDLIBS = -static -lraylib -lopengl32 -lgdi32 -lwinmm -lpthread -lwinmm -lws2_32
		
		ifeq ($(SERVER_WEB_SUPPORT), TRUE)
			CFLAGS += -I$(OPENSSL_INCLUDE_PATH) -L$(OPENSSL_LIB_PATH) -DMG_ENABLE_OPENSSL=1 -lssl -lcrypto -lbcrypt 
		endif
	endif
	
	
endif

all: $(BUILD_DIR)

$(BUILD_DIR): $(OBJS) $(BUILD_DEPENDENCIES)
ifeq ($(PLATFORM_OS),WINDOWS)
		@if not exist "$(@D)" mkdir "$(@D)"
else
		@mkdir -p "$(@D)"
endif
	$(CC) $(OBJS) -o $(BUILD_DIR) $(CFLAGS) $(INCLUDE_PATHS) $(LDFLAGS) $(LDLIBS) $(CDIRECTIVES)

ifeq ($(PLATFORM_OS),WINDOWS)
$(OBJ_DIR)/%.o: %.c
	@if not exist "$(@D)" mkdir "$(@D)"
	$(CC) -c $< -o $@ $(CFLAGS) $(INCLUDE_PATHS) $(CDIRECTIVES)
else
$(OBJ_DIR)/%.o: %.c
	@mkdir -p "$(@D)"
	$(CC) -c $< -o $@ $(CFLAGS) $(INCLUDE_PATHS) $(CDIRECTIVES)
endif

ifneq ($(BUILD_SERVER),TRUE)
$(OBJ_DIR)/server/src/world/world.o: CDIRECTIVES += -DMIDLESS_STB_DS_EXTERNAL
endif

clean:
    ifeq ($(PLATFORM_OS),WINDOWS)
		del *.o *.exe /s
    endif
    ifeq ($(PLATFORM_OS),LINUX)
		rm -fv *.o 
    endif

.PHONY: all clean
