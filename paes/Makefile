.PHONY: all clean indent doc 

ROOT_DIR = ..
ATISTREAMSDKROOT=/home/odroid/task/OpenCL_SDK
CC = gcc
#CFLAGS = $(DEFINES) -Wall -g -Wextra -Werror -pedantic -pedantic-errors -std=c99 -I '$(ATISTREAMSDKROOT)/include/'
CFLAGS = $(DEFINES) -Wall -g -Wextra -pedantic -pedantic-errors -std=c99 -D_XOPEN_SOURCE=600 -I $(ATISTREAMSDKROOT)/include/ -I$(ROOT_DIR)/smartmeter -lpthread -lrt
LDFLAGS = -L$(ATISTREAMSDKROOT)/lib -L$(ROOT_DIR)/libs $ -lOpenCL -lsmartmeter -lusb-1.0
SOURCES = $(wildcard *.c)
OBJECTS = $(patsubst %.c, %.o, $(SOURCES))
OPENCL_SOURCE = paes.cl
PREPROCESSED_OPENCL_SOURCE = preprocessed_$(OPENCL_SOURCE)
TARGET = paes
	
all: $(TARGET)

$(TARGET): $(OBJECTS) $(PREPROCESSED_OPENCL_SOURCE)
	$(CC) -o $(TARGET) $(OBJECTS) $(CFLAGS) $(LDFLAGS) 

$(PREPROCESSED_OPENCL_SOURCE): $(OPENCL_SOURCE)
	cpp $(DEFINES) $(OPENCL_SOURCE) $(PREPROCESSED_OPENCL_SOURCE)

clean:
	rm -fr $(TARGET) *.o *.i *.s *~ doc/ $(PREPROCESSED_OPENCL_SOURCE)

install:
 
indent:
	indent -kr -i8 -l300 *.c *.cl *.h

doc:
	doxygen doxygen.cfg
	firefox doc/index.html
