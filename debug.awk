#!/usr/bin/env awk
/[[:alnum:]._-]*: [0-9]* / {
    match($0, /[[:alnum:]._-]*: [0-9]* /)
    split(substr($0, RSTART, RLENGTH), a, ":")
    split(a[2], b, " ")
    if (a[1] && b[1]) {
        print a[1] " " b[1] " " substr($0, RSTART + RLENGTH)
    }
}
