/*
 *  assembler.h
 *  dang
 *
 *  Created by Ellie on 30/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdint.h>

typedef struct assembler_output_t {
    uintptr_t m_bytecode_length;
    uintptr_t m_bytecode_start;
    uint8_t   m_bytecode[];
} assembler_output_t;

// pass NULL filename to read from stdin
assembler_output_t *assemble(const char *);

#endif
