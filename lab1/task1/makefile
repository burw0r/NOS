
lab1:
	gcc cigarette_smokers_problem.c -o task1.bin -Wall -Wpedantic -g -fsanitize=address

clean:
	rm -f ./task1.bin

run: lab1
	./task1.bin

format:
	clang-format -i cigarette_smokers_problem.c
