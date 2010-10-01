/*
 *  bits.c
 *  dang
 *
 *  Created by Ellie on 2/10/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#include <limits.h>

#include "bits.h"

uintptr_t nextupow2(uintptr_t x) {
    if (x == 0)  return 1;
    
    --x;
    for (unsigned i = 1; i < sizeof(uintptr_t) * CHAR_BIT; i = i << 1) {
        x = x | (x >> i);
    }
         
    return x + 1;
}
         
        
