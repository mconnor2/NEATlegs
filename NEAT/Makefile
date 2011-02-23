CXXFLAGS = -g 
CXXFLAGS += -O2
#CXXFLAGS += -D_DEBUG_PRINT

INC :=
LIBS := 

#SDL_INC = `sdl-config --cflags`
INC += -I/usr/include -I/usr/include/SDL 
SDL_LIBS := -L/usr/lib -lSDL -lSDL_gfx -lSDL_ttf -lpthread

LIBS += -lconfig++

INC  += -I/home/mconnor/projects/physics/Box2D_v1.4.3/Box2D/Include
BOX_LIBS := -L/home/mconnor/local/lib -lBox2D

CXX = g++ 

CXXFLAGS += $(INC)

TARGETS = xorTest poleBalance
TESTS = maxTest.cpp

SRCS :=
include Rules.mk
SRCS := $(patsubst NEAT/%.cpp,%.cpp, $(SRCS))

OBJS := $(patsubst %.cpp,%.o, $(filter %.cpp,$(SRCS)))

.SUFFIXES: .cpp

.PHONY: all
all: $(TARGETS)

xorTest: $(OBJS) xorTest.o
	$(CXX) $(CFLAGS) -o xorTest $(OBJS) xorTest.o $(LIBS)

poleBalance: $(OBJS) poleBalance.o
	$(CXX) $(CFLAGS) -o poleBalance $(OBJS) poleBalance.o $(LIBS) $(BOX_LIBS) $(SDL_LIBS)


.PHONY: clean
clean:
	rm $(OBJS) $(TARGETS)

#Obviously taken from 'Recursive Make Considered Harmful'

include $(OBJS:.o=.d)

%.d: %.cpp
	../depend.sh `dirname $*.cpp` $(CXXFLAGS) $*.cpp > $@
