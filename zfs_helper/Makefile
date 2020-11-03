IDIR =.
CC=g++
CFLAGS=-Wall -fexceptions -I$(IDIR)
PREPROCESSOROPTS=
#PREPROCESSOROPTS=-DSQLITE_HAS_CODEC

ODIR=obj
BIN=bin

LIBS=

#_DEPS = CSqlcipherDB.h EncryptDecrypt.h
#DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = stringtools.o main.o 
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

VPATH=%.cpp 
#SRCFILES = main.cpp src/CSqlcipherDB.cpp src/EncryptDecrypt.cpp

$(ODIR)/%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(PREPROCESSOROPTS)

$(BIN)/zfs_helper: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ 
