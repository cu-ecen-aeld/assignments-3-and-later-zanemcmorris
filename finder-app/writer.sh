# Accepts the following arguments: the first argument is a full path to a file (including filename) on the filesystem, referred to below as writefile;
#  the second argument is a text string which will be written within this file, referred to below as writestr

# Exits with value 1 error and print statements if any of the arguments above were not specified
# Creates a new file with name and path writefile with content writestr, overwriting any existing file and creating the path if it doesn’t exist. Exits with value 1 and error print statement if the file could not be created.

#!/bin/bash


numArgs=$#
# echo numArgs: ${numArgs}
arg1=$1
arg2=$2
# echo Arg1: ${arg1}
# echo Arg2: ${arg2}

if [ ! ${numArgs} -eq 2 ]
then
    echo "Writer.sh expected exactly two arguments. First is path to file, second is text to write"
    exit 1
fi

if [ ! -f ${arg1} ]
then
    # If the file doesn't exist, create it.
    # echo "Did not find a file at $1. Creating it."
    mkdir -p "$(dirname -- "$1")" && touch -- "$1"
fi 

echo $2 > $1 # Print second argument into file specified by first arg

exit 0
