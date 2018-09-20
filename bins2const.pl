#!/usr/bin/perl -w

$od="od -t x4 -w16 -v -A n";

sub dumpfile
{
    my($fn, $vn)=@_;
    
    open(BIN, "$od $fn |") || die "Can't dump $fn: $!\n";

    print "const unsigned long ${vn}[]={\n";
    while(<BIN>){
        chomp;
        s/(\S+)/"0x${1},"/ge;
        print "$_\n";
    }
    print "};\n\n";
    close(BIN);
}

die "Need filename prefix and entry point on command line\n" if($#ARGV < 1);

$file=shift;
$entname=shift;

dumpfile("${file}.bin", "fithbin");
dumpfile("${file}.dat", "fithdat");

open(MAP, "${file}.map") || die "Can't read ${file}.map: $!\n";
for(<MAP>){
    chomp;
    my($addr, $func)=split(/\s/, $_);
    if($func eq $entname){
        print "constexpr unsigned long fithmain=0x${addr};\n";
    }
}
