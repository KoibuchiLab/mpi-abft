#Makefile for atlasctl

CC=mpicc
GCCVERSION:=$(shell $(CC) -dumpversion)
dot:=.
empty:=
space:=$(empty) $(empty)
GCCVERSION:=$(subst $(dot),$(space), $(GCCVERSION))
GCCMAJOR:=$(word 1, $(GCCVERSION))
GCCMINOR:=$(word 2, $(GCCVERSION))

ifeq (1,$(shell expr $(GCCMAJOR) \>= 4))
  OPT=-O2 -g
  CFLAGS=$(OPT) -Wall -W -Wno-pointer-sign -Wno-parentheses -Wstrict-prototypes -Werror -pthread
else
  OPT=-O2 -g
  CFLAGS=$(OPT) -Wall -W -Wno-parentheses -Wstrict-prototypes -Werror -pthread
endif

LIBS= -lm -L/usr/local/lib -LLib_xbar/

PREFIX=.

atlasctl: atlasctl.c ./Lib_xbar/Xbar_SR.c 
	$(CC) $(CFLAGS) -o atlasctl atlasctl.c  ./Lib_xbar/Xbar_SR.c $(LIBS) 

clean:
	rm -f atlasctl 
