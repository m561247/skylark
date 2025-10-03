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

#ifndef _H_SKYLARK_CONFIG_
#define _H_SKYLARK_CONFIG_

#ifdef __cplusplus
extern "C" {
#endif

void on_config_file_url(wchar_t *path, int len, const wchar_t *p);
bool on_config_lua_run(const wchar_t *file, const char *parg);
int  on_config_accel_loader(void);

#ifdef __cplusplus
}
#endif

#endif
