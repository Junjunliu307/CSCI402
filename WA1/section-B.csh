#!/bin/tcsh -f
#
# put this file in the directory that contains the w1data subdirectory (this directory should also have all your .c files)

echo "(B) Sort..."

                /bin/rm -f f?.sort f??.sort
                foreach f (0 1 2 3 4 5 6 7 8 9 10 11 12 13 14)
                    echo "===> w1data/f$f"
                    ./warmup1 sort w1data/f$f > f$f.sort
                    diff w1data/f$f.sort f$f.sort
                end

                /bin/rm -f f?.sort f??.sort
                foreach f (15 16 17 18 19 20 21 22 23 24 25 26 27 28 29)
                    echo "===> w1data/f$f"
                    cat w1data/f$f | ./warmup1 sort > f$f.sort
                    diff w1data/f$f.sort f$f.sort
                end
                /bin/rm -f f?.sort f??.sort
