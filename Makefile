CFLAGS = -O3 -g 
SDL_INC = -I/usr/include -I/usr/include/SDL -I/usr/local/include/SDL
SDL_LIBS = -L/usr/lib -L/usr/local/lib64 -lSDL -lSDL_gfx -lpthread

INC = -I/home/mconnor/projects/physics/Box2D_v1.4.3/Box2D/Include
LIBS = -L/home/mconnor/local/lib -lBox2D

CXX = g++ 

TARGET = legs
SRCS = BoxScreen.cpp World.cpp Creature.cpp legs.cpp 
OBJS = BoxScreen.o World.o Creature.o legs.o 


.SUFFIXES: .cpp

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CFLAGS) -o $(TARGET) $(OBJS) $(SDL_LIBS) $(LIBS)

clean:
	rm -f *~ $(OBJS) $(TARGET)

.cpp.o:
	$(CXX) $(CFLAGS) $(SDL_INC) $(INC) -c $*.cpp
