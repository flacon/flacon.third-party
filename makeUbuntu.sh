#!/bin/sh

OUT_DIR=~/flacon-3party-deb
RELEASES='oneiric'
PROGS='mac ttaenc'

mkdir -p ${OUT_DIR}

for PROG in ${PROGS}; do
    for RELEASE in ${RELEASES}; do
	./makeDeb.sh -s --release ${RELEASE}  -o ${OUT_DIR} ${PROG}
    done
done

 # Create send2launchpad.sh script
echo "dput ppa:razor-qt/ppa *_source.changes" > ${OUT_DIR}/send2launchpad.sh
chmod u+x ${OUT_DIR}/send2launchpad.sh