#include <stdio.h>
#include <string.h>

void compress(){

    // FILE *image = ();
}


int main(int argc, char *argv[])
{
    // skontrolujeme, či vôbec nejaký argument je
    if (argc < 2)
    {
        printf("Pouzitie: %s <compress|decompress>\n", argv[0]);
        return 1;
    }

    // argv[1] obsahuje to slovo za ./main
    if (strcmp(argv[1], "compress") == 0)
    {
        printf("Spustam kompresiu...\n");
        // tu by bol tvoj kod na kompresiu
    }
    else if (strcmp(argv[1], "decompress") == 0)
    {
        printf("Spustam dekompresiu...\n");
        // tu by bol tvoj kod na dekompresiu
    }
    else
    {
        printf("Neznamy prikaz: %s\n", argv[1]);
        printf("Pouzi 'compress' alebo 'decompress'\n");
        return 1;
    }

    return 0;
}
