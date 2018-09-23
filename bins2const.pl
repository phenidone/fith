#!/usr/bin/perl -w

$od="od -t x4 -w16 -v -A n";

sub dumpfile
{
    my($fn, $vn, $prefix, $sz)=@_;
    
    open(BIN, "$od $fn |") || die "Can't dump $fn: $!\n";

    print "$prefix unsigned long ${vn}[]={\n";
    while(<BIN>){
        chomp;
        s/(\S+)/"0x${1},"/ge;
        print "$_\n";
    }
    print "};\n\n";
    print "extern const unsigned long $sz=sizeof($vn)/sizeof(unsigned long);\n\n";
    close(BIN);
}

die "Need filename prefix and entry point on command line\n" if($#ARGV < 1);

$file=shift;
$entname=shift;

dumpfile("${file}.bin", "fithbin", "extern const", "BINSZ");
dumpfile("${file}.dat", "fithdat", "extern", "DATSZ");

open(MAP, "${file}.map") || die "Can't read ${file}.map: $!\n";
for(<MAP>){
    chomp;
    my($addr, $func)=split(/\s/, $_);
    if($func eq $entname){
        print "extern const unsigned long fithmain=0x${addr};\n";
    }
}
