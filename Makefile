CC = gcc
VERSION = 1.2.1
CFLAGS = -g -O2
LIBS = -lcrypto -lssl -lz 
INSTALL=/bin/install -c
prefix=/usr/local
bindir=$(prefix)${exec_prefix}/bin
DESTDIR=
FLAGS=$(CFLAGS) -DPACKAGE_NAME=\"\" -DPACKAGE_TARNAME=\"\" -DPACKAGE_VERSION=\"\" -DPACKAGE_STRING=\"\" -DPACKAGE_BUGREPORT=\"\" -DSTDC_HEADERS=1 -DHAVE_LIBZ=1 -DHAVE_LIBSSL=1 -DHAVE_LIBCRYPTO=1 
OBJ=containerfiles.o outputfiles.o common.o cbsnews.o dailymotion.o  ehow.o  youtube.o servicetypes.o extract_text.o download.o display.o

all: $(OBJ)
	@cd libUseful-2.0; $(MAKE)
	$(CC) $(FLAGS) -o movgrab main.c $(LIBS) $(OBJ) libUseful-2.0/libUseful-2.0.a

clean:
	@rm -f movgrab *.o libUseful-2.0/*.o libUseful-2.0/*.a libUseful-2.0/*.so

containerfiles.o: containerfiles.c containerfiles.h
	$(CC) $(FLAGS) -c containerfiles.c

outputfiles.o: outputfiles.c outputfiles.h
	$(CC) $(FLAGS) -c outputfiles.c

common.o: common.c common.h
	$(CC) $(FLAGS) -c common.c

cbsnews.o: cbsnews.c cbsnews.h
	$(CC) $(FLAGS) -c cbsnews.c

dailymotion.o: dailymotion.c dailymotion.h
	$(CC) $(FLAGS) -c dailymotion.c

ehow.o: ehow.c ehow.h
	$(CC) $(FLAGS) -c ehow.c

youtube.o: youtube.c youtube.h
	$(CC) $(FLAGS) -c youtube.c

servicetypes.o: servicetypes.c servicetypes.h
	$(CC) $(FLAGS) -c servicetypes.c

download.o: download.c download.h
	$(CC) $(FLAGS) -c download.c

display.o: display.c display.h
	$(CC) $(FLAGS) -c display.c

extract_text.o: extract_text.c extract_text.h
	$(CC) $(FLAGS) -c extract_text.c


install:
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) movgrab $(DESTDIR)$(bindir)
