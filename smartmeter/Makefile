all: lib

CFLAGS   ?= -Wall -g -fpic
CXXFLAGS ?= -Wall -g -fpic -std=c++11
LDFLAGS  ?= -Wall -g

COBJS     = hid.o 
CPPOBJS   = smartmeter.o
OBJS      = $(COBJS) $(CPPOBJS)
LIBS_USB  = `pkg-config libusb-1.0 --libs` -lrt -lpthread
LIBS      = $(LIBS_USB)

TARGET    = libsmartmeter.a

$(COBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $(INCLUDES) $< -o $@

$(CPPOBJS): %.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $(INCLUDES) $< -o $@

lib: $(CPPOBJS) $(COBJS)
	ar rvs $(TARGET) smartmeter.o hid.o
	ranlib $(TARGET)

install: 
	cp $(TARGET) $(install-dir)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: clean lib linstall
