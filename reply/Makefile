#gcc version
CC  = gcc
CXX = g++
GCCVER := $(shell $(CC) -dumpversion | awk -F. '{ print $$1"."$$2}' )

INC += -I../include/spp_incl/ -I../include  -I./
#LIB += -lsppasync -lasync_epoll -lcore
LIB += 

#BUILD_DIR=../../build 
OBJ_DIR=./obj
LIB_DIR=.

LIB_NAME=spp_reply.so 

TARGET = $(LIB_DIR)/$(LIB_NAME)

OBJ = $(OBJ_DIR)/reply.o \
      $(OBJ_DIR)/replyMsg.o

OBJ_EXT= .o
CXXSRC_EXT= .cpp
CSRC_EXT= .c

ifeq ($(ARCH),32)
	CFLAGS += -m32 -march=pentium4
endif
CFLAGS  += -g -fPIC -shared -Wall -O2 -pipe -fno-ident -MMD `mysql_config --cflags --libs` -D_GNU_SOURCE -D_REENTRANT #-Wl,--hash-style=both

CXXFLAGS+=$(CFLAGS)


$(OBJ_DIR)/%$(OBJ_EXT): %$(CXXSRC_EXT)
	@echo
	@echo "Compiling $< ==> $@..."
	@-mkdir -p $(OBJ_DIR)
	$(CXX) $(INC) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%$(OBJ_EXT): %$(CSRC_EXT)
	@echo
	@echo "Compiling $< ==> $@..."
	@-mkdir -p $(OBJ_DIR)
	$(CC) $(INC) $(CFLAGS) -c $< -o $@


all:$(TARGET)

$(TARGET) : $(OBJ_DIR)/JobInfo.o $(OBJ_DIR)/JobPublish.o $(OBJ_DIR)/replyMsg.o $(OBJ_DIR)/reply.o
	@echo
	@echo "Building $^ ==> $@..."
	@-mkdir -p $(LIB_DIR)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIB)
	install *.so ../../bin

clean:
	@echo
	@echo "Cleaning..." 
	rm -rf $(OBJ_DIR)
	rm -f $(TARGET)	

all32:
	make -e ARCH=32
