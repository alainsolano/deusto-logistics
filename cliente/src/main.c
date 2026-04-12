#include <stdio.h>
#include <string.h>
#include "menu.h"

int main(void) {
    char id_operario[16] = "grupo_6";

    printf("Iniciando terminal 'Deusto Logistics'...\n");

    menu_principal(0, id_operario);

    printf("\nPrograma finalizdo correctamente.\n");

    return 0;
}