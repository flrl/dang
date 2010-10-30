/*
 *  main.c
 *  dang
 *
 *  Created by Ellie on 30/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */
 
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int assemble(uint8_t **bytecode, size_t *bytecode_len);


const char *options = "a:o:";

int  opt_asm_flag = 0;
char *opt_asm_file = NULL;
char *opt_out_file = NULL;

void usage(void);
char *make_filename(const char *, const char *, const char *);

int main(int argc, char *const argv[]) {
    int c;

    while ((c = getopt(argc, argv, options)) != -1) {
        switch (c) {
            case 'a':
                opt_asm_flag = 1;
                opt_asm_file = strdup(optarg);
                break;
            case 'o':
                opt_out_file = strdup(optarg);
                break;
            default:
                usage();
                return 1;
        }
    }
    argc -= optind;
    argv += optind;

    if (opt_asm_flag && opt_asm_file != NULL) {
        if (strcmp(opt_asm_file, "-") != 0)  freopen(opt_asm_file, "r", stdin);

        uint8_t *bytecode;
        size_t bytecode_len;
        if (0 == assemble(&bytecode, &bytecode_len)) {
            if (opt_out_file == NULL)  opt_out_file = make_filename(opt_asm_file, ".dang", ".dong");
            FILE *output = fopen(opt_out_file, "wb");
            write(fileno(output), bytecode, bytecode_len);
            fclose(output);
        }
        else {
            fprintf(stderr, "error assembling file %s\n", opt_asm_file);
            return 2;
        }
    }
    
    if (opt_asm_file)  free(opt_asm_file);
    if (opt_out_file)  free(opt_out_file);
    
    return 0;
}

void usage(void) {
    fprintf(stderr, "%s", "Usage:  dang [-a file]\n");
}

char *make_filename(const char *orig, const char *orig_ext, const char *new_ext) {
    char *filename = NULL;
    int pos;
    
    assert(orig_ext != NULL);
    assert(new_ext != NULL);
    
    if (strcmp(orig, "-") == 0) {
        filename = malloc(2 + strlen(new_ext));
        strcpy(filename, "a");
        strcat(filename, new_ext);
        return filename;
    }
    
    if ((pos = strlen(orig) - strlen(orig_ext)) > 0) {
        if (strcmp(&orig[pos], orig_ext) == 0) {
            // extension found
            filename = strdup(orig);
            filename[pos] = '\0';
            strncat(filename, new_ext, strlen(orig_ext));
            return filename;
        }
    }

    // no extension found
    filename = malloc(strlen(orig) + strlen(new_ext) + 1);
    strcpy(filename, orig);
    strcat(filename, new_ext);
    
    return filename;
}
