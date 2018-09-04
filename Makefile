
INCLUDES = fithi.h
CPPFLAGS = -W -Wall -g -DNDEBUG

all: fithi fithe

fithi: fithi.o maini.o
	g++ -o $@ $+

fithe: fithe.o maine.o
	g++ -o $@ $+

maini.o: main.cc $(INCLUDES)
	g++ $(CPPFLAGS) -DFULLFITH -c -o $@ $<

fithi.o: fithi.cc $(INCLUDES)
	g++ $(CPPFLAGS) -DFULLFITH -c -o $@ $<

maine.o: main.cc $(INCLUDES)
	g++ $(CPPFLAGS) -c -o $@ $<

fithe.o: fithi.cc $(INCLUDES)
	g++ $(CPPFLAGS) -c -o $@ $<

clean:
	rm -f *.o
