CXX       	:= gcc
FLAGS		:= -pthread -g

all: overseer controller

overseer: ./overseer.c ./proc_map.c ./log.c ./thread_pool.c
	$(CXX) $(FLAGS) -o ./overseer ./overseer.c ./proc_map.c ./log.c ./thread_pool.c

controller: ./controller.c
	$(CXX) $(FLAGS) -o ./controller ./controller.c

clean:
	-rm controller overseer