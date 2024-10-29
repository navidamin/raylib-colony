#**************************************************************************************************
#
#   raylib makefile for Desktop platforms, Web (Wasm), Raspberry Pi (DRM mode) and Android
#
#   Copyright (c) 2013-2024 Ramon Santamaria (@raysan5)
#
#**************************************************************************************************

.PHONY: all clean

# Define required environment variables
#------------------------------------------------------------------------------------------------
PLATFORM              ?= PLATFORM_DESKTOP

# Define project variables
PROJECT_NAME          ?= colony_game
PROJECT_VERSION       ?= 1.0
PROJECT_BUILD_PATH    ?= build
BIN_DIR              ?= .

# Source directories
SRC_DIR              ?= .
SOURCES = $(SRC_DIR)/main.cpp \
          $(SRC_DIR)/Colony/colony.cpp \
          $(SRC_DIR)/Engine/Engine.cpp \
          $(SRC_DIR)/Planet/planet.cpp \
          $(SRC_DIR)/Sect/sect.cpp \
          $(SRC_DIR)/Unit/unit.cpp

# raylib library variables
RAYLIB_SRC_PATH       ?= /home/navid/Applications/raylib/src
RAYLIB_INCLUDE_PATH   ?= $(RAYLIB_SRC_PATH)
RAYLIB_LIB_PATH       ?= $(RAYLIB_SRC_PATH)

# Library type used for raylib: STATIC (.a) or SHARED (.so/.dll)
RAYLIB_LIBTYPE        ?= STATIC

# Define compiler path on Windows
COMPILER_PATH         ?= C:\raylib\w64devkit\bin

# Build mode for project: DEBUG or RELEASE
BUILD_MODE            ?= RELEASE

# PLATFORM_WEB: Default properties
BUILD_WEB_ASYNCIFY    ?= FALSE
BUILD_WEB_SHELL       ?= minshell.html
BUILD_WEB_HEAP_SIZE   ?= 128MB
BUILD_WEB_STACK_SIZE  ?= 1MB
BUILD_WEB_ASYNCIFY_STACK_SIZE ?= 1048576
BUILD_WEB_RESOURCES   ?= TRUE
BUILD_WEB_RESOURCES_PATH  ?= resources

# Determine PLATFORM_OS in case PLATFORM_DESKTOP selected
ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    ifeq ($(OS),Windows_NT)
        PLATFORM_OS = WINDOWS
        export PATH := $(COMPILER_PATH):$(PATH)
    else
        UNAMEOS = $(shell uname)
        ifeq ($(UNAMEOS),Linux)
            PLATFORM_OS = LINUX
        endif
        ifeq ($(UNAMEOS),FreeBSD)
            PLATFORM_OS = BSD
        endif
        ifeq ($(UNAMEOS),OpenBSD)
            PLATFORM_OS = BSD
        endif
        ifeq ($(UNAMEOS),NetBSD)
            PLATFORM_OS = BSD
        endif
        ifeq ($(UNAMEOS),DragonFly)
            PLATFORM_OS = BSD
        endif
        ifeq ($(UNAMEOS),Darwin)
            PLATFORM_OS = OSX
        endif
    endif
endif

# Define C++ compiler
CXX = g++

ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    ifeq ($(PLATFORM_OS),OSX)
        CXX = clang++
    endif
    ifeq ($(PLATFORM_OS),BSD)
        CXX = clang++
    endif
endif
ifeq ($(PLATFORM),PLATFORM_WEB)
    CXX = em++
endif

# Define compiler flags
CXXFLAGS = -std=c++17 -Wall -I$(RAYLIB_INCLUDE_PATH) \
           -I$(SRC_DIR) -I$(SRC_DIR)/Colony -I$(SRC_DIR)/Engine \
           -I$(SRC_DIR)/Planet -I$(SRC_DIR)/Sect -I$(SRC_DIR)/Unit \
           -D_DEFAULT_SOURCE $(PROJECT_CUSTOM_FLAGS)

ifeq ($(BUILD_MODE),DEBUG)
    CXXFLAGS += -g -D_DEBUG
else
    CXXFLAGS += -s -O2
endif

# Define include paths
INCLUDE_PATHS = -I. -Iexternal -I$(RAYLIB_INCLUDE_PATH)

ifeq ($(PLATFORM),PLATFORM_DRM)
    INCLUDE_PATHS += -I/usr/include/libdrm
endif
ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    ifeq ($(PLATFORM_OS),BSD)
        INCLUDE_PATHS += -I/usr/local/include
    endif
endif

# Define library paths
LDFLAGS = -L. -L$(RAYLIB_LIB_PATH)

ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    ifeq ($(PLATFORM_OS),WINDOWS)
        LDFLAGS += $(RAYLIB_SRC_PATH)/raylib.rc.data
        ifeq ($(BUILD_MODE), RELEASE)
            LDFLAGS += -Wl,--subsystem,windows
        endif
    endif
    ifeq ($(PLATFORM_OS),BSD)
        LDFLAGS += -Lsrc -L/usr/local/lib
    endif
endif

# Define libraries to link
ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    ifeq ($(PLATFORM_OS),WINDOWS)
        LDLIBS = -lraylib -lopengl32 -lgdi32 -lwinmm -lcomdlg32 -lole32
        ifeq ($(RAYLIB_LIBTYPE),SHARED)
            LDLIBS += -lpthread
        else
            LDLIBS += -static -lpthread
        endif
    endif
    ifeq ($(PLATFORM_OS),LINUX)
        LDLIBS = -lraylib -lGL -lm -lpthread -ldl -lrt
        ifeq ($(USE_WAYLAND_DISPLAY),TRUE)
            LDLIBS += -lwayland-client -lwayland-cursor -lwayland-egl -lxkbcommon
        else
            LDLIBS += -lX11
        endif
    endif
    ifeq ($(PLATFORM_OS),OSX)
        LDLIBS = -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo
    endif
    ifeq ($(PLATFORM_OS),BSD)
        LDLIBS = -lraylib -lGL -lpthread -lm -lX11 -lXrandr -lXinerama -lXi -lXxf86vm -lXcursor
    endif
endif

# Define all object files
OBJS = $(SOURCES:$(SRC_DIR)/%.cpp=$(PROJECT_BUILD_PATH)/%.o)

# Default target entry
all:
	$(MAKE) $(PROJECT_NAME)

# Project target
$(PROJECT_NAME): create_dirs $(OBJS)
	$(CXX) -o $(BIN_DIR)/$(PROJECT_NAME)$(EXT) $(OBJS) $(CXXFLAGS) $(INCLUDE_PATHS) $(LDFLAGS) $(LDLIBS) -D$(PLATFORM)

# Create directories
create_dirs:
	mkdir -p $(PROJECT_BUILD_PATH)
	mkdir -p $(PROJECT_BUILD_PATH)/Colony
	mkdir -p $(PROJECT_BUILD_PATH)/Engine
	mkdir -p $(PROJECT_BUILD_PATH)/Planet
	mkdir -p $(PROJECT_BUILD_PATH)/Sect
	mkdir -p $(PROJECT_BUILD_PATH)/Unit

# Compile source files
$(PROJECT_BUILD_PATH)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) -c $< -o $@ $(CXXFLAGS) $(INCLUDE_PATHS) -D$(PLATFORM)

# Clean everything
clean:
ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    ifeq ($(PLATFORM_OS),WINDOWS)
		del *.o *.exe /s
    endif
    ifeq ($(PLATFORM_OS),LINUX)
		find . -type f -executable -delete
		rm -fv *.o
    endif
    ifeq ($(PLATFORM_OS),OSX)
		rm -f *.o external/*.o $(PROJECT_NAME)
    endif
endif
ifeq ($(PLATFORM),PLATFORM_DRM)
	find . -type f -executable -delete
	rm -fv *.o
endif
ifeq ($(PLATFORM),PLATFORM_WEB)
	del *.o *.html *.js
endif
	@echo Cleaning done
