##
##  Makefile for PMSat: a parallel version of MiniSat
##

CSRCS     = $(wildcard *.C)
CHDRS     = $(wildcard *.h)
COBJS     = $(addsuffix .o, $(basename $(CSRCS)))

EXEC      = pmsat

#you may need to change this directory:

INCLUDE_DIR = /opt/mpich/include


CXX       = g++
MPICC     = mpicxx
CFLAGS    = -Wall -ffloat-store 
COPTIMIZE = -O3


.PHONY : build clean depend

CFLAGS+=$(COPTIMIZE) 



mpi:    build $(EXEC)


build:
	@echo Building $(EXEC)


## Build rule

%.o %.op %.od %.or:	%.C
	@echo Compiling: $<
	@$(CXX) $(CFLAGS) -c -o $@ $< -I $(INCLUDE_DIR)

## Linking rules (standard/profile/debug/release)
$(EXEC): $(COBJS)
	@echo Linking $@
	@$(MPICC) $(COBJS) -x c++ $(CFLAGS) -lz -o $@   

clean:
	@rm -f $(EXEC) $(COBJS) depend.mak


## Make dependencies
depend:	depend.mak
depend.mak: $(CSRCS) $(CHDRS)
	@echo Making dependencies ...
	@$(CXX) -MM $(CSRCS) > depend.mak
	@cp depend.mak /tmp/depend.mak.tmp
	@sed "s/o:/op:/" /tmp/depend.mak.tmp >> depend.mak
	@sed "s/o:/od:/" /tmp/depend.mak.tmp >> depend.mak
	@sed "s/o:/or:/" /tmp/depend.mak.tmp >> depend.mak
	@rm /tmp/depend.mak.tmp

include depend.mak
