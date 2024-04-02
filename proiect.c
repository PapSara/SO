/*Subiectul proiectului: monitorizarea modificarilor aparute in directoare de a lungul timpului.
Saptamana 1: Utilizatorul poate specifica directorul pe care doreste sa il monitorizeze in linia de comanda, ca prim argument. 
Se vor urmari atat modificarile aparute la nivelul directorului, cat si la nivelul subarborelui. Se va parcurge directorul si 
intregul sau subrbore si se face un SNAPSHOT. Acest snapshot poate fi un fisier, care contine toate datele pe care le furnizeaza directorul
si subarborele si care sunt considerate necesare.*/

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
 
        if (lstat(path, &buff) == -1) { // Am modificat aici
            printf("Eroare la obținerea informațiilor despre %s.\n", path);
            continue;
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

        // Scrie informatiile in snapshot
        write(snaps, &file_info, sizeof(file_info));

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
