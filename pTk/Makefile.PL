use Config;
use ExtUtils::MakeMaker;
use lib '..';
use Tk::MMtry qw(try_compile);
use Tk::MMutil;
use Tk::Config;

WriteMakefile(
    'NAME'	=> 'Tk::pTk',
    'VERSION'	=> $Tk::Config::VERSION,
    'LINKTYPE'	=> 'static',
    'DEFINE'	=> $define,
    'INC'	=> "$inc -I.",
    'SKIP'	=> [qw(static_lib dynamic dynamic_lib distclean 
                       test perldepend dist makeaperl xs_o)],
    'clean' 	=> {'FILES' => 'libpTk.a *%'}
);

sub MY::xs_c {
'
# Rules for building .t table files from .h files
.SUFFIXES: .t .h 
.h.t :
	$(PERL) mkVFunc $< 
'
}

sub MY::post_constants 
{
 my $objects = "LIBOBJS =";
 unless (defined($Config{'i_stdlib'}) && try_compile("config/Hstrtoul.c"))
  {
   $objects .= " strtoul.o" 
  }
 unless (defined($Config{'i_string'}) && try_compile("config/Hstrdup.c"))
  {
   $objects .= " strdup.o" 
  }
 my $len     = length($objects);
 foreach (<[LXt]*.c>)
  {
   s/\.c/.o/;
   if (($len+length($_)+1) > 72)
    {
     $objects .= "\\\n\t";
     $len = 8;
    }
   $objects .= " $_";
   $len     += 1+length($_);
  } 
 return "\n" . $objects . "\n";
};

sub MY::install {
'
install :: all
';
}


sub MY::static 
{
 '
static  ::	libpTk.a 

dynamic :: static 

libpTk.a: $(LIBOBJS) 
	ar cr libpTk.a $(LIBOBJS)
	$(RANLIB) libpTk.a

';

}


sub MY::realclean {
'
realclean :: clean
	rm -f Makefile
';
}

sub MY::postamble {
'
Lang.t  : Lang.h   mkVFunc
tk.t    : tk.h     mkVFunc
tkInt.t : tkInt.h  mkVFunc
Xlib.t  : Xlib.h   mkVFunc
tkImgPhoto.t  : tkImgPhoto.h   mkVFunc

config :: Lang.t tk.t tkInt.t tkImgPhoto.t

';
}

sub MY::post_initialize
{
 my ($self) = @_;
 my %files = ();
 my $name;
 my $dir = $self->catdir('$(INST_ARCHLIBDIR)','pTk');
 foreach $name ($self->lsdir("."))
  {
   next if ($name =~ /^\./);
   next unless (-f $name);
   $files{$name} = 1 if ($name =~ /\.[tm]$/);
  }
 foreach $name (sort(@{$self->{H}},keys %files))
  {
   $self->{PM}->{$name} = $self->catfile($dir,$name);
  }
 '';
}





