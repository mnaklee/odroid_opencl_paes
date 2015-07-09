
INSTALL_DIR = $(CURDIR)/bin
LIBS_DIR = $(CURDIR)/libs

.PHONY: all paes smartmeter clean install


all: smartmeter install paes 


paes:
	export TARGETDIR
	cd paes && make all

smartmeter:
	cd smartmeter && make all

install:
	cd smartmeter && make install-dir=$(LIBS_DIR) install

clean:
	cd paes && make clean
	cd smartmeter && make clean
	rm -f libs/*.a

