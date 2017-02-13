#!/bin/sh

PROG=do_test

SRCS="testenv.cc stairs.cc"

CXXFLAGS="-I/usr/include -D_FILE_OFFSET_BITS=64 -O2 -DIS_TEST -Wall -Werror"

die()
{
	echo "!!! There are compilation errors."
	exit 1
}

OBJS=`echo $SRCS | sed -e "s|\.c[c]*|\.o|g"`

rm -f $OBJS

for src in $SRCS; do
	cc $CXXFLAGS -c $src || die
done

cc $CXXFLAGS stairs.o testenv.o -o $PROG || die

./$PROG

