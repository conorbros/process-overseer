CXX       	:= gcc
#FLAGS		:= -Wall -Wextra -Werror -Wshadow -Wdouble-promotion -Wpedantic -pthread
FLAGS		:= -Wall -Wextra -Wshadow -Wdouble-promotion -Wpedantic -pthread -g


BIN := build
SRC := src

all: overseer controller timer timer2

overseer: ./src/overseer.c
	$(CXX) $(FLAGS) -o ./$(BIN)/overseer ./$(SRC)/overseer.c

controller: ./src/controller.c
	$(CXX) $(FLAGS) -o ./$(BIN)/controller ./$(SRC)/controller.c

timer: ./test/timer.c
	$(CXX) $(FLAGS) -o ./build/timer ./test/timer.c

timer2: ./test/timer2.c
	$(CXX) $(FLAGS) -o ./build/timer2 ./test/timer2.c

clean:
	-rm $(BIN)/*