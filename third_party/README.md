# BUG FIXES

## Ramulator

To compile Ramulator, the following changes to the `ramulator/Makefile` are applied from `./Makefile`
These patches can be found also in `../../miscs/patch/ramulator.patch` but for some reason it does not work.

`sed -i '10s/clang++/g++/' ramulator/Makefile` or `sed -i '10s/^/#/' ramulator/Makefile`
clang is not installed on the system, so we use g++ instead.

`sed '12!d' ramulator/Makefile | grep 'fPIC' || sed -i '12s/-Wall/& -fPIC/' ramulator/Makefile`
we need to compile with the -fPCI flag to make it work with the shared library.
the first part avoids adding the flag twice.

`sed -i '39s/libtool -static -o/ar rcs /' ramulator/Makefile`
`sed '40!d' ramulator/Makefile | grep 'ranlib' || sed -i '40i\	ranlib $$@' ramulator/Makefile`
libtool version on newer systems does not support the -static flag, so we use ar and ranlib instead.