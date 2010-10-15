#!/bin/sh
#
# make_floatptr_h.sh
# dang
# 
# Created by Ellie on 15/10/10.
# Copyright 2010 Ellie. All rights reserved.
#

NOW=`date +%F_%T`
$CC -o $NOW-make_floatptr_h.out -xc - <<'END'
#include <stdint.h>
#include <stdio.h>

int main(int argc, char **argv) {
    const char *t = (sizeof(long double) <= sizeof(intptr_t) ? "long double" :
               sizeof(double) <= sizeof(intptr_t) ? "double" :
               sizeof(float) <= sizeof(intptr_t) ? "float" : "float /*fallback*/");
    puts("#ifndef FLOATPTR_T");
    puts("#define FLOATPTR_T");
    printf("typedef %s floatptr_t;\n", t);
    puts("#endif");
    return 0;
}
END
test 0 -eq $? && 
./$NOW-make_floatptr_h.out > floatptr_t.h &&
rm ./$NOW-make_floatptr_h.out
