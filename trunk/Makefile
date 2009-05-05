CC = g++
THIRDPARTY = /home/txl/xserver/third_party
CFLAGS = -g -I$(THIRDPARTY)/boost/include/boost-1_39 -I$(THIRDPARTY)/gflags/include -I$(THIRDPARTY)/pcre/include
LIBS = -L$(THIRDPARTY)/pcre/lib -lpcre -lpcrecpp -lpthread -L$(THIRDPARTY)/gflags/lib -lgflags -L$(THIRDPARTY)/boost/lib -lboost_system-gcc40-mt -lboost_thread-gcc40-mt
.cpp.o:
	@rm -f $@
	$(CC) $(CFLAGS) -c $*.cpp

CSRC := $(shell ls *.cpp)
CHEADER := $(shell ls *.hpp)

OBJ = $(CSRC:.cpp=.o)
ALL = httpd

all: $(ALL)

$(OBJ) : $(CHEADER)
httpd: $(OBJ)
	@rm -f $(@)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(LIBS) $(NETLIBS)

clean:
	@rm httpd *.o
