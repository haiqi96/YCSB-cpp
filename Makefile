#
#  Makefile
#  YCSB-cpp
#
#  Copyright (c) 2020 Youngjae Lee <ls4154.lee@gmail.com>.
#  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
#


#---------------------build config-------------------------

DEBUG_BUILD ?= 0
EXTRA_CXXFLAGS ?= -I/example/rocksdb/include
EXTRA_LDFLAGS ?= -L/example/rocksdb -ldl -lz -lsnappy -lzstd -lbz2 -llz4

BIND_LEVELDB ?= 0
BIND_ROCKSDB ?= 1
BIND_LMDB ?= 0

NEED_GPERFTOOLS=0
CXXFLAGS+=$(CPPFLAGS) -std=c++0x -DNDEBUG -O2 -D__const__= -pipe -W -Wall -Wno-unused-parameter -fPIC -fno-omit-frame-pointer
BRPC_PATH=/home/haiqi/incubator-brpc
include $(BRPC_PATH)/config.mk

HDRS+=$(BRPC_PATH)/output/include
LIBS+=$(BRPC_PATH)/output/lib

HDRPATHS=$(addprefix -I, $(HDRS))
LIBPATHS=$(addprefix -L, $(LIBS))
COMMA=,
SOPATHS=$(addprefix -Wl$(COMMA)-rpath$(COMMA), $(LIBS))

CLIENT_SOURCES = client.cpp
SERVER_SOURCES = server.cpp
PROTOS = $(wildcard *.proto)

PROTO_OBJS = $(PROTOS:.proto=.pb.o)
PROTO_GENS = $(PROTOS:.proto=.pb.h) $(PROTOS:.proto=.pb.cc)
CLIENT_OBJS = $(addsuffix .o, $(basename $(CLIENT_SOURCES))) 
SERVER_OBJS = $(addsuffix .o, $(basename $(SERVER_SOURCES))) 

STATIC_LINKINGS += -lbrpc
LINK_OPTIONS_SO = -Xlinker "-(" $^ -Xlinker "-)" $(STATIC_LINKINGS) $(DYNAMIC_LINKINGS)
LINK_OPTIONS = -Xlinker "-(" $^  -Wl,-Bstatic $(STATIC_LINKINGS) -Wl,-Bdynamic -Xlinker "-)" $(DYNAMIC_LINKINGS)

#----------------------------------------------------------

ifeq ($(DEBUG_BUILD), 1)
	CXXFLAGS += -g
else
	CXXFLAGS += -O2
	CPPFLAGS += -DNDEBUG
endif

ifeq ($(BIND_LEVELDB), 1)
	LDFLAGS += -lleveldb
	SOURCES += $(wildcard leveldb/*.cc)
endif

ifeq ($(BIND_ROCKSDB), 1)
	LDFLAGS += -lrocksdb
	SOURCES += $(wildcard rocksdb/*.cc)
endif

ifeq ($(BIND_LMDB), 1)
	LDFLAGS += -llmdb
	SOURCES += $(wildcard lmdb/*.cc)
endif

CXXFLAGS += -std=c++17 -Wall -pthread $(EXTRA_CXXFLAGS) -I./
LDFLAGS += $(EXTRA_LDFLAGS) -lpthread
SOURCES += $(wildcard core/*.cc)
OBJECTS += $(SOURCES:.cc=.o)
DEPS += $(SOURCES:.cc=.d)
EXEC = ycsb

all: $(EXEC)

%.pb.cc %.pb.h:%.proto
	@echo "> Generating $@"
	@$(PROTOC) --cpp_out=. --proto_path=. $(PROTOC_EXTRA_ARGS) $<

$(EXEC): $(OBJECTS) $(PROTO_OBJS) 
	@echo "> Linking $@"
	@$(CXX) $(CXXFLAGS) $(LIBPATHS) $(LINK_OPTIONS) $(LDFLAGS) -o $@

.cc.o:
	@$(CXX) $(CXXFLAGS) $(HDRPATHS) -c -o $@ $<
	@echo "  CC      " $@

%.d : %.cc
	@$(CXX) $(CXXFLAGS) $(HDRPATHS) -MM -MT '$(<:.cc=.o)' -o $@ $<

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

clean:
	find . -name "*.[od]" -delete
	$(RM) $(EXEC) $(PROTO_OBJS)

.PHONY: clean
