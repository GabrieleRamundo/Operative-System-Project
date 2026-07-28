FLAGS= -Wvla -Wextra -Werror -g 

all: bin/director bin/worker bin/user bin/ticket_dispenser bin/add_users bin/common_functions.o

bin/director: bin/director.o bin/runtime.o	
	gcc $(FLAGS) bin/director.o bin/runtime.o -o bin/director 
bin/worker: bin/common_functions.o bin/worker.o
	gcc $(FLAGS) bin/worker.o bin/common_functions.o -o bin/worker
bin/worker.o: src/worker.c
	gcc $(FLAGS) -c src/worker.c -o bin/worker.o
bin/user.o: src/user.c
	gcc $(FLAGS) -c src/user.c -o bin/user.o
bin/user: bin/common_functions.o bin/user.o
	gcc $(FLAGS) bin/user.o bin/common_functions.o -o bin/user
bin/ticket_dispenser.o: src/ticket_dispenser.c
	gcc $(FLAGS) -c src/ticket_dispenser.c -o bin/ticket_dispenser.o
bin/ticket_dispenser: bin/common_functions.o bin/ticket_dispenser.o
	gcc $(FLAGS) bin/ticket_dispenser.o bin/common_functions.o -o bin/ticket_dispenser

bin/add_users: src/add_users.c
	gcc $(FLAGS) src/add_users.c -o bin/add_users
bin/runtime.o: src/runtime.c
	gcc $(FLAGS) -c src/runtime.c -o bin/runtime.o
bin/director.o: src/director.c
	mkdir -p bin
	gcc $(FLAGS) -c src/director.c -o bin/director.o
bin/common_functions.o: src/common_functions.c
	mkdir -p bin
	gcc $(FLAGS) -c src/common_functions.c -o bin/common_functions.o
add_users: bin/add_users
	./bin/add_users 4
code:
	code src/director.c src/worker.c src/user.c src/ticket_dispenser.c src/add_users.c src/runtime.c src/common_functions.c
run: bin/director bin/worker bin/user bin/ticket_dispenser bin/add_users
	mkdir -p data/tmp
	./bin/director data/config_timeout.conf
explode: bin/director bin/worker bin/user bin/ticket_dispenser bin/add_users
	mkdir -p data/tmp
	./bin/director data/config_explode.conf
gdb: bin/director bin/worker bin/user bin/ticket_dispenser bin/add_users
	mkdir -p data/tmp
	gdb ./bin/director 
valgrind: bin/director bin/worker bin/user bin/ticket_dispenser bin/add_users
	mkdir -p data/tmp
	valgrind --leak-check=full --track-origins=yes -s ./bin/director data/config_timeout.conf 
clean:
	rm -rf ./bin ./data/tmp
	ipcrm -a