LIB=-lpthread -ljson_linux-gcc-4.4.7_libmt
CFLAGS= -Wall -O
LIBPATH=-L/usr/local/lib64
OBJS=route_main.o route_manager.o 

route_module:$(OBJS)
	g++ $(CFLAGS) $(LIBPATH) $(LIB) $(OBJS) -o route_module

route_main.o:route_main.cpp
	g++ $(CFLAGS) $(LIBPATH) $(LIB) -c route_main.cpp -o route_main.o

route_manager.o:route_manager.cpp
	g++ $(CFLAGS) $(LIBPATH) $(LIB) -c route_manager.cpp -o route_manager.o 

common.o:common.cpp
	g++ $(CFLAGS) $(LIBPATH) $(LIB) -c common.cpp -o common.o 

clean:
	rm -rf *.o route_module

