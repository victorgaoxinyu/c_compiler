#ifndef C_COMPILER_H
#define C_COMPILER_H

struct compile_process
{
    /* data */
};


int compile_file(const char* filename, const char* out_filename, int flags);

#endif