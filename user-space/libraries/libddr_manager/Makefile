# Makefile for DDR Manager Library
# Version: 1.0.0
# Author: Sebastian

# ============================================================================
# Configuration
# ============================================================================

LIBRARY_NAME = ddr_manager
VERSION_MAJOR = 1
VERSION_MINOR = 0
VERSION_PATCH = 0
VERSION = $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

# Compiler and flags
CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -O2 -fPIC -D_GNU_SOURCE
CXXFLAGS = -Wall -Wextra -O2 -fPIC -D_GNU_SOURCE
LDFLAGS = -shared -Wl,-soname,lib$(LIBRARY_NAME).so.$(VERSION_MAJOR)
INCLUDES = -I.

# Debug flags
ifdef DEBUG
	CFLAGS += -g -DDEBUG
	CXXFLAGS += -g -DDEBUG
endif

# Installation directories
PREFIX ?= /usr/local
LIBDIR = $(PREFIX)/lib
INCDIR = $(PREFIX)/include
DESTDIR =

# ============================================================================
# Files
# ============================================================================

# Source files
SRCS = ddr_manager.c
OBJS = $(SRCS:.c=.o)

# Header files
HEADERS = ddr_manager.h

# Library names
LIBRARY_SO = lib$(LIBRARY_NAME).so
LIBRARY_SO_VERSION = $(LIBRARY_SO).$(VERSION_MAJOR)
LIBRARY_SO_FULL = $(LIBRARY_SO_VERSION).$(VERSION_MINOR).$(VERSION_PATCH)
LIBRARY_A = lib$(LIBRARY_NAME).a

# ============================================================================
# Targets
# ============================================================================

.PHONY: all clean distclean install uninstall test docs help

all: $(LIBRARY_SO_VERSION) $(LIBRARY_A)

# Shared library
$(LIBRARY_SO_FULL): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)
	ln -sf $(LIBRARY_SO_FULL) $(LIBRARY_SO_VERSION)
	ln -sf $(LIBRARY_SO_VERSION) $(LIBRARY_SO)

$(LIBRARY_SO_VERSION): $(LIBRARY_SO_FULL)

# Static library
$(LIBRARY_A): $(OBJS)
	$(AR) rcs $@ $(OBJS)

# Object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# ============================================================================
# Installation
# ============================================================================

install: all
	@echo "Installing DDR Manager library..."
	install -d $(DESTDIR)$(LIBDIR)
	install -m 755 $(LIBRARY_SO_FULL) $(DESTDIR)$(LIBDIR)
	ln -sf $(LIBRARY_SO_FULL) $(DESTDIR)$(LIBDIR)/$(LIBRARY_SO_VERSION)
	ln -sf $(LIBRARY_SO_VERSION) $(DESTDIR)$(LIBDIR)/$(LIBRARY_SO)
	install -m 644 $(LIBRARY_A) $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(INCDIR)
	install -m 644 $(HEADERS) $(DESTDIR)$(INCDIR)
	@echo "Installation complete."
	@echo "  Libraries: $(LIBDIR)"
	@echo "  Headers: $(INCDIR)"
	@echo "  Run 'ldconfig' to update library cache"

# ============================================================================
# Uninstallation
# ============================================================================

uninstall:
	@echo "Uninstalling DDR Manager library..."
	rm -f $(DESTDIR)$(LIBDIR)/$(LIBRARY_SO)*
	rm -f $(DESTDIR)$(LIBDIR)/$(LIBRARY_A)
	rm -f $(DESTDIR)$(INCDIR)/$(HEADERS)
	@echo "Uninstallation complete."

# ============================================================================
# Test
# ============================================================================

test: $(LIBRARY_SO_VERSION)
	@echo "Building test program..."
	$(CC) -o test_ddr_manager test_ddr_manager.c -I. -L. -l$(LIBRARY_NAME) -lpthread
	@echo "Running test..."
	./test_ddr_manager
	rm -f test_ddr_manager

# ============================================================================
# Documentation
# ============================================================================

docs:
	@echo "Generating documentation..."
	@which doxygen > /dev/null || (echo "Doxygen not found. Install doxygen to generate docs."; exit 1)
	doxygen Doxyfile 2>/dev/null || echo "Doxyfile not found. Using default settings..."
	@echo "Documentation generated in docs/html/"

# ============================================================================
# Clean
# ============================================================================

clean:
	rm -f $(OBJS) $(LIBRARY_SO)* $(LIBRARY_A) test_ddr_manager
	rm -rf docs/html

distclean: clean
	rm -f *~ *.bak *.swp

# ============================================================================
# Help
# ============================================================================

help:
	@echo "DDR Manager Library Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all         - Build shared and static libraries (default)"
	@echo "  install     - Install libraries and headers"
	@echo "  uninstall   - Remove installed files"
	@echo "  test        - Build and run test program"
	@echo "  docs        - Generate documentation"
	@echo "  clean       - Remove build files"
	@echo "  distclean   - Remove all generated files"
	@echo "  help        - Show this help"
	@echo ""
	@echo "Variables:"
	@echo "  PREFIX      - Installation prefix (default: /usr/local)"
	@echo "  DEBUG       - Enable debug build (set to 1)"
	@echo "  DESTDIR     - Destination directory for installation"
	@echo ""
	@echo "Examples:"
	@echo "  make DEBUG=1"
	@echo "  make install PREFIX=/opt/ddr_manager"
	@echo "  make DESTDIR=build_root install"
