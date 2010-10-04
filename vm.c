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

int vm_execute(const uint8_t *bytecode, size_t bytecode_length, size_t start_index, data_stack_t *data_stack) {
    size_t counter = start_index;
    int incr;

    return_stack_t return_stack;
    return_stack_init(&return_stack);
    
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
                incr = instruction_table[instruction](&bytecode[counter], counter, &data_stack, &return_stack);
                assert(incr != 0);
                counter += incr;
                break;
        }
    }
    return_stack_destroy(&return_stack);
    
    return 0;
}

