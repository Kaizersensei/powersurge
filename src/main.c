/*
 * Open Surge Engine
 * main.c - entry point
 * Copyright 2008-2026 Alexandre Martins <alemartf(at)gmail.com>
 * http://opensurge2d.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <allegro5/allegro.h> /* included for cross-platform compatibility; see https://liballeg.org/a5docs/trunk/getting_started.html#the-main-function */
#include "labs/lab.h"
#include "core/global.h"
#include "core/engine.h"
#include "core/commandline.h"

#if defined(_WIN32)
#include <windows.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>

/* Marked portable runtimes keep relative engine/data reads beside the executable.
 * Explicit game-folder selection retains its existing caller-owned semantics. Signed: President. */
static bool bsl_portable_directory(int argc, char** argv)
{
    for(int i = 1; i < argc; i++)
        if(strcmp(argv[i], "--game") == 0 || strcmp(argv[i], "--game-folder") == 0)
            return true;

    wchar_t folder[32768], marker[32768];
    DWORD length = GetModuleFileNameW(NULL, folder, 32768);
    if(length == 0 || length >= 32768)
        return false;
    wchar_t* separator = wcsrchr(folder, L'\\');
    if(separator == NULL)
        return true;
    *separator = L'\0';
    if(wcslen(folder) + wcslen(L"\\borsurgelab.runtime") >= 32768)
        return false;
    wcscpy(marker, folder);
    wcscat(marker, L"\\borsurgelab.runtime");
    DWORD attributes = GetFileAttributesW(marker);
    if(attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
        return true;
    if(!SetCurrentDirectoryW(folder)) {
        fprintf(stderr, "Cannot enter portable runtime directory (Windows error %lu).\n", (unsigned long)GetLastError());
        return false;
    }
    return true;
}
#endif

#if defined(__ANDROID__)
#include <setjmp.h>
jmp_buf main_jump_buffer;
#endif

/*
 * main()
 * Entry point
 */
int main(int argc, char **argv)
{
#if defined(__ANDROID__)
    char* args[] = { GAME_UNIXNAME, "--mobile" };
    argc = sizeof(args) / sizeof(args[0]);
    argv = args;

    /* exit gracefully */
    if(setjmp(main_jump_buffer) != 0)
        return 1;
#endif

#if defined(_WIN32)
    if(!bsl_portable_directory(argc, argv))
        return 1;
#endif

    if(ps_lab_prepare(&argc, &argv) < 0) return ps_lab_exit_code();
    commandline_t cmd = commandline_parse(argc, argv);

    do {
        engine_init(&cmd);
        engine_mainloop();
        engine_release();
        if(engine_must_restart(&cmd) && !ps_lab_apply_switch(&cmd)) return ps_lab_exit_code();
    } while(engine_must_restart(NULL));

    return ps_lab_finish();
}
