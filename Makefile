CFLAGS = -O3 -g 
SDL_INC = -I/usr/include -I/usr/include/SDL 
SDL_LIBS = -L/usr/lib -lSDL -lSDL_gfx -lpthread

INC = -I/home/mconnor/projects/physics/Box2D_v2.0.1/Box2D/Include
LIBS = -L/home/mconnor/projects/physics/Box2D_v2.0.1/Box2D/Source/Gen/float -lbox2d

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
