use lib '..';
use Tk::MMutil;
Tk::MMutil::TkExtMakefile(
      'MYEXTLIB' => 'xpm/libXpm.a',
      'OBJECT' => '$(O_FILES)',
      'dynamic_ptk' => 0
      );

sub MY::postamble {
'
$(MYEXTLIB)  : FORCE
	cd xpm; $(MAKE) libXpm.a
';
}


