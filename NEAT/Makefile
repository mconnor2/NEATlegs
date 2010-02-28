CFLAGS = -g 
#CFLAGS += -D_DEBUG_PRINT
#SDL_INC = -I/usr/include -I/usr/include/SDL -I/usr/local/include/SDL
SDL_INC = `sdl-config --cflags`
SDL_LIBS = -L/usr/lib -lSDL -lSDL_gfx -lSDL_ttf -lpthread

BOX_INC = -I/home/mconnor/projects/physics/Box2D_v1.4.3/Box2D/Include
BOX_LIBS = -L/home/mconnor/local/lib -lBox2D

CXX = g++ 

TARGETS = xorTest poleBalance
SRCS = random.cpp Network.cpp InnovationStore.cpp Genome.cpp Sepecie.cpp
OBJS = random.o Network.o InnovationStore.o Genome.o Specie.o

TESTS = maxTest.cpp


.SUFFIXES: .cpp

all: $(TARGETS)

xorTest: $(OBJS) xorTest.o
	$(CXX) $(CFLAGS) -o xorTest $(OBJS) xorTest.o $(LIBS)

poleBalance: $(OBJS) poleBalance.o
	$(CXX) $(CFLAGS) -o poleBalance $(OBJS) poleBalance.o $(LIBS) $(SDL_LIBS)

#$(TARGET): $(OBJS)
#	$(CXX) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

clean:
	rm -f *~ $(OBJS) $(TARGET)

.cpp.o:
	$(CXX) $(CFLAGS) $(INC) $(SDL_INC) -c $*.cpp
