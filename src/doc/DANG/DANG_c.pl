#!/usr/bin/perl
#

if ($0 =~ /DANG_c\.pl/) {$dang_mode = 1;}
else {$dang_mode = 0;}

# if (dang_mode) {
#
# DANG Compiler
# -------------
#
# Converts the comments in the DOSEMU source code into items suitable for
# inclusion in the DANG.
#
# The Markers are :
#
# DANG_BEGIN_MODULE    / DANG_END_MODULE
# DANG_BEGIN_CHANGELOG / DANG_END_CHANGELOG
# DANG_BEGIN_REMARK    / DANG_END_REMARK
# DANG_BEGIN_FUNCTION  / DANG_END_FUNCTION
# DANG_BEGIN_NEWIDEA   / DANG_END_NEWIDEA
# DANG_FIXTHIS
#
#
# Version 0.1:
# 11/ 6/94 AM    Coding commences ... finally !
# Version 0.2:
# 12/ 6/94 AM    James suggests SGML - so here goes (it's easier than texinfo)
# Version 0.3:
# ??/??/?? AM    Added the configuration file
# Version 0.4:
#  7/ 9/94 AM    Made major modifications to rationalise the FUNCTION system.
#                Modified the options. Added Indexing. Fixed single line
#                item stripping.
# Version 0.5:
#  3/ 2/96 AM    Made this thing PERL5 happy, and added a couple of minor
#		 features. Added -i. This is likely to be the last version.
#                Hopefully this will be succeeded by a more generic system.
# Version 0.5a:
# 18/ 3/97 AM    Changed the method of finding the version number.
#
# Version 0.6:
# 18/ 6/00 AM    Finally produces valid linuxdoc-96 output
#
# <NOTE: This doesn't use DANG markers, since it is a) not part of DOSEMU and
#  b) the markers at the start would confuse things ...!>
#
# }
#
# if (!dang_mode) {
#
# NOTE: This is retrieved from the work of Alistair MacDonald
#       and (though adapted to general purpose) remains
#       (C) Alistair MacDonald <alistair@slitesys.demon.co.uk> 
#
#       The original software is named DANG (Dosemu Alterer Novice Guide) and
#       as such is part of the DOSEMU distribution.
#
# SIDOC (Source Imbedded DOCumentation) Compiler
# ---------------------------------------------
#
# Converts the comments in the source code into items suitable for
# inclusion in the SIDOC.
#
# The Markers are :
#
# SIDOC_BEGIN_MODULE    / SIDOC_END_MODULE
# SIDOC_BEGIN_CHANGELOG / SIDOC_END_CHANGELOG
# SIDOC_BEGIN_REMARK    / SIDOC_END_REMARK
# SIDOC_BEGIN_FUNCTION  / SIDOC_END_FUNCTION
# SIDOC_BEGIN_NEWIDEA   / SIDOC_END_NEWIDEA
# SIDOC_FIXTHIS
#
# }
#
#
# Command line options 
# --------------------
# -v           - verbose. 
# -c fname     - use configuration file.
# -i           - 'Irritate' by commenting on missing items.
# fname ...    - verify the files.
#

undef $opt_c;

require "getopts.pl";

&Getopts('ivc:');		# Look for the flags ...

{
    local (*INPUT);

    undef @groups;
    undef @intro;
    undef @finally;

    $main_title = "";
    $main_author = "";
    $main_abstract = "";

    if ($opt_c !~ /.+/) {
	die "$0: The option -c takes one compulsory argument - a configuration filename.\n";
    } elsif (defined $opt_c) {
	$INPUT = "<". $opt_c;
	open (INPUT) || die "Can't open $INPUT as a configuration file.";
    
	while (<INPUT>) {
	    &build_config ($_);
	}
	&build_config (undef);
	close INPUT;

	if ($dang_mode) {&get_dosemu_version;}
	else {&get_version;}
	
	if ($dang_mode) {$OUTPUT = ">>DANG.sgml";}
	else {$OUTPUT = ">>SIDOC.sgml";}
	open OUTPUT;

	print OUTPUT <<"EndOfPreamble";
<!doctype linuxdoc public "-//LinuxDoc//DTD LinuxDoc 96//EN">

<linuxdoc>

<article>

<titlepag>
<title> $main_title </title>
<author> <name> $main_author </name> </author>
<date> version $version </date>
<abstract>
$main_abstract
</abstract>
</titlepag>

<toc>

<sect><heading>Introduction</heading>

<p>
EndOfPreamble


        if (defined @intro) {
	    $INPUT = "<". $base_dir. shift @intro;
	    open (INPUT) || die "Can't open $INPUT as a introduction file.";

	    while (<INPUT>) {
		$temp = &protect_specials($_);
		print OUTPUT $temp;
	    }
	    print OUTPUT "\n";
	    
	    close INPUT;

	    foreach (@intro) {
		$INPUT = "<". $base_dir. $_;
		open (INPUT) || die "Can't open $INPUT as an introduction file.";
		
		print OUTPUT "-----\n\n";
		while (<INPUT>) {
		    $temp = &protect_specials($_);
		    print OUTPUT $temp;
		}
		print OUTPUT "\n";
		
		close $INPUT;
	    }
	    
	    print OUTPUT "\n\n";
	    
	    undef @intro;
	} elsif ($opt_i) {
	    print OUTPUT "<p>There appears to be no Introduction at this time.</p>\n\n";
	}			       

	print OUTPUT "</p></sect>\n\n";
	
	foreach $group (@groups) {
	    ($name, $summary, @elements) = split(/[ \t\n]+/, $group);
	   
	    if ((!defined $name) || (!defined $summary) || (!defined @elements))
	    {
		die "Missing Data in the group $group\n";
	    }

	    print OUTPUT "<sect><heading>The $name group of Modules</heading>\n<p>\n";
	    
	    $INPUT = "<". $base_dir. $summary;
	    open (INPUT) || die "Can't open $INPUT as a summary file.";
	    
	    while (<INPUT>) {
		$temp = &protect_specials($_);
		print OUTPUT $temp;
	    }
	    print OUTPUT "</p>\n\n";
	    
	    close INPUT;
	    
	    foreach $fname (@elements)
	    {
		&scan_file ($fname);
	    }

	    print OUTPUT "</sect>\n\n";
	}      

	if (defined @finally) {

	    print OUTPUT "<sect><heading>And Finally ...</heading>\n\n<p>";
	    
	    $INPUT = "<". $base_dir. shift @finally;
	    open (INPUT) || die "Can't open $INPUT as a termination file.";
	    
	    while (<INPUT>) {
		$temp = &protect_specials($_);
		print OUTPUT $temp;
	    }
	    print OUTPUT "</p>\n";
	    
	    close INPUT;
	    
	    foreach (@finally) {
		$INPUT = "<". $base_dir. $_;
		open (INPUT) || die "Can't open $INPUT as an termination file.";
		
		print OUTPUT "<p>-----</p>\n\n<p>";
		while (<INPUT>) {
		    $temp = &protect_specials($_);
		    print OUTPUT $temp;
		}
		print OUTPUT "</p>\n";
		
		close $INPUT;
	    }

	    print OUTPUT "\n\n";

	    undef @finally;
	}			       
	print OUTPUT "</sect>\n\n";


	print OUTPUT "\n</article>\n";
	print OUTPUT "\n</linuxdoc>\n";
    } else {		# No configuration file ...
	$OUTPUT = ">/dev/null";
	foreach $fname (@ARGV)
	{
	    &scan_file ($fname);
	}				
		
    }
	 
    close OUTPUT;
}			       

# Sets $version to be the Version of DOSEMU used to generate the source.
# This is grabbed from dosconfig.h

sub get_dosemu_version  {
    local (*INPUT);
    local ($dat);
    $INPUT = $base_dir. "../VERSION";
    open INPUT;
    $dat = <INPUT>;
    chop $dat;
    $version = "dosemu-$dat";
    close INPUT;
}

# Sets $version to be the Version of the source paket.
# This is grabbed from main Makefile

sub get_version  {
    local (*INPUT);
    local ($pl, $ver, $sublv);

    $INPUT = $base_dir. "Makefile";
    open INPUT;

    while (<INPUT>) {
        if (/PATCHLEVEL="(.*)"/) {
	    $pl = $1;
	    next;
        }        
        if (/^VERSION="(.*)"/) {
	    $ver = $1;
        }        
        if (/^SUBLEVEL="(.*)"/) {
	    $sublv = $1;
        }        
    }

    $version = "v$ver.$sublv.$pl";
    close INPUT;
}

# Yup - Its another one of these pattern matchers .... 8-(
#

sub build_config {
    local ($input) = $_;

    # Right - Config files:
    # '#' signify comments - any extra character will be deleted.
    # GROUP: <name> <summary> <file1> <file2> ... <filen>
    #   This may be spread over multiple lines ... Groups are related files
    #   ie the mouse files, the video files. They are related in DANG/SIDOC too ...
    # SUMMARY: <summary>
    #   The master summary file.
    # BASEDIR: directory
    #   Directory to run _all_ paths from ... (no direct paths .. 8-( )
    #
    # note: Comments may appear anywhere on the line, but the keywords must
    #       be the first non-whitespace characters ...

    if (!defined $input) {
	if ($group) {
	    push (@groups, $data);
	    undef $data;
	    $group --;
	}

	return;
    }

    s/\#.*$//;			# Strip out comments ...

    chop;

    if (/^\s*TITLE:\s*/) {
        $main_title = "$main_title" . $';
	return;
    }

    if (/^\s*AUTHOR:\s*/) {
        $main_author = "$main_author" . $';
	return;
    }

    if (/^\s*ABSTRACT:\s*/) {
        $main_abstract = "$main_abstract" . $';
	return;
    }

    if (/^\s*GROUP:\s*/) {
	if ($group) {
	    push (@groups, $data);
	} else {
	    $group ++;
	}
	$data = $';
	return;
    }

    if (/^\s*INTRODUCTION:\s*/) {
	if ($group) {
	    push (@groups, $data);
	    undef $data;
	    $group --;
	}
	push (@intro, $');
	return;
    }

    if (/^\s*FINALLY:\s*/) {
	if ($group) {
	    push (@groups, $data);
	    undef $data;
	    $group --;
	}
	push (@finally, $');
	return;
    }

    if (/^\s*BASEDIR:\s*/) {
	if ($group) {
	    push (@groups, $data);
	    undef $data;
	    $group --;
	}
	$base_dir = $';
	return;
    }

    if ($group) {
	$data .= $_;
    }
}



# This converts any special characters in the output into the relevant
# protected items ....

sub protect_specials {
    local ($text) = @_;
    local ($save);

    $save = $*;
    $* = 1;

    $text =~ s/&/&amp;/g;
    $text =~ s/\<\\/&etago;/g;
    $text =~ s/</&lt;/g;
    $text =~ s/>/&gt;/g;
    $text =~ s/\$/&dollar;/g;
    $text =~ s/\#/&num;/g;
    $text =~ s/%/&percnt;/g;
#    $text =~ s/\'{1}/''/g;
#    $text =~ s/\`{1}/``/g;
#    $text =~ s/"/&dquo;/g;     # " (For emacs !)
    $text =~ s/\[/&lsqb;/g;
    $text =~ s/\]/&rsqb;/g;

    $* = $save;

    return $text;
}


# This back-converts any special characters in the output into the relevant
# protected items ....

sub un_protect_specials {
    local ($text) = @_;
    local ($save);

    $save = $*;
    $* = 1;

    $text =~ s/&rsqb;/\]/g;

    $text =~ s/&lsqb;/\[/g;
    $text =~ s/&dquot;/"/g;     # " (For emacs !)
    $text =~ s/&percnt;/%/g;
    $text =~ s/&num;/\#/g;
    $text =~ s/&dollar;/\$/g;
    $text =~ s/&gt;/>/g;
    $text =~ s/&lt;/</g;
    $text =~ s/&etago;/\<\\/g;
    $text =~ s/&amp;/&/g;


    $* = $save;

    return $text;
}


#
# This is the routine which does all the scanning. 
# The scanning information is only mentioned here. This program is not intended
# to be expandable - that would require the use of a constructed function to 
# achieve any sort of speed. (Which would look neater I suspect ... 8-( )
#
# NOTE: I hate perls pattern matching sometimes. It would be nice to have an 
#       option to have the smallest match instead of only having the longest
#       match. Ho Hum - such is life.
#

sub scan_file  {
    local ($filename) = @_;

    local ($INPUT) = "<". $base_dir. $filename;


    undef @fixthis;		# Just to make sure ...!
    undef @functions;
    undef @structures;
    undef @modules;
    undef @newideas;
    undef @changes;
    undef @remarks;


    # Slurp in the file .... 
    # Paragraph mode might be simpler, but ....

    if ($opt_v) {
	print "Please Wait - Loading file $filename & Scanning\n";
    }

    open INPUT;

    $line = 0;

    while (<INPUT>)
    {
	$line ++;

	if (/(SIDOC|DANG)_BEGIN_MODULE/) {
	    # Module Start Found ...
	    if ($module != 0) {
		print "Already Found a Module start at line $start_line. Ignoring.\n";
	    } else {
		$module ++;
		if ($scanning != 0) {
		    &abort_scan ($filename)
		} else {
		    if ($opt_v) {
			print "MODULE Start found at line $line.\n";
		    }

		    $scanning ++;
		    $start_line = $line;
		    $dat = $';
		}
	    }
	    next;
	}

	if (/(SIDOC|DANG)_END_MODULE/) {
	    # Module End Found.
	    if ($module != 1) {
		print "No Module Start found (END at line $line). Ignoring.\n";
	    } else {
		if ($opt_v) {
		    print "MODULE end found at line $line.\n";
		}

		$module --;
		$dat .= $`;
		push (@modules, &strip_comments ($dat));
		undef $dat;
		$scanning --;
	    }
	    next;
	}

	if (/(SIDOC|DANG)_BEGIN_CHANGELOG/) {
	    # Changelog Start Found ...
	    if ($changes != 0) {
		print "Already Found a Changelog start at line $start_line. Ignoring.\n";
	    } else {
		$changes ++;
		if ($scanning != 0) {
		    &abort_scan ($filename)
		} else {
		    if ($opt_v) {
			print "CHANGELOG Start found at line $line.\n";
		    }

		    $scanning ++;
		    $start_line = $line;
		    $dat = $';
		}
	    }
	    next;
	}

	if (/(SIDOC|DANG)_END_CHANGELOG/) {
	    # Changelog End Found.
	    if ($changes != 1) {
		print "No Changelog Start found (END at line $line). Ignoring.\n";
	    } else {
		if ($opt_v) {
		    print "CHANGELOG end found at line $line.\n";
		}		# 

		$changes --;
		$dat .= $`;
		push (@changes, &strip_comments ($dat));
		undef $dat;
		$scanning --;
	    }
	    next;
	}

	if (/(SIDOC|DANG)_BEGIN_FUNCTION/) {
	    # Function Start Found ...
	    if ($functions != 0) {
		print "Already Found a Function start at line $start_line. Ignoring.\n";
	    } else {
		$functions ++;
		if ($scanning != 0) {
		    &abort_scan ($filename)
		} else {
		    if ($opt_v) {
			print "FUNCTION Start found at line $line.\n";
		    }

		    $scanning ++;
		    $start_line = $line;
		    $dat = $';
		}
	    }
	    next;
	}

	if (/(SIDOC|DANG)_END_FUNCTION/) {
	    # Function End Found.
	    if ($functions != 1) {
		print "No Function Start found (END at line $line). Ignoring.\n";
	    } else {
		if ($opt_v) {
		    print "FUNCTION end found at line $line.\n";
		}

		$functions --;
		$dat .= $`;
		push (@functions, &strip_comments ($dat));
		undef $dat;
		$scanning --;
	    }
	    next;
	}

	if (/(SIDOC|DANG)_BEGIN_STRUCT/) {
	    # Structure Start Found ...
	    if ($structures != 0) {
		print "Already Found a Structure start at line $start_line. Ignoring.\n";
	    } else {
		$structures ++;
		if ($scanning != 0) {
		    &abort_scan ($filename)
		} else {
		    if ($opt_v) {
			print "FUNCTION Start found at line $line.\n";
		    }

		    $scanning ++;
		    $start_line = $line;
		    $dat = $';
		}
	    }
	    next;
	}

	if (/(SIDOC|DANG)_END_STRUCT/) {
	    # Function End Found.
	    if ($structures != 1) {
		print "No Function Start found (END at line $line). Ignoring.\n";
	    } else {
		if ($opt_v) {
		    print "FUNCTION end found at line $line.\n";
		}

		$structures --;
#this also prefixes '/*' if present :-(		$dat .= $`;
		push (@structures, &strip_comments ($dat));
		undef $dat;
		$scanning --;
	    }
	    next;
	}

	if (/(SIDOC|DANG)_BEGIN_REMARK/) {
	    # Remark Start Found ...
	    if ($remarks != 0) {
		print "Already Found a Remark start at line $start_line. Ignoring.\n";
	    } else {
		$remarks ++;
		if ($scanning != 0) {
		    &abort_scan ($filename)
		} else {
		    if ($opt_v) {
			print "REMARK Start found at line $line.\n";
		    }

		    $scanning ++;
		    $start_line = $line;
		    $dat = $';
		}
	    }
	    next;
	}

	if (/(SIDOC|DANG)_END_REMARK/) {
	    # Remark End Found.
	    if ($remarks != 1) {
		print "No Remark Start found (END at line $line). Ignoring.\n";
	    } else {
		if ($opt_v) {
		    print "REMARK end found at line $line.\n";
		}

		$remarks --;
		$dat .= $`;
		push (@remarks, &strip_comments ($dat));
		undef $dat;
		$scanning --;
	    }
	    next;
	}

	if (/(SIDOC|DANG)_BEGIN_NEWIDEA/) {
	    # NEWIDEA Start Found ...
	    if ($newideas != 0) {
		print "Already Found a NewIdea start at line $start_line. Ignoring.\n";
	    } else {
		$newideas ++;
		if ($scanning != 0) {
		    &abort_scan ($filename)
		} else {
		    if ($opt_v)  {
			print "NEWIDEA Start found at line $line.\n";
		    }

		    $scanning ++;
		    $start_line = $line;
		    $dat = $';
		}
	    }
	    next;
	}

	if (/(SIDOC|DANG)_END_NEWIDEA/) {
	    # NEWIDEA End Found.
	    if ($newideas != 1) {
		print "No NewIdeas Start found (END at line $line). Ignoring.\n";
	    } else {
		if ($opt_v) {
		    print "NEWIDEA end found at line $line.\n";
		}

		$newideas --;
		$dat .= $`;
		push (@newideas, &strip_comments ($dat));
		undef $dat;
		$scanning --;
	    }
	    next;
	}

	if (/(SIDOC|DANG)_FIXTHIS/) {
	    # Fixthis Found ...
	    if ($scanning != 0) {
		die "Alreading Scanning an Item - Missing end (started at line $start_line.) Aborting\n";
	    } else {
		if ($opt_v) {
		    print "FIXTHIS found at line $line.\n";
		}

		$start_line = $line;
		push (@fixthis, &strip_comments ($'));
		undef $dat;
	    }
	    next;
	}

	$dat .= $_;
    }
    close INPUT;

    if ($opt_v) {
	print "Finished scanning\n";
	print "Interpreting the file\n";	
    }

    &handle_modules ($filename);
    &handle_changes ($filename);
    &handle_functions ($filename);
    &handle_structures ($filename);
    &handle_remarks ($filename);
    &handle_fixthis ($filename);
    &handle_newideas ($filename);

    if ($opt_v) {
	print "Finished with file $filename\n";
    }

}

sub abort_scan {
    local ($filename) = @_;
    
    if ($module == 1) {
	die ("At line $line in $filename I found a new marker.\nI was already scanning a MODULE section which started at line $start_line.\n*** ABORTING ***\n");
    }
    if ($functions == 1) {
	die ("At line $line in $filename I found a new marker.\nI was already scanning a FUNCTION section which started at line $start_line.\n*** ABORTING ***\n");
    }
    if ($structures == 1) {
	die ("At line $line in $filename I found a new marker.\nI was already scanning a FUNCTION section which started at line $start_line.\n*** ABORTING ***\n");
    }
    if ($remarks == 1) {
	die ("At line $line in $filename I found a new marker.\nI was already scanning a REMARKS section which started at line $start_line.\n*** ABORTING ***\n");
    }
    if ($changes == 1) {
	die ("At line $line in $filename I found a new marker.\nI was already scanning a CHANGELOG section which started at line $start_line.\n*** ABORTING ***\n");
    }
    if ($newideas == 1) {
	die ("At line $line in $filename I found a new marker.\nI was already scanning a NEWIDEAS section which started at line $start_line.\n*** ABORTING ***\n");
    }

    die ("At line $line in $filename I found a new marker.\nI was already scanning something which started at line $start_line.\n(I would tell you what it was, but apparently I've fogotten what it was!)\n*** ABORTING ***\n");
    
}

sub strip_comments {
    local ($text) = @_;
    local ($save);

    $save = $*;
    $* = 1;

#             / */ \s* \n \s* /*              / \n  /g
    $text =~ s#\*/\s*\n\s*/\*#\n#g;
    $text =~ s#^\s*//##g;

    $text =~ s#^\s*\*/\s*\n##g;
    $text =~ s#^\s*/\*\s*\n##g;
    $text =~ s#^\s*/\*\s*##g;
#print "AAAAAAA $text BBBBBBB\n";

    $text =~ s#^\s*\*##g;
    
    $text =~ s#^\s*# #;
    $text =~ s#^\n# \n#;
#    $text =~ s#\s*\*//?$# #;

    $* = $save;

    return &protect_specials ($text);
}

sub handle_subremarks {
    local ($dat,$all) = @_;
    local (@data);
    local ($inverb, $inremark);

    @data = split (/\n/, $dat);
    $inverb = 0;
    $inremark = 0;
    foreach $data (@data) {
	if ($data =~ /^\s*VERB\s*/) {
	    print OUTPUT "</p><p><verb>\n";
	    $inverb =1;
	    next;
	}
	if ($inverb) {
	    if ($data =~ /^\s*\/VERB\s*/) {
		print OUTPUT "</verb></p>\n<p>";
		$inverb =0;
	    }
	    else {
		$data = un_protect_specials($data);
		print OUTPUT "$data\n";
	    }
	    next;
	}
	if ($all) {
	    print OUTPUT "$data\n";
	    next;
	}
	if ($data =~ /^\s*REMARK\s*/) {
	    $inremark =1;
	    next;
	}
	if ($inremark) {
	    if ($data =~ /^\s*\/REMARK\s*/) {
		$inremark =0;
	    }
	    else {
		print OUTPUT "$data\n";
	    }
	    next;
	}
    }    
}

sub handle_modules {
    local ($filename) = @_;
    local ($mod, $count);

    $count = 0;

    if (defined @modules && $#modules > 0) {
      print OUTPUT "<sect1><heading>$filename Information</heading>\n<p>\n";

	foreach $mod (@modules) {
	    &handle_subremarks($mod,0);
	    if ($mod =~ /maintainer:/io) {
		if ($opt_v) {
		    print "Found maintainer details ...\n";
		}
		if ($count > 0) {
		    print OUTPUT "</p><p>-----</p>\n\n<p>", $`, "\n";
		}
		print OUTPUT "</p><p>Maintainers:</p>\n<p>\n";
		$maints = $';
		while ($maints =~ /\s*(.*)\s*&lt;(.*)&gt;/g) {
		    $addr = $2;
		    $who = $1;
		    if ($opt_v) {
			print "Maintainer: $who <$addr>\n";
		    }
		    print OUTPUT "$who <htmlurl url=\"mailto:$addr\" name=\"&lt;" .$addr ."&gt;\">&nl;\n";
    		}
		print OUTPUT "</p>\n<p>";
	    } else {
	        if ($count > 0) {
		    print OUTPUT "</p><p>-----</p>\n\n<p>", $mod, "</p>\n<p>";
		}
	    }
	    $count ++;
	}

	undef @modules;

    print OUTPUT "</p>\n</sect1>\n";
    
    } elsif ($opt_i) {
      print OUTPUT "<sect1><heading>$filename Information</heading>\n<p>\n";
	print OUTPUT "There appears to be no MODULE information for this file.\n\n";
    print OUTPUT "</p>\n</sect1>\n";
    }

}


# This does nothing - We are ignoring the Changes.

sub handle_changes {
    local ($filename) = @_;

    undef @changes;
}


sub handle_functions {
    local ($filename) = @_;
    local (@nodes);
    local ($this, $count, @data, $data);
    local ($inargslist, @args);
    local ($inverb,$inproto,$skip);

    if (defined @functions) {
	print OUTPUT "<sect1><heading>Functions in $filename</heading>\n\n";

  	foreach (@functions) {
#	    /\s*(\w*)/;
	    /\s*(.*)\n/;
	    push (nodes, $1);
	}

	print OUTPUT "<p>These are the functions defined in $filename.</p>\n\n";
    
	$save = $*;
	$* = 1;
	
	while ($this = shift @nodes ) {
	    @data = split (/\n/, shift (@functions));
	    shift @data;	# Ignore the first line - the function name

	    if (join ('', @data) =~ /^\s*$/) {
	      next;
	    }

	    print OUTPUT "<sect2><heading>$this</heading>\n\n\n<p>\n";
	    $inargslist = 0;
	    $inretlist = 0;
	    undef @args;

	    $inverb = 0;
	    $inproto = 0;
	    $skip = 0;
	    foreach $data (@data) {
		if ($data =~ /^\s*(\/*)SKIP\s*/) {
		    if ($1 eq "/") {$skip = 0;}
		    else {$skip = 1;}
		    next;
		}
		if ($skip) {
		    if ($data =~ /^\s*\/*VERB\s*|^\s*\/*PROTO\s*/) {
			$skip = 0;
		    }
		    else {next;}
		}
		if ($data =~ /^\s*VERB\s*/) {
		    print OUTPUT "</p><p><verb>\n";
		    $inverb =1;
		    next;
		}
		if ($inverb) {
		    if ($data =~ /^\s*\/VERB\s*/) {
			print OUTPUT "</verb></p><p>\n";
			$inverb =0;
		    }
		    else {
			$data = un_protect_specials($data);
			print OUTPUT "$data\n";
		    }
		    next;
		}
		if ($data =~ /^\s*PROTO\s*/) {
		    if ($inproto) {print OUTPUT "</verb>\n";}
		    print OUTPUT "</p><p><verb>\n";
		    $inproto =1;
		    next;
		}
		if ($inproto) {
		    if ($data =~ /^\s*\{|^\s*\/PROTO\s*/) {
			print OUTPUT "</verb></p><p>\n";
			$inproto =0;
			$skip = 1;
		    }
		    else {
			$data = un_protect_specials($data);
			print OUTPUT "$data\n";
		    }
		    next;
		}
		if ($data =~ /^\s*\/PROTO\s*/) {
			$inproto =0;
			next;
		}
 		if ($data =~ /^\s*$/) {	# Blank line ....
		    if ($inargslist || $inretlist) {next;} # .... skip
		}
		if ($data =~ /return:/io) { # Want to treat data as return
		    print OUTPUT "</p><p>Return values mean:&nl;\n";
		    $inretlist = 1;
		    $inargslist = 0;
		    next;
		}

		if ($data =~ /arguments:/io) { # Want to treat data as args
		    print OUTPUT "</p><p>Arguments are:&nl;\n";
		    $inargslist = 1;
		    $inretlist = 0;
		    next;
		}

		if ($data =~ /description:/io) { # Descriptive (default)
		    $inargslist = 0;
		    $inretlist = 0;
		    next;
 		}

		if ($inargslist || $inretlist) { # storing list items
		    push (args, $data);
		    next;
		} 

		if (defined @args) { # no longer storing - time to output
		    print OUTPUT "<itemize>\n";
		    foreach $_ (@args) {
			print OUTPUT "<item> ", $_, "</item>\n";
		    }
		    print OUTPUT "</itemize>\n";
		    undef @args;
		}
		print OUTPUT $data, "\n";
		
	    }

	    if (defined @args) { # no longer storing - time to output
		print OUTPUT "<itemize>\n";
		foreach $_ (@args) {
		    print OUTPUT "<item> ", $_, "</item>\n";
		}
		print OUTPUT "</itemize>\n";
	    }

	    print OUTPUT "</p>\n</sect2>\n";
	} 

	undef @functions;

	print OUTPUT "</sect1>\n\n";
	
    } elsif ($opt_i)  {
	print OUTPUT "<p>We appear to have no information on the functions in $filename.</p>\n\n";
    }

}

sub handle_structures {
    local ($filename) = @_;
    local (@nodes);
    local ($this, $count, @data, $data);
    local ($inargslist, @args, $arg, $insubarg);
    local ($inverb) = 0;

    if (defined @structures) {
	print OUTPUT "<sect1><heading>Data Definitions in $filename</heading>\n\n<p>\n";

  	foreach (@structures) {
	    /\s*(.*)\n/;
	    push (nodes, $1);
	}

	print OUTPUT "These are the structures and/or data defined in $filename.\n\n";
    
	$save = $*;
	$* = 1;
	
	while ($this = shift @nodes ) {
	    print OUTPUT "<sect2><heading>$this</heading>\n\n\n<p>\n";
	    @data = split (/\n/, shift (@structures));
	    shift @data;	# Ignore the first line - the header line
	    $inargslist = 0;
	    $inretlist = 0;
	    undef @args;

	    $inverb = 0;
	    foreach $data (@data) {
		if ($data =~ /^\s*VERB\s*/) {
		    print OUTPUT "</p>\n<p><verb>\n";
		    $inverb =1;
		    next;
		}
		if ($inverb) {
		    if ($data =~ /^\s*\/VERB\s*/) {
			print OUTPUT "</verb></p>\n<p>";
			$inverb =0;
		    }
		    else {
			$data = un_protect_specials($data);
			print OUTPUT "$data\n";
		    }
		    next;
		}
 		if ($data =~ /^\s*$/) {	# Blank line ....
		    if ($inargslist) {next;}	        # .... skip
		}

		if ($data =~ /elements:/io) { # Want to treat data as args
		    print OUTPUT "</p><p>Elements are:&nl;\n";
		    $inargslist = 1;
		    next;
		}

		if ($data =~ /description:/io) { # Descriptive (default)
		    $inargslist = 0;
		    next;
 		}

		if ($inargslist) { # storing list items
		    push (args, $data);
		    next;
		} 

		if (defined @args) { # no longer storing - time to output
		    print OUTPUT "<itemize>\n";
		    $insubarg =0;
		    foreach $arg (@args) {
			if ($insubarg) {
			    if ($arg =~ /\*\//) {
				print OUTPUT "$`\n";
				$insubarg = 0;
				next;
			    }
			    print OUTPUT $arg, "\n";
			    next;
			}
			if ($arg =~ /\/\*/) {
			    print OUTPUT "<item> ", $`, "</item>\n\n";
			    $arg = $';
			    if ($arg =~ /\*\//) {$arg = $`;}
			    else {$insubarg = 1;}
			    print OUTPUT  $arg, "\n";
			    next;
			}
			print OUTPUT "<item> ", $arg, "</item>\n";
		    }
		    print OUTPUT "</itemize>\n";
		    undef @args;
		}
		print OUTPUT $data, "\n";
		
	    }

	    if (defined @args) { # no longer storing - time to output
		print OUTPUT "<itemize>\n";
		$insubarg = 0;
		foreach $arg (@args) {
			if ($insubarg) {
			    if ($arg =~ /\*\//) {
				print OUTPUT "$`\n";
				$insubarg = 0;
				next;
			    }
			    print OUTPUT $arg, "\n";
			    next;
			}
			if ($arg =~ /\/\*/) {
			    print OUTPUT "<item> ", $`, "</item>\n";
			    $arg = $';
			    if ($arg =~ /\*\//) {$arg = $`;}
			    else {$insubarg = 1;}
			    print OUTPUT  $arg, "\n";
			    next;
			}
			print OUTPUT "<item> ", $arg, "</item>\n";
		}
		print OUTPUT "</itemize>\n";
	    }

	    print OUTPUT "\n\n";
	} 

	undef @structures;

	print OUTPUT "</p>\n</sect1>\n\n";
    } elsif ($opt_i)  {
	print OUTPUT "We appear to have no information on the structures in $filename.\n\n";
    }
	
}

sub handle_newideas {
    local ($filename) = @_;

    if (defined @newideas) {
	print OUTPUT "<sect1><heading>New Ideas for $filename</heading>\n\n<p>\n";
    
	print OUTPUT (shift @newideas), "\n";

	foreach (@newideas) {
	    print OUTPUT "</p><p>-----</p>\n\n<p>", $_, "\n";
	}

	undef @newideas;

	print OUTPUT "</p></sect1>\n\n";

    } elsif ($opt_i) {
	print OUTPUT "Apparently, there are no new ideas for $filename.\n\n";
    }

}

sub handle_remarks {
    local ($filename) = @_;

    if (defined @remarks) {
	print OUTPUT "<sect1><heading>Remarks in $filename</heading>\n\n<p>\n";
    
	&handle_subremarks(shift @remarks,1);
#	print OUTPUT (shift @remarks), "\n";

	foreach (@remarks) {
#	    print OUTPUT "-----\n\n", $_, "\n";
	    print OUTPUT "</p><p>-----</p>\n\n<p>";
	    &handle_subremarks($_,1);
	}

	undef @remarks;

	print OUTPUT "</p>\n</sect1>\n\n";
    } elsif ($opt_i) {
	print OUTPUT "Apparently, no-one has anything interesting to say about $filename.\n\n";
    }
}

sub handle_fixthis {
    local ($filename) = @_;

    if (defined @fixthis) {
	print OUTPUT "<sect1><heading>Items for Fixing in $filename</heading>\n\n<p>\n";

	print OUTPUT (shift @fixthis), "\n";

	foreach (@fixthis) {
	    print OUTPUT "</p><p>-----</p>\n\n<p>", $_, "\n";
	}

	undef @fixthis;
	
	print OUTPUT "</p>\n</sect1>\n\n";
    } elsif ($opt_i) {
	print OUTPUT "Apparently, nothing needs fixing in $filename.\n\n";
    }

}



# Local Variables: 
# mode:perl
# End:


