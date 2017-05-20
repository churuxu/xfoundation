cd test
gmp make || exit 1
cd make
make || exit 1
./test  || exit 1
