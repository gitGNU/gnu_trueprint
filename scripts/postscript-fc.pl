#!/usr/bin/perl
# Postscript files clean . 
# Author: Jeffrin Jose T. <ahiliation@yahoo.co.in>

@files = ("main.ps");
foreach $file (@files) {
    unlink($file);
}
