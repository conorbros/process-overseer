CXX       	:= gcc
FLAGS		:= -Wall -Wextra -Werror -Wshadow -Wdouble-promotion -Wpedantic -pthread -g
#FLAGS		:= -Wall -Wextra -Wshadow -Wdouble-promotion -Wpedantic -pthread -g -fsanitize=address
# FLAGS		:= -Wall -Wextra -Wshadow -Wdouble-promotion -Wpedantic -pthread -g -fsanitize=thread -O1
INCLUDE := include

BIN := build
SRC := src

all: overseer controller timer timer2

overseer: ./src/overseer.c ./src/proc_map.c ./src/log.c ./src/thread_pool.c
	$(CXX) $(FLAGS) -I$(INCLUDE) -o ./$(BIN)/overseer ./$(SRC)/overseer.c ./$(SRC)/proc_map.c ./src/log.c ./src/thread_pool.c

controller: ./src/controller.c
	$(CXX) $(FLAGS) -I$(INCLUDE) -o ./$(BIN)/controller ./$(SRC)/controller.c

timer: ./test/timer.c
	$(CXX) $(FLAGS) -I$(INCLUDE) -o ./build/timer ./test/timer.c

timer2: ./test/timer2.c
	$(CXX) $(FLAGS) -o ./build/timer2 ./test/timer2.c

clean:
	-rm $(BIN)/*