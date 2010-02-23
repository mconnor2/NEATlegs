CFLAGS = -g 
#CFLAGS += -D_DEBUG_PRINT
SDL_INC = -I/usr/include -I/usr/include/SDL -I/usr/local/include/SDL
SDL_LIBS = -L/usr/lib -L/usr/local/lib64 -lSDL -lSDL_gfx -lpthread

BOX_INC = -I/home/mconnor/projects/physics/Box2D_v1.4.3/Box2D/Include
BOX_LIBS = -L/home/mconnor/local/lib -lBox2D

CXX = g++ 

TARGET = xorTest
SRCS = random.cpp Network.cpp InnovationStore.cpp Genome.cpp Sepecie.cpp xorTest.cpp
OBJS = random.o Network.o InnovationStore.o Genome.o Specie.o xorTest.o

TESTS = maxTest.cpp


.SUFFIXES: .cpp

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)
clean:
	rm -f *~ $(OBJS) $(TARGET)

.cpp.o:
	$(CXX) $(CFLAGS) $(INC) -c $*.cpp
