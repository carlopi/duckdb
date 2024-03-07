#!/bin/bash

# Main extension uploading script

# Usage: ./scripts/upload-staging-asset.sh <file>
# <file>                : File to be uploaded

set -e

DRY_RUN_PARAM=""

# dryrun if AWS key is not set
if [ -z "$AWS_ACCESS_KEY_ID" ]; then
  DRY_RUN_PARAM="--dryrun"
fi

# dryrun if repo is not duckdb/duckdb
if [ "$GITHUB_REF" != "duckdb/duckdb" ]; then
  DRY_RUN_PARAM="--dryrun"
fi

TARGET="git_describe_not_provided"

# dryrun if repo is not duckdb/duckdb
if [ -z "$OVERRIDE_GIT_DESCRIBE" ]; then
  DRY_RUN_PARAM="--dryrun"
else
  TARGET="$OVERRIDE_GIT_DESCRIBE"
fi

# upload versioned version
if [[ $4 != 'true' ]]; then
  DRY_RUN_PARAM="--dryrun"
fi

python -m ensurepip
python -m pip install awscli

for var in "$@"
do
    aws s3 cp $var s3://duckdb-staging/stage/$TARGET/. $DRY_RUN_PARAM
done
