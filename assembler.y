/*
 *  assembler.y
 *  dang
 *
 *  Created by Ellie on 29/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

%{
#include "bytecode.h"
    int yylex(void);
    void yyerror(const char *);
%}

%token INST
%token ID


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

param   :   NUM
        |   STR
        |   '&' ID
        ;

%%