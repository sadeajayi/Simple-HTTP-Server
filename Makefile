CXX = g++ -fPIC -g
NETLIBS= -lnsl

all: myhttpd daytime-server daytime-client use-dlopen hello.so

daytime-client : daytime-client.o
	$(CXX) -o $@ $@.o $(NETLIBS)

daytime-server : daytime-server.o
	$(CXX) -o $@ $@.o $(NETLIBS)


myhttpd : myhttpd.o
	$(CXX) -o $@ $@.o $(NETLIBS) -lpthread

use-dlopen: use-dlopen.o
	$(CXX) -o $@ $@.o $(NETLIBS) -ldl

hello.so: hello.o
	ld -G -o hello.so hello.o

%.o: %.cc
	@echo 'Building $@ from $<'
	$(CXX) -o $@ -c -I. $<

clean:
	rm -f *.o use-dlopen hello.so
	rm -f *.o myhttpd daytime-server daytime-client

