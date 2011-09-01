#!/bin/sh

TESTS=$*

PARENTTEST=${TEST}

for i in ${TESTS}; do
	export TEST=$i
        pushd $i
	sh ./runtest.sh || exit 1
	popd
done
