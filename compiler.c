#include "compiler.h"
#include <stdarg.h>
#include <stdlib.h>
#include "helpers/vector.h"

struct lex_process_functions compiler_lex_functions = {
    .next_char = compile_process_next_char,
    .peek_char = compile_process_peek_char,
    .push_char = compile_process_push_char,
};

void compiler_error(struct compile_process *compiler, const char *msg, ...) // can take infinite parameters
{
    va_list args;
    va_start(args, msg);         /* init va_list, the first is the va_list obj to be init */
    vfprintf(stderr, msg, args); /* the second is the last named arg of the function, in this case msg */
    va_end(args);
    fprintf(stderr, " on line %i, col %i in file %s\n", compiler->pos.line, compiler->pos.col, compiler->pos.filename);
    exit(-1);
}

void compiler_warning(struct compile_process *compiler, const char *msg, ...) // can take infinite parameters
{
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, " on line %i, col %i in file %s\n", compiler->pos.line, compiler->pos.col, compiler->pos.filename);
}

int compile_file(const char *filename, const char *output_filename, int flags)
{
    struct compile_process *process = compile_process_create(filename, output_filename, flags);
    if (!process)
    {
        return COMPILER_FAILED_WITH_ERRORS;
    }

    // Perform lexical analysis

    struct lex_process *lex_process = lex_process_create(process, &compiler_lex_functions, NULL);
    if (!lex_process)
    {
        return COMPILER_FAILED_WITH_ERRORS;
    }

    if (lex(lex_process) != LEXICAL_ANALYSIS_ALL_OK)
    {
        return COMPILER_FAILED_WITH_ERRORS;
    }

    process->token_vec = lex_process->token_vec;

    // Perform parsing

    if (parse(process) != PARSE_ALL_OK)
    {
        return COMPILER_FAILED_WITH_ERRORS;
    }

    // Perform code generation

    return COMPILER_FILE_COMPILED_OK;
}