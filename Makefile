# Makefile, versao 3
# Sistemas Operativos, DEI/IST/ULisboa 2016-17

# CFLAGS=-g -Wall -pedantic
CFLAGS=gcc -g -Wall -pedantic -pthread -std=gnu99

# To compile both programs
all: i-banco i-banco-terminal



# To compile i-banco
i-banco: i-banco.o contas.o
	$(CFLAGS) -o i-banco i-banco.o contas.o

i-banco.o: i-banco.c i-banco.h contas.h
	$(CFLAGS) -c i-banco.c

contas.o: contas.c contas.h
	$(CFLAGS) -c contas.c



# To compile i-banco-terminal

i-banco-terminal: i-banco-terminal.o commandlinereader.o
	$(CFLAGS) -o i-banco-terminal i-banco-terminal.o commandlinereader.o

i-banco-terminal.o: i-banco-terminal.c i-banco.h commandlinereader.h
	$(CFLAGS) -c i-banco-terminal.c
	
commandlinereader.o: commandlinereader.c commandlinereader.h
	$(CFLAGS) -c commandlinereader.c



# Utilities
clean:
	rm -f *.o i-banco i-banco-terminal i-banco-pipe i-banco-terminal-* *.txt

zip:
	zip P4_franciscobarros85069_mafaldagaspar77921_grupo83_2016 *.c *.h *.txt Makefile