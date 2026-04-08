#!/bin/bash

################################################################################
# Unpack the file A1.tar.gz (which should be on the format specified in the
# instructions of assignment 1 in Parallel and Distributed Programming. 
# Check that the required files exist and can be built respectively.
# Run the binary for different number of processes. Exit with exit code 1 if
# the file structure is not the required one or the program computes the wrong
# result. Then also print a message saying what's wrong. If everything seems OK,
# print a message saying that the file is ready for submission, and exit with
# exit code 0.
################################################################################

# Extract tar, enter directory, build (clean first and check that files were removed)
echo "Extracting and entering A1 directory"
tar -xzf A1.tar.gz || exit 1
cd A1 || exit 1
report=`find . -maxdepth 1 -name "A1_Report.pdf" | wc -l`
if [ 1 != "$report" ]; then
	echo "A1_Report.pdf is missing!"
	exit 1
fi
echo "Building"
make clean || exit 1
binary=`find . -maxdepth 1 -name "sum" | wc -l`
if [ 0 != "$binary" ]; then
	echo "make clean does not work as expected!"
	exit 1
fi
make || exit 1
binary=`find . -maxdepth 1 -name "sum" | wc -l`
if [ 1 != "$binary" ]; then
	echo "stencil not built by 'make all'!"
	exit 1
fi
echo "OK"

make clean || exit 1
echo "Your file is ready for submission. Well done!"
