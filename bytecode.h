/*
 *  bytecode.h
 *  dang
 *
 *  Created by Ellie on 3/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef BYTECODE_H
#define BYTECODE_H

#include "instruction_table.h"

typedef enum instruction_t {
    i__MIN = 0,
/*-- INSTRUCTIONS START --*/
    i_END = 0,
    i_NOOP = 1,
    i_CALL,         /* function_handle_t */
    i_RETURN,
    i_DROP,
    i_SWAP,
    i_DUP,
    i_OVER,
    i_BRANCH,       /* intptr_t */
    i_BRANCH0,      /* intptr_t */
    i_SYMDEF,       /* uint32_t identifier_t */
    i_SYMFIND,      /* identifier_t */
    i_SYMCLONE,     /* identifier_t */
    i_SYMUNDEF,     /* identifier_t */
    i_SRLOCK,
    i_SRUNLOCK,
    i_SRREAD,
    i_SRWRITE,
    i_ARINDEX,
    i_ARPUSH,
    i_ARPOP,
    i_ARSHFT,
    i_ARUNSHFT,
    i_HRINDEX,
    i_HRKEYEX,
    i_HRKEYDEL,
    i_CRREAD,
    i_CRWRITE,
    i_FRCALL,
    i_INTLIT,       /* intptr_t */
    i_INTADD,
    i_INTSUBT,
    i_INTMULT,
    i_INTDIV,
    i_INTMOD,
    i_STRLIT,       /* uint16_t */
    i_STRCAT,
    i_FLTLIT,       /* floatptr_t */
    i_FLTADD,
    i_FLTSUBT,
    i_FLTMULT,
    i_FLTDIV,
    i_FLTMOD,
    i_FUNLIT,       /* function_handle_t */
    i_OUT,
    i_OUTL,
/*-- INSTRUCTIONS END --*/
    
//    iOVER,
//    iTUCK,
//    iPICK,
//    iROLL,
//    iROT,
//    iTOR,
//    i2DROP,
//    iNDROP,
//    i2DUP,
//    iNDUP,
//    i2SWAP,
//    iDUPIFNE0,
//    iINCR,
//    iDECR,
//    iADD,
//    iSUBT,
//    iMULT,
//    iDIV,
//    iMOD,
//    iCMPE,
//    iCMPNE,
//    iCMPLT,
//    iCMPGT,
//    iCMPLTE,
//    iCMPGTE,
//    iCMPE0,
//    iCMPNE0,
//    iCMPLT0,
//    iCMPGT0,
//    iCMPLTE0,
//    iCMPGTE0,
//    iBAND,
//    iBOR,
//    iBXOR,
//    iBINV,
//    !,
//    @,
//    +!,
//    -!,
//    C!,
//    C@,
//    C@C!,
//    CMOVE,
//    ALLOT,
//    >R,
//    2>R,
//    R>,
//    2R>,
//    R@,
//    2R@,
//    R1+,
//    R1-,
//    >CTRL,
//    2>CTRL,
//    CTRL>,
//    2CTRL>,
//    CTRL@,
//    2CTRL@,
//    ASSERT,
//    .S,
//    KEY,
//    EMIT,
//    WORD,
//    NUMBER,
//    U.R,
//    .R,
//    FIND,
//    DE>CFA,
//    DE>DFA,
//    DE>NAME,
//    DFA>DE,
//    DFA>CFA,
//    CREATE,
//    ,,
//    [,
//    ],
//    :,
//    ;,
//    IMMEDIATE,
//    COMPILE-ONLY,
//    HIDDEN,
//    HIDE,
//    ',
//    BRANCH,
//    0BRANCH,
//    LITSTRING,
//    TELL,
//    UGROW,
//    UGROWN,
//    USHRINK,
//    QUIT,
//    ABORT,
//    breakpoint,
//    CELLS,
//    /CELLS,
//    EXECUTE,
//    POSTPONE,
//    THROW,
//    CATCH,
    
    i__MAX,
} instruction_t;

#endif
