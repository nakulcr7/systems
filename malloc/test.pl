#!/usr/bin/perl
use 5.16.0;
use warnings FATAL => 'all';

use Test::Simple tests => 29;
use IO::Handle;

system("(cd tests && make clean && make) > /dev/null");

sub parse_stats {
    my ($text) = @_;

    my @lines = split /\n/, $text;
    my $stats = {};

    for (@lines) {
        /^==/ and next;
        /^(\w+):\s+(\d+)/ or next;
        $stats->{lc $1} = $2;
    }

    return $stats;
}

sub run_test {
    my ($test) = @_;
    system(qq{(./tests/$test > stdout.txt) 2> stderr.txt});
    my $out = `cat stdout.txt`;
    my $err = `cat stderr.txt`;

    my $stats = parse_stats($err);
    chomp $out;
    return ($out, $stats);
}

my ($out, $stats);

($out, $stats) = run_test("01-hello");
ok($out eq "Hello, World", "01 output");
ok($stats->{mapped} == 1,  "01 pages mapped");
ok($stats->{allocs} == 1,  "01 malloc count");
ok($stats->{frees} == 1,   "01 free count");

($out, $stats) = run_test("02-array-sum");
ok($out eq "Sum = 523776",  "02 output");
ok($stats->{mapped} == 3,   "02 pages mapped");
ok($stats->{unmapped} == 3, "02 pages unmapped");
ok($stats->{allocs} == 1,   "02 malloc count");
ok($stats->{frees} == 1,    "02 free count");
ok($stats->{freelen} == 0,  "02 freelist");

($out, $stats) = run_test("03-list-sum");
ok($out eq "Sum = 45",     "03 output");
ok($stats->{mapped} == 1,  "03 pages mapped");
ok($stats->{allocs} == 10, "03 malloc count");
ok($stats->{frees} == 10,  "03 free count");
ok($stats->{freelen} == 1, "03 freelist");

($out, $stats) = run_test("04-two-lists");
ok($out eq "Sum = 9900",    "04 output");
ok($stats->{mapped} == 2,   "04 pages mapped");
ok($stats->{allocs} == 200, "04 malloc count");
ok($stats->{frees}  == 200, "04 free count");
ok($stats->{freelen} == 1,  "04 freelist");

($out, $stats) = run_test("05-many-sizes");
ok($out eq "Sum = 499500",  "05 output");
ok($stats->{mapped} >= 600 && $stats->{mapped} <= 1000, "05 pages mapped");
ok($stats->{unmapped} >= 200 && $stats->{unmapped} <= 600, "05 pages unmapped");
ok($stats->{allocs} == 1001, "05 malloc count");
ok($stats->{frees}  == 1001, "05 free count");
ok($stats->{freelen} >= 1 && $stats->{freelen} <= 100,  "05 freelist");

sub check_errors {
    my ($errs) = @_;
    chomp $errs;
    if ($errs ne "") {
        $errs =~ s/^/# /mg;
        warn $errs;
        return 0;
    }
    else {
        return 1;
    }
}

sub valgrind {
    my ($test) = @_;

    my $vg = "valgrind -q --leak-check=full --log-file=valgrind.out";
    system(qq{(timeout -k 10 10 $vg "./tests/$test" 2>&1) > /dev/null});
    my $errs = `cat -n valgrind.out`;
    return check_errors($errs);
}

sub clang_check {
    my $errs = `clang-check -analyze *.c -- 2>&1`;
    return check_errors($errs);
}

ok(clang_check(), "clang check");
ok(valgrind("02-array-sum"), "valgrind 02");
ok(valgrind("03-list-sum"), "valgrind 03");

