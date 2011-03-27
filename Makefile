CXX = g++ 
CXXFLAGS := -O3 
CXXFLAGS += -g 
#CXXFLAGS += -pg

INC := -I/usr/include -I/usr/include/SDL 
LIBS := -L/usr/lib -lSDL -lSDL_gfx -lSDL_ttf -lpthread

INC += -I../physics/Box2D_v2.1.2/Box2D/Box2D
INC += -I../physics/Box2D_v2.1.2/Box2D
LIBS += -L../physics/Box2D_v2.1.2/Box2D/Box2D -lBox2D


#####################################################################
# If you want to use Intell Threading Building Blocks, include the lines
# below, making sure the TBB30_INSTALL_DIR and TBB30_BIN_DIR environment
# variables are set to make it work
CXXFLAGS += -DUSE_TBB
INC += -I$(TBB30_INSTALL_DIR)/include/

# Use Debug or Release, set TBB30_BIN_DIR appropriately
LIBS += -L$(TBB30_BIN_DIR) -ltbb_debug
#LIBS += -L$(TBB30_BIN_DIR) -ltbb

CXXFLAGS += -DPROFILE

#CXXFLAGS += `pkg-config --cflags libconfig++`
LIBS += -lconfig++ #`pkg-config --libs libconfig++`

TARGETS := legs hopper

SRCS := BoxScreen.cpp World.cpp Creature.cpp

PHYS_OBJS := $(patsubst %.cpp,%.o, $(filter %.cpp,$(SRCS)))

include NEAT/Rules.mk

OBJS := $(patsubst %.cpp,%.o, $(filter %.cpp,$(SRCS)))

CXXFLAGS += $(INC)

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
