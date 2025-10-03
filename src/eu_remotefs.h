/******************************************************************************
 * This file is part of Skylark project
 * Copyright ©2025 Hua andy <hua.andy@gmail.com>

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * at your option any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *******************************************************************************/

#ifndef _EU_REMOTE_FS_H_
#define _EU_REMOTE_FS_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef void (CALLBACK *remote_opt)(void *);

typedef struct _remotefs
{
    char        servername[100+1];
    char        protocol[20+1];
    char        networkaddr[MAX_PATH+1];
    int         port;
    int         accesss;
    char        user[20+1];
    char        pwd[32+1];
    char        key_path[MAX_PATH+1];
    char        passphrase[32+1];
    bool        cfg;
    remote_opt  curl_opt;
    struct list_head node_server;
}remotefs;

extern struct list_head    list_server ;

void on_remote_manager(void);
void on_remotefs_draw_combo(HWND hwnd, const HDC hdc, RECT rc);
remotefs *on_remote_list_find(const TCHAR *url);
CURL* on_remote_init_socket(const char *, remotefs *pserver);
unsigned __stdcall on_remote_load_config(void *);

#ifdef __cplusplus
}
#endif

#endif  // _EU_REMOTE_FS_H_
