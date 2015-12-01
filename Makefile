all : particle.exe

particle.exe : particle.c
	gcc -Wall -Werror -o particle.exe -mwindows particle.c -lgdi32

clean :
	rm particle.exe
