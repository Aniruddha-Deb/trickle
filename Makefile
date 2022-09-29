CPPC=g++
NORMAL_FLAGS := -std=c++14
PEDANTIC_FLAGS := -g

RM=rm

ifeq ($(DEBUG),1)
	CPPFLAGS := $(NORMAL_FLAGS) $(PEDANTIC_FLAGS)
else
	CPPFLAGS := $(NORMAL_FLAGS) -O3
endif

ifneq ($(OS),Windows_NT)
	uname_os := $(shell uname)
	ifneq ($(uname_os),Darwin)
		CPPFLAGS += -I/usr/include/kqueue
		LFLAGS += -lkqueue -lpthread
	endif
endif

SRC_SERVER := src/server.cpp
SRC_CLIENT := src/client.cpp
SRC_CLIENTMGR := src/clientmgr.cpp

ifeq ($(PROTO), TCP)
	SRC_SERVER += src/client_connection_tcp.cpp src/server_tcp.cpp
	SRC_CLIENT += src/client_tcp.cpp
	SRC_CLIENTMGR += src/client_tcp.cpp
	SERVER := bin/server_tcp
	CLIENT := bin/client_tcp
	CLIENTMGR := bin/clientmgr_tcp
else
	SRC_SERVER += src/client_connection_udp.cpp src/server_udp.cpp
	SRC_CLIENT += src/client_udp.cpp
	SRC_CLIENTMGR += src/client_udp.cpp
	SERVER := bin/server_udp
	CLIENT := bin/client_udp
	CLIENTMGR := bin/clientmgr_udp
endif

OBJ_SERVER := $(patsubst src/%.cpp, obj/%.o, $(SRC_SERVER))
DEP_SERVER := $(patsubst src/%.cpp, obj/%.d, $(SRC_SERVER))

OBJ_CLIENT := $(patsubst src/%.cpp, obj/%.o, $(SRC_CLIENT))
DEP_CLIENT := $(patsubst src/%.cpp, obj/%.d, $(SRC_CLIENT))

OBJ_CLIENTMGR := $(patsubst src/%.cpp, obj/%.o, $(SRC_CLIENTMGR))
DEP_CLIENTMGR := $(patsubst src/%.cpp, obj/%.d, $(SRC_CLIENTMGR))

INC := #-I./include/

all: $(SERVER) $(CLIENT) $(CLIENTMGR)

server: $(SERVER)

client: $(CLIENT)

package: $(SERVER) $(CLIENT) $(CLIENTMGR)
	mkdir -p build
	mkdir -p build/trickle_proj
	mkdir -p build/trickle_proj/res
	cp -r src build/trickle_proj
	cp -r res build/trickle_proj
	cp Makefile build/trickle_proj
	cp writeup/writeup.pdf build/trickle.pdf
	cp trickle_demo.sh build/
	cd build && zip -r trickle.zip *

$(SERVER): $(OBJ_SERVER)
	$(CPPC) $(CPPFLAGS) $(INC) $(LIB) $^ -o $@ $(LFLAGS)

$(CLIENT): $(OBJ_CLIENT)
	$(CPPC) $(CPPFLAGS) $(INC) $(LIB) $^ -o $@ $(LFLAGS)

$(CLIENTMGR): $(OBJ_CLIENTMGR)
	$(CPPC) $(CPPFLAGS) $(INC) $(LIB) $^ -o $@ $(LFLAGS)

-include $(DEP_SERVER)
-include $(DEP_CLIENT)

obj/%.o: src/%.cpp Makefile
	$(CPPC) $(CPPFLAGS) -MMD -MP $(INC) $(LIB) -c $< -o $@ $(LFLAGS)

clean:
	$(RM) obj/* bin/*
	$(RM) -r build/*

bin/%.o: src/%.cpp
