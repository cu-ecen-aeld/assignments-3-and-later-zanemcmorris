# Accepts the following runtime arguments:
#  the first argument is a path to a directory on the filesystem, referred to below as filesdir;
#  the second argument is a text string which will be searched within these files, referred to below as searchstr

# Exits with return value 1 error and print statements if any of the parameters above were not specified

# Exits with return value 1 error and print statements if filesdir does not represent a directory on the filesystem

# Prints a message "The number of files are X and the number of matching lines are Y" where X is the number of files in the directory and all subdirectories
#  and Y is the number of matching lines found in respective files, where a matching line refers to a line which contains searchstr (and may also contain additional content).
#!/bin/bash

numArgs=$#
# echo numArgs: ${numArgs}
arg1=$1
arg2=$2
# echo Arg1: ${arg1}
# echo Arg2: ${arg2}

if [ ${numArgs} -eq 2 ]
then
    echo "found 2 args"
else
    echo "Expected exactly two arguments. First is path to directory, second is search string"
    exit 1
fi

if [ ! -d ${arg1} ]
then
    echo "Expected a directory for arg 1. Exiting."
    exit 1
# else
    # echo Found directory at $1
fi 

# Now we need to recursively search each file and each line for the search str.
# IF the searchstr is present then we increment numFilesFound by one and num lines increments by one
numFilesFound=0
numLinesFound=0
while IFS= read -r -d '' file; do
    # echo $file
    if [ -f "$file" ]; then
        # echo processing file
        foundLineInFile=0
        while read -r line; do
            # echo ${line}
            if [[ "$line" == *"$2"* ]];
            then
                ((numLinesFound++))
                if [ $foundLineInFile -eq 0 ]
                then
                    ((numFilesFound++))
                    foundLineInFile=1
                fi
            fi  
        done < $file
    else
        echo Not a file
        continue
    fi
done < <(find "$1" -type f -print0)

echo Found $numFilesFound with $numLinesFound lines containing $2 in directory $1
exit 0
