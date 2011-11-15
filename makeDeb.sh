#!/bin/bash

function help {
  echo "Usage: makeDeb.sh [otions] <path-to-source>"
  echo
  echo "Options"
  echo "  -h|--help             display this message"
  echo "  -o|--outdirt=DIR      write result to DIR"
  echo "  -r|--release=RELEASE  release name (sid, maveric, natty etc.)"
  echo "  -s|--sign             sign a result files"
  echo "  -b|--binary           build a binary package, if ommited build only only a source package"
}

TYPE='-S'
SIGN='-uc -us'
OUT_DIR='./'

while [ $# -gt 0 ]; do
  case $1 in
    -h|--help)
        help
        exit
      ;;

    -o|outdir)
        OUT_DIR=$2;
        shift 2
      ;;

    -r|--release)
        RELEASE=$2
        shift 2
      ;;

    --ver)
        VER=$2
        shift 2
      ;;

    -b|--binary)
        TYPE='-b'
        shift
      ;;

    -s|--sign)
        SIGN=''
        shift
      ;;
    --)
        shift
        break
      ;;

    *)
        SRC_DIR=$1
        shift
      ;;

  esac
done


if [ -z "${SRC_DIR}" ]; then
  echo "missing path-to-source operand" >&2
  help
  exit 2
fi


if [ -z "${RELEASE}" ]; then
  echo "missing release option"
  help
  exit 2
fi

if [ ! -d ${OUT_DIR} ]; then
    echo "${OUT_DIR}: No such directory"
    exit 2
fi


OUT_DIR=`cd ${OUT_DIR}; pwd`
NAME=`ls ${SRC_DIR}/*.orig.tar.gz | sed -e's|.*/\(.*\).*_.*|\1|'`
VER=` ls ${SRC_DIR}/*.orig.tar.gz | sed -e's/.*_\(.*\).orig.tar.gz/\1/'`

echo "*******************************$f"
echo " Name: ${NAME}"
echo " Ver:  ${VER}"
echo " Type: ${TYPE}"
echo " Release: ${RELEASE}"
echo " Src dir: ${SRC_DIR}"
echo " Out dir: ${OUT_DIR}"
echo "*******************************"

DIR=${OUT_DIR}/${NAME}-${VER}
rm -rf ${DIR}

# Sources .......................................
cp --force ${SRC_DIR}/${NAME}_${VER}.orig.tar.gz ${OUT_DIR}
cp --force -R ${SRC_DIR}/${NAME}-${VER} ${DIR}/

DATE=`date -R`
YEAR=`date +'%Y'`

for f in `find ${DIR}/debian -type f `; do
    sed -i \
        -e"s/%NAME%/${NAME}/g"  \
        -e"s/%VER%/${VER}/g" \
        -e"s/%RELEASE%/${RELEASE}/g" \
        -e"s/%DATE%/${DATE}/g" \
        -e"s/%YEAR%/${YEAR}/g" \
        $f
done

# Build .........................................
cd ${DIR} && debuild ${TYPE} ${SIGN} -rfakeroot
rm -r ${DIR}

