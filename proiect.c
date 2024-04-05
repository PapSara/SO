/*Subiectul proiectului: monitorizarea modificarilor aparute in directoare de a lungul timpului.
Saptamana 1: Utilizatorul poate specifica directorul pe care doreste sa il monitorizeze in linia de comanda, ca prim argument. 
Se vor urmari atat modificarile aparute la nivelul directorului, cat si la nivelul subarborelui. Se va parcurge directorul si 
intregul sau subrbore si se face un SNAPSHOT. Acest snapshot poate fi un fisier, care contine toate datele pe care le furnizeaza directorul
si subarborele si care sunt considerate necesare.
Saptamana 2: comparam snapshot-ul anterior cu cel curent si primim mai multe directoare ca argumente
Se actualizeaza functionalitatea prog a.i sa primeasca mai multe arg, dar nu mai mult de 10, niciun arg nu se repeta.Prog proceseaza doar dir si ignora alte tipuri de arg, in cazul in care se inregistreaza modif comparam cel curent cu cel anterior, il inlocuiesti pe cel anterior cu cel curent sau il stergem pe cel anterior ca sa puteam compara snapshot-ul urmator cu ultima modificare.Functionalitatea codului va fi extinsa si va primi un arg suplimentar care primeste dir de iesire care primeste toate snapshot-urile
gcc -Wall -o exec prog.c
./exec arg1 arg2 -o iesire
./exec -o output input1 input2
*/

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

// Structura pentru stocarea informatiilor despre un fisier
typedef struct Info {
    char name[256];
    ino_t inode; /* inode number */
    off_t size; /* total size, in bytes */
    time_t modified_time;
}INFO;

// Functie pentru a crea un snapshot si a scrie informatiile intr-un fisier care se creeaza la momentul executiei
void createSnapshot(const char *directory, const char *snapshot) {
    DIR *dir = opendir(directory);
    if (dir == NULL) {
        printf("Nu am putut deschide directorul.\n");
        exit(EXIT_FAILURE);
    }
    
    int snaps = open(snapshot, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    /*
    O_WRONLY - deschidere numai pentru scriere
    O_CREAT - crearea fisierului, daca el nu exista deja; daca e folosita cu aceasta optiune,
    functia open trebuie sa primeasca si parametrul mode.
    O_TRUNC - daca fisierul exista, continutul lui este sters
    S_IRUSR - drept de citire pentru proprietarul fisierului (user)
    S_IWUSR - drept de scriere pentru proprietarul fisierului (user)
    */
    if (snaps == -1) {
        printf("Nu am putut crea fișierul de snapshot.\n");
        closedir(dir);
        exit(EXIT_FAILURE);
    }

    struct dirent *in;
    struct stat buff;
    INFO file_info;

    // Parcurge fiecare intrare in director
    while ((in = readdir(dir)) != NULL) {
        // Construieste calea completa pentru fiecare fisier / director
        /*int snprintf(char *str, size_t size, const char *format, …);
        Parameters:

*str : is a buffer.
size : is the maximum number of bytes (characters) that will be written to the buffer.
format : C string that contains a format string that follows the same specifications as format in printf
… : the optional ( …) arguments are just the string formats like (“%d”, myint) as seen in printf.
Return value:

The number of characters that would have been written on the buffer, if ‘n’ had been sufficiently large. The terminating null character is not counted.
If an encoding error occurs, a negative number is returned.
        */
	char path[512];
        snprintf(path, sizeof(path), "%s/%s", directory, in->d_name);
 
        if (lstat(path, &buff) == -1) {
            printf("Eroare la obtinerea informatiilor");
            exit(EXIT_FAILURE);
        }

        // Ignora "." și ".."
        if (strcmp(in->d_name, ".") == 0 || strcmp(in->d_name, "..") == 0) {
            continue;
        }

        // Introducerea informatiilor in structura
        strncpy(file_info.name, in->d_name, sizeof(file_info.name));
        file_info.inode = buff.st_ino;
        file_info.size = buff.st_size;
        file_info.modified_time = buff.st_mtime;
        
        // Scrierea numele fișierului
        write(snaps, file_info.name, strlen(file_info.name));
        write(snaps, " ", 1);

        // Convertirea inode-ului în șir de caractere și scrierea sa în fișier
        char inode_buffer[32];
        int inode_length = snprintf(inode_buffer, sizeof(inode_buffer), "%lu", (unsigned long)file_info.inode);
        write(snaps, inode_buffer, inode_length);
        write(snaps, " ", 1);

        // Convertirea dimensiunii în șir de caractere și scrierea sa în fișier
        char size_buffer[32];
        int size_length = snprintf(size_buffer, sizeof(size_buffer), "%lld", (long long)file_info.size);
        write(snaps, size_buffer, size_length);
        write(snaps, " ", 1);

        // Convertirea datei de modificare în șir de caractere și scrierea sa în fișier
        char modified_time_buffer[32];
        int modified_time_length = snprintf(modified_time_buffer, sizeof(modified_time_buffer), "%ld", (long)file_info.modified_time);
        write(snaps, modified_time_buffer, modified_time_length);

        // Scrierea unui caracter nou de linie pentru a termina linia
        write(snaps, "\n", 1);


        // Verifica daca este director si apeleaza recursiv functia
        if (S_ISDIR(buff.st_mode)) {
            createSnapshot(path, snapshot);
        }
    }

    closedir(dir);
    close(snaps);
}

int main(int argc, char *argv[]) {
    // Verifica daca calea directorului a fost data in linia de comanda si daca este singurul argument
    if (argc != 2) {
        printf("Argumente insuficiente sau prea multe\n");
        exit(EXIT_FAILURE);
    }

    // Verifica daca argumentul este un director
    struct stat statbuf;
    if (lstat(argv[1], &statbuf) == -1 || !S_ISDIR(statbuf.st_mode)) {
        printf("Argumentul trebuie sa fie director.\n");
        exit(EXIT_FAILURE);
    }

    // Creeaza snapshot-ul
    createSnapshot(argv[1], "snapshot.txt");

    printf("Snapshot-ul a fost creat cu succes.\n");
    return 0;
}
