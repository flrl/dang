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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bytecode.h"
#include "debug.h"
#include "vmtypes.h"

    int yylex(void);
    void yyerror(const char *);
    
    char *read_identifier(void);
    char *read_quoted(void);
    int read_hex_byte(void);
    int read_octal_byte(void);
    
    typedef struct line_t line_t;
    typedef struct param_t param_t;
    typedef struct label_t label_t;
    typedef struct label_ref_t label_ref_t;
    
    label_t *get_label(const char *);
    line_t *get_line(label_t *, uintptr_t, param_t *);
    void append_param(param_t *, param_t *);

    static line_t *g_lines = NULL;
    static line_t *g_last_line = NULL;
    static label_t *g_labels = NULL;
    static label_t *g_last_label = NULL;

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

%token <ival> INTEGER
%token <fval> FLOAT
%token <sval> STRING LABEL
%token <instruction> INST

%type <label> label
%type <line> line
%type <param> params params1 param

%%

program :   /* empty */
        |   program line                { if (!g_lines) g_lines = $2; if (g_last_line) g_last_line->m_next = $2; g_last_line = $2; }
        ;

line    :   label '\n'                  { $$ = get_line($1, UINTPTR_MAX, NULL); }
        |   label INST params '\n'      { $$ = get_line($1, $2, $3); }
        ;

label   :   /* empty */                 { $$ = NULL; }
        |   LABEL ':'                   { $$ = get_label($1); }
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
        |   '&' LABEL                   { $$ = calloc(1, sizeof(*$$)); $$->m_type = P_LABEL_LOC; $$->m_value.as_label = get_label($2); }
        |   '~' LABEL                   { $$ = calloc(1, sizeof(*$$)); $$->m_type = P_LABEL_OFF; $$->m_value.as_label = get_label($2); }
        ;

%%

void append_param(param_t *list, param_t *param) {
    param_t *p = list;
    while (p && p->m_next != NULL) {
        p = p->m_next;
    }
    p->m_next = param;
}

line_t *get_line(label_t *label, uintptr_t instruction, param_t *params) {
    line_t *line = calloc(1, sizeof(*line));
    
    line->m_label = label;
    line->m_instruction = instruction;
    line->m_params = params;
    line->m_position = UINTPTR_MAX;
    line->m_length = UINTPTR_MAX;
    return line;
}

label_t *get_label(const char *key) {
    label_t *label = g_labels;
    
    while (label != NULL) {
        if (0 == strcmp(label->m_label, key))  return label;
        label = label->m_next;
    }
    
    label = calloc(1, sizeof(*label));
    label->m_label = strdup(key);

    if (!g_labels)  g_labels = label;
    if (g_last_label)  g_last_label->m_next = label;
    g_last_label = label;
    return label;
}

int assemble(uint8_t **bytecode, size_t *bytecode_len) {
    int status;
    line_t *line;
    
    status = yyparse();
    if (status != 0)  return status;

    // walk the parse tree (g_lines, g_labels) and resolve all the lengths and positions
    line = g_lines;
    uintptr_t next_position = 0;
    while (line != NULL) {
        if (line->m_instruction == UINTPTR_MAX) {
            // this line hasn't got an instruction (i.e. it's just a floating label)
            line->m_length = 0;
        }
        else if (line->m_instruction == i_STRLIT) {
            // special case length for string literals -- need to include the length of the string itself
            line->m_length = instruction_sizes[i_STRLIT];
            if (line->m_params != NULL && line->m_params->m_type == P_STRING) {
                line->m_length += strlen(line->m_params->m_value.as_string);
            }
        }
        else {
            line->m_length = instruction_sizes[line->m_instruction];
        }

        line->m_position = next_position;
        if (line->m_length > 1) {
            // complex instructions should have their position aligned one byte before a four byte boundary
            while (line->m_position % 4 < 3)  line->m_position++;
        }
    
        next_position = line->m_position + line->m_length;
        line = line->m_next;
    }
    
    // allocate enough space for the bytecode and set length
    *bytecode_len = g_last_line->m_position + g_last_line->m_length + 1;
    *bytecode = calloc(*bytecode_len, sizeof(**bytecode));

    // pre-fill bytecode with no-op instructions, except for the very last byte
    memset(*bytecode, i_NOOP, *bytecode_len - 1);
    
    // reduce each line to bytecode and inject it at the right position
    line = g_lines;
    while (status == 0 && line != NULL) {
        *bytecode[line->m_position] = (uint8_t) line->m_instruction;
        if (line->m_length > 1) {
            switch (line->m_instruction) {
                case i_CALL:
                case i_FUNLIT:
                    if (line->m_params != NULL && line->m_params->m_type == P_LABEL_LOC) {
                        function_handle_t handle = line->m_params->m_value.as_label->m_line->m_position;
                        * (function_handle_t *) &(*bytecode[line->m_position + 1]) = handle;
                    }
                    else {
                        debug("instruction '%s' requires a label location parameter\n", instruction_names[line->m_instruction]);
                        status = -1;
                    }
                    break;
                
                case i_INTLIT:
                    if (line->m_params != NULL && line->m_params->m_type == P_INTEGER) {
                        intptr_t i = line->m_params->m_value.as_integer;
                        * (intptr_t *) &(*bytecode[line->m_position + 1]) = i;
                    }
                    else {
                        debug("instruction '%s' requires an integer parameter\n", instruction_names[line->m_instruction]);
                        status = -1;
                    }
                    break;

                case i_FLTLIT:
                    if (line->m_params != NULL && line->m_params->m_type == P_FLOAT) {
                        floatptr_t f = line->m_params->m_value.as_integer;
                        * (floatptr_t *) &(*bytecode[line->m_position + 1]) = f;
                    }
                    else {
                        debug("instruction '%s' requires a float parameter\n", instruction_names[line->m_instruction]);
                        status = -1;
                    }
                    break;

                case i_STRLIT:
                    if (line->m_params != NULL && line->m_params->m_type == P_STRING) {
                        uint16_t length = line->m_length - instruction_sizes[i_STRLIT];
                        * (uint16_t *) &(*bytecode[line->m_position + 1]) = length;
                        memcpy(&(*bytecode[line->m_position + instruction_sizes[i_STRLIT]]), line->m_params->m_value.as_string, length);
                    }
                    else {
                        debug("instruction '%s' requires a string parameter\n", instruction_names[line->m_instruction]);
                        status = -1;
                    }
                    break;

                case i_BRANCH:
                case i_0BRANCH:
                    if (line->m_params != NULL && line->m_params->m_type == P_LABEL_OFF) {
                        intptr_t offset = line->m_params->m_value.as_label->m_line->m_position - line->m_position;
                        * (intptr_t *) &(*bytecode[line->m_position + 1]) = offset;
                    }
                    else {
                        debug("instruction '%s' requires a label offset parameter\n", instruction_names[line->m_instruction]);
                        status = -1;
                    }
                    break;

                case i_SYMDEF:
                    if (line->m_params != NULL && line->m_params->m_type == P_INTEGER) {
                        uint32_t flags = (uint32_t) line->m_params->m_value.as_integer;
                        if (line->m_params->m_next != NULL && line->m_params->m_next->m_type == P_INTEGER) {
                            identifier_t identifier = (identifier_t) line->m_params->m_next->m_value.as_integer;
                            * (uint32_t *) &(*bytecode[line->m_position + 1]) = flags;
                            * (identifier_t *) &(*bytecode[line->m_position + 1 + sizeof(flags)]) = identifier;
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
                        * (identifier_t *) &(*bytecode[line->m_position + 1]) = identifier;
                    }
                    else {
                        debug("instruction '%s' requires an integer identifier parameter\n", instruction_names[line->m_instruction]);
                        status = -1;
                    }
                    break;

                default:
                    debug("unhandled instruction: %s\n", instruction_names[line->m_instruction]);
                    break;
            }
        }
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
    
    label_t *label = g_labels;
    while (label != NULL) {
        free(label->m_label);
        label_t *tmp = label;
        label = label->m_next;
        free(tmp);
    }
    g_labels = g_last_label = NULL;
            
    return status;
}

int peekchar(void) {
    int c = getchar();
    ungetc(c, stdin);
    return c;
}

void yyerror(char const *s) {
    printf("%s\n", s);
}

int yylex(void) {
    int c;
    
    // skip over whitespace
    do {
        c = getchar();
    } while (c == ' ' || c == '\t');
    
    // bail out at end of file
    if (c == EOF)  return EOF;
    
    if (isdigit(c)) {
        // parse a literal number - either int or float
        intptr_t i = digittoint(c);
        
        while (isdigit(peekchar())) {
            c = getchar();
            i = 10 * i + digittoint(c);
        }
        
        if (peekchar() == '.') {
            floatptr_t f = i, div = 10.0;
            
            while (isdigit(peekchar())) {
                c = getchar();
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
        ungetc(c, stdin);
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
        char *str = read_quoted();
        
        yylval.sval = str;
        return STRING;
    }
    
    if (c == ';') {
        // comment - discard until eol
        do {
            c = getchar();
        } while (c != EOF && c != '\n');
        return c;
    }
    
    // anything else, return the character read
    return c;
}

char *read_identifier(void) {
    int c;
    size_t i = 0, buflen = 64;
    char *buffer = calloc(1, buflen);
    
    while ((c = peekchar()) && (c == '_' || isalnum(c))) {
        if (buflen - i < 2) {
            buflen *= 2;
            char *tmp = calloc(1, buflen);
            memcpy(tmp, buffer, i);
            free(buffer);
            buffer = tmp;
        }
        c = getchar();
        buffer[i++] = c;
    }
    
    return buffer;
}

char *read_quoted(void) {
    int c, found_closing_double_quote = 0;
    size_t i = 0, buflen = 64;
    char *buffer = calloc(1, buflen);
    
    while (!found_closing_double_quote) {
        if (buflen - i <= 2) {
            buflen *= 2;
            char *tmp = calloc(1, buflen);
            memcpy(tmp, buffer, i);
            free(buffer);
            buffer = tmp;
        }
        switch ((c = getchar())) {
            case '"':
                found_closing_double_quote = 1;
                break;
            case '\\':
                switch (peekchar()) {
                    case '\\':  getchar(); buffer[i++] = '\\'; break;
                    case '\'':  getchar(); buffer[i++] = '\''; break;
                    case '"':   getchar(); buffer[i++] = '"';  break;
                    case 'a':   getchar(); buffer[i++] = '\a'; break;
                    case 'b':   getchar(); buffer[i++] = '\b'; break;
                    case 'f':   getchar(); buffer[i++] = '\f'; break;
                    case 'n':   getchar(); buffer[i++] = '\n'; break;
                    case 'r':   getchar(); buffer[i++] = '\r'; break;
                    case 't':   getchar(); buffer[i++] = '\t'; break;
                    case 'v':   getchar(); buffer[i++] = '\v'; break;
                        
                    case 'x':
                        getchar();
                        buffer[i++] = read_hex_byte();
                        break;
                        
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                        buffer[i++] = read_octal_byte();
                        break;
                        
                    default:  // unrecognised escape sequence, keep the backslash and carry on as usual
                        buffer[i++] = c;
                }
                break;
                
            case EOF:  // FIXME handle this differently from \n?
            case '\n':
                buffer[i] = '\0';
                fprintf(stderr, "Unterminated string literal\n");
                free(buffer);
                return NULL;
                
            default:
                buffer[i++] = c;
        }
    }

    return buffer;
}

int read_hex_byte(void) {
    int i = 0, val;
    char digits[3], *endptr;
    
    memset(digits, 0, sizeof(digits));
    while (i < 2 && isxdigit(peekchar())) {
        digits[i] = (char) getchar();
    }
    
    val = strtol(digits, &endptr, 16);
    if (endptr != NULL) {
        fprintf(stderr, "warning: invalid characters in hex byte '\\x%s'\n", digits);
    }
    return (char) val;
}

int read_octal_byte(void) {
    int i = 0, val;
    char digits[4], *endptr;
    
    memset(digits, 0, sizeof(digits));
    while (i < 3 && isdigit(peekchar())) {
        digits[i] = (char) getchar();
    }
    
    val = strtol(digits, &endptr, 8);
    if (endptr != NULL) {
        fprintf(stderr, "warning: invalid characters in octal byte '\\%s'\n", digits);
    }
    if (val > 0xFF) {
        fprintf(stderr, "warning: octal byte value out of range '\\%s'\n", digits);
    }
    return (char) val;
}
