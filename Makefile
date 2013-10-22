CXX = g++
CFLAGS = -mtune=native -msse4.1 -pthread -pipe -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64  
CFLAGS += -O3 -Wall 
# for debugging comment this
CFLAGS += -fomit-frame-pointer -fno-strict-aliasing -funroll-loops -fno-strength-reduce
# for debugging uncomment this
#CFLAGS += -Wextra -Wredundant-decls -Wpadded -Wunreachable-code -Wdisabled-optimization -Wno-missing-field-initializers -Wno-unused-parameter -Wno-write-strings -g

LDFLAGS = -ffast-math

OSVERSION := $(shell uname -s)
LIBS = -lgmp -lgmpxx -lcrypto -lssl -pthread

ifeq ($(OSTYPE),cygwin)
  CFLAGS += -std=gnu++0x
endif

ifeq ($(OSVERSION),Linux)
	LIBS += -lrt
	CFLAGS += -std=c++0x
endif

# You might need to edit these paths too
LIBPATHS = -L/usr/local/lib -L/usr/lib
INCLUDEPATHS = -I/usr/local/include -I/usr/include -Isrc/primecoinMiner/includes/

ifeq ($(OSVERSION),Darwin)
	GOT_MACPORTS := $(shell which port)
ifdef GOT_MACPORTS
	LIBPATHS += -L/opt/local/lib
	INCLUDEPATHS += -I/opt/local/include
endif
endif

JHLIB = src/primecoinMiner/jhlib/customBuffer.o \
	src/primecoinMiner/jhlib/fastString_eprintf.o \
	src/primecoinMiner/jhlib/packetBuffer.o \
	src/primecoinMiner/jhlib/fastString.o \
	src/primecoinMiner/jhlib/hashTable_uint32.o \
	src/primecoinMiner/jhlib/simpleList.o \
	src/primecoinMiner/jhlib/simpleHTTP.o \
	src/primecoinMiner/jhlib/streamWrapper.o

OBJS = \
	src/primecoinMiner/bn2.o \
	src/primecoinMiner/bn2_div.o \
	src/primecoinMiner/ticker.o \
	src/primecoinMiner/jsonBuilder.o \
	src/primecoinMiner/jsonClient.o \
	src/primecoinMiner/jsonObject.o \
	src/primecoinMiner/jsonParser.o \
	src/primecoinMiner/jsonrpc.o \
	src/primecoinMiner/prime.o \
	src/primecoinMiner/main.o \
	src/primecoinMiner/miner.o \
	src/primecoinMiner/ripemd160.o \
	src/primecoinMiner/sha256.o \
	src/primecoinMiner/transaction.o \
	src/primecoinMiner/xptClient.o \
	src/primecoinMiner/xptClientPacketHandler.o \
	src/primecoinMiner/xptPacketbuffer.o
#	src/primecoinMiner/xptServer.o \
#	src/primecoinMiner/xptServerPacketHandler.o

all: jhprimeminer
  
src/primecoinMiner/jhlib/%.o: src/primecoinMiner/jhlib/%.cpp
	$(CXX) -c $(CFLAGS) -I./src/primecoinMiner/jhlib $< -o $@

src/primecoinMiner/%.o: src/primecoinMiner/%.cpp
	$(CXX) -c $(CFLAGS) $(INCLUDEPATHS) $< -o $@ 

jhprimeminer: $(OBJS:src/primecoinMiner/%=src/primecoinMiner/%) $(JHLIB:src/primecoinMiner/jhlib/%=src/primecoinMiner/jhlib/%)
	$(CXX) $(CFLAGS) $(LIBPATHS) $(INCLUDEPATHS) -o $@ $^ $(LIBS)

clean:
	-rm -f jhprimeminer
	-rm -f src/primecoinMiner/*.o
	-rm -f src/primecoinMiner/jhlib/*.o
