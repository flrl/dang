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

#include "assembler.h"

const char *options = "aho:";

int  opt_asm_flag = 0;
char *opt_out_file = NULL;
char *opt_in_file = NULL;

void usage(void);
char *make_filename(const char *, const char *, const char *);

int main(int argc, char *const argv[]) {
    int c;

    while ((c = getopt(argc, argv, options)) != -1) {
        switch (c) {
            case 'a':
                opt_asm_flag = 1;
                break;
            case 'h':
                usage();
                return 0;
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
    
    if (argc) {
        opt_in_file = strdup(argv[0]);
    }
    else {
        opt_in_file = strdup("-");
    }

    if (opt_asm_flag && opt_in_file != NULL) {
        if (strcmp(opt_in_file, "-") != 0)  freopen(opt_in_file, "r", stdin);

        assembler_output_t *assembly;
        if ((assembly = assemble()) != NULL) {
            if (opt_out_file == NULL)  opt_out_file = make_filename(opt_in_file, ".dang", ".dong");
            FILE *output = fopen(opt_out_file, "wb");
            write(fileno(output), "dong", 4);
            write(fileno(output), assembly, sizeof(*assembly) + assembly->m_bytecode_length);
            fclose(output);
        }
        else {
            fprintf(stderr, "error assembling file %s\n", opt_in_file);
            return 2;
        }
    }
    
    if (opt_in_file)    free(opt_in_file);
    if (opt_out_file)   free(opt_out_file);
    
    return 0;
}

void usage(void) {
    const char *usage =
"Usage:\n"
"    dang [options] infile\n"
"\n"
"Options:\n"
"    -a      Treat infile as assembly language input, and compile it into a bytecode file.\n"
"            If no output filename is specified, one is constructed by either replacing a\n"
"            \".dang\" extension on the input file with \".dong\", or if no \".dang\" extension\n"
"            exists, by simply appending a \".dong\" extension.\n"
"            If the input filename is not specified, or is the special value \"-\", input\n"
"            is read from the standard input channel, and the default output filename is\n"
"            \"a.dong\".\n"
"\n"
"    -h      Displays this help message\n"
"\n"
"    -o outfile\n"
"            Specifies the output filename to be used by the -a, ... options.\n"
"\n"
    ;
    fputs(usage, stderr);
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
