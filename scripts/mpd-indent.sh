#!/bin/sh
indent -npro -kr -i8 -ts8 -sob -l80 -ss -ncs -cdw -cd0 -c0 -cp0 "$@"

# there doesn't seem to be an indent switch for this, but this
# forces goto labels to the left-most column, without indentation
perl -i -p -e 's/^\s+(\w+):/$1:/g unless /^\s+default:/' "$@"
