use lib '.';
BEGIN 
 {
  $VERSION = 'b11';
  require "./myConfig";
 }

use Tk::MMutil;

Tk::MMutil::TkExtMakefile(
    'VERSION'  => $VERSION,
    'NAME'     => 'Tk',
    'DISTNAME' => "Tk",
    'MYEXTLIB' => 'pTk/libpTk.a',
    'LIBS'    => ["$xlib -lX11 -lpt -lsocket -lnsl -lm"],
    'OBJECT'  => '$(BASEEXT).o tkGlue.o leak_util.o',
    'MAP_TARGET' => 'tkperl',
    'clean'      => { FILES => 'pTk/tkConfig.h Tk/Config.pm' },
    @macro
   );
                           
sub MY::test {
'
TEST_VERBOSE=0
TEST_TYPE=test_$(LINKTYPE)

test :: $(TEST_TYPE)

test_dynamic :: all
	$(PERL) -I$(INST_ARCHLIB) -I$(INST_LIB) demos/widget

test_ : test_dynamic

test_static :: all $(MAP_TARGET)
	./$(MAP_TARGET) -I$(INST_ARCHLIB) -I$(INST_LIB) demos/widget

';
}

sub MY::post_initialize
{
 my ($self) = @_;
 my %files = ();
 my $name;
 my $dir = $self->catdir('$(INST_ARCHLIBDIR)','Tk');
 delete $self->{PM}->{'Tk/Config.pm'};
 $self->{PM}->{'Tk/Config.pm'} = $self->catfile($dir,'Config.pm');
 # $files{'typemap'} = 1;
 foreach $name ($self->lsdir("."))
  {
   next if ($name =~ /^\./);
   next unless (-f $name);
   $files{$name} = 1 if ($name =~ /\.[tm]$/);
   $files{$name} = 1 if ($name =~ /\.def$/);
  }
 foreach $name (sort(@{$self->{H}},keys %files))
  {
   $self->{PM}->{$name} = $self->catfile($dir,$name);
  }
 '';
}

sub MY::postamble {
    '
config       :: tkGlue.t

$(BASEEXT).o : tkGlue.h
tkGlue.o     : tkGlue.c tkGlue.h pTk/Methods.def tkGlue.t
Makefile     : myConfig

tkGlue.t     : tkGlue.h pTk/mkVFunc
	$(PERL) pTk/mkVFunc tkGlue.h

$(MYEXTLIB)  : config FORCE
	cd pTk; $(MAKE) 

perlmain.c : config Makefile

pTk/Methods.def : $(MYEXTLIB)

';
}


