/*
 *  assembler.y
 *  dang
 *
 *  Created by Ellie on 29/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

%{
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assembler.h"
#include "bytecode.h"
#include "debug.h"
#include "floatptr_t.h"
#include "util.h"
#include "vmtypes.h"

    static int yylex(void);
    static void yyerror(const char *);
    
    static char *read_identifier(void);
    static char *read_quoted(int);
    static int read_hex_byte(void);
    static int read_octal_byte(void);
    static int read_hex_value(void);
    static int read_octal_value(void);
    
    typedef struct line_t line_t;
    typedef struct param_t param_t;
    typedef struct label_t label_t;
    typedef struct label_ref_t label_ref_t;
    
    static label_t *get_label(const char *, const char *);
    static line_t *get_line(label_t *, uintptr_t, param_t *);
    static void append_param(param_t *, param_t *);

    static FILE     *g_input = NULL;
    static line_t   *g_lines = NULL;
    static line_t   *g_last_line = NULL;
    static label_t  *g_labels = NULL;
    static label_t  *g_last_label = NULL;
    static char     *g_last_label_defined = NULL;

    struct label_t {
        label_t *m_next;
        char *m_label;
        line_t *m_line;
    };
    
    struct param_t {
        param_t *m_next;
        enum param_type_t { P_INTEGER, P_FLOAT, P_STRING, P_LABEL_LOC, P_LABEL_OFF, } m_type;
        union param_value_t {
            intptr_t as_integer;
            floatptr_t as_float;
            char *as_string;
            label_t *as_label;
        } m_value;
    };
    
    struct line_t {
        line_t *m_next;
        uintptr_t m_position;
        uintptr_t m_length;
        label_t *m_label;
        param_t *m_params;
        uintptr_t m_instruction;
    };
    
    
%}

%union {
    intptr_t ival;
    floatptr_t fval;
    char *sval;
    uintptr_t instruction;
    label_t *label;
    line_t *line;
    param_t *param;
}

%error-verbose
%locations

%token <ival> INTEGER
%token <fval> FLOAT
%token <sval> STRING LABEL
%token <instruction> INST

%type <label> labeldef label
%type <line> line
%type <param> params params1 param

%%

program :   /* empty */
        |   program line                { if (!g_lines) g_lines = $2; if (g_last_line) g_last_line->m_next = $2; g_last_line = $2; }
        ;

line    :   labeldef '\n'               { $$ = get_line($1, UINTPTR_MAX, NULL); }
        |   labeldef INST params '\n'   { $$ = get_line($1, $2, $3); }
        ;

labeldef:   /* empty */                 { $$ = NULL; }
        |   LABEL ':'                   { $$ = get_label($1, NULL); g_last_label_defined = $$->m_label; free($1); }
        |   '.' LABEL ':'               { $$ = get_label(g_last_label_defined, $2); free($2); }
        ;

params  :   /* empty */                 { $$ = NULL; }
        |   params1                     { $$ = $1; }
        ;

params1 :   param                       { $$ = $1; }
        |   params1 ',' param           { $$ = $1; append_param($$, $3); }
        ;

param   :   INTEGER                     { $$ = calloc(1, sizeof(*$$)); $$->m_type = P_INTEGER; $$->m_value.as_integer = $1; }
        |   FLOAT                       { $$ = calloc(1, sizeof(*$$)); $$->m_type = P_FLOAT; $$->m_value.as_float = $1; }
        |   STRING                      { $$ = calloc(1, sizeof(*$$)); $$->m_type = P_STRING; $$->m_value.as_string = $1; }
        |   '&' label                   { $$ = calloc(1, sizeof(*$$)); $$->m_type = P_LABEL_LOC; $$->m_value.as_label = $2; }
        |   '~' label                   { $$ = calloc(1, sizeof(*$$)); $$->m_type = P_LABEL_OFF; $$->m_value.as_label = $2; }
        ;

label   :   LABEL '.' LABEL             { $$ = get_label($1, $3); free($1); free($3); }
        |   '.' LABEL                   { $$ = get_label(g_last_label_defined, $2); free($2); }
        |   LABEL                       { $$ = get_label($1, NULL); free($1); }
        ;

%%

static void append_param(param_t *list, param_t *param) {
    param_t *p = list;
    while (p && p->m_next != NULL) {
        p = p->m_next;
    }
    p->m_next = param;
}

static line_t *get_line(label_t *label, uintptr_t instruction, param_t *params) {
    line_t *line = calloc(1, sizeof(*line));
    
    line->m_label = label;
    line->m_instruction = instruction;
    line->m_params = params;
    line->m_position = UINTPTR_MAX;
    line->m_length = UINTPTR_MAX;
    return line;
}

static label_t *get_label(const char *prim, const char *sub) {
    label_t *label = g_labels;
    char *key;
    size_t keylen = 0;

    if (prim)   keylen += strlen(prim);
    if (sub)    keylen += 1 + strlen(sub);

    if (keylen == 0)  return NULL;

    key = calloc(1, ++keylen);

    if (prim) {
        strcat(key, prim);
    }
    
    if (sub) {
        strcat(key, ".");
        strcat(key, sub);
    }
        
    while (label != NULL) {
        if (0 == strcmp(label->m_label, key)) {
            free(key);
            return label;
        }
        label = label->m_next;
    }
    
    label = calloc(1, sizeof(*label));
    label->m_label = key;

    if (!g_labels)  g_labels = label;
    if (g_last_label)  g_last_label->m_next = label;
    g_last_label = label;

    // n.b. don't free key, as the label now owns it
    return label;
}

assembler_output_t *assemble(const char *filename) {
    int status;
    line_t *line;
    label_t *label;

    if (filename != NULL) {
        g_input = fopen(filename, "r");
    }
    else {
        g_input = stdin;
    }
    
    if (g_input != NULL) {
        flockfile(g_input);
        status = yyparse();
        funlockfile(g_input);
        fclose(g_input);
        g_input = NULL;
    }
    else {
        status = errno;
        perror(filename);
    }
    
    if (status != 0)  return NULL;

    // walk the parse tree (g_lines, g_labels) and resolve all the lengths and positions
    line = g_lines;
    uintptr_t next_position = 1;    // compile the first instruction to location 1
    while (line != NULL) {
        if (line->m_instruction == UINTPTR_MAX) {
            // this line hasn't got an instruction (i.e. it's just a floating label)
            line->m_length = 0;
        }
        else if (line->m_instruction == i_STR || line->m_instruction == i_STRING) {
            // special case length for string literals -- need to include the length of the string itself
            line->m_length = instruction_sizes[line->m_instruction];
            if (line->m_params != NULL && line->m_params->m_type == P_STRING) {
                line->m_length += strlen(line->m_params->m_value.as_string);
            }
        }
        else {
            line->m_length = instruction_sizes[line->m_instruction];
        }

        line->m_position = next_position;
        if (line->m_length > 2 && line->m_instruction != i_STR) {
            // complex instructions should have their position aligned one byte before a four byte boundary
            while (line->m_position % 4 < 3)  line->m_position++;
        }
        
        if (line->m_label != NULL) {
            if (line->m_label->m_line == NULL) {
                line->m_label->m_line = line;
            }
            else if (line->m_label->m_line == line) {
                ;
            }
            else {
                debug("multiple definitions of label %s\n", line->m_label->m_label);
                status = -1;
            }
        }
    
        next_position = line->m_position + line->m_length;
        line = line->m_next;
    }
    
    label = g_labels;
    while (label != NULL) {
        if (label->m_line == NULL) {
            debug("label '%s' is used but never defined\n", label->m_label);
            status = -1;
        }
        label = label->m_next;
    }
    
    // allocate space for the output and initialise values
    size_t len = g_last_line->m_position + g_last_line->m_length + 1;
    assembler_output_t *output = malloc(sizeof(*output) + len);
    output->m_bytecode_length = len;
    output->m_bytecode_start = 1;   // first instruction is at location 1 by default
    memset(output->m_bytecode, i_NOOP, len - 1);
    output->m_bytecode[0] = i_END;          // guard values
    output->m_bytecode[len - 1] = i_END;    // guard values

    // reduce each line to bytecode and inject it at the right position
    line = g_lines;
    while (status == 0 && line != NULL) {
        if (line->m_length == 0) {
            line = line->m_next;
            continue;   
        }
        
        output->m_bytecode[line->m_position] = (uint8_t) line->m_instruction;
        if (line->m_length > 1) {
            switch (line->m_instruction) {
                case i_BYTE:
                case i_OPEN:
                    if (line->m_params != NULL && line->m_params->m_type == P_INTEGER) {
                        uint8_t i = (uint8_t) line->m_params->m_value.as_integer;
                        output->m_bytecode[line->m_position + 1] = i;
                    }
                    else {
                        debug("instruction '%s' requires an integer parameter\n", instruction_names[line->m_instruction]);
                        status = -1;
                    }
                    break;
                
                case i_CALL:
                case i_CORO:
                case i_FUNLIT:
                    if (line->m_params != NULL && line->m_params->m_type == P_LABEL_LOC) {
                        function_handle_t handle = line->m_params->m_value.as_label->m_line->m_position;
                        * (function_handle_t *) &(output->m_bytecode[line->m_position + 1]) = handle;
                    }
                    else {
                        debug("instruction '%s' requires a label location parameter\n", instruction_names[line->m_instruction]);
                        status = -1;
                    }
                    break;
                
                case i_INT:
                    if (line->m_params != NULL && line->m_params->m_type == P_INTEGER) {
                        intptr_t i = line->m_params->m_value.as_integer;
                        * (intptr_t *) &(output->m_bytecode[line->m_position + 1]) = i;
                    }
                    else {
                        debug("instruction '%s' requires an integer parameter\n", instruction_names[line->m_instruction]);
                        status = -1;
                    }
                    break;

                case i_FLOAT:
                    if (line->m_params != NULL && line->m_params->m_type == P_FLOAT) {
                        floatptr_t f = line->m_params->m_value.as_float;
                        * (floatptr_t *) &(output->m_bytecode[line->m_position + 1]) = f;
                    }
                    else {
                        debug("instruction '%s' requires a float parameter\n", instruction_names[line->m_instruction]);
                        status = -1;
                    }
                    break;

                case i_STR:
                    if (line->m_params != NULL && line->m_params->m_type == P_STRING) {
                        uint8_t length = line->m_length - instruction_sizes[i_STR];
                        * (uint8_t *) &(output->m_bytecode[line->m_position + 1]) = length;
                        memcpy(&(output->m_bytecode[line->m_position + instruction_sizes[i_STR]]), line->m_params->m_value.as_string, length);
                    }
                    else {
                        debug("instruction '%s' requires a string parameter\n", instruction_names[line->m_instruction]);
                        status = -1;
                    }
                    break;

                case i_STRING:
                    if (line->m_params != NULL && line->m_params->m_type == P_STRING) {
                        uint32_t length = line->m_length - instruction_sizes[i_STRING];
                        * (uint32_t *) &(output->m_bytecode[line->m_position + 1]) = length;
                        memcpy(&(output->m_bytecode[line->m_position + instruction_sizes[i_STRING]]), line->m_params->m_value.as_string, length);
                    }
                    else {
                        debug("instruction '%s' requires a string parameter\n", instruction_names[line->m_instruction]);
                        status = -1;
                    }
                    break;

                case i_JMP:
                case i_JMP0:
                case i_JMPU:
                    if (line->m_params != NULL && line->m_params->m_type == P_LABEL_OFF) {
                        intptr_t offset = line->m_params->m_value.as_label->m_line->m_position - line->m_position;
                        * (intptr_t *) &(output->m_bytecode[line->m_position + 1]) = offset;
                    }
                    else {
                        debug("instruction '%s' requires a label offset parameter\n", instruction_names[line->m_instruction]);
                        status = -1;
                    }
                    break;

                case i_SYMDEF:
                    if (line->m_params != NULL && line->m_params->m_type == P_INTEGER) {
                        flags32_t flags = (flags32_t) line->m_params->m_value.as_integer;
                        if (line->m_params->m_next != NULL && line->m_params->m_next->m_type == P_INTEGER) {
                            identifier_t identifier = (identifier_t) line->m_params->m_next->m_value.as_integer;
                            * (flags32_t *) &(output->m_bytecode[line->m_position + 1]) = flags;
                            * (identifier_t *) &(output->m_bytecode[line->m_position + 1 + sizeof(flags)]) = identifier;
                        }
                        else {
                            debug("instruction '%s' requires integer flags and integer identifier parameters\n", instruction_names[i_SYMDEF]);
                            status = -1;
                        }                        
                    }
                    else {
                        debug("instruction '%s' requires integer flags and integer identifier parameters\n", instruction_names[i_SYMDEF]);
                        status = -1;
                    }
                    break;
                    
                case i_SYMFIND:
                case i_SYMUNDEF:
                    if (line->m_params != NULL && line->m_params->m_type == P_INTEGER) {
                        identifier_t identifier = (identifier_t) line->m_params->m_value.as_integer;
                        * (identifier_t *) &(output->m_bytecode[line->m_position + 1]) = identifier;
                    }
                    else {
                        debug("instruction '%s' requires an integer identifier parameter\n", instruction_names[line->m_instruction]);
                        status = -1;
                    }
                    break;

                default:
                    debug("unhandled multibyte instruction: %s\n", instruction_names[line->m_instruction]);
                    break;
            }
        }
        else {
            if (line->m_params != NULL) {
                debug("unused parameters being ignored by instruction %s\n", instruction_names[line->m_instruction]);
            }
        }
        line = line->m_next;
    }    
    
    // if there was a main label defined, make it the start point
    if (status == 0 && (label = get_label("main", NULL))) {
        output->m_bytecode_start = label->m_line->m_position;
    }
    
    // clean up the parse tree
    line = g_lines;
    while (line != NULL) {
        param_t *param = line->m_params;
        while (param != NULL) {
            if (param->m_type == P_STRING)  free(param->m_value.as_string);
            param_t *tmp = param;
            param = param->m_next;
            free(tmp);
        }

        line_t *tmp = line;
        line = line->m_next;
        free (tmp);
    }
    g_lines = g_last_line = NULL;
    
    label = g_labels;
    while (label != NULL) {
        free(label->m_label);
        label_t *tmp = label;
        label = label->m_next;
        free(tmp);
    }
    g_labels = g_last_label = NULL;
            
    if (status != 0) {
        free(output);
        return NULL;
    }
    
    return output;
}

static void yyerror(char const *s) {
    printf("line %i column %i - %s\n", yylloc.first_line, yylloc.first_column, s);
}

#ifndef digittoint
#define digittoint(c) ((c) - '0')
#endif

static inline int peek(void) {
    return peekc_unlocked(g_input);
}

static inline int next(void) {
    int c = getc_unlocked(g_input);
    ++yylloc.last_column;
    return c;
}

static int yylex(void) {
    int c;
    
    // skip over whitespace
    do {
        c = next();
    } while (c == ' ' || c == '\t');
    
    // update location
    yylloc.first_line = yylloc.last_line;
    yylloc.first_column = yylloc.last_column;
    
    // bail out at end of file
    if (c == EOF)  return EOF;

    if (c == '0' && peek() != '.') {
        // parse a literal integer in either hex or octal
        if (peek() == 'x') {
            c = next();
            yylval.ival = read_hex_value();
            return INTEGER;
        }
        else {
            yylval.ival = read_octal_value();
            return INTEGER;
        }
    }

    if (isdigit(c)) {
        // parse a literal number - either int or float
        intptr_t i = digittoint(c);
        
        while (isdigit(peek())) {
            c = next();
            i = 10 * i + digittoint(c);
        }
        
        if (peek() == '.') {
            floatptr_t f = i, div = 10.0;
            
            next();  // consume the '.'
            while (isdigit(peek())) {
                c = next();
                f += digittoint(c) / div;
                div *= 10.0;
            }
            
            yylval.fval = f;
            return FLOAT;
        }
        else {
            yylval.ival = i;
            return INTEGER;
        }
    }
    
    if (isalpha(c)) {
        // parse a string -- either an instruction or an identifier
        ungetc(c, g_input);
        --yylloc.last_column;
        char *str = read_identifier();

        for (size_t i = 0; i < i__MAX; i++) {
            if (0 == strcmp(str, instruction_names[i])) {
                free(str);
                yylval.instruction = i;
                return INST;
            }
        }
        
        yylval.sval = str;
        return LABEL;
    }
    
    if (c == '"') {
        // read a literal string
        char *str = read_quoted('"');
        
        yylval.sval = str;
        return STRING;
    }
    
    if (c == '\'') {
        // read a single-quoted character
        char *str = read_quoted('\'');
        if (str) {
            yylval.ival = str[0];
            if (strlen(str) > 1)  fprintf(stderr, "Ignoring extra characters in character literal: %s\n", str + 1);
            free(str);
        }
        return INTEGER;
    }
    
    if (c == ';' || c == '#') {
        // comment - discard until eol
        do {
            c = next();
        } while (c != EOF && c != '\n');
        return c;
    }
    
    if (c == '\n') {
        ++yylloc.last_line;
        yylloc.last_column = 0;
    }

    // by default just return the character read
    return c;
}

char *read_identifier(void) {
    int c;
    size_t i = 0, buflen = 64;
    char *buffer = calloc(1, buflen);
    
    while ((c = peek()) && (c == '_' || isalnum(c))) {
        if (buflen - i < 2) {
            buflen *= 2;
            char *tmp = calloc(1, buflen);
            memcpy(tmp, buffer, i);
            free(buffer);
            buffer = tmp;
        }
        c = next();
        buffer[i++] = c;
    }
    
    return buffer;
}

static char *read_quoted(int delimiter) {
    int c, found_closing_delim = 0;
    size_t i = 0, buflen = 64;
    char *buffer = calloc(1, buflen);
    
    while (!found_closing_delim) {
        if (buflen - i <= 2) {
            buflen *= 2;
            char *tmp = calloc(1, buflen);
            memcpy(tmp, buffer, i);
            free(buffer);
            buffer = tmp;
        }
        switch ((c = next())) {
            case '\\':
                switch (peek()) {
                    case '\\':  next(); buffer[i++] = '\\'; break;
                    case '\'':  next(); buffer[i++] = '\''; break;
                    case '"':   next(); buffer[i++] = '"';  break;
                    case 'a':   next(); buffer[i++] = '\a'; break;
                    case 'b':   next(); buffer[i++] = '\b'; break;
                    case 'f':   next(); buffer[i++] = '\f'; break;
                    case 'n':   next(); buffer[i++] = '\n'; break;
                    case 'r':   next(); buffer[i++] = '\r'; break;
                    case 't':   next(); buffer[i++] = '\t'; break;
                    case 'v':   next(); buffer[i++] = '\v'; break;
                        
                    case 'x':
                        next();
                        buffer[i++] = read_hex_byte();
                        break;
                        
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                        buffer[i++] = read_octal_byte();
                        break;
                        
                    default:  // unrecognised escape sequence, ignore the backslash
                        buffer[i++] = next();
                }
                break;
                
            case EOF:
            case '\n':
                buffer[i] = '\0';
                fprintf(stderr, "Unterminated quoted string\n");
                free(buffer);
                return NULL;
                
            default:
                if (c == delimiter) {
                    found_closing_delim = 1;
                }
                else {
                    buffer[i++] = c;
                }
                break;
        }
    }

    return buffer;
}

static int read_hex_byte(void) {
    int i = 0, val;
    char digits[3], *endptr=NULL;
    
    memset(digits, 0, sizeof(digits));
    while (i < 2 && isxdigit(peek())) {
        digits[i++] = (char) next();
    }
    
    val = strtol(digits, &endptr, 16);
    if (endptr != NULL && *endptr != '\0') {
        fprintf(stderr, "warning: invalid characters in hex byte '\\x%s'\n", digits);
    }
    return (char) val;
}

static int read_octal_byte(void) {
    int i = 0, val;
    char digits[4], *endptr=NULL;
    
    memset(digits, 0, sizeof(digits));
    while (i < 3 && isdigit(peek())) {
        digits[i++] = (char) next();
    }
    
    val = strtol(digits, &endptr, 8);
    if (endptr != NULL && *endptr != '\0') {
        fprintf(stderr, "warning: invalid characters in octal byte '\\%s'\n", digits);
    }
    if (val > 0xFF) {
        fprintf(stderr, "warning: octal byte value out of range '\\%s'\n", digits);
    }
    return (char) val;
}

static int read_hex_value(void) {
    int i = 0, val;
    char digits[64], *endptr=NULL;
    
    memset(digits, 0, sizeof(digits));
    while(i < sizeof(digits) - 1 && isxdigit(peek())) {
        digits[i++] = (char) next();
    }
    
    val = strtol(digits, &endptr, 16);
    if (endptr != NULL && *endptr != '\0') {
        fprintf(stderr, "warning: invalid characters in hex value '0x%s'\n", digits);
    }
    
    return val;
}

static int read_octal_value(void) {
    int i = 0, val;
    char digits[64], *endptr=NULL;
    
    memset(digits, 0, sizeof(digits));
    while(i < sizeof(digits) - 1 && isdigit(peek())) {
        digits[i++] = (char) next();
    }
    
    val = strtol(digits, &endptr, 8);
    if (endptr != NULL && *endptr != '\0') {
        fprintf(stderr, "warning: invalid characters in octal value '0%s'\n", digits);
    }
    
    return val;
}
