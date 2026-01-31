#!/bin/sh
# finder.sh - POSIX sh version



filesdir="$1"

searchstr="$2"



# Validate args

if [ "$#" -ne 2 ]; then

    echo "Expected exactly two arguments. First is path to directory, second is search string"

    exit 1

fi



if [ ! -d "$filesdir" ]; then

    echo "Expected a directory for arg 1. Exiting."

    exit 1

fi



# Count files (regular files only) recursively

numFilesFound=$(find "$filesdir" -type f 2>/dev/null | wc -l)



# Count matching lines across all files

# grep -R: recursive, -n not needed, -h suppress filename, -F treats searchstr literally

# Redirect errors in case of unreadable files

numLinesFound=$(grep -R -F "$searchstr" "$filesdir" 2>/dev/null | wc -l)



echo "The number of files are $numFilesFound and the number of matching lines are $numLinesFound"

exit 0




# # Accepts the following runtime arguments:
# #  the first argument is a path to a directory on the filesystem, referred to below as filesdir;
# #  the second argument is a text string which will be searched within these files, referred to below as searchstr

# # Exits with return value 1 error and print statements if any of the parameters above were not specified

# # Exits with return value 1 error and print statements if filesdir does not represent a directory on the filesystem

# # Prints a message "The number of files are X and the number of matching lines are Y" where X is the number of files in the directory and all subdirectories
# #  and Y is the number of matching lines found in respective files, where a matching line refers to a line which contains searchstr (and may also contain additional content).

# numArgs=$#
# # echo numArgs: ${numArgs}
# arg1=$1
# arg2=$2
# echo Arg1: ${arg1}
# echo Arg2: ${arg2}

# if [ ${numArgs} -eq 2 ]
# then
#     echo "found 2 args"
# else
#     echo "Expected exactly two arguments. First is path to directory, second is search string"
#     exit 1
# fi

# if [ ! -d ${arg1} ]
# then
#     echo "Expected a directory for arg 1. Exiting."
#     exit 1
# # else
#     # echo Found directory at $1
# fi 

# # Now we need to recursively search each file and each line for the search str.
# # IF the searchstr is present then we increment numFilesFound by one and num lines increments by one
# numFilesFound=0
# numLinesFound=0
# while IFS= read -r -d '' file; do
#     # echo $file
#     if [ -f "$file" ]; then
#         # echo processing file
#         foundLineInFile=0
#         while read -r line; do
#             # echo ${line}
#             if [[ "$line" == *"$2"* ]];
#             then
#                 ((numLinesFound++))
#                 if [ $foundLineInFile -eq 0 ]
#                 then
#                     ((numFilesFound++))
#                     foundLineInFile=1
#                 fi
#             fi  
#         done < $file
#     fi
# done < <(find "$1" -type f -print0)

# echo The number of files are $numFilesFound and the number of matching lines are $numLinesFound in Arg1: $1
# exit 0

