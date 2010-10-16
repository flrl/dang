#!/usr/bin/env perl

use warnings;
use strict;

use Data::Dumper;

my $output = shift @ARGV;
print "Usage: $0 outfile [ files... ]\n" and exit 1 if not $output;

my @instructions;

my $process_flag = 0;
while (<>) {
    $process_flag = 1, next if m{^\s*/\*-- INSTRUCTIONS START --\*/\s*$};
    $process_flag = 0, next if m{^\s*/\*-- INSTRUCTIONS END --\*/\s*$};
#    print "$process_flag: $_";
    next if not $process_flag;

    chomp;

    my ($instruction, $value) = m/^.*i_(\w+)(?:\s*=\s*(\w+))?,/;

    # FIXME handle values
    push @instructions, $instruction;
}

#print Dumper \@instructions;
#__END__

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
