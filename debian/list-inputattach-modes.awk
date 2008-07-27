#!/usr/bin/awk -f

/^  --.*-/ {
    print ".TP";
    print "\\fB\\-" substr($2, 2) "\\fR, \\fB\\-\\-" substr($1, 3) "\\fR";
    remainder = "";
    for (i = 3; i <= NF; i++)
	remainder = remainder $i " ";
    print remainder;
}