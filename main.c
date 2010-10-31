/*
 *  main.c
 *  dang
 *
 *  Created by Ellie on 30/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */
 
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "array.h"
#include "assembler.h"
#include "channel.h"
#include "hash.h"
#include "scalar.h"
#include "vm.h"

const char *options = "aho:x";

int  opt_asm_flag = 0;
int  opt_exe_flag = 0;
char *opt_out_file = NULL;
char *opt_in_file = NULL;

void usage(void);
char *make_filename(const char *, const char *, const char *);

int main(int argc, char *const argv[]) {
    int c, status=0;

    while ((c = getopt(argc, argv, options)) != -1) {
        switch (c) {
            case 'a':
                opt_asm_flag = 1;
                opt_exe_flag = 0;
                break;
            case 'h':
                usage();
                return 0;
            case 'o':
                opt_out_file = strdup(optarg);
                break;
            case 'x':
                opt_exe_flag = 1;
                opt_asm_flag = 0;
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

    if (opt_asm_flag) {
        if (opt_in_file != NULL) {
            assembler_output_t *data;
            if ((data = assemble(opt_in_file)) != NULL) {
                if (opt_out_file == NULL)  opt_out_file = make_filename(opt_in_file, ".dang", ".dong");
                FILE *out = fopen(opt_out_file, "wb");
                if (out != NULL) {
                    fwrite("dong", 4, 1, out);
                    fwrite(data, sizeof(*data) + data->m_bytecode_length, 1, out);
                    fclose(out);
                }
                else {
                    status = errno;
                    perror(opt_out_file);
                }
                free(data);
            }
            else {
                fprintf(stderr, "error assembling file `%s'\n", opt_in_file);
                status = 1;
            }
        }
        else {
            fprintf(stderr, "no input file specified\n");
            status = 1;
        }
    }

    if (opt_exe_flag) {
        if (opt_in_file != NULL) {
            FILE *in = fopen(opt_in_file, "rb");
            if (in != NULL) {
                char check[5] = {0};
                fread(&check, 4, 1, in);
                assert(strcmp(check, "dong") == 0);

                assembler_output_t header;
                fread(&header, sizeof(header), 1, in);

                uint8_t *bytecode = malloc(sizeof(*bytecode) * header.m_bytecode_length);            
                fread(bytecode, header.m_bytecode_length, 1, in);

                fclose(in);
                
                scalar_pool_init();
                array_pool_init();
                hash_pool_init();
                channel_pool_init();
                
                vm_context_t context;
                vm_context_init(&context, bytecode, header.m_bytecode_length, header.m_bytecode_start);
                vm_execute(&context);
                vm_context_destroy(&context);

                symboltable_garbage_collect();
                
                channel_pool_destroy();
                hash_pool_destroy();
                array_pool_destroy();
                scalar_pool_destroy();
                
                free(bytecode);
            }
            else {
                status = errno;
                perror(opt_in_file);
            }
        }
        else {
            fprintf(stderr, "no input file specified\n");
            status = 1;
        }
    }
    
    if (opt_in_file)    free(opt_in_file);
    if (opt_out_file)   free(opt_out_file);
    
    return status;
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
"    -x      Treat infile as compiled bytecode as produced by -a, and execute it\n"
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
