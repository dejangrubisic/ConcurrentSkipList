CC=gcc
#CFLAGS=-g -fopenmp #-c -Wall
#LDFLAGS=
#SOURCES=test.c cskiplist.c
#OBJECTS=$(SOURCES:.cpp=.o)
#EXECUTABLE=test_csl
#
#all: $(SOURCES) $(EXECUTABLE)
#
#$(EXECUTABLE): $(OBJECTS)
#	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
#
#.cpp.o:
#	$(CC) $(CFLAGS) $< -o $@

#clean:
#	/bin/rm -rf cskiplist


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
	$(CC) $(DEBUG) $(OMP) -o $(EXEC)-debug $(SOURCES) -lrt

# build the serial version of the program
$(EXEC)-serial: $(EXEC).c
	$(CC) $(OPT) -o $(EXEC)-serial $(SOURCES) -lrt

# build the optimized parallel version of the program
$(EXEC): $(EXEC).c
	$(CC) $(OPT) $(OMP) -o $(EXEC) $(SOURCES) -lrt
	set env $OMP_PROC_BIND=true

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
