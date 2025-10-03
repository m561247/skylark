/*******************************************************************************
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

#ifndef _H_DOCUMENT_MAP_
#define _H_DOCUMENT_MAP_

#define MOVE_DOWN 1
#define MOVE_UP 0

// c++ compiler
#ifdef __cplusplus
extern "C" {
#endif

eu_tabpage* on_map_launch(void);
eu_tabpage* on_map_edit(void);
void on_map_reload(eu_tabpage *pedit);
void on_map_scroll(eu_tabpage *pnode, eu_tabpage *ptr_map);
void on_map_size(const eu_tabpage *pnode, const int flags);
HWND on_map_hwnd(void);

#ifdef __cplusplus
}
#endif

#endif  //_H_DOCUMENT_MAP_
