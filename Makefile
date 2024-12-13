main: main.c cmd_shell.c path.c
	gcc -o main main.c cmd_shell.c path.c

clean:
	rm -f main