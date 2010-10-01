/*
 *  string.h
 *  dang
 *
 *  Created by Ellie on 26/09/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef STRING_H
#define STRING_H

typedef struct string *String;

String string_create(const char *);
String string_destroy(String self);

String string_clone(const String self);
size_t string_length(const String self);
const char *string_ccstr(const String);

int string_assign(String self, const String other);
int string_append(String self, const String other);


#endif
