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

    my ($instruction, $extras) = m/^\s*i_(\w+)(?:\s*=\s*\w+)?,(?:\s*\/\*\s([\w\s]+)\s\*\/)?/;
    next if not $instruction;
    $extras = '' if not $extras;

    push @instructions, { instruction => $instruction, extras => [ split /\s+/, $extras ] };
}

my $instruction;

open my $header, '>', "$output.h" or die "$output.h: $!";
    print $header <<'PREAMBLE';
#ifndef INSTRUCTION_TABLE_H
#define INSTRUCTION_TABLE_H
struct vm_context_t;
typedef int(*instruction_func)(struct vm_context_t *);
extern const instruction_func instruction_table[];
extern const char *instruction_names[];
extern const unsigned instruction_sizes[];

PREAMBLE

    for $instruction (@instructions) {
        print $header "int inst_$instruction->{instruction}(struct vm_context_t *);\n"
    }

    print $header <<'POSTAMBLE';

#endif
POSTAMBLE
close $header;

open my $source, '>', "$output.c" or die "$output.c: $!";
    print $source "#include \"instruction_table.h\"\n";
    print $source "#include \"vmtypes.h\"\n";

    print $source "const instruction_func instruction_table[] = {\n";
    for $instruction (@instructions) {
        print $source "\t&inst_$instruction->{instruction},\n";
    }
    print $source "};\n";

    print $source "const char *instruction_names[] = {\n";
    for $instruction (@instructions) {
        print $source "\t\"" . lc($instruction->{instruction}) . "\",\n";
    }
    print $source "};\n";

    print $source "const unsigned instruction_sizes[] = {\n";
    for $instruction(@instructions) {
        my $extras = join ' + ', map { "sizeof($_)" } @{$instruction->{extras}};
        $extras = '0' if not $extras;
        print $source "\tsizeof(uint8_t) + $extras,\n";
    }
    print $source "};\n";
close $source;
