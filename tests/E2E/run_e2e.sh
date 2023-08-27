#!/bin/bash

echo -e "Running E2E tests..."
#do go into every directory
#1 is exchange, 2 is trader(write), 3 is trader(read), 4 is e2e directory
for d in $4*/
do
    #count=$(($(ls $d -1 | wc -l) - 3))
    echo -e "Running tests in $d..."
    cmd=($1 "${d}products.txt")
    files_file=""
    for f in $d*
    do
        #files.txt is the input file, out.txt is the output, products.txt products
        if [ "$f" = "${d}files.txt" ]
        then
            echo "found files file"
            files_file="$f"
        elif [ "$f" = "${d}out.txt" ]
        then
            echo "found output file"
        elif [ "$f" = "${d}products.txt" ]
        then
            echo "found product file"
        elif [ "$f" = "${d}generate.py" ]
        then
            echo "found generation file"
        else
            cmd+=($2)
        fi
    done
    cmd+=($3)

    "${cmd[@]}" < $files_file | diff "${d}out.txt" -
    if [[ $? -eq 1 ]]
    then
        echo -e "\terrored"
        exit 1
    else
        echo "tests in ${d} passed"
    fi
    #echo "${cmd[@]}"
done

exit 0
