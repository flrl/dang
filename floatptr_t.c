/*
 *  floatptr_t.c
 *  dang
 *
 *  Created by Ellie on 14/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <stdint.h>
#include <stdio.h>

int main(int argc, char **argv) {
    const char *t = (sizeof(long double) <= sizeof(intptr_t) ? "long double" :
               sizeof(double) <= sizeof(intptr_t) ? "double" :
               sizeof(float) <= sizeof(intptr_t) ? "float" : "float /*fallback*/");
    puts("#ifndef FLOATPTR_T");
    puts("#define FLOATPTR_T");
    printf("typedef %s floatptr_t\n", t);
    puts("#endif");
    return 0;
}