.PHONY: all clean

RAYLIB_PATH=C:\raylib\raylib
PROJECT := game.exe

COMPILER_PATH ?= C:\raylib\w64devkit\bin
export PATH := $(COMPILER_PATH):$(PATH)

DIR_SRC += .
DIR_SRC += ./src
DIR_SRC += ./src/chunk
DIR_SRC += ./src/block
DIR_SRC += ./src/entity
DIR_SRC += ./src/gui
DIR_SRC += ./src/networking

DIR_INC = $(addprefix -I, $(DIR_SRC))

SRC_C += $(wildcard $(addsuffix /*.c, $(DIR_SRC)))
OBJ := $(patsubst %.c, %.o, $(SRC_C))
EXE := $(PROJECT)

CC := gcc
CFLAGS = -Wall -std=c99 -D_DEFAULT_SOURCE -Wno-missing-braces -Wl,--subsystem,windows
INCLUDE_PATHS = -I$(DIR_INC) -I$(RAYLIB_PATH)/src -I$(RAYLIB_PATH)/src/external
LDFLAGS = -L. -L$(RAYLIB_PATH)/src
LDLIBS = -lraylib -lopengl32 -lgdi32 -lwinmm -lpthread -lws2_32 -lwinmm

.PHONY:all

all:
	mingw32-make $(PROJECT)

$(PROJECT): $(OBJ)
	$(CC) -o bin/$(PROJECT) $(OBJ) $(CFLAGS) $(INCLUDE_PATHS) $(LDFLAGS) $(LDLIBS) -DPLATFORM_DESKTOP

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS) $(INCLUDE_PATHS) -DPLATFORM_DESKTOP

clean:
	del *.o *.exe /s