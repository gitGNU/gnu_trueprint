#!/usr/bin/perl
# Postscript files clean . 
# Author: Jeffrin Jose T. 
# <ahiliation@yahoo.co.in>

use strict;    
use warnings;
    
unlink glob "*.ps";
unlink glob "*.PS";
