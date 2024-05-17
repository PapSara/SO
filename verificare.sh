#!/bin/bash

# Verifica daca un argument a fost furnizat
if test $# -lt 1 || test $# -gt 1 
then
    echo "Nu a fost furnizat niciun argument sau au fost furnizate prea multe"
    exit 1
fi

filename=$1

# Verifica daca fisierul exista
if test ! -f "$filename" 
 then
    echo "Fisierul nu exista sau nu a fost furnizat un fisier"
    exit 1
fi

# Reda temporar permisiunile fisierului

chmod u+r "$filename"

# Numara liniile, cuvintele si caracterele

num_lines=$(wc -l < "$filename")
num_words=$(wc -w < "$filename")
num_chars=$(wc -m < "$filename")

echo "Lines: $num_lines, Words: $num_words, Characters: $num_chars"

# Verifica daca fisierul este suspect
if test "$num_lines" -lt 3  && test "$num_words" -gt 1000  && test "$num_chars" -gt 2000 
then
    echo "Fisierul este suspect pentru ca are prea putine linii si prea multe cuvinte si caractere"
    echo "$filename" # Afiseaza numele fisierului daca este periculos
    chmod u-r "$filename"
    exit 1
fi

# Verifica daca fisierul contine caractere non-ASCII
if LC_ALL=C grep -q '[^[:print:][:space:]]' "$filename"; 
then
    echo "Au fost gasite caractere non-ASCII."
    echo "$filename" # Afiseaza numele fisierului daca este periculos
    chmod u-r "$filename"
    exit 1
fi

# Defineste cuvintele cheie periculoase
keywords="corrupted dangerous risk attack malware malicious"

# Cauta cuvinte cheie in fisier
for keyword in $keywords; 
    do
        if grep -qi $keyword "$filename"; then
            echo "Cuvantul suspect ($keyword) a fost gasit in fisier."
            echo "$filename"  # Afiseaza numele fisierului daca este periculos
            chmod u-r "$filename"
            exit 1
        fi
    done

chmod u-r "$filename"
echo "SAFE"  # Afiseaza SAFE daca nu sunt gasite cuvinte sau caractere periculoase
exit 0
