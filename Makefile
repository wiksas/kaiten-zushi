all:
	gcc main.c -o main
	gcc kucharz.c -o kucharz
	gcc obsluga.c -o obsluga
	gcc kierownik.c -o kierownik
	gcc klient.c -o klient -lpthread