#! /usr/bin/perl
#
# bison preprocessor for DOSEMU to merge an auxiliary grammar/lexer file
# from a plugin.
# USAGE:
#    bisonpp.pl -y mainfile mergefile >result
# or
#    bisonpp.pl -l mainfile mergefile >result

sub read_file {
# USAGE: read_file("filename", \$buffer);
  local($file, $buf) = @_;
  local $saved_rs = "undef";
  open(RFIN, "<$file") || return 0;
  if (defined $/) { $saved_rs = $/; undef $/; }
  $$buf = <RFIN>;
  if ($saved_rs ne "undef") { $/ = $saved_rs; }
  close(RFIN);
  return 1;
}

sub split_parser_file {
# USAGE: split_parser_file("filename", \@string_array);
  local($fname, $p) = @_;
  local $buf;
  read_file($fname, \$buf) || return 0;
  if ($buf =~
   /(.*?)(\%\{.*?\%\}[ \t]*?\n)(.*?\n)(\%\%[ \t]*?\n.*?\n\%\%[ \t]*?\n)(.*)/s) {
  #  111  22222222222222222222  33333  4444444444444444444444444444444  55
    $$p[0] = $1; # prefix comments
    $$p[1] = $2; # C DECLARATIONS
    $$p[2] = $3; # BISON DECLARATIONS
    $$p[3] = $4; # GRAMMAR RULES
    $$p[4] = $5; # ADDITIONAL C CODE
    return 1;
  }
  return 0;
}

sub split_lexer_merge_file {
# USAGE: split_lexer_merge_file("filename", \%hash);
  local($fname, $p) = @_;
  local ($buf, $ret);
  read_file($fname, \$buf) || return 0;
  $ret = 0;
  while ($buf =~ /\t\/\*\s*([^\*]+?)\s*\*\/\s*\n(.*?\n)(\t\/\*|$)/sgc) {
    $ret = 1;
    $$p{$1} = $2;
    pos($buf) -=3;
  }
  return $ret;
}

sub split_grammar {
# USAGE: split_grammar(\$grammar, \@string_array, \%directory);
  local($grammar, $p, $dir) = @_;
  local $line;
  %$dir = ();
  while ($$grammar =~ /\n(\S+?)(\s*:.*?\n)[\w\%]/sgc) {
    $line = $1 . $2;
    $$dir{$1} = @$p;
    $line =~ s/^\s+//;              # trim leading whites
    $$p[@$p] = $line;
    pos($$grammar) -= 2;
  }
}

sub add_grammar_rule {
# USAGE: add_grammar_rule($key, $rule, \@grammar, \%directory);
  local($key, $rule, $grammar, $dir) = @_;
  $$dir{$key} = @$grammar;
  $$grammar[@$grammar] = $rule;
}

sub merge_grammar_rule {
# USAGE: merge_grammar_rule($key, $rule, \@grammar, \%directory);
  local($key, $rule, $grammar, $dir) = @_;
  local($i, $rest);
  $i = $$dir{$key};
  $$grammar[$i] =~ /^\S+?\s*:\s*(.*)/s;
  $rest = $1;
  $rule =~ s/[ \t]*;\s*\n//s;	# remove trailing ';'
  $rule =~ s/^(\S+?\s*:\s*)\|\s*/$1/s; # to be save: remove leading '|'
  $$grammar[$i] = "$rule\t\t| $rest";
}

if ($ARGV[2] eq "") {
  print STDERR "USAGE: { -y | -l } bisonpp.pl mainfile mergefile >result\n";
  exit 1;
}

$mode = $ARGV[0];
$mainfile = $ARGV[1];
$mergefile = $ARGV[2];

if ( $mode eq "-l" ) {
  # preprocessing lexer file
  read_file($mainfile, \$main) || die("cannot read file $mainfile\n");
  split_lexer_merge_file($mergefile, \%h) || die("cannot read/pars file $$mergefile\n");
  foreach $i (keys(%h)) {
    if (  $main !~ s/\n\t\/\* $i \*\//\n\t\/\* $i \*\/\n$h{$i}/s ) {
      $main =~ s/\n\t\/\* strings \*\//\n\t\/\* $i \*\/\n$h{$i}\n\n\t\/\* strings \*\//s;
    }
  }
  print $main;
  exit 0;
}

# read main parser file
split_parser_file($mainfile, \@main) || die("cannot read file $mainfile or pars error\n");

# read in file we want to merge into main parser file
split_parser_file($mergefile, \@merge) || die("cannot read file $mergefile or pars error\n");

# append comment prefix
if ($merge[0] !~ /^[ \t\n]*$/s) {
  $main[0] .= $merge[0];
}

# append C DECLARATIONS
if ($merge[1] !~ /\%\{[ \t\n]*\%\}/s) {
  $merge[1] =~ s/\%\{(.*?\%\})/$1/s;
  $main[1] =~ s/\n\%\}/\n\n$merge[1]/;
}

# append BISON DECLARATIONS
if ($merge[2] !~ /^[ \t\n]*$/s) {
  $main[2] .= $merge[2];
}

# merge grammar contents
split_grammar(\$main[3], \@mainr, \%maind);
split_grammar(\$merge[3], \@merger, \%merged);
foreach $i (@merger) {
  $i =~ /^(\S+?)\s*:/s;
  $key = $1;
  if ($maind{$key}) { 
    merge_grammar_rule($key, $i, \@mainr, \%maind);
  }
  else {
    add_grammar_rule($key, $i, \@mainr, \%maind);
  }
}

# append ADDITIONAL C CODE
if ($merge[4] !~ /^[ \t\n]*$/s) {
  $main[4] .= $merge[4];
}

# print assembled whole
for ($i=0; $i < 5; $i++) {
  if ($i == 3) {
    print "\%\%\n\n";
    foreach $j (@mainr) {
      print "$j";
    }
    print "\%\%\n";
  }
  else {print $main[$i];}
}
exit 0;
