#!/usr/bin/perl -w

use strict;

my ($data, $line);

$data ="";

while (defined ($line = <>)) { 
  $data .= $line;
}
       
$data =~ s/&lcub;/\{/g;
$data =~ s/&rcub;/\}/g;
$data =~ s/&lsqb;/\[/g;
$data =~ s/&rsqb;/\]/g;
$data =~ s/&bsol;/\\/g;
$data =~ s/&num;/\#/g;
$data =~ s/&circ;/^/g;
$data =~ s/&verbar;/|/g;
$data =~ s/&lowbar;/_/g;
$data =~ s/&percnt;/%/g;
$data =~ s/&hellip;/.../g;
$data =~ s/&tilde;/~/g;

# XMLish stuff
#$data =~ s/<!DOCTYPE [^>]*>//;
#$data =~ s#<(xref [^>]+)>#<$1 />#g;

$data =~ s/<para>\s+<para>/<para>/gm;

print $data;       
