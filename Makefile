main: main.c cmd_shell.c
	gcc -o main main.c cmd_shell.c

clean:
	rm -f main