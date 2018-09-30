
INCLUDES = fithi.h fithfile.h crc.h
CPPFLAGS = -W -Wall -Wno-unused-parameter -Os -DNDEBUG
# CPPFLAGS = -W -Wall -Wno-unused-parameter -DNDEBUG -g

all: fithi fithe fithp crctest

crctest: crc.o crctest.o
	g++ -o $@ $+

fithi: fithf.o mainf.o fithfile.o crc.o
	g++ -o $@ $+

fithe: fithi.o main.o fithfile.o crc.o
	g++ -o $@ $+

fithp: fithi.o plcsim.o
	g++ -o $@ $+

mainf.o: main.cc $(INCLUDES)
	g++ $(CPPFLAGS) -DFULLFITH -c -o $@ $<

fithf.o: fithi.cc $(INCLUDES)
	g++ $(CPPFLAGS) -DFULLFITH -c -o $@ $<

%.o: %.c $(INCLUDES)
	g++ $(CPPFLAGS) -c -o $@ $<

clean:
	rm -f *.o

clobber:
	rm -f fithi fithe fithp crctest
