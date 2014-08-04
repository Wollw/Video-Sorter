TARGET=video-sorter
CXX = g++
LIBS=opencv libgflags
STD=c++11

all:
	g++ src/main.cpp -o bin/$(TARGET) `pkg-config $(LIBS) --libs --cflags` --std=$(STD)

debug:
	g++ src/main.cpp -o bin/$(TARGET) `pkg-config $(LIBS) --libs --cflags` --std=$(STD) -g

clean:
	if [ -e bin/$(TARGET) ]; then rm bin/$(TARGET); fi
