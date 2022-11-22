
lab2:
	g++ lamport.cpp -o task2.bin -Wall -Wpedantic -g -fsanitize=address

clean:
	rm -f ./task2.bin

run: lab2
	./task2.bin

format:
	clang-format -i lamport.cpp
