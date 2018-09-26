# Version original para Phi 
LOG = -D_DEBUG0
CFLAGS = -O3 -std=c++11
IO = -D_IO_TIME
BH = -D_BH

EXE = nbody_pthread
OUTPUT = $(EXE) *_log
v = bh

all: clean p

p:
	$(eval TARGET := nbody_pthread)
	icc -mmic src/nbody-pthread.cpp src/utils.cpp src/IGP_library.c -o $(TARGET) -lpthread $(LOG) $(CFLAGS)

clean:
	-@rm $(OUTPUT) 2>/dev/null || true
