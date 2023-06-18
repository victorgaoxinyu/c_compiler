#include <stdio.h>
#include "helpers/vector.h"
#include "compiler.h"

int main()
{

    printf("%i", align_value(2, 4));

    return 0;
    
    int res = compile_file("./test.c", "./test", 0);
    if (res == COMPILER_FILE_COMPILED_OK)
    {
        printf("Everything compiled fine\n");
    }
    else if (res == COMPILER_FAILED_WITH_ERRORS)
    {
        printf("Compile failed\n");
    }
    else
    {
        printf("Unknown response for compile file\n");
    }

    return 0;
}