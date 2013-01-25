# makefile for OpenCV-AWK

AWK_H_DIR = ../gawk-4.0.1
CC = gcc
CFLAGS = -Wall -fPIC -shared -g -c -O2 -DHAVE_STRING_H -DHAVE_SNPRINTF -DHAVE_STDARG_H -DHAVE_VPRINTF -DDYNAMIC -I/usr/include/opencv -I${AWK_H_DIR}
LDFLAGS = -shared -lopencv_highgui -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_objdetect

all: OpenCV-AWK.so
	gawk 'BEGIN{ extension("./OpenCV-AWK.so", "dlload") }'
	cp OpenCV-AWK.so sample/

OpenCV-AWK.so: OpenCV-AWK.c makefile
	${CC} $< ${CFLAGS}
	${CC} OpenCV-AWK.o -o $@ ${LDFLAGS}
