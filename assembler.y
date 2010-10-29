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
#include "floatptr_t.h"

    int yylex(void);
    void yyerror(const char *);
    
    char *read_identifier(void);
    char *read_quoted(void);
    int read_hex_byte(void);
    int read_octal_byte(void);
%}

%union {
    intptr_t ival;
    floatptr_t fval;
    char *sval;
    uint8_t instruction;
    void *id;
}

%token <ival> INTEGER
%token <fval> FLOAT
%token <sval> STRING
%token <instruction> INST
%token <sval> ID

%%

program :   /* empty */
        |   program line
        ;

line    :   '\n'
        |   label INST params '\n'
        ;

label   :   /* empty */
        |   ID ':'
        ;

params  :   /* empty */
        |   params ',' param
        ;

param   :   INTEGER
        |   FLOAT
        |   STRING
        |   '&' ID
        ;

%%

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
        int i = digittoint(c);
        
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
        char *str = read_identifier();

        for (size_t i = 0; i < i__MAX; i++) {
            if (0 == strcmp(str, instruction_names[i])) {
                free(str);
                yylval.instruction = i;
                return INST;
            }
        }
        
        yylval.sval = str;
        return ID;
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
    
    buffer[i++] = c;
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
        fprintf(stderr, "warning: invalid characters in hex byte '%s'\n", digits);
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
        fprintf(stderr, "warning: invalid characters in octal byte '%s'\n", digits);
    }
    if (val > 0xFF) {
        fprintf(stderr, "warning: octal byte value out of range '%s'\n", digits);
    }
    return (char) val;
}
