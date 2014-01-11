###############################################################################
#
# Makefile for ex2 - OS
#
# Student:
# erana06 - Eran Amar, ID 301786067 , eran.amar@mail.huji.ac.il 
#
###############################################################################

COMPILE=g++
CFLAGS=-Wall 

TAR=tar
TARFLAGS=cvf
TARNAME=ex2.tar
TARSRCS= uthreads.cpp uthreads.hh Makefile uthreads_helpers.cpp uthreads_helpers.hh

REMOVE=rm
RMFLAGS=-f

AR=ar
ARFLAGS=rcu
UTHREADSLIB=libuthreads.a
RANLIB=ranlib
    
all: $(UTHREADSLIB)	

$(UTHREADSLIB): uthreads_helpers.o uthreads.o
	$(AR) $(ARFLAGS) $(UTHREADSLIB) uthreads.o uthreads_helpers.o 
	$(RANLIB) $(UTHREADSLIB)
		
uthreads_helpers.o: uthreads.hh uthreads_helpers.hh uthreads_helpers.cpp
	$(COMPILE) $(CFLAGS) -c uthreads_helpers.cpp -o uthreads_helpers.o	
	
uthreads.o: uthreads.hh uthreads_helpers.hh uthreads.cpp
	$(COMPILE) $(CFLAGS) -c uthreads.cpp -o uthreads.o
		
tar:
	$(REMOVE) $(RMFLAGS) $(TARNAME)
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)
	
clean:
	$(REMOVE) $(RMFLAGS) *.o *~ *core $(TARNAME) $(UTHREADSLIB) 


