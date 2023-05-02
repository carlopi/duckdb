#!/bin/bash

rm -f hash_concats
touch hash_concats

split -b 1M $1

FILES="x*"
for f in $FILES
do
	# sha256 a segment
	scripts/compute-hash.sh $f >> hash_concats
	rm $f
done

# sha256 the concatenation
hexdump -C hash_concats
scripts/compute-hash.sh hash_concats > hash_composite
hexdump -C hash_composite
ls -la hash_concats hash_composite

cat hash_composite
