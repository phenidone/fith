
INCLUDES = fithi.h
CPPFLAGS = -W -Wall -g -DFULLFITH 

fithi: fithi.o main.o
	g++ -o $@ $+

%.o: %.cc $(INCLUDES)
	g++ $(CPPFLAGS) -c -o $@ $<

clean:
	rm -f *.o
