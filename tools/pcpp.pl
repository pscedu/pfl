#!/usr/bin/env perl
# $Id$
# %ISC_START_LICENSE%
# ---------------------------------------------------------------------
# Copyright 2010-2018, Pittsburgh Supercomputing Center
# All rights reserved.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the
# above copyright notice and this permission notice appear in all
# copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
# DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
# PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
# TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.
# --------------------------------------------------------------------
# %END_LICENSE%

use strict;
use warnings;
use PFL::Getoptv;
use File::Basename;

sub usage {
	warn "usage: $0 [-FRT] [-H hack] file\n";
	exit 1;
}

sub fatal {
	warn "$0: ", @_, "\n";
	exit 1;
}

my %hacks = (
	yyerrlab	=> 0,
	yylex_return	=> 0,
	yysyntax_error	=> 0,
	yytext		=> 0,
);
my %opts;
getoptv("FH:RT", \%opts) or usage;
usage unless @ARGV == 1;

if ($opts{H}) {
	foreach my $hack (@{ $opts{H} }) {
		die "$0: invalid hack: $hack" unless exists $hacks{$hack};
		$hacks{$hack} = 1;
	}
}

my $fn = $ARGV[0];

open F, "<", $fn or die "$fn: $!\n";
local $/;
my $data = <F>;
close F;

if ($opts{F}) {
	print $data;
	exit;
}

my $output = "";

# debug file ID
$output .= qq{# 1 "$fn"\n};

my $inject_tracing = !$opts{T};
my $inject_returns = !$opts{R};

my $pos;
my $lvl = 0;
my $foff;
my $linenr = 0;
my $undef_pci = 0;

sub advance {
	my ($len) = @_;

	my $str = substr($data, $pos, $len);
	$output .= $str;
	my @m = $str =~ /\n/g;
	$linenr += @m;
	$pos += $len;
}

sub in_array {
	my ($needle, $r_hay) = @_;
	for (@$r_hay) {
		return 1 if $_ eq $needle;
	}
	return 0;
}

# Determines if a type is aggregate (e.g. struct).  Notably typedefs
# will be missed.
sub type_is_aggregate {
	my ($type) = @_;
	my @types = qw();
	return $type =~ /^(?:lnet|cfs)_.*_t$/ ||
	   $type =~ /struct\b[^*]*$/ ||
	   in_array($type, \@types);
}

sub get_containing_func_return_type {
	return "" unless defined $foff;

	my $plevel = 0;
	my $j;
	# Skip past function arguments.
	for ($j = $foff; $j > 0; $j--) {
		if (substr($data, $j, 1) eq ")") {
			$plevel++;
		} elsif (substr($data, $j, 1) eq "(") {
			if (--$plevel == 0) {
				$j--;
				last;
			}
		}
	}
	# Skip function name.
	$j-- while substr($data, $j, 1) =~ /\s/;
	$j-- while substr($data, $j - 1, 1) =~ /[a-zA-Z0-9_]/;

	$j-- while substr($data, $j - 1, 1) =~ /\s/;

	# Gather function return type.
	my $len = 0;
	$j--, $len++ while substr($data, $j - 1, 1) =~ /[ a-z*A-Z0-9_]/;
	my $type = substr($data, $j, $len);
	return $type;
}

sub get_containing_func_name {
	return "" unless defined $foff;

	my $plevel = 0;
	my $j;
	for ($j = $foff; $j > 0; $j--) {
		if (substr($data, $j, 1) eq ")") {
			$plevel++;
		} elsif (substr($data, $j, 1) eq "(") {
			if (--$plevel == 0) {
				$j--;
				last;
			}
		}
	}
	$j-- while substr($data, $j, 1) =~ /\s/;
	my $len = 1;
	$j--, $len++ while substr($data, $j - 1, 1) =~ /[a-zA-Z0-9_]/;
	return substr($data, $j, $len);
}

sub get_func_args {
	my $plevel = 0;
	my ($j, $k, @av);
	for ($j = $pos; $j > 0; $j--) {
		if (substr($data, $j, 1) eq "," && $plevel == 1) {
			unshift @av, substr($data, $j + 1, $k - $j);
			$k = $j - 1;
		} elsif (substr($data, $j, 1) eq ")") {
			$k = $j - 1 if ++$plevel == 1;
		} elsif (substr($data, $j, 1) eq "(") {
			if (--$plevel == 0) {
				unshift @av, substr($data, $j + 1, $k - $j) if
				    $k - $j;
				last;
			}
		}
	}
	local $_;
	s/^\s+|\s+$//g for @av;
	@av = () if @av == 1 && $av[0] eq "void";
	my $pci = "";
	if (@av > 0 && $av[0] =~ /struct\s+pfl_callerinfo\s*\*(\w+)/) {
		$pci = $1;
		shift @av;
	}
	my @deletions;
	my $index = 0;
	my @new_av;
	for (@av) {
		# void (*foo)(const char *, int)
		unless (s/^.*?\(\s*\*(.*?)\).*/$1/s) {
			# __unusedx const char *foo[BLAH]
			s/\[.*//s;
			if (s/^(.*?)\s*(\w+)$/$2/s) {
				next if type_is_aggregate($1);
			}
		}
		push @new_av, $_;
	}
	@av = @new_av;
	pop @av if @av > 1 && $av[$#av] eq "...";
	return ($pci, @av);
}

sub get_containing_tag {
	my $blevel = 1;
	my ($j);
	for ($j = $pos - 1; $j > 0; $j--) {
		if (substr($data, $j, 1) eq '"') {
			for ($j--; $j > 0; $j--) {
				last if substr($data, $j, 1) eq '"' and
				    substr($data, $j - 1, 1) ne "\\";
			}
		} elsif (substr($data, $j, 1) eq "'") {
			for ($j--; $j > 0; $j--) {
				last if substr($data, $j, 1) eq "'" and
				    substr($data, $j - 1, 1) ne "\\";
			}
		} elsif (substr($data, $j, 1) eq "}") {
			$blevel++;
		} elsif (substr($data, $j, 1) eq "{") {
			if (--$blevel == 0) {
				my $k;
				for ($j--; $j > 0; $j--) {
					last if substr($data, $j, 1) !~ /\s/;
				}
				for ($k = $j; $k > 0; $k--) {
					last if substr($data, $k-1, 1) !~ /[a-z0-9_]/i;
				}
				return substr($data, $k, $j - $k + 1);
			}
		}
	}
	return "";
}

sub containing_func_is_dead {
	return 0 unless defined $foff;
	my $j = $foff;
	while (--$j > 0) {
		last if substr($data, $j, 1) eq ";";
	}
	return substr($data, $j, $foff - $j) =~ /\b__dead\b/;
}

sub dec_level {
	$foff = undef unless --$lvl;
	fatal "brace level $lvl < 0 at $linenr: $ARGV[0]" if $lvl < 0;
	if ($undef_pci && $lvl == 0) {
		$output .= "\n#undef _pfl_callerinfo\n";
		$undef_pci = 0;
	}
}

sub count_newlines {
	my ($s) = @_;
	return "/*$s*/" if $s =~ /\b(?:NOTREACHED|FALLTHROUGH)\b/;
	return join '', $s =~ /\\?\n/g;
}

$data =~ s{/\*(.*?)\*/}{ count_newlines($1) }egs;

for ($pos = 0; $pos < length $data; ) {
	if (substr($data, $pos, 1) eq "#") {
		# skip preprocessor
		advance(1);
		my $esc = 0;
		for (; $pos < length($data); advance(1)) {
			if ($esc) {
				$esc = 0;
			} elsif (substr($data, $pos, 1) eq "\\") {
				$esc = 1;
			} elsif (substr($data, $pos, 1) eq "\n") {
				last;
			}
		}
		advance(1);
	} elsif (substr($data, $pos, 2) eq q{//}) {
		# skip single-line comments
		if (substr($data, $pos + 2) =~ m[\n]) {
			advance($+[0] + 1);
		} else {
			advance(length($data) - $pos);
		}
	} elsif (substr($data, $pos, 1) =~ /['"]/) {
		my $ch = $&;
		# skip strings
		advance(1);
		my $esc = 0;
		for (; $pos < length($data); advance(1)) {
			if ($esc) {
				$esc = 0;
			} elsif (substr($data, $pos, 1) eq "\\") {
				$esc = 1;
			} elsif (substr($data, $pos, 1) eq $ch) {
				last;
			}
		}
		advance(1);
	} elsif ($lvl == 0 && substr($data, $pos) =~ /^([^=]\s*\n)({\s*\n)/s) {
		my $plen = $+[1];
		my $slen = $+[2] - $-[2];

		# catch routine entrance
		warn "nested function?\n" if defined $foff;
		$foff = $pos;
		my ($pci, @args) = get_func_args();
		advance($plen);
		if (get_containing_func_name() ne "main" and $pci) {
			$output .= qq{#define _pfl_callerinfo $pci\n};
			$undef_pci = 1;
		}

		# -1 to exclude newline so we don't skew line numbers
		advance($slen - 1);

		if (get_containing_func_name() ne "main" and $inject_tracing) {
			$output .= qq{psclog_trace("enter %s};
			my $endstr = "";
			foreach my $arg (@args) {
				$output .= " $arg=%p:%ld";
				$endstr .= ", (void *)(unsigned long)$arg, (long)$arg";
			}
			$output .= qq{", __func__$endstr);};
		}
		advance(1);
		$lvl++;
	} elsif (substr($data, $pos) =~ /^return(\s*;\s*}?)/s) {
		# catch 'return' without an arg
		my $end = $1;
		$pos += $-[1];
		my $elen = $+[1] - $-[1];
		if ($inject_returns) {
			$output .= "PFL_RETURNX()";
		} else {
			$output .= "return";
		}
		advance($elen);
		dec_level() if $end =~ /}/;
	} elsif (substr($data, $pos) =~ /^return(\s*(?:\(\s*".*?"\s*\)|".*?"))(\s*;\s*}?)/s) {
		# catch 'return' with string literal arg
		my $rv = $1;
		my $end = $2;
		$pos += $-[1];
		my $rvlen = $+[1] - $-[1];
		my $elen = $+[2] - $-[2];
		if ($inject_returns) {
			$output .= "PFL_RETURN_STR("
		} else {
			$output .= "return (";
		}
		advance($rvlen);
		$output .= ")";
		advance($elen);
		dec_level() if $end =~ /}/;
	} elsif (substr($data, $pos) =~ /^return\b(\s*(?:\(\s*\d+\s*\)|\d+))(\s*;\s*}?)/s) {
		# catch 'return' with numeric literal arg
		my $rv = $1;
		my $end = $2;
		$pos += $-[1];
		my $rvlen = $+[1] - $-[1];
		my $elen = $+[2] - $-[2];
		if ($inject_returns) {
			$output .= "PFL_RETURN_LIT(";
		} else {
			$output .= "return (";
		}
		advance($rvlen);
		$output .= ")";
		advance($elen);
		dec_level() if $end =~ /}/;
	} elsif (substr($data, $pos) =~ /^return\b(\s*.*?)(\s*;\s*}?)/s) {
		# catch 'return' with an arg
		my $rv = $1;
		my $end = $2;
		$pos += $-[1];
		my $rvlen = $+[1] - $-[1];
		my $elen = $+[2] - $-[2];
		my $skiplen = 0;

		my $tag = "PFL_RETURN";
		if ($rv =~ /^\s*\(\s*PCPP_STR\s*\((.*)\)\s*\)$/) {
			$pos += $-[1];
			$rvlen -= $-[1];
			$skiplen = $+[0] - $+[1];
			$rvlen -= $skiplen;
			$tag = "PFL_RETURN_STR";
		} elsif ($rv =~ /^\s*\(?\s*yytext\s*\)?\s*$/ && $hacks{yytext}) {
			$tag = "PFL_RETURN_STR";
		} elsif (type_is_aggregate(get_containing_func_return_type)) {
			$tag = "PFL_RETURN_AGGR";
		}

		if ($inject_returns) {
			$output .= "$tag(";
		} else {
			$output .= "return (";
		}
		advance($rvlen);
		$output .= ")";
		$pos += $skiplen;
		advance($elen);
		dec_level() if $end =~ /}/;
	} elsif ($lvl == 1 && substr($data, $pos) =~ /^(?:psc_fatalx?|exit|errx?)\s*\([^;]*?\)\s*;\s*}/s) {
		# XXX this pattern skips psc_fatal("foo; bar")
		# because of the embedded semi-colon

		# skip no return conditions
		advance($+[0]);
		dec_level();
	} elsif ($lvl == 1 && substr($data, $pos) =~ /^goto\s*\w+\s*;\s*}/s) {
		# skip no return conditions
		advance($+[0]);
		dec_level();
	} elsif ($lvl == 1 && substr($data, $pos) =~ m[^\s*/\*\s*NOTREACHED\s*\*/\s*}]s) {
		# skip no return conditions
		advance($+[0]);
		dec_level();
	} elsif ($hacks{yyerrlab} && $lvl == 1 && substr($data, $pos) =~ /^\n\s*yyerrlab:\s*$/m) {
		advance($+[0]);
		$output .= "if (0) goto yyerrlab;";
		$hacks{yyerrlab} = 0;
	} elsif (substr($data, $pos) =~ /^\w+/) {
		advance($+[0]);
	} elsif (substr($data, $pos, 1) eq "{") {
		$lvl++;
		advance(1);
	} elsif (substr($data, $pos, 1) eq "}") {
		if ($lvl == 1 && defined $foff) {
			if (substr($data, $pos + 1) =~ /^\s*\n/s) {
				# catch implicit 'return'
				for (;;) {
					last if containing_func_is_dead;
					last if $hacks{yysyntax_error} and
					    get_containing_func_name eq "yysyntax_error";
					last if $hacks{yylex_return} and
					    get_containing_tag eq "YY_DECL";
					if ($inject_returns) {
						$output .= "PFL_RETURNX();";
					}
					last;
				}
			}
		}
		advance(1);
		dec_level();
	} else {
		advance(1);
	}
}

if ($inject_tracing or $inject_returns) {
	my @lines = split /\n/, $output;
	for (my $i = $#lines; $i > 0; $i--) {
		if ($lines[$i] =~ /^#include/) {
			splice @lines, $i, 0,
			  qq{#include "pfl/log.h"},
			  qq{# $i "$fn"};
			last;
		}
	}
	$output = join "\n", @lines;
}

print $output;
