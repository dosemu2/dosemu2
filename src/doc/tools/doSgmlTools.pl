#!/usr/bin/perl -w

#
# This script wraps the DOSEMU calls to sgmltools to try to get the best 
# output on as many systems as possible. It can also be used to upgrade
# parts of your SGML system.
#

use strict;
#use Getopt::Mixed;
use Getopt::Std;
use File::Basename;

my ($openJadeOptions) = "-V %generate-article-toc% -V %section-autolabel% -V %shade-verbatim% -V %indent-screen-lines%='    ' -V '(define (toc-depth nd) 3 )'";
my ($lynxOptions) = "-force_html -nolist -dump";

my ($softVer) = "0.99";

#&Getopt::Mixed::getOptions("v verbose>v V Version>V t text>t h html>h ? help>? c check>c");
&getopts('vVth?c');

my ($verbose) = 0;

if (defined $main::opt_V && $main::opt_V == 1) {
  &displayVersion;
  exit(0);
}

if ((defined $main::opt__ && $main::opt__ == 1) 
    || (!defined $main::opt_h && !defined $main::opt_t 
	&& !defined $main::opt_c)) {
  &usage();
  exit(0);
}

if (defined $main::opt_v && $main::opt_v == 1) {
  $verbose = 1;
  $| = 1;
  print "Being verbose\n";
}

my (%versions) = &getSoftwareVersions();
my (%theStylesheets) = &getStylesheets();

if (defined $main::opt_c && $main::opt_c == 1) {
  &checkSetup();
  exit(0);
}

my ($theFile);

# Process the files

foreach $theFile (@ARGV) {

  if (defined $main::opt_h && $main::opt_h == 1) {
    &convertToHTML($theFile);
  }
  
  if (defined $main::opt_t && $main::opt_t == 1) {
    &convertToText($theFile);
  }
}


sub usage {
print <<"EO_USAGE";
$0 usage:

  $0 [options] [actions] [file ...]

  Options:
  -v, --verbose
    Turn on verbose output

  Actions:
  -h, --html
    Convert to HTML

  -s, --htmls
    Convert to multiple HTML files

  -t, --text
    Convert to Text

  -V, --Version
    Print version number and exit

  -c, --check
    Check the setup

At least one of the actions conversions must be specified.

EO_USAGE
}

sub getSoftwareVersions {
  my (%versions, $result);

  # Check for SGMLTOOLS

  $result = `sgmltools -V`;

  if ($result =~ /version ([^ \n]+)/) {
    $versions{'sgmltools'} = $1;
    if ($verbose) {
      print "Found SGMLtools version $1\n";
    }
  } else {
    $result = `sgml2html --wibblefoo`;
    if ($result =~ /papersize/) {
      $versions{'sgmltools'} = 1;
      if ($verbose) {
	print "Found SGMLtools version 1\n";
      }
    }
  }

  # Check for (open)Jade

  $result = `openjade -v --wibblefoo 2>&1`;

  if ($result =~ /\"?[Oo]pen[Jj]ade\"? version "([^\"]+)"/) {
    $versions{'openjade'} = $1;
    if ($verbose) {
      print "Found OpenJade version $1\n";
    }
  }

  $result = `jade -v --wibblefoo 2>&1`;
  if ($result =~ /Jade version "([^\"]+)"/) {
    $versions{'jade'} = $1;
    if ($verbose) {
      print "Found Jade version $1\n";
    }
  }
   
  # Check for Lynx

  $result = `lynx -version`;

  if ($result =~ /Lynx Version ([^ ]+)/) {
    $versions{'lynx'} = $1;
    if ($verbose) {
      print "Found Lynx version $1\n";
    }
  }

  return %versions;
}

sub getStylesheets {
  my (%theStylsheet);
  my (@theCatalogs) = ();
  my ($thisCatalog);
  my ($line, $file);
  my ($type, $fname);

  if (exists $ENV{'SGML_CATALOG_FILES'}) {
    @theCatalogs = split(/:/,$ENV{'SGML_CATALOG_FILES'});
  } else {
    if ( -e "/etc/sgml/catalog") {

      if ($verbose) {
	print "Added entry for \"/etc/sgml/catalog\" to catalog list\n";
      }

      unshift (@theCatalogs, "/etc/sgml/catalog");
    }
    if ( -e "/usr/local/lib/sgml/catalog") {

      if ($verbose) {
	print "Added entry for \"/usr/local/lib/sgml/catalog\" to catalog list.\n";
      }
      
      unshift (@theCatalogs, "/usr/local/lib/sgml/catalog");
    }
  }

  while ($#theCatalogs >= 0) {
    $thisCatalog = shift (@theCatalogs);

    if (! defined $thisCatalog || length($thisCatalog) < 1) {
      if ($verbose) {
	print "Skipping null catalog entru\n";
      }
      next;
    }

    open (CATFILE, $thisCatalog)
      or do {
	warn "Failed to open catalog file: $thisCatalog\n";
	next;
      };

    if ($verbose) {
      print "Scanning catalog file: $thisCatalog\n";
    }

    $file = "";
    while (defined ($line = <CATFILE>)) {
      $file .= $line;
    }
    close (CATFILE);

    while ($file =~ /CATALOG\s+\"([^\"]+)\"/gm) {
      if ($verbose) {
	print "Adding entry for \"$1\" to catalog list.\n";
      }
      
      unshift (@theCatalogs, $1);
    }

    while ($file =~ m!PUBLIC\s+\"-//.*//.*DocBook (Print|HTML) Stylesheet//EN\"\s+(\S+)!gm) {
      $type = lc($1);
      $fname = $2;
      if (substr($fname,0,1) ne "/") {
	$fname = &dirname($thisCatalog) ."/" .$fname;
      }

      if ($verbose) {
	print "Set \"$type\" stylesheet to \"$fname\".\n";
      }
      
      $theStylesheets{$type} = $fname;
    }

  }
  
  return %theStylesheets;
}

sub convertToHTML {
  my ($theFile, $theOutputFile) = @_;
  my ($command);

  if (! defined $theOutputFile) {
    $theOutputFile = &dirname($theFile) ."/" .&basename($theFile,('.sgml','.xml')) 
      .".html";
  }

  if ($verbose) {
    print "Converting $theFile to HTML: $theOutputFile\n";
  }

  $command = "";

  if (exists $versions{'sgmltools'} && $versions{'sgmltools'} ge "3.0.2") {
    # Assume Version 3.0.2 and above are.
    $command = "sgmltools -b onehtml --jade-opt=\"$openJadeOptions\" $theFile";
    
  } elsif (exists $versions{'sgmltools'} && $versions{'sgmltools'} ge "3.0") {
    # Assume Version 3 and above are.
    $command = "sgmltools -b html -jade-opt=\"-V nochunks $openJadeOptions -o $theOutputFile \" $theFile";

  } elsif (exists $versions{'openjade'} && $versions{'openjade'} ge "1.3") {
    # Try running openjade (>= 1.3) directly.
    $command = "openjade -t sgml -V nochunks $openJadeOptions  -d " .$theStylesheets{'html'} ." $theFile > $theOutputFile";

  } elsif (exists $versions{'openjade'}) {
    # Try running openjade directly.
    $command = "openjade -t sgml -V nochunks $openJadeOptions  -d " .$theStylesheets{'html'} ." $theFile > $theOutputFile";

  } elsif (exists $versions{'jade'}) {
    # Try running jade directly.
    $command = "jade -t sgml -d " .$theStylesheets{'html'} ." $theFile > $theOutputFile";

  } elsif (exists $versions{'sgmltools'} && $versions{'sgmltools'} ge "2.0") {
    # Assume Version 2
    $command = "sgmltools -b html -o $theOutputFile $theFile";

  }
  
  if (length($command)) {
    if ($verbose) {
      print "About to execute:\n$command\n";
    }
    
    system $command;
  } else {
    print "No command to execute -- can't convert.\n";
  }
}

sub convertToText {
  my ($theFile) = @_;
  my ($theOutputFile) = &dirname($theFile) ."/" .&basename($theFile, ('.sgml', '.xml')) .".txt";
  my ($theHTMLFile) = "";

  my ($command);

  if ($verbose) {
    print "Converting $theFile to text: $theOutputFile\n";
  }

  if (exists $versions{'sgmltools'} && $versions{'sgmltools'} ge "3.0.2") {
    # Assume Version 3.0.2 and above are.
    $command = "sgmltools -b lynx --jade-opt=\"$openJadeOptions\" $theFile";
  } else {
    if (!defined $main::opt_h) {
      # Need to build the HTML first
      $theHTMLFile = $theOutputFile.".t.html";
      &convertToHTML($theFile, $theHTMLFile);
    } else {
      $theHTMLFile = $theOutputFile;
      $theHTMLFile =~ s/\.txt$/\.html/;
    }

#    if (exists $versions{'lynx'} && $versions{'lynx'} ge "2.8.3") {
#      $command = "lynx $lynxOptions -with_backspaces $theHTMLFile > $theOutputFile";
#    } elsif (exists $versions{'lynx'}) { 
      $command = "lynx $lynxOptions $theHTMLFile > $theOutputFile";
#    }  
  }

  if (length($command)) {
    if ($verbose) {
      print "About to execute:\n$command\n";
    }
    
    system $command;
  } else {
    print "No command to execute -- can't convert.\n";
  }

  if (!defined $main::opt_h) {
    if ($verbose) {
      print "Cleaning up temporary HTML file\n";
    }
    
    if (length($theHTMLFile)) {
      unlink ($theHTMLFile);
    }
  }
}

sub displayVersion {
  print "$0: version $softVer\n";
}

sub checkSetup {
  print "OpenJade [",
    (exists $versions{'openjade'}) ? $versions{'openjade'} : "Not installed",
    "] - ";
  if (exists $versions{'openjade'}) {
    if ($versions{'openjade'} ge "1.3") {
      print "OK\n";
    } else {
      print "upgrade to at least version 1.3 (http://openjade.sourcefourge.net/)\n";
    }
  } else {
    print "install at least OpenJade version 1.3 (http://openjade.sourcefourge.net/)\n";
  }

  print "Jade [",
    (exists $versions{'jade'}) ? $versions{'jade'} : "Not installed",
    "] - OpenJade is preferred\n";

  my ($styleVer) = "Not installed";

  my ($line);
  if (exists $theStylesheets{'html'}) {
    open (STYLE,$theStylesheets{'html'})
      or warn ("Failed to open HTML stylesheet \"" .$theStylesheets{'html'} ."\": $!\n");
    while (defined ($line = <STYLE>)) {
      if ($line =~ /Id: docbook.dsl,v (\S+)/) {
	$styleVer = $1;
      }
    }

    close (STYLE);
  }

  print "HTML Stylesheets [$styleVer] - ";
  if ($styleVer ge "1.27") {
    print "OK\n";
  } elsif ($styleVer eq "Not installed") {
    print "install at least version 1.54 (http://www.nwalsh.com/docbook/dsssl/)\n";
  } else {
    print "upgrade to at least version 1.54 (http://www.nwalsh.com/docbook/dsssl/)\n";
  }

  print "Lynx [",
    (exists $versions{'lynx'}) ? $versions{'lynx'} : "Not installed",
    "] - ";
  if (exists $versions{'lynx'}) {
    if ($versions{'lynx'} ge "2.8.3") {
      print "OK\n";
    } else {
      print "suggest upgrading to at least version 2.8.3 (http://lynx,browser.org/)\n";
    }
  } else {
    print "install at least lynx version 2.8.3 (http://lynx.brwoser.org/)\n";
  }


}
