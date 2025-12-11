CXX = g++
CC = gcc
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude
CFLAGS = -std=c11 -Wall -Wextra -Iinclude -fPIC
SRCDIR = src
APIDIR = api
INCDIR = include
OBJDIR = obj
BINDIR = bin
LIBDIR = lib

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    OS = linux
    RM = rm -f
    MKDIR = mkdir -p
    RMDIR = rm -rf
    EXECUTABLE_SUFFIX =
    STATIC_LIB_PREFIX = lib
    STATIC_LIB_SUFFIX = .a
    PATH_SEP = /
    RUN_PREFIX = ./
else
    OS = windows
    RM = del /Q
    MKDIR = mkdir
    RMDIR = rmdir /S /Q
    EXECUTABLE_SUFFIX = .exe
    STATIC_LIB_PREFIX = lib
    STATIC_LIB_SUFFIX = .a
    PATH_SEP = \\
    RUN_PREFIX = .\\
endif

SOURCES = $(wildcard $(SRCDIR)/*.cpp)
API_SOURCES = $(APIDIR)/holylua_api.c
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
API_OBJECTS = $(OBJDIR)/holylua_api.o

TARGET = $(BINDIR)/holylua$(EXECUTABLE_SUFFIX)
STATIC_LIB = $(LIBDIR)/$(STATIC_LIB_PREFIX)holylua$(STATIC_LIB_SUFFIX)

all: $(STATIC_LIB) $(TARGET)

$(TARGET): $(OBJECTS) $(STATIC_LIB) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) -L$(LIBDIR) -lholylua

$(STATIC_LIB): $(API_OBJECTS) | $(LIBDIR)
	ar rcs $@ $(API_OBJECTS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/holylua_api.o: $(APIDIR)/holylua_api.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	$(MKDIR) $(OBJDIR)

$(BINDIR):
	$(MKDIR) $(BINDIR)

$(LIBDIR):
	$(MKDIR) $(LIBDIR)

clean:
	-$(RMDIR) $(OBJDIR)
	-$(RMDIR) $(BINDIR)
	-$(RMDIR) $(LIBDIR)
ifeq ($(OS),windows)
	-$(RM) output.c
	-$(RM) output$(EXECUTABLE_SUFFIX)
	-$(RM) test_output.c
	-$(RM) test_output$(EXECUTABLE_SUFFIX)
else
	-$(RM) output.c
	-$(RM) output
	-$(RM) test_output.c
	-$(RM) test_output
endif

run: $(TARGET)
	$(RUN_PREFIX)$(BINDIR)/holylua$(EXECUTABLE_SUFFIX) examples/test.hlua

compile-output: $(STATIC_LIB)
	gcc output.c -o output$(EXECUTABLE_SUFFIX) -Iinclude -L$(LIBDIR) -lholylua -lm
	$(RUN_PREFIX)output$(EXECUTABLE_SUFFIX)

test: all
	$(RUN_PREFIX)$(BINDIR)/holylua$(EXECUTABLE_SUFFIX) examples/test.hlua -o test_output
	gcc test_output.c -o test_output$(EXECUTABLE_SUFFIX) -Iinclude -L$(LIBDIR) -lholylua -lm
	$(RUN_PREFIX)test_output$(EXECUTABLE_SUFFIX)

help:
	@echo "Holy Lua Compiler Makefile (Cross-platform)"
	@echo "==========================================="
	@echo "Detected OS: $(OS)"
	@echo ""
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all           Build everything (default)"
	@echo "  clean         Remove all generated files"
	@echo "  run           Build and run with example"
	@echo "  test          Build and run test"
	@echo "  compile-output Compile generated output.c"
	@echo "  help          Show this help message"

.PHONY: all clean run compile-output test help