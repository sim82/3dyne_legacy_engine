DH_EXEC=$1

rm -f files.txt
find . -name "*" -type f -not -name "hash.bin" | colrm 1 2 > files.txt

$DH_EXEC
