#!/bin/bash

set -e
set -x

CMAKE_CONFIG=Release
EXT_BASE_PATH=build/release

if [ "${FORCE_32_BIT:0}" == "1" ]; then
  FORCE_32_BIT_FLAG="-DFORCE_32_BIT=1"
else
  FORCE_32_BIT_FLAG=""
fi

FILES="${EXT_BASE_PATH}/extension/*/*.duckdb_extension"

EXTENSION_LIST=""
for f in $FILES
do
	ext=`basename $f .duckdb_extension`
	EXTENSION_LIST="${EXTENSION_LIST}-$ext"
done
mkdir -p testext
cd testext

if [ "$2" = "oote" ]; then
  CMAKE_ROOT="../duckdb"
else
  CMAKE_ROOT=".."
fi

cmake -DCMAKE_BUILD_TYPE=${CMAKE_CONFIG} ${FORCE_32_BIT_FLAG} -DTEST_REMOTE_INSTALL="${EXTENSION_LIST}" ${CMAKE_ROOT}
cmake --build . --config ${CMAKE_CONFIG}
cd ..

duckdb_path="testext/duckdb"
unittest_path="testext/test/unittest"
if [ ! -f "${duckdb_path}" ]; then
  duckdb_path="testext/${CMAKE_CONFIG}/duckdb.exe"
  unittest_path="testext/test/${CMAKE_CONFIG}/unittest.exe"
fi

echo "$DUCKDB_EXTENSION_SIGNING_PK" > private.pem

for f in $FILES
do
	ext=`basename $f .duckdb_extension`
	echo $ext
	# calculate SHA256 hash of extension binary
	openssl dgst -binary -sha256 $f > $f.hash
	# encrypt hash with extension signing private key to create signature
	openssl pkeyutl -sign -in $f.hash -inkey private.pem -pkeyopt digest:sha256 -out $f.sign
	# append signature to extension binary
	cat $f.sign >> $f
	# compress extension binary
	gzip < $f > "$f.gz"
	# upload compressed extension binary to S3
	# aws s3 cp $f.gz s3://duckdb-extensions/$2/$1/$ext.duckdb_extension.gz --acl public-read
	install_path=${ext}
	${duckdb_path} -c "INSTALL '${install_path}'"
	${duckdb_path} -c "LOAD '${ext}'"
done

rm private.pem

for f in $FILES
do
	ext=`basename $f .duckdb_extension`
	install_path=${ext}
	unsigned_flag=
	if [ "$1" = "local" ]
	then
		install_path=${f}
		unsigned_flag=-unsigned
	fi
	echo ${install_path}
	${duckdb_path} ${unsigned_flag} -c "INSTALL '${install_path}'"
	${duckdb_path} ${unsigned_flag} -c "LOAD '${ext}'"
done
${unittest_path}
