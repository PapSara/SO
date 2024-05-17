#!/bin/bash

if [ $# -lt 1 ] || [ $# -gt 1 ]; then
    echo "Nu a fost furnizat niciun argument sau au fost furnizate prea multe"
    exit 1
fi

filename=$1

if [ ! -f "$filename" ]; then
    echo "Fisierul nu exista sau nu a fost furnizat un fisier"
    exit 1
fi

chmod 777 "$filename"

num_lines=$(wc -l < "$filename")
num_words=$(wc -w < "$filename")
num_chars=$(wc -m < "$filename")

echo "Lines: $num_lines, Words: $num_words, Characters: $num_chars"

if [ "$num_lines" -lt 3 ] && [ "$num_words" -gt 1000 ] && [ "$num_chars" -gt 2000 ]; then
    echo "Fisierul este suspect pentru ca are prea putine linii si prea multe cuvinte si caractere"
    COUNTER=1
fi

if LC_ALL=C grep -q '[^[:print:][:space:]]' "$filename"; then
    echo "Au fost gasite caractere non-ASCII."
    COUNTER=1
fi

keywords="corrupted dangerous risk attack malware malicious"

for keyword in $keywords; do
    if grep -qi "$keyword" "$filename"; then
        echo "Cuvantul suspect ($keyword) a fost gasit in fisier."
        COUNTER=1
    fi
done

if [ "${COUNTER:-0}" -eq 1 ]; then
    echo "PERICOL"
    exit 1
else
    chmod 000 "$filename"
    echo "SAFE"
    exit 0
fi
