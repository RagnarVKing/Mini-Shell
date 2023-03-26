#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause

# (Re)Create tests/ directory with saved information.
rm -fr ../tests/
cp -r "$CHECKER_DATA_DIRECTORY"/../tests ..

# Everything happens in the tests/ directory.
pushd ../tests > /dev/null || exit 1

echo -e "### RUNING LINTER\n\n"
make lint

echo -e "\n\n### RUNING CHECKER\n\n"
make check

popd > /dev/null || exit 1
