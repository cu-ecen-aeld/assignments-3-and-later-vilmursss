#!/bin/sh

# Function to display usage
usage() {
    echo "Usage: $0 <directory_path> <search_string>"
    exit 1
}

# Check if exactly two arguments are passed
if [ "$#" -ne 2 ]; then
    usage
fi

# Assign arguments to variables
DIRECTORY_PATH=$1
SEARCH_STRING=$2

# Check if the first argument is a valid directory
if [ ! -d "$DIRECTORY_PATH" ]; then
    echo "Error: $DIRECTORY_PATH is not a valid directory."
    exit 1
fi

# Count the number of files in the directory
FILE_COUNT=$(find "$DIRECTORY_PATH" -type f | wc -l)

# Count the number of matching lines in the files in directory recursively
LINE_COUNT=$(grep -r "$SEARCH_STRING" "$DIRECTORY_PATH" | wc -l)

echo "The number of files are $FILE_COUNT and the number of matching lines are $LINE_COUNT"

# Exit with success code
exit 0
