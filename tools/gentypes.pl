#!/usr/bin/perl -W
# $Id$
# %PSC_COPYRIGHT%

use strict;
use warnings;
use PFL::Getoptv;

sub usage {
	die "usage: $0 [-g pat] [-h headers] [-x exclude] file\n";
}

sub in_array {
	my ($ar, $elem) = @_;
	local $_;

	foreach (@$ar) {
		return 1 if $_ eq $elem;
	}
	return (0);
}

sub uniq {
	my @a;
	local $_;

	foreach (@_) {
		push @a, $_ unless @a && $_ eq $a[$#a];
	}
	return @a;
}

sub filter {
	my $xr = shift;
	my @a;
	local $_;

	foreach (@_) {
		push @a, $_ unless in_array $xr, $_;
	}
	return @a;
}

my %opts;
getoptv("g:h:x:", \%opts) or usage();
usage() unless @ARGV == 1;

my $outfn = $ARGV[0];

my @vals;
my @types;
my @hdrs;
my @enums;
@hdrs = uniq sort map { glob } @{ $opts{h} } if $opts{h};
if ($opts{x}) {
	@hdrs = filter [ map { glob } @{ $opts{x} } ], @hdrs;
}
my $lvl = 0;
my $in_enum = 0;
foreach my $hdr (@hdrs) {
	open HDR, "<", $hdr or die "$hdr: $!\n";
	while (<HDR>) {
		if ($lvl) {
			$lvl++ if /^\s*#\s*if/;
			$lvl-- if /^\s*#\s*endif/;
		} elsif ($in_enum) {
			$in_enum = 0 if /}/;
			push @enums, $1 if /^\s*(\w+)/;
		} else {
			$lvl = 1 if /^\s*#\s*if\s+0\s*$/;
			push @types, "struct $1" if /^(?:typedef\s+)?struct\s+(\w+)\s*{\s*/;
			push @types, $1 if /^typedef\s+(?:struct\s+)?(?:\w+)\s+(\w+)\s*;\s*/;
			$in_enum = 1 if /^enum\s*(\w*)\s*{\s*/;

			if ($opts{g}) {
				foreach my $pat (@{ $opts{g} }) {
					push @vals, /($pat)/;
				}
			}
		}
	}
	close HDR;
}

open OUT, "<", $outfn;
my $lines = eval {
	local $/;
	return <OUT>;
};
close OUT;

my $includes = join '', map { s!(?:include|\.\.)/!!g; qq{#include "$_"\n} } @hdrs;
$lines =~ s!(?<=/\* start includes \*/\n).*?(?=/\* end includes \*/)!$includes!s;

my $types = join '', map { "\tPRTYPE($_);\n" } uniq sort @types;
$lines =~ s!(?<=/\* start structs \*/\n).*?(?=/\* end structs \*/)!$types\t!s;

my $vals = join '', map { "\tPRVAL($_);\n" } uniq sort @vals;
$lines =~ s!(?<=/\* start constants \*/\n).*?(?=/\* end constants \*/)!$vals\t!s;

my $enums = join '', map { "\tPRVAL($_);\n" } uniq sort @enums;
$lines =~ s!(?<=/\* start enums \*/\n).*?(?=/\* end enums \*/)!$enums\t!s;

open OUT, ">", $outfn;
print OUT $lines;
close OUT;

exit 0;
