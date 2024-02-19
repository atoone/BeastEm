SDL2_CFLAGS=$(shell sdl2-config --cflags)
SDL2_LIBS=$(shell sdl2-config --libs)
CXXFLAGS:=$(CXXFLAGS) $(SDL2_CFLAGS) -std=c++20 -O2
LDFLAGS:=$(LDFLAGS) -lstdc++ -lm $(SDL2_LIBS) -lSDL2_net -lSDL2_ttf -lSDL2_gfx

BINARY?=beastem
OBJECTS=beastem.o 			\
		src/i2c.o		\
		src/beast.o 		\
		src/digit.o 		\
		src/display.o 		\
		src/rtc.o		\
		src/debug.o 		\
		src/listing.o 		\
		src/instructions.o 	\
		src/videobeast.o

.PHONY: all clean

all: $(BINARY)

$(BINARY): $(OBJECTS)

clean:
	$(RM) $(BINARY) $(OBJECTS)
	
