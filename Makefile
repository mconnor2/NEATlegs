CFLAGS = -O3 -g 
SDL_INC = -I/usr/include -I/usr/include/SDL 
SDL_LIBS = -L/usr/lib -lSDL -lSDL_gfx -lSDL_ttf -lpthread

INC = -I../physics/Box2D_v2.0.1/Box2D/Include
LIBS = -L../physics/Box2D_v2.0.1/Box2D/Source/Gen/float -lbox2d

CFLAGS += `pkg-config --cflags libconfig++`
LIBS += `pkg-config --libs libconfig++`

CXX = g++ 

TARGETS = legs hopper
SRCS = BoxScreen.cpp World.cpp Creature.cpp
OBJS = BoxScreen.o World.o Creature.o

OBJS += NEAT/random.o NEAT/Network.o NEAT/InnovationStore.o NEAT/Genome.o NEAT/Specie.o NEAT/GeneticAlgorithm.o

.SUFFIXES: .cpp

all: $(TARGETS)

legs: $(OBJS) legs.o
	$(CXX) $(CFLAGS) -o legs $(OBJS) legs.o $(SDL_LIBS) $(LIBS)

hopper: $(OBJS) hopper.o
	$(CXX) $(CFLAGS) -o hopper $(OBJS) hopper.o $(SDL_LIBS) $(LIBS)

#$(TARGET): $(OBJS)
#	$(CXX) $(CFLAGS) -o $(TARGET) $(OBJS) $(SDL_LIBS) $(LIBS)

clean:
	rm -f *~ $(OBJS) $(TARGETS)

.cpp.o:
	$(CXX) $(CFLAGS) $(SDL_INC) $(INC) -c $*.cpp
