LIB=-lpthread
CFLAGS= -Wall -O
OBJS=route_main.o route_manager.o 

route_module:$(OBJS)
	g++ $(CFLAGS) $(LIB) $(OBJS) -o route_module

route_main.o:route_main.cpp
	g++ $(CFLAGS) $(LIB) -c route_main.cpp -o route_main.o

route_manager.o:route_manager.cpp
	g++ $(CFLAGS) $(LIB) -c route_manager.cpp -o route_manager.o 

clean:
	rm -rf *.o route_module

