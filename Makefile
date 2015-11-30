all : particle.exe

particle.exe : particle.c
	gcc -o particle.exe -mwindows particle.c -lgdi32
