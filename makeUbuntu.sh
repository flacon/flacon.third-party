#!/bin/sh

OUT_DIR="Debs"
RELEASES=''
RELEASES="${RELEASES} yakkety"
PROGS='mac ttaenc'

mkdir -p ${OUT_DIR}

for PROG in ${PROGS}; do
    for RELEASE in ${RELEASES}; do
	./makeDeb.sh -s --release ${RELEASE}  -o ${OUT_DIR} ${PROG}
    done
done

 # Create send2launchpad.sh script
echo "dput ppa:flacon/ppa *_source.changes" > ${OUT_DIR}/send2launchpad.sh
chmod u+x ${OUT_DIR}/send2launchpad.sh
