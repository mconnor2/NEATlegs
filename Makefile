CXX = g++ 
CXXFLAGS := -O3 
CXXFLAGS += -g 
#CXXFLAGS += -pg

INC := -I/usr/include -I/usr/include/SDL 
LIBS := -L/usr/lib -lSDL -lSDL_gfx -lSDL_ttf -lpthread

#INC += -I../physics/Box2D_v2.0.1/Box2D/Include
#LIBS += -L../physics/Box2D_v2.0.1/Box2D/Source/Gen/float -lbox2d
INC += -I../physics/Box2D_v2.1.2/Box2D/Box2D
INC += -I../physics/Box2D_v2.1.2/Box2D
LIBS += -L../physics/Box2D_v2.1.2/Box2D/Box2D -lBox2D

INC += -I$(TBB30_INSTALL_DIR)/include/

LIBS += -L$(TBB30_BIN_DIR) -ltbb_debug
#LIBS += -L$(TBB30_BIN_DIR) -ltbb

#CXXFLAGS += `pkg-config --cflags libconfig++`
LIBS += -lconfig++ #`pkg-config --libs libconfig++`

TARGETS := legs hopper

SRCS := BoxScreen.cpp World.cpp Creature.cpp

PHYS_OBJS := $(patsubst %.cpp,%.o, $(filter %.cpp,$(SRCS)))

include NEAT/Rules.mk

OBJS := $(patsubst %.cpp,%.o, $(filter %.cpp,$(SRCS)))

CXXFLAGS += $(INC)

CXXFLAGS += -DPROFILE

.SUFFIXES: .cpp 

.PHONY: all
all: $(TARGETS)

legs: $(PHYS_OBJS) legs.o
	$(CXX) $(CXXFLAGS) -o legs $(PHYS_OBJS) legs.o $(LIBS)

hopper: $(OBJS) hopper.o
	$(CXX) $(CXXFLAGS) -o hopper $(OBJS) hopper.o $(LIBS)

test: $(OBJS) testMult.o
	$(CXX) $(CXXFLAGS) -o testMult $(OBJS) testMult.o $(LIBS)

.PHONY: clean
clean:
	-rm $(OBJS) $(TARGETS)

#Obviously taken from 'Recursive Make Considered Harmful'

include $(OBJS:.o=.d)

%.d: %.cpp
	./depend.sh `dirname $*.cpp` $(CXXFLAGS) $*.cpp > $@
