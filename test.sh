cd test
pwd
gmp make linux || exit 1
cd make
pwd
make || exit 1
pwd
./test  || exit 1
