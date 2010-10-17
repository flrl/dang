#!/usr/bin/env perl
#
# make_instruction_table.pl
# dang
# 
# Created by Ellie on 15/10/10.
# Copyright 2010 Ellie. All rights reserved.
#

use warnings;
use strict;

my $output = shift @ARGV;
print "Usage: $0 outfile [ files... ]\n" and exit 1 if not $output;

my @instructions;

my $process_flag = 0;
while (<>) {
    $process_flag = 1, next if m{^\s*/\*-- INSTRUCTIONS START --\*/\s*$};
    $process_flag = 0, next if m{^\s*/\*-- INSTRUCTIONS END --\*/\s*$};
    next if not $process_flag;

    chomp;

    my ($instruction, $value) = m/^.*i_(\w+)(?:\s*=\s*(\w+))?,/;

    # FIXME handle values
    push @instructions, $instruction;
}

my $instruction;

open my $header, '>', "$output.h" or die "$output.h: $!";
    print $header <<'PREAMBLE';
#ifndef INSTRUCTION_TABLE_H
#define INSTRUCTION_TABLE_H
struct vm_context_t;
typedef int(*instruction_func)(struct vm_context_t *);
extern const instruction_func instruction_table[];

PREAMBLE

    for $instruction (@instructions) {
        print $header "int inst_$instruction(struct vm_context_t *);\n"
    }

    print $header <<'POSTAMBLE';

#endif
POSTAMBLE
close $header;

open my $source, '>', "$output.c" or die "$output.c: $!";
    print $source <<'PREAMBLE';
#include "instruction_table.h"

const instruction_func instruction_table[] = {
PREAMBLE
    for $instruction (@instructions) {
        print $source "\t&inst_$instruction,\n";
    }

    print $source <<'POSTAMBLE';
};
POSTAMBLE
close $source;
