CC=gcc
CFLAGS= -Wint-conversion -Wall
EXEC=test
SOURCES=test.c cskiplist.c
OBJ =  $(EXEC) $(EXEC)-debug #$(EXEC)-serial

W :=`grep processor /proc/cpuinfo | wc -l`

# flags
OPT=-O3 -g
DEBUG=-O0 -g
OMP=-fopenmp

all: $(SOURCES) $(OBJ)

# build the debug parallel version of the program
$(EXEC)-debug: $(EXEC).c
	$(CC) $(CFLAGS) $(DEBUG) $(OMP) -o $(EXEC)-debug $(SOURCES) -lrt
	cp $@ bin/

# build the serial version of the program
$(EXEC)-serial: $(EXEC).c
	$(CC) $(CFLAGS) $(OPT) -o $(EXEC)-serial $(SOURCES) -lrt
	cp $@ bin/
# build the optimized parallel version of the program
$(EXEC): $(EXEC).c
	$(CC) $(CFLAGS) $(OPT) $(OMP) -o $(EXEC) $(SOURCES) -lrt
	set env $OMP_PROC_BIND=true
	cp $@ bin/

#run the optimized program in parallel
runp: $(EXEC)
	@echo use make runp W=nworkers
	./$(EXEC)

#run the serial version of your program
#runs: $(EXEC)-serial
#	@echo use make runs
#	./$(EXEC)-serial


clean:
	/bin/rm -rf $(OBJ) check
