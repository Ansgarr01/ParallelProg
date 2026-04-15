###############################################################################
# Makefile for assignment 2, Parallel and Distributed Computing.
###############################################################################

CC = mpicc
CFLAGS = -std=c99 -g -O3
LIBS = -lm

BIN = stencil parallel_stensil

all: $(BIN)

stencil: stencil.c stencil.h
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

parallel_stensil: parallel_stensil.c stencil.h
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	$(RM) $(BIN)
