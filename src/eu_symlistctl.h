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

#ifndef _H_SKYLARK_SYMBOLLISTCTL_
#define _H_SKYLARK_SYMBOLLISTCTL_

#ifdef __cplusplus
extern "C" {
#endif

int on_symlist_create(eu_tabpage *pnode);
int on_symlist_reqular(eu_tabpage *pnode);
int on_symlist_jump_item(eu_tabpage *pnode);
int on_symlist_update_theme(eu_tabpage *pnode);
void on_symlist_wait(eu_tabpage *pnode);

#ifdef __cplusplus
}
#endif

#endif  // _H_SKYLARK_SYMBOLLISTCTL_
