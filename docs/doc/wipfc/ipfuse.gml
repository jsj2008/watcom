.*
.chap *refid='ipfuse' Using the &ipfname.
.*
.ix '&ipfcmd' 'Command line format'
.ix '&ipfcmd command line' 'invoking &ipfcmd'
.ix 'Command line format' '&ipfcmd'
.ix 'Invoking the &ipfname'
.np
The &ipfname is a command line utility. It may be invoked as follows:
.code begin
&ipfcmd [options] input.ipf 
.code end
.*
.section *refid='ipfenv' Environment Variables
.*
.ix 'WIPFC environment variable'
.ix 'IPFCARTWORK environment variable'
.ix 'IPFCIMBED environment variable'
.ix 'TMP environment variable'
.ix 'Environment variable' 'WIPFC'
.ix 'Environment variable' 'IPFCARTWORK'
.ix 'Environment variable' 'IPFCIMBED'
.ix 'Environment variable' 'TMP'
.np
The &ipfname depends upon several environment variables that must be set correctly before
&ipfcmd is run.
.begnote $setptnt 12
.note WIPFC
This must be set to &ipfpath, which is the location of the files defining the local language encoding.
.note IPFCARTWORK
Must point to your source of bit-mapped graphic images if your source file uses any.
.note IPFCIMBED
Must point to your source of input files if your main source file includes other source files.
.note TMP
Must point to a directory where temporary files that are generated by &ipfname can be stored.
.endnote
.*
.section Command Line Flags
.*
.np
The behavior of the &ipfname can be altered using command line options. Each flag begins with a delimiter.
Either '-' or '/' can be used. Each flag is given a longer descriptive name here, but only the first 
character after the delimiter is actually significant. Flag names are not case-sensitive.
.*
.beglevel
.*
.section Switches
.ix 'inf switch'
.ix 'quiet switch'
.ix 'suppress-search switch'
.ix 'xref switch'
.ix 'Command line switch' 'inf'
.ix 'Command line switch' 'quiet'
.ix 'Command line switch' 'suppress-search'
.ix 'Command line switch' 'xref'
.ix 'Flag' 'inf'
.ix 'Flag' 'quiet'
.ix 'Flag' 'suppress-search'
.ix 'Flag' 'xref'
.np
The switches change the state of the &ipfname in a yes/no fashion.
.begnote $compact $setptnt 12
.notehd1 Name
.notehd2 Description
.note inf
Generate an 'inf' file. Otherwise a 'hlp' file is created by default.
.note quiet
Operate quietly, suppressing the usual copyright information display.
.note suppress-search
Suppress the generation of the full-text search table in the output file. This makes the help file
smaller, but the user can't search for individual words.
.note xref
Generate additional output information, including a cross-reference of identifiers. This is saved in
a file 'output-name.log' where 'output-name' is the base name of the help file being created.
.endnote
.*
.section Options
.np
The options give additional information to the &ipfname. Each option is immediately followed by the information
to be passed into &ipfcmd..
.*
.beglevel
.*
.section localization
.ix 'localization option'
.ix 'Command line option' 'localization'
.ix 'Option' 'localization'
.np
Help files can be created for different locales. This is done by using the
.keyword localization
option in the form of
.code begin
-l xx_YY
.code end
where xx_YY is the name of the locale (for example, en_US). The localization information is store in a text
file (en_US.nls in this case). You can create your own locales from the sample file xx_YY.nls, but you may also
need to create a corresponding entity file containing the definitions of :HDREF refid='ipfsym'..
.*
.section output-name
.ix 'output-name option'
.ix 'Command line option' 'output-name'
.ix 'Option' 'output-name'
.np
You can specify the name of the help file that &ipfcmd generates by using the
.keyword output-name
option. For example,
.code begin
-o MyHelp
.code end
will cause &ipfcmd to generate an output file (either inf or hlp depending on the -inf switch) with a base
name of 'MyHelp' 
.*
.section warning-level
.ix 'warning-level option'
.ix 'Command line option' 'warning-level'
.ix 'Option' 'warning-level'
.np
The &ipfname generates three different levels of warnings (1, 2, and 3). By default, all levels of warnings 
are displayed. You can suppress the display of warnings higher than level
.keyword n
by using the
.keyword warning-level
option. For example,
.code begin
-w1
.code end
suppresses all warnings higher than level 1.
.*
.endlevel
.*
.endlevel

