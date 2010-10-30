/*
 *  main.c
 *  dang
 *
 *  Created by Ellie on 30/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */
 
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int assemble(uint8_t **bytecode, size_t *bytecode_len);


const char *options = "a:";

int asm_flag = 0;
char *asm_file = NULL;

void usage(void);

int main(int argc, char *const argv[]) {
    int c;

    while ((c = getopt(argc, argv, options)) != -1) {
        switch (c) {
            case 'a':
                asm_flag = 1;
                asm_file = strdup(optarg);
                break;
            default:
                usage();
                return 1;
        }
    }
    argc -= optind;
    argv += optind;

    if (asm_flag && asm_file != NULL) {
        freopen(asm_file, "r", stdin);

        uint8_t *bytecode;
        size_t bytecode_len;
        if (0 == assemble(&bytecode, &bytecode_len)) {
            char *outfilename = malloc(strlen(asm_file) + 6);
            strcpy(outfilename, asm_file);
            strcat(outfilename, ".dong");
            FILE *output = fopen(outfilename, "wb");
            write(fileno(output), bytecode, bytecode_len);
            fclose(output);
        }
        else {
            fprintf(stderr, "error assembling file %s\n", asm_file);
            return 2;
        }
    }
    
    return 0;
}

void usage(void) {
    fprintf(stderr, "%s", "Usage:  dang [-a file]\n");
}
