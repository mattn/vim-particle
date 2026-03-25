UNAME := $(shell uname -s)

ifeq ($(UNAME),Linux)
TARGET = particle
SRCS   = particle_linux.c
CFLAGS = -Wall -Werror
LIBS   = -lX11 -lXrender -lm -lpthread
else
TARGET = particle.exe
SRCS   = particle_windows.c
CFLAGS = -Wall -Werror -mwindows
LIBS   = -lgdi32 -lmsimg32
endif

all : $(TARGET)

$(TARGET) : $(SRCS)
	gcc $(CFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

clean :
	rm -f particle particle.exe
