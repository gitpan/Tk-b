use Config;
use ExtUtils::MakeMaker;

# The existence of the ./pTk/Makefile.PL file causes MakeMaker
# to automatically include Makefile code for the targets
#	config, all, clean, realclean and sdbm/Makefile
# which perform the corresponding action in the subdirectory.

use lib '../..';
use Tk::Config;
use Tk::MMutil;

Tk::MMutil::TkExtMakefile(
    'LINKTYPE'  => 'static',
    'INC'	=> "$inc -I.",
    'SKIP'	=> [qw(static static_lib dynamic dynamic_lib distclean 
                       test perldepend xs_c xs_o)],
    'clean' 	=> {'FILES' => 'libXpm.a *%'}
);

sub MY::install {
'
install :: all
';
}

sub MY::top_targets {
'
all :: linkext

static  ::	libXpm.a

dynamic ::	libXpm.a

libXpm.a: $(O_FILES)
	ar cr libXpm.a $(O_FILES)
	$(RANLIB) libXpm.a

config ::

';
}

sub MY::realclean {
	'
realclean :: clean
	rm -f Makefile
';
}



