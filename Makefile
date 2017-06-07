#CC=gcc
#CPP=g++
APP_BINARY=cam_cap
VERSION = 0.1
PREFIX=/usr/local/bin

WARNINGS = -Wall


CFLAGS = -std=gnu99 -O2 -DLINUX -DVERSION=\"$(VERSION)\" $(WARNINGS)
CPPFLAGS = $(CFLAGS)

OBJECTS= cam_cap.o v4l2uvc.o color.o utils.o


all:    cam_cap

clean:
	@echo "Cleaning up directory."
	rm -f *.a *.o $(APP_BINARY) core *~ log errlog

install:
	install $(APP_BINARY) $(PREFIX)

# Applications:
cam_cap: $(OBJECTS)
	$(CC)   $(OBJECTS) $(XPM_LIB) $(MATH_LIB) -ljpeg -o $(APP_BINARY)
