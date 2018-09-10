
INCLUDES = fithi.h
CPPFLAGS = -W -Wall -Wno-unused-parameter -g -DNDEBUG

all: fithi fithe fithp

fithi: fithi.o maini.o
	g++ -o $@ $+

fithe: fithe.o maine.o
	g++ -o $@ $+

fithp: fithe.o plcsim.o
	g++ -o $@ $+

maini.o: main.cc $(INCLUDES)
	g++ $(CPPFLAGS) -DFULLFITH -c -o $@ $<

fithi.o: fithi.cc $(INCLUDES)
	g++ $(CPPFLAGS) -DFULLFITH -c -o $@ $<

maine.o: main.cc $(INCLUDES)
	g++ $(CPPFLAGS) -c -o $@ $<

fithe.o: fithi.cc $(INCLUDES)
	g++ $(CPPFLAGS) -c -o $@ $<

plcsim.o: plcsim.cc $(INCLUDES)
	g++ $(CPPFLAGS) -c -o $@ $<

clean:
	rm -f *.o
