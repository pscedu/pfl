#!/usr/bin/perl -W
# $Id$
# %PSC_START_COPYRIGHT%
# -----------------------------------------------------------------------------
# Copyright (c) 2010, Pittsburgh Supercomputing Center (PSC).
#
# Permission to use, copy, and modify this software and its documentation
# without fee for personal use or non-commercial use within your organization
# is hereby granted, provided that the above copyright notice is preserved in
# all copies and that the copyright and this permission notice appear in
# supporting documentation.  Permission to redistribute this software to other
# organizations or individuals is not permitted without the written permission
# of the Pittsburgh Supercomputing Center.  PSC makes no representations about
# the suitability of this software for any purpose.  It is provided "as is"
# without express or implied warranty.
# -----------------------------------------------------------------------------
# %PSC_END_COPYRIGHT%

# Better algorithm:
#	use file mtime as long as mtime != ctime
#	and svn diff -I\$Id yields isn't empty
# XXX if file was copied from a revision, switch to that file to continue

use strict;
use warnings;
use PFL::Getoptv;

sub usage {
	warn "usage: $0 [-D var=value] file ...\n";
	exit 1;
}

my %opts;
getoptv("D:", \%opts) or usage;
my %incvars = map { die "bad variable format: $_" unless /=/; ($`, $') } @{ $opts{D} };

my @m = qw(0 January February March April May June July
    August September October November December);

my %m;
@m{@m} = (1 .. @m);

sub get_unless_last_rev {
	my ($fn, $d, $m, $y) = @_;

	my @out = `svn log '$fn'`;

	foreach my $ln (@out) {
		my ($t_rev, $t_y, $t_m, $t_d) =
		    ($ln =~ /^r(\d+) \s+ \| \s+ (?:\w+) \s+ \| \s+
		    (\d+)-(\d+)-0*(\d+) \s+ (\d+):(\d+):(\d+)/x) or next;

		# if this revision solely was comprised of comments or date bump, ignore
		my $prev_rev = $t_rev - 1;
		my $t_out = `svn diff -r $prev_rev:$t_rev --diff-cmd=diff -x '-I^\\.Dd -I^\\.\\\\"' '$fn'`;
		my $cnt = ($t_out =~ tr/\n//);

		next unless $cnt > 2;

		return if $t_y == $y && $t_m == $m && $t_d == $d;
		return ($t_d, $t_m, $t_y);
	}
}

sub slurp {
	my ($fn) = @_;
	local $/;

	open F, "<", $fn or die "$fn: $!\n";
	my $data = <F>;
	close F;

	return $data;
}

my ($d, $m, $y, $sm);

foreach my $fn (@ARGV) {
	my $out = `svn diff --diff-cmd=diff -x '-I\$Id -I^\\.\\\\"' '$fn'`;
	my $cnt = ($out =~ tr/\n//);
	my $data;

	if ($cnt > 2) {
		# has local changes, bump date
		$data = slurp $fn;
		my ($mt) = (stat $fn)[9];
		($d, $m, $y) = (localtime $mt)[3 .. 5];
		$y += 1900;
		$m++;
		goto bump;
	}

	$data = slurp $fn;
	($sm, $d, $y) = ($data =~ /^\.Dd (\w+) (\d+), (\d+)$/m) or next;
	$m = $m{$sm};

	($d, $m, $y) = get_unless_last_rev($fn, $d, $m, $y) or next;

 bump:
	# update the Dd date
	$data =~ s/^\.Dd .*/.Dd $m[$m] $d, $y/m;

	my $T = qr[^\.\\"\s*]m;

	# parse MODULES
	my %mods;
	if ($data =~ /$T%PFL_MODULES\s*(.*)\s*%/m) {
		my @mods = split /\s+/, $1;
		@mods{@mods} = (1) x @mods;
	}

	my $DATA = { };

	sub eval_data {
		my ($str) = @_;
		my @c = grep { /$T/ } split /(?<=\n)/, $str;
		my @t = eval join '', map { /$T/; $' } @c;
		return (\@c, @t);
	}

	sub expand_include {
		my ($fn, $p) = @_;
		$fn =~ s/\$(\w+)/$incvars{$1}/ge;

		open INC, "<", $fn or die "$fn: $!\n";
		my @lines = <INC>;
		close INC;

		shift @lines if $lines[0] =~ /$T\$Id/;

		my ($code, %av) = eval_data($p);
		@{$DATA}{keys %av} = values %av;
		return join '', @$code, @lines;
	}

	# process includes
	$data =~ s/($T%PFL_INCLUDE\s+(.*?)\s*{\n)(.*?)($T}%)/
	    $1 . expand_include($2, $3) . $4/gems;

	sub expand_list {
		my ($code, %k) = eval_data($_[0]);
		my $str = "";

		foreach my $k (sort keys %k) {
			$str .= ".It Cm $k\n$k{$k}\n";
		}
		return $str;
	}

	# process lists
	$data =~ s/$T%PFL_LIST\s*{\n(.*?)$T}%\n/expand_list($1)/gems;

	sub expand_expr {
		my ($code, @t) = eval_data($_[0]);
		return join('', @t) . "\n";
	}

	# process expressions
	$data =~ s/$T%PFL_EXPR\s*{\n(.*?)$T}%\n/expand_expr($1)/gems;

	# overwrite
	open F, ">", $fn or die "$fn: $!\n";
	print F $data;
	close F;
} continue {
	system("groff -z $fn");
}
