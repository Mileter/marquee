# Makefile for Marquee Programs
# Requires MinGW-w64 with g++

CXX = g++
CC  = gcc
LD = g++
RES = windres
CXXFLAGS = -std=c++11 -Wall -Wextra -c
CCFLAGS  = -Wall -Wextra -c -DUNICODE
LDFLAGS = -static-libgcc -static-libstdc++ -Wl,--subsystem,windows -O2 -municode
LDFLAGS_TUI = -static-libgcc -static-libstdc++ -O2
LIBS = -lcomctl32 -lgdi32 -luser32 -lkernel32 -lcomdlg32

DBGFLAGS=

USEICONS ?= Y
CONVERT = magick
ICON_BG_COLOR = "\#00FF40"

# GNU Indent
INDENT = indent
# DO NOT CHANGE: Allman style
INDENT_FLAGS = awk '{sub(/[[:space:]]*\#.*/, "")} NF && $$1 !~ /^\#/ {printf "%s ", $$0} END {print ""}' linux1.cfg

ALL_TARGETS := editor.exe renderer.exe validate.exe
ifeq ($(filter Y y,$(USEICONS)),Y)
  ALL_TARGETS := makeicons $(ALL_TARGETS)
endif

all: $(ALL_TARGETS)
test: standard.mly

########################### ICONS ###########################

makeicons: rc/edit.ico rc/renderer.ico rc/validate.ico

rc/edit.glass.png: rc/edit.png
ifneq ($(filter Y y,$(USEICONS)),)
	@echo "Creating edit.glass.png (transparent background)..."
	$(CONVERT) rc/edit.png -transparent $(ICON_BG_COLOR) rc/edit.glass.png
else
	@echo "WARNING! Skipping icons not supported!"
	@echo "Skipping edit.glass.png, USEICONS is not enabled"
endif

rc/renderer.glass.png: rc/renderer.png
ifneq ($(filter Y y,$(USEICONS)),)
	@echo "Creating renderer.glass.png (transparent background)..."
	$(CONVERT) rc/renderer.png -transparent $(ICON_BG_COLOR) rc/renderer.glass.png
else
	@echo "Skipping renderer.glass.png, USEICONS is not enabled"
endif

rc/validate.glass.png: rc/validate.png
ifneq ($(filter Y y,$(USEICONS)),)
	@echo "Creating validate.glass.png (transparent background)..."
	$(CONVERT) rc/validate.png -transparent $(ICON_BG_COLOR) rc/validate.glass.png
else
	@echo "Skipping validate.glass.png, USEICONS is not enabled"
endif

rc/edit.ico: rc/edit.glass.png
ifneq ($(filter Y y,$(USEICONS)),)
	@echo "Creating edit.ico..."
	$(CONVERT) rc/edit.glass.png -colors 256 rc/edit.ico
else
	@echo "Skipping edit.ico, USEICONS is not enabled"
endif

rc/renderer.ico: rc/renderer.glass.png
ifneq ($(filter Y y,$(USEICONS)),)
	@echo "Creating renderer.ico..."
	$(CONVERT) rc/renderer.glass.png -colors 256 rc/renderer.ico
else
	@echo "Skipping renderer.ico, USEICONS is not enabled"
endif

rc/validate.ico: rc/validate.glass.png
ifneq ($(filter Y y,$(USEICONS)),)
	@echo "Creating validate.ico..."
	$(CONVERT) rc/validate.glass.png -colors 256 rc/validate.ico
else
	@echo "Skipping validate.ico, USEICONS is not enabled"
endif

########################### NOT ICONS ###########################
	
editor.exe: editor/editor.c rc/edit.rc rc/edit.png
	$(CC) $(DBGFLAGS) $(CCFLAGS) -o editor.o editor/editor.c
ifneq ($(filter Y y,$(USEICONS)),)
	$(RES) -o editorrc.o rc/edit.rc
endif
	$(LD) $(DBGFLAGS) -o editor.exe editor.o editorrc.o $(LDFLAGS) $(LIBS)

renderer.exe: renderer/renderer.c rc/renderer.rc rc/renderer.png
	$(CC) $(DBGFLAGS) $(CCFLAGS) -o renderer.o renderer/renderer.c
ifneq ($(filter Y y,$(USEICONS)),)
	$(RES) -o rendererrc.o rc/renderer.rc
endif
	$(LD) $(DBGFLAGS) -o renderer.exe renderer.o rendererrc.o $(LDFLAGS) $(LIBS)

validate.exe: validate/validate.c rc/validate.rc rc/validate.png
	$(CC) $(DBGFLAGS) $(CCFLAGS) -o validate.o validate/validate.c
ifneq ($(filter Y y,$(USEICONS)),)
	$(RES) -o validaterc.o rc/validate.rc
endif
	$(LD) $(DBGFLAGS) -o validate.exe validate.o validaterc.o $(LDFLAGS_TUI)

# test/test_layout.cpp
#	$(CXX) -o test_layout.exe test/test_layout.cpp -static-libgcc -static-libstdc++
#	./test_layout.exe
standard.mly: renderer.exe validate.exe
	./validate.exe mly/standard.mly
	./renderer.exe mly/standard.mly

clean:
	rm -f *.exe *.log *.res *.o rc/*.glass.png rc/*.ico
	rm -rf build/

install:
	@echo Making portable installation in build/.
	mkdir build/
	cp -f renderer.exe build/renderer.exe
	cp -f validate.exe build/validate.exe
	cp -f editor.exe build/editor.exe
	cp -f mly/standard.mly build/reference.mly
	cp -f LICENSE build/license

####### Format target #######

ALL_FILES := $(shell find . -name "*.c" -o -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" -o -name "*.C" -o -name "*.h" -o -name "*.hpp" -o -name "*.hh" -o -name "*.hxx" -o -name "*.H")

format:
	@export flags=$$($(INDENT_FLAGS));\
	for file in $(ALL_FILES); do \
		echo "Formatting $$file"; \
		$(INDENT) "$$flags" "$$file"; \
	done

rmbackups:
	find . -name "*.c~" -delete
	find . -name "*.cpp~" -delete
	find . -name "*.cc~" -delete
	find . -name "*.cxx~" -delete
	find . -name "*.C~" -delete
	find . -name "*.h~" -delete
	find . -name "*.hpp~" -delete
	find . -name "*.hh~" -delete
	find . -name "*.hxx~" -delete
	find . -name "*.H~" -delete

####### End format target #######

.PHONY: all clean install test standard.mly format rmbackups