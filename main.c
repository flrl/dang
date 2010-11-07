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

#include "assembler.h"
#include "vm.h"

static const char *options = "acho:x";

static int  opt_is_assembly    = 0;
static int  opt_compile_only   = 0;
static int  opt_is_bytecode    = 0;
static char *output_file_name  = NULL;
static char *program_file_name = NULL;
static assembler_output_t *program = NULL; // FIXME("make program a useful type\n");

static void usage(void);
static char *make_filename(const char *, const char *, const char *);
static int read_bytecode_file(const char *, assembler_output_t **);
static int write_bytecode_file(const char *, const assembler_output_t *);

int main(int argc, char *const argv[]) {
    int c, status=0;

    while ((c = getopt(argc, argv, options)) != -1) {
        switch (c) {
            case 'a':
                opt_is_assembly = 1;
                opt_is_bytecode = 0;
                break;
            case 'c':
                opt_compile_only = 1;
                break;
            case 'h':
                usage();
                return 0;
            case 'o':
                output_file_name = strdup(optarg);
                break;
            case 'x':
                opt_is_assembly = 0;
                opt_is_bytecode = 1;
                break;
            default:
                usage();
                return 1;
        }
    }
    argc -= optind;
    argv += optind;
    
    if (argc) {
        if (strcmp(argv[0], "-") != 0)  program_file_name = strdup(argv[0]);
    }
    
    if (opt_is_assembly) {
            program = assemble(program_file_name);  // reads stdin if filename is NULL
    }
    else if (opt_is_bytecode) {
        if (opt_compile_only == 0) {
            status = read_bytecode_file(program_file_name, &program);   // reads stdin if filename is NULL
        }
        else {
            FIXME("some meaningful error message when -c and -x are specified together\n");
            status = EINVAL;
        }
    }
    else {
        FIXME("parse and compile the language file\n");
        program = NULL;
    }

    if (program) {
        if (opt_compile_only) {
            if (output_file_name != NULL) {
                if (strcmp(output_file_name, "-") == 0) {
                    free(output_file_name);
                    output_file_name = NULL;
                }
            }
            else {
                output_file_name = make_filename(program_file_name, ".dang", ".dong");
            }
            status = write_bytecode_file(output_file_name, program);  // writes to stdout if filename is NULL
        }
        else {
            status = vm_main(program->m_bytecode, program->m_bytecode_length, program->m_bytecode_start);
        }
        
        free(program);
    }
    else {
        FIXME("print some error if there's no program to execute\n");
    }

    if (program_file_name)  free(program_file_name);
    if (output_file_name)   free(output_file_name);
    
    return status;
}

static void usage(void) {
/***
Usage:
    dang [options] programfile [arguments]

Options:
    -a      Treat programfile as assembly language. 

    -c      Compile programfile to bytecode, but do not execute it.  
    
            By default, the compiled bytecode is written to a file called
            \"programfile.dong\", where the \".dong\" extension replaces the
            \".dang\" extension, if any.
            
            To specify an alternative output file name, see the -o option.

    -h      Display this help message.

    -o      Specifies the output filename to be used by the -c option.

    -x      Treat programfile as compiled bytecode input (as produced by the
            -c option).
***/
    const char *usage =
"Usage:\n"
"    dang [options] programfile [arguments]\n"
"\n"
"Options:\n"
"    -a      Treat programfile as assembly language. \n"
"\n"
"    -c      Compile programfile to bytecode, but do not execute it.  \n"
"    \n"
"            By default, the compiled bytecode is written to a file called\n"
"            \"programfile.dong\", where the \".dong\" extension replaces the\n"
"            \".dang\" extension, if any.\n"
"            \n"
"            To specify an alternative output file name, see the -o option.\n"
"\n"
"    -h      Display this help message.\n"
"\n"
"    -o      Specifies the output filename to be used by the -c option.\n"
"\n"
"    -x      Treat programfile as compiled bytecode input (as produced by the\n"
"            -c option).\n"
    ;
    fputs(usage, stderr);
}

static char *make_filename(const char *orig, const char *orig_ext, const char *new_ext) {
    char *filename = NULL;
    int pos;
    
    assert(orig_ext != NULL);
    assert(new_ext != NULL);
    
    if (orig == NULL) {
        filename = malloc(2 + strlen(new_ext));
        strcpy(filename, "a");
        strcat(filename, new_ext);
        return filename;
    }
    else {    
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
}

static int read_bytecode_file(const char *filename, assembler_output_t **result) {
    int status = 0;
    
    FILE *in = (filename != NULL ? fopen(filename, "rb") : stdin);
    if (in != NULL) {
        char check[5] = {0};
        fread(&check, 4, 1, in);
        assert(strcmp(check, "dong") == 0);
        
        assembler_output_t header = {0};
        fread(&header, sizeof(header), 1, in);
        
        assembler_output_t *program = malloc(sizeof(*program) + (sizeof(*(*program).m_bytecode) * header.m_bytecode_length));
        if (program != NULL) {
            memcpy(program, &header, sizeof(*program));
            fread(program->m_bytecode, header.m_bytecode_length, 1, in);
            fclose(in);
            *result = program;
        }
        else {
            status = errno;
            perror("some error message here");
        }
    }
    else {
        status = errno;
        perror(filename);
    }
    
    return status;
}


static int write_bytecode_file(const char *filename, const assembler_output_t *program) {
    int status = 0;
    
    FILE *out = (filename != NULL ? fopen(output_file_name, "wb") : stdout);
    if (out != NULL) {
        fwrite("dong", 4, 1, out);
        fwrite(program, sizeof(*program) + program->m_bytecode_length, 1, out);
        fclose(out);
    }
    else {
        status = errno;
        perror(output_file_name);
    }

    return status;
}
