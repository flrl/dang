/*
 *  vm.c
 *  dang
 *
 *  Created by Ellie on 3/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <assert.h>

#include "bytecode.h"
#include "vm.h"

int vm_execute(const uint8_t *bytecode, size_t bytecode_length, size_t start_index, data_stack_scope_t *data_stack, void *return_stack) {
    
    size_t counter = start_index;
    int incr;
    
    while (counter < bytecode_length) {
        uint8_t instruction = bytecode[counter];
        assert(instruction < i__MAX);
        switch(instruction) {
            case iEND:
                return 0;
            case iNOOP:
                ++counter;
                break;
            default:
                incr = instruction_table[instruction](&bytecode[counter], data_stack, return_stack);
                assert(incr != 0);
                counter += incr;
                break;
        }
    }
    
    return 0;
}