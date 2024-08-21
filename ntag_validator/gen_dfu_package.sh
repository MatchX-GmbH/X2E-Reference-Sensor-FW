#! /bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

PYTHON_FILE=../scripts/gen_dfu_package.py
BIN_FILE=build/ntag_validator.bin

#
cd ${SCRIPT_DIR}
python ${PYTHON_FILE} ${BIN_FILE}


