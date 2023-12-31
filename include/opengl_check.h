/**
    \file opengl_check.h
*/
#pragma once

bool check_glerror(const char *cmd);

#if defined(NDEBUG)
#define CHK(cmd) cmd
#else
#define CHK(cmd)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        cmd;                                                                                                           \
        while (check_glerror(#cmd))                                                                                    \
        {                                                                                                              \
        }                                                                                                              \
    } while (false)
#endif