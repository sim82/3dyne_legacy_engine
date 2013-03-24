DH_EXEC=$1
COLRM_EXEC=$2

if [ -z "$COLRM_EXEC" ]; then
	COLRM_EXEC=colrm
fi

rm -f files.txt
find . -name "*" -type f -not -name "hash.bin" | $COLRM_EXEC 1 2 > files.txt

$DH_EXEC
