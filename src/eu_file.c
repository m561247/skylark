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

#include <fcntl.h>
#include "framework.h"
#include <tlhelp32.h>

static volatile long file_close_id = 0;
static HANDLE file_event_final = NULL;

static bool
on_file_set_filter(const TCHAR *ext, TCHAR **pfilter)
{
    TCHAR p_all[] = {'A','l','l',' ','F','i','e','s','(','*','.','*',')','\0','*','.','*','\0','\0'};
    if (!(*pfilter = (TCHAR *)calloc(sizeof(TCHAR), MAX_BUFFER + 1)))
    {
        return false;
    }
    if (ext)
    {
        TCHAR *pext = NULL;
        TCHAR *pdesc = NULL;
        TCHAR exts[QW_SIZE+1] = {0};
        char u8_exts[QW_SIZE+1] = {0};
        _sntprintf(exts, QW_SIZE, _T("*%s;"), ext);
        WideCharToMultiByte(CP_UTF8, 0, exts, -1, u8_exts, QW_SIZE, NULL, NULL);
        for (doctype_t *mapper = eu_doc_get_ptr(); mapper && mapper->doc_type; ++mapper)
        {
            if (mapper->extname && strstr(mapper->extname, u8_exts) != NULL)
            {
                pext = eu_utf8_utf16(&mapper->extname[1], NULL);
                pdesc = eu_utf8_utf16(mapper->filedesc, NULL);
                if (_tcslen(pext) > 0 && pext[_tcslen(pext) - 1] == ';')
                {
                    pext[_tcslen(pext) - 1] = 0;
                }
                break;
            }
        }
        if (pext && pdesc)
        {
            int l = _sntprintf(*pfilter, MAX_BUFFER, _T("%s(%s)"), pdesc, pext);
            int ext_len = _tcslen(exts) > 0 ? (int)_tcslen(exts) - 1 : 0;
            exts[ext_len] = 0;   // strip ';'
            memcpy(*pfilter + l + 1, exts, ext_len * sizeof(TCHAR));
            memcpy(*pfilter + l + ext_len + 2, p_all, sizeof(p_all));
            free(pext);
            free(pdesc);
            return true;
        }
    }
    memcpy(*pfilter, p_all, sizeof(p_all));
    return true;
}

int
on_file_open_filename_dlg(HWND hwnd, TCHAR *file_name, int name_len)
{
    OPENFILENAME ofn = {sizeof(OPENFILENAME),};
    if (!file_name)
    {
        return EUE_POINT_NULL;
    }
    LOAD_I18N_RESSTR(IDS_TOOLBAR_1, lptext);
    *file_name = 0;
    ofn.hwndOwner = hwnd ? hwnd : eu_module_hwnd();
    ofn.hInstance = eu_module_handle();
    ofn.lpstrFile = file_name;
    ofn.nMaxFile = name_len;
    ofn.lpstrFilter = _T("All Files(*.*)\0*.*\0" \
                         "C/C++ Files(*.c;*.cpp;*.cxx;*.h;*.hpp;*.rc)\0*.c;*.cpp;*.cxx;*.h;*.hpp;*.rc\0" \
                         "Script Files(*.bat;*.cmd;*.sh;*.lua;*.pl;*.py)\0*.bat;*.cmd;*.sh;*.lua;*.pl;*.py\0" \
                         "HTML Files(*.htm;*.html;*.xml;*.css;*.js)\0*.htm;*.html;*.xml;*.css;*.js\0" \
                         "PE Files(*.exe;*.com;*.dll;*.sys)\0*.exe;*.com;*.dll;*.sys\0\0");
    ofn.lpstrTitle = lptext;
    ofn.Flags = OFN_ENABLESIZING | OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_HIDEREADONLY;
    return !GetOpenFileName(&ofn);
}

static int
on_file_get_filename_dlg(TCHAR *file_name, int name_len, const TCHAR *pcd)
{
    int err = SKYLARK_OK;
    TCHAR *filter = NULL;
    OPENFILENAME ofn = {sizeof(ofn),};
    if (!file_name ||!pcd)
    {
        return EUE_POINT_NULL;
    }
    LOAD_I18N_RESSTR(IDS_TOOLBAR_2, lptext);
    TCHAR *p = _tcsrchr(file_name, _T('.'));
    if (!on_file_set_filter(p, &filter) || !filter)
    {
        return EUE_EXT_FILTER_ERR;
    }
    ofn.hwndOwner = eu_module_hwnd();
    ofn.hInstance = eu_module_handle();
    ofn.lpstrFile = file_name;
    ofn.nMaxFile = name_len;
    ofn.lpstrFilter = filter;
    ofn.lpstrTitle = lptext;
    ofn.lpstrInitialDir = pcd;
    ofn.Flags = OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
    err = !GetSaveFileName(&ofn);
    free(filter);
    // 重置默认工作目录
    SetCurrentDirectory(eu_module_path);
    return err;
}

static void
on_file_kill_tree(const uint32_t self)
{
    bool more;
    volatile int i = 1;
    static uint32_t edit_pid[MAX_SIZE] = {0};
    PROCESSENTRY32W pe32 = {sizeof(PROCESSENTRY32W)};
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        eu_logmsg("File: CreateToolhelp32Snapshot error %lu\n", GetLastError());
        return;
    }
    edit_pid[0] = self;
    more = Process32FirstW(snapshot,&pe32);
    while (more)
    {
        if (pe32.th32ParentProcessID > 4 && pe32.th32ProcessID != self && (wcscmp(__ORIGINAL_NAME, (LPCWSTR)pe32.szExeFile) == 0))
        {
            edit_pid[i++] = pe32.th32ProcessID;
            if (i >= MAX_SIZE)
            {
                break;
            }
        }
        more = Process32NextW(snapshot,&pe32);
    }
    CloseHandle(snapshot);
    if (edit_pid[1] > 0)
    {
        for (int i = 0; i < MAX_SIZE && edit_pid[i] > 0 ; ++i)
        {
            if (edit_pid[i] != edit_pid[0])
            {
                HANDLE handle = OpenProcess(PROCESS_TERMINATE, false, edit_pid[i]);
                if (handle)
                {
                    TerminateProcess(handle, (DWORD)-1);
                    CloseHandle(handle);
                    edit_pid[i] = 0;
                }
            }
        }
    }
}

void
on_file_update_time(eu_tabpage *pnode, const time_t m, const bool saved)
{
    const HWND htab = on_tabpage_hwnd(pnode);
    if (htab)
    {
        eu_tabpage *p = NULL;
        if (m)
        {
            pnode->st_mtime = m;
        }
        else if (pnode->pathfile[0])
        {
            struct _stat statbuf;
            _tstat(pnode->pathfile, &statbuf);
            pnode->st_mtime = statbuf.st_mtime;
        }
        if ((p = on_tabpage_dup_at(htab, pnode->pathfile)))
        {
            p->st_mtime = pnode->st_mtime;
            if (saved)
            {
                p->fn_modify = false;
            }
        }
    }
}

static int
on_file_refresh_recent_menu(void *data, int count, char **column, char **names)
{
    HMENU hre = (HMENU)data;
    struct eu_config *pconf = eu_get_config();
    const int index = GetMenuItemCount(hre);
    if (index < pconf->file_recent_number)
    {
        for (int i = 0; i < count && column[i]; ++i)
        {
            if (column[i][0] != 0)
            {
                TCHAR ptr_row[MAX_BUFFER] = {0};
                if (MultiByteToWideChar(CP_UTF8, 0, column[i], -1, ptr_row, MAX_BUFFER))
                {
                    if (_tcsrchr(ptr_row, _T('&')))
                    {
                        eu_wstr_replace(ptr_row, MAX_BUFFER, _T("&"), _T("&&"));
                    }
                    AppendMenu(hre, MF_POPUP | MF_STRING, IDM_HISTORY_BASE + index, ptr_row);
                }
            }
        }
        return SKYLARK_OK;
    }
    return SKYLARK_SQL_END;
}

void
on_file_update_recent_menu(void)
{
    HWND hwnd = eu_module_hwnd();
    HMENU hroot = GetMenu(hwnd);
    if (hroot)
    {
        HMENU hre = NULL;
        HMENU hfile = GetSubMenu(hroot, FILES_MENU);
        struct eu_config *pconf = eu_get_config();
        hre = hfile ? GetSubMenu(hfile, FT_MENU_RECENT_SUB) : NULL;
        if (hre && pconf)
        {
            int count = GetMenuItemCount(hre);
            for (int index = 0; index < count; ++index)
            {
                DeleteMenu(hre, 0, MF_BYPOSITION);
            }
            if (pconf->file_recent_number > 0)
            {
                on_sql_mem_post("SELECT szName FROM file_recent ORDER BY szDate DESC;", on_file_refresh_recent_menu, (void *)hre);
            }
            if ((count = GetMenuItemCount(hre)) == 0)
            {
                LOAD_I18N_RESSTR(IDC_MSG_HISTORY_ZERO, m_str);
                if (AppendMenu(hre, MF_POPUP | MF_STRING, IDM_HISTORY_BASE, m_str))
                {
                    util_enable_menu_item(hroot, IDM_HISTORY_BASE, false);
                }
            }
        }
    }
}

static void
on_file_push_recent(const eu_tabpage *pnode)
{
    file_recent *precent = (file_recent *)calloc(1, sizeof(file_recent));
    if (precent)
    {
        if (pnode && util_make_u8(pnode->pathfile, precent->path, MAX_BUFFER)[0])
        {   // 也支持16进制编辑器获取实时位置
            precent->postion = on_sci_call(pnode, SCI_GETCURRENTPOS, 0, 0);
            precent->hex = pnode->hex_mode;
            on_sql_file_recent_thread(precent);
        }
        free(precent);
    }
}

static file_backup *
on_file_get_bakup(const eu_tabpage *pnode)
{
    file_backup *pbak = (file_backup *)calloc(1, sizeof(file_backup));
    if (pnode && pbak)
    {
        pbak->x = -1, pbak->y = -1;
        pbak->postion = on_sci_call(pnode, SCI_GETCURRENTPOS, 0, 0);
        pbak->tab_id = on_tabpage_get_index(pnode);
        pbak->cp = pnode->codepage;
        pbak->bakcp = pnode->bakcp;
        pbak->eol = pnode->eol;
        pbak->blank = pnode->is_blank;
        pbak->hex = pnode->hex_mode;
        pbak->focus = pnode->tab_focus;
        pbak->zoom = pnode->zoom_level != SELECTION_ZOOM_LEVEEL ? pnode->zoom_level : 0;
        pbak->status = pnode->be_modify ? 1 : 0;
        pbak->view = pnode->view;
        _tcscpy(pbak->rel_path, pnode->pathfile);
        _tcscpy(pbak->bak_path, pbak->status ? pnode->bakpath : _T(""));
        if (!TAB_HEX_MODE(pnode) && !TAB_HAS_PDF(pnode))
        {
            on_search_page_mark(pnode, pbak->mark_id, MAX_BUFFER-1);
            on_search_fold_kept(pnode, pbak->fold_id, MAX_BUFFER-1);
        }
    }
    return pbak;
}

void
on_file_filedb_update(const eu_tabpage *pnode)
{
    file_backup *pbak = NULL;
    const bool doing = eu_get_config() && eu_get_config()->m_session && eu_get_config()->m_up_notify > 0;
    if (doing && pnode && (pbak = (on_file_get_bakup(pnode))))
    {
        eu_update_backup_table(pbak, DB_FILE);
        free(pbak);
    }
}

void
on_file_clear_recent(void)
{
    HMENU hroot = GetMenu(eu_module_hwnd());
    if (hroot)
    {
        HMENU hre = NULL;
        HMENU hfile = GetSubMenu(hroot, FILES_MENU);
        hre = hfile ? GetSubMenu(hfile, FT_MENU_RECENT_SUB) : NULL;
        if (hre && (on_sql_file_recent_clear() == 0))
        {
            bool ret = false;
            int count = GetMenuItemCount(hre);
            for (int index = 0; index < count; ++index)
            {
                if (!(ret = DeleteMenu(hre, 0, MF_BYPOSITION)))
                {
                    break;
                }
            }
            if (ret)
            {
                LOAD_I18N_RESSTR(IDC_MSG_HISTORY_ZERO, m_str);
                if (AppendMenu(hre, MF_POPUP | MF_STRING, IDM_HISTORY_BASE, m_str))
                {
                    util_enable_menu_item(hroot, IDM_HISTORY_BASE, false);
                }
            }
        }
    }
}

/******************************************************************
 * _tsplitpath_s (MSVCRT for wine)
 *
 * Secure version of _tsplitpath
 */
static int
on_file_tsplitpath(const TCHAR *inpath, TCHAR *drive, size_t sz_drive, TCHAR *dir, size_t sz_dir, 
                   TCHAR *fname, size_t sz_fname, TCHAR *ext, size_t sz_ext)
{
    const TCHAR *p, *end;
    if (!inpath || (!drive && sz_drive) || (drive && !sz_drive) || (!dir && sz_dir) || (dir && !sz_dir) ||
        (!fname && sz_fname) || (fname && !sz_fname) || (!ext && sz_ext) || (ext && !sz_ext))
    {
        *_errno() = EINVAL;
        return EINVAL;
    }
    if (inpath[0] && inpath[1] == ':')
    {
        if (drive)
        {
            if (sz_drive <= 2)
            {
                goto do_error;
            }
            drive[0] = inpath[0];
            drive[1] = inpath[1];
            drive[2] = 0;
        }
        inpath += 2;
    }
    else if (drive)
    {
        drive[0] = '\0';
    }
    /* look for end of directory part */
    end = NULL;
    for (p = inpath; *p; p++)
    {
        if (*p == '/' || *p == '\\') end = p + 1;
    }
    if (end) /* got a directory */
    {
        if (dir)
        {
            if (sz_dir <= (size_t)(end - inpath))
            {
                goto do_error;
            }
            memcpy(dir, inpath, (end - inpath) * sizeof(wchar_t));
            dir[end - inpath] = 0;
        }
        inpath = end;
    }
    else if (dir)
    {
        dir[0] = 0;
    }
    /* look for extension: what's after the last dot */
    end = NULL;
    for (p = inpath; *p; p++)
    {
        if (*p == '.') end = p;
    }
    if (!end)
    {
        end = p; /* there's no extension */
    }
    if (fname)
    {
        if (sz_fname <= (size_t)(end - inpath))
        {
            goto do_error;
        }
        memcpy(fname, inpath, (end - inpath) * sizeof(wchar_t));
        fname[end - inpath] = 0;
    }
    if (ext)
    {
        if (sz_ext <= _tcslen(end))
        {
            goto do_error;
        }
        _tcscpy(ext, end);
    }
    return 0;
do_error:
    if (drive) drive[0] = '\0';
    if (dir) dir[0] = '\0';
    if (fname) fname[0] = '\0';
    if (ext) ext[0] = '\0';
    *_errno() = ERANGE;
    return ERANGE;
}

void
on_file_splite_path(const TCHAR *full_path, TCHAR *pathname, TCHAR *filename, TCHAR *mainname, TCHAR *extname)
{
    TCHAR drv[_MAX_DRIVE];
    TCHAR ext[_MAX_EXT] = {0};
    TCHAR path[MAX_BUFFER] = {0};
    TCHAR part[MAX_PATH] = {0};
    TCHAR *ptr_part = mainname ? mainname : part;
    TCHAR *ptr_ext = extname ? extname : ext;
    on_file_tsplitpath(full_path, drv, _MAX_DRIVE, path, MAX_BUFFER, ptr_part, MAX_PATH, ptr_ext, _MAX_EXT);
    if (pathname)
    {
        *pathname = 0;
        if (_tcslen(drv) > 0 && _tcslen(path) > 0)
        {
            _sntprintf(pathname, MAX_BUFFER, _T("%s%s"), drv, path);
        }
    }
    if (filename)
    {
        *filename = 0;
        if (_tcslen(ptr_part) > 0 || _tcslen(ptr_ext) > 0)
        {
            _sntprintf(filename, MAX_PATH, _T("%s%s"), ptr_part, ptr_ext);
        }
    }
}

int
on_file_new(const HWND htab, eu_tabpage *psrc)
{
    eu_tabpage *pnode = psrc;
    TCHAR filename[100] = {0};
    const uint8_t *bom_str = NULL;
    if (pnode || (pnode = (eu_tabpage *) calloc(1, sizeof(eu_tabpage))))
    {
        pnode->is_blank = true;
        pnode->be_modify = false;
        pnode->fn_modify = false;
        pnode->begin_pos = -1;
        pnode->view = (htab && htab == HSLAVE_GET) ? 1 : 0;
        pnode->tab_id = -1;
        if (on_tabpage_generator(htab, filename, 100)[0])
        {
            _tcscpy(pnode->pathfile, filename);
            _tcscpy(pnode->filename, filename);
        }
        if (on_tabpage_insert(pnode) < SKYLARK_OK)
        {
            eu_safe_free(pnode);
            return EUE_INSERT_TAB_FAIL;
        }
        else
        {
            on_sci_before_file(pnode, true);
            on_sci_call(pnode, SCI_CLEARALL, 0, 0);
            pnode->eol = eu_get_config()->new_file_eol;
            pnode->codepage = eu_get_config()->new_file_enc;
        }
        switch (pnode->codepage)
        {
            case IDM_UNI_UTF8B:
                bom_str = (const uint8_t *) "\xEF\xBB\xBF";
                break;
            case IDM_UNI_UTF16LEB:
                bom_str = (const uint8_t *) "\xFF\xFE";
                break;
            case IDM_UNI_UTF16BEB:
                bom_str = (const uint8_t *) "\xFE\xFF";
                break;
            default:
                break;
        }
        if (bom_str)
        {
            on_encoding_set_bom(bom_str, pnode);
        }
        if ((htab && htab == HSLAVE_GET) && !eu_get_config()->eu_tab.slave_show)
        {
            eu_get_config()->eu_tab.slave_show = true;
        }
        on_file_update_time(pnode, time(NULL), false);
        on_sci_after_file(pnode, true);
        if ((pnode->tab_id = on_tabpage_selection(pnode)) >= 0)
        {
            pnode->initial = 1;
        }
        return SKYLARK_OK;
    }
    return EUE_POINT_NULL;
}

uint64_t
on_file_get_avail_phys(void)
{
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);
    return (statex.ullAvailPhys);
}

bool
on_file_map_hex(eu_tabpage *pnode, HANDLE hfile, const size_t nbyte)
{
    if (!pnode || pnode->phex)
    {
        return false;
    }
    // 如果是二进制文件, 使用内存映射
    pnode->phex = (PHEXVIEW) calloc(1, sizeof(HEXVIEW));
    if (!pnode->phex)
    {
        return false;
    }
    // PAGE_READWRITE, write immediately
    pnode->phex->hmap = share_create(hfile, PAGE_WRITECOPY, nbyte, NULL);
    if (pnode->phex->hmap == NULL)
    {
        eu_logmsg("File: share_create failed, cause %lu\n", GetLastError());
        eu_safe_free(pnode->phex);
        return false;
    }
    // 映射全部文件
    pnode->phex->pbase = (uint8_t *)share_map(pnode->phex->hmap, nbyte, FILE_MAP_COPY);
    if (pnode->phex->pbase == NULL)
    {
        eu_logmsg("File: share_map failed, cause %lu\n", GetLastError());
        share_close(pnode->phex->hmap);
        eu_safe_free(pnode->phex);
        return false;
    }
    pnode->phex->ex_style |= (pnode->file_attr & FILE_ATTRIBUTE_READONLY) ? HVS_READONLY : 0;
    eu_close_handle(pnode->phex->hmap);
    return true;
}

static int
on_file_set_codepage(eu_tabpage *pnode, const HANDLE hfile, const TCHAR *pfull)
{
    int bytesread = 0;
    int err = SKYLARK_OK;
    uint8_t *buf = NULL;
    const int check_len = (const int)(pnode->raw_size > BUFF_8M ? BUFF_8M : pnode->raw_size);
    do
    {
        if (!(buf = (uint8_t *)malloc(check_len+1)))
        {
            err = EUE_NOT_ENOUGH_MEMORY;
            break;
        }
        if (!ReadFile(hfile, buf, check_len, &bytesread, NULL))
        {
            MSG_BOX_ERR(IDC_MSG_OPEN_FAIL, IDC_MSG_ERROR, MB_ICONERROR | MB_OK);
            err = EUE_API_READ_FILE_ERR;
            break;
        }
        if (bytesread < 1)
        {
            // 为空的新文件, 设为默认模式
            pnode->eol = eu_get_config()->new_file_eol;
            pnode->codepage = eu_get_config()->new_file_enc;
        }
        else // 如果是utf8编码, 获取换行符, 否则, 等编码转换后再获取换行符
        {
            buf[bytesread] = '\0';
            pnode->codepage = eu_try_encoding(buf, bytesread, false, pfull);
            if (pnode->codepage < IDM_UNI_UTF16LE)
            {
                pnode->eol = on_encoding_line_mode((const char *)buf, bytesread);
            }
            if (pnode->codepage < IDM_OTHER_BIN)
            {
                on_encoding_set_bom((const uint8_t *) buf, pnode);
            }
        }
    } while(0);
    eu_safe_free(buf);
    return err;
}

static size_t
on_file_header_parser(void *hdr, size_t size, size_t nmemb, void *userdata)
{
    const size_t cb = size * nmemb;
    if (hdr && userdata)
    {
        char *p = NULL;
        char *u8_file = ((char (*)[MAX_PATH])userdata)[0];
        if (u8_file[0] && (p = strstr(hdr, u8_file)) && (p[strlen(p) - 1] == '\r' || p[strlen(p) - 1] == '\n'))
        {
            eu_logmsg("File: we do util_split_attr, hdr = [%s]\n", (const char*)hdr);
            util_split_attr(hdr, userdata, ' ');
            return 0;
        }
    }
    return cb;
}

static void
on_file_attr_parser(eu_tabpage *pnode, char (*pstr)[MAX_PATH])
{
    if (pnode && pstr)
    {
        for (int i = 0; i < TWO_DISM; ++i)
        {   // 第四列, 是文件尺寸
            if (i == 4 && pstr[i][0])
            {   // 预先获取文件大小
                pnode->raw_size = _atoi64(pstr[i]);
                eu_logmsg("File: %s, raw_size = %I64u\n", __FUNCTION__, pnode->raw_size);
            }
        }
    }
}

static int
on_file_remote_lenght(eu_tabpage *pnode, const wchar_t *path)
{
    char *url = NULL;
    CURL *curl = NULL;
    CURLcode res = EUE_CURL_INIT_FAIL;
    char data[TWO_DISM][MAX_PATH] = {0};
    remotefs *pserver = on_remote_list_find(path);
    if (!pserver)
    {
        return res;
    }
    if ((url = eu_utf16_utf8(path, NULL)) == NULL)
    {
        return res;
    }
    do
    {
        char *p = NULL;
        if ((p = strrchr(url, '/')) && strlen(p) > 1)
        {
            p[1] = 0;
        }
        if (!(curl = on_remote_init_socket(url, pserver)))
        {
            break;
        }
        // INOUT, 传送一个文件名, 返回一个数组指针
        data[0][0] = ' ';
        util_make_u8(pnode->filename, &data[0][1], MAX_PATH - 2);
        eu_curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
        eu_curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
        eu_curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "sftp");
        eu_curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_file_header_parser);
        eu_curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);
        if ((res = eu_curl_easy_perform(curl)) == CURLE_OK || res == CURLE_WRITE_ERROR)
        {
            res = CURLE_OK;
            on_file_attr_parser(pnode, data);
        }
        else
        {
            const char *err_string = eu_curl_easy_strerror(res);
            eu_logmsg("File: %s, error[%d]: %s\n", __FUNCTION__, res, err_string);
            pnode->raw_size = 0;
        }
    } while(0);
    if (curl)
    {
        eu_curl_easy_cleanup(curl);
    }
    free(url);
    return res;
}

static int
on_file_preload(eu_tabpage *pnode, file_backup *pbak)
{
    int err = SKYLARK_OK;
    HANDLE hfile = NULL;
    const TCHAR *pfull = (const TCHAR *)(pbak ? pbak->rel_path : NULL);
    if (!pnode || !pbak)
    {
        return EUE_TAB_NULL;
    }
    if (pbak->status && eu_exist_file(pbak->bak_path))
    {
        pfull = pbak->bak_path;
    }
    if (STR_IS_NUL(pfull))
    {
        return EUE_PATH_NULL;
    }
    else 
    {
        pnode->hex_mode = pbak->hex;
    }
    if (url_has_remote(pfull))
    {
        if ((err = on_file_remote_lenght(pnode, pfull)) != SKYLARK_OK)
        {
            eu_logmsg("File: on_file_remote_lenght return false, case: %d\n", err);
            err = EUE_CURL_NETWORK_ERR;
        }
        return err;
    }
    else if ((pnode->file_attr = GetFileAttributes(pfull)) == INVALID_FILE_ATTRIBUTES)
    {
        return EUE_FILE_ATTR_ERR;
    }
    if (!share_open_file(pfull, true, OPEN_EXISTING, &hfile))
    {
        return EUE_API_OPEN_FILE_ERR;
    }
    if (!util_file_size(hfile, &pnode->raw_size))
    {
        err = EUE_FILE_SIZE_ERR;
        goto pre_clean;
    }
    if (pbak->cp)
    {   // 存在备份,不再测试编码
        pnode->eol = pbak->eol;
        pnode->nc_pos = pbak->postion;
        pnode->codepage = pbak->cp;
        // 如果是二进制文件, 以on_file_map_hex打开
        if (!TAB_NOT_BIN(pnode))
        {
            if (!on_file_map_hex(pnode, hfile, 0))
            {
                err = EUE_MAP_HEX_ERR;
            }
            else 
            {
                pnode->hex_mode = TYPES_HEX;
            }
            goto pre_clean;
        }
    }
    if (TAB_HEX_MODE(pnode))
    {   // 如果是16进制模式, 文本以及插件也可能处于16进制模式
        if ((err = !on_file_map_hex(pnode, hfile, 0)) != 0)
        {
            err = EUE_MAP_HEX_ERR;
            goto pre_clean;
        }
        if (pbak->cp)
        {
            pnode->phex->hex_ascii = true;
            on_encoding_set_bom_from_cp(pnode);
        }
        else
        {
            err = on_file_set_codepage(pnode, hfile, pfull);
        }
        goto pre_clean;
    }
    if (pnode->pmod)
    {   // 如果加载了插件, 后续on_file_load再次检查
        err = SKYLARK_OK;
        goto pre_clean;
    }
    if (pbak->cp)
    {
        on_encoding_set_bom_from_cp(pnode);
        goto pre_clean;
    }
    if ((err = on_file_set_codepage(pnode, hfile, pfull)) == SKYLARK_OK && !TAB_NOT_BIN(pnode))
    {   // 不在备份中打开, 测试是否16进制文件?
        if (!on_file_map_hex(pnode, hfile, 0))
        {
            err = EUE_MAP_HEX_ERR;
        }
        else 
        {
            pnode->hex_mode = TYPES_HEX;
        }
    }
pre_clean:
    share_close(hfile);
    return err;
}

int
on_file_load_plugins(eu_tabpage *pnode, bool route_open)
{
    int ret = np_plugins_initialize(pnode->pmod, &pnode->plugin);
    if (ret == NP_NO_ERROR && pnode->plugin)
    {
        eu_logmsg("File: np_plugins_initialize ok!\n");
        ret = pnode->plugin->funcs.newp(&pnode->plugin->npp, NULL);
        if (ret == 0)
        {
            pnode->hex_mode = TYPES_PLUGIN;
            pnode->codepage = IDM_OTHER_PLUGIN;
            pnode->plugin->win.window = pnode->hwnd_sc;
            pnode->plugin->funcs.setwindow(&pnode->plugin->npp, &pnode->plugin->win);
            if (!route_open)
            {
                char u8_file[MAX_BUFFER] = {0};
                util_make_u8(eu_exist_file(pnode->bakpath) ? pnode->bakpath : pnode->pathfile, u8_file, MAX_BUFFER);
                pnode->plugin->stream.url = (const char *)u8_file;
                pnode->plugin->stream.end = (intptr_t)pnode->raw_size;
                ret = pnode->plugin->funcs.asfile(&pnode->plugin->npp, &pnode->plugin->stream);
            }
            else
            {
                uint16_t type;
                pnode->plugin->stream.end = (intptr_t)pnode->raw_size;
                ret = pnode->plugin->funcs.newstream(&pnode->plugin->npp, &pnode->plugin->stream, false, &type);
                if (ret == NP_NO_ERROR)
                {
                    pnode->plugin->funcs.writeready(&pnode->plugin->npp, &pnode->plugin->stream);
                }
            }
        }
    }
    return ret;
}

int
on_file_load(eu_tabpage *pnode, file_backup *pbak, const bool force)
{
    size_t len = 0;
    size_t buf_len = 0;
    size_t err = SKYLARK_OK;
    bool is_utf8 = false;
    TCHAR *pfull = NULL;
    npn_stream uf_stream = {0};
    if (!pnode)
    {
        return EUE_TAB_NULL;
    }
    if (pbak && pbak->status && eu_exist_file(pbak->bak_path))
    {
        pfull = pbak->bak_path;
        is_utf8 = true;
    }
    else
    {
        pfull = pnode->pathfile;
    }
    if (TAB_HEX_MODE(pnode) || (pnode->is_blank && !is_utf8))
    {   // 16进制模式下, on_file_preload已经打开
        // 新建的空白文件, 不用载入数据
        return SKYLARK_OK;
    }
    if (pnode->pmod)
    {
        eu_logmsg("File: on_file_load_plugins execute\n");
        return on_file_load_plugins(pnode, false);
    }
    if (STR_IS_NUL(pfull))
    {
        return EUE_PATH_NULL;
    }
    if (!force)
    {
        uf_stream.size = (size_t)pnode->raw_size;
    }
    if (!util_open_file(pfull, &uf_stream))
    {
        MSG_BOX_ERR(IDC_MSG_OPEN_FAIL, IDC_MSG_ERROR, MB_ICONERROR | MB_OK);
        return EUE_OPEN_FILE_ERR;
    }
    if (force)
    {
        // reassign variables
        pnode->raw_size = uf_stream.size;
    }
    if (is_utf8)
    {
        len = uf_stream.size;
    }
    else
    {
        len = uf_stream.size - pnode->pre_len;
    }
    if (!is_utf8 &&
       ((pnode->codepage > IDM_UNI_UTF8B && pnode->codepage < IDM_OTHER_BIN) ||
       (pnode->codepage >= IDM_ICONV_001 && pnode->codepage <= IDM_ICONV_133)))
    {
        char *pdst = NULL;
        size_t dst_len = 0;
        euconv_t evd = {0};
        evd.src_from = eu_query_encoding_name(pnode->codepage);
        evd.dst_to = "utf-8";
        err = on_encoding_do_iconv(&evd, (char *) (uf_stream.base+pnode->pre_len), &len, &pdst, &dst_len);
        if (err == (size_t) -1)
        {
            eu_logmsg("File: %s, on_encoding_do_iconv error\n", __FUNCTION__);
            MSG_BOX(IDC_MSG_ICONV_FAIL2, IDC_MSG_ERROR, MB_ICONERROR | MB_OK);
            // 载入源数据, 然后让用户指定编码
            on_sci_call(pnode, SCI_ADDTEXT, len, (LPARAM)(uf_stream.base+pnode->pre_len));
            pnode->codepage = IDM_UNKNOWN;
            err = SKYLARK_OK;
        }
        if (pdst)
        {
            if (pnode->eol < 0)
            {
                pnode->eol = on_encoding_line_mode(pdst, dst_len);
            }
            on_sci_call(pnode, SCI_ADDTEXT, dst_len, (LPARAM)(pdst));
        }
        eu_safe_free(pdst);
    }
    else if (is_utf8)
    {
        on_sci_call(pnode, SCI_ADDTEXT, len, (LPARAM)(uf_stream.base));
    }
    else
    {
        on_sci_call(pnode, SCI_ADDTEXT, len, (LPARAM)(uf_stream.base+pnode->pre_len));
    }
    uf_stream.close(&uf_stream);
    return (int)err;
}

static void
on_file_update_postion(eu_tabpage *p, const file_backup *pbak)
{
    if (p)
    {
        sptr_t line = 0;
        const intptr_t x = pbak ? pbak->x : p->x;
        const intptr_t y = pbak ? pbak->y : p->y;
        if (p->hex_mode)
        {
            if (x >= 0)
            {
                p->nc_pos = (x > 0 ? x - 1 : x) * 16;
            }
            else if (x == -2)
            {
                line = on_sci_call(p, SCI_GETLINECOUNT, 0, 0);
                p->nc_pos = line > 0 ? (line - 1) * 16 : 0;
            }
            if (y >= 0)
            {
                p->nc_pos += (y > 0 ? y - 1 : y) % 16;
            }
            else if (y == -2)
            {
                p->nc_pos += 16 - 1;
            }
        }
        else
        {
            if (x >= 0)
            {
                p->nc_pos = on_sci_call(p, SCI_POSITIONFROMLINE, x > 0 ? x - 1 : 0, 0);
            }
            else if (x == -2)
            {
                line = on_sci_call(p, SCI_GETLINECOUNT, 0, 0);
                p->nc_pos = on_sci_call(p, SCI_GETLINEENDPOSITION, line - 1, 0) + (sptr_t)strlen(on_encoding_get_eol(p));
            }
            if (y >= 0)
            {
                p->nc_pos += (y > 0 ? y - 1 : 0);
            }
            else if (y == -2)
            {
                if (x == -2)
                {
                    p->nc_pos = on_sci_call(p, SCI_GETLENGTH, 0, 0);
                }
                else if (x >= 0)
                {
                    p->nc_pos = on_sci_call(p, SCI_GETLINEENDPOSITION, x > 0 ? x - 1 : 0, 0);
                }
            }
        }
        if (p->nc_pos < 0)
        {
            p->nc_pos = 0;
        }
    }
}
/**************************************************************************************
 * 文件是否打开已经打开, 参数selection代表是否选中新打开的标签
 * 如果已经打开, 返回标签号, 其值 >= 0, 否则, 返回SKYLARK_NOT_OPENED(-2)
 * 如果出现其他错误, 返回值 < SKYLARK_NOT_OPENED
 **************************************************************************************/
static int
on_file_open_if(const TCHAR *pfile, const bool selection)
{
    int res = SKYLARK_NOT_OPENED;
    eu_tabpage *pnode = NULL;
    HWND htab[2] = {HMAIN_GET, HSLAVE_SHOW ? HSLAVE_GET : NULL};
    if (STR_IS_NUL(pfile))
    {
        return EUE_PATH_NULL;
    }
    for (int k = 0; k < 2 && htab[k]; ++k)
    {
        for (int index = 0, count = TabCtrl_GetItemCount(htab[k]); index < count; ++index)
        {
            if ((pnode = on_tabpage_get_ptr(htab[k], index)) == NULL)
            {
                eu_logmsg("File: on_tabpage_get_ptr return false\n");
                res = SKYLARK_TABCTRL_ERR;
                break;
            }
            if (_tcsicmp(pnode->pathfile, pfile) == 0)
            {
                if (selection)
                {
                    if (pnode->view && !eu_get_config()->eu_tab.slave_show)
                    {
                        eu_get_config()->eu_tab.slave_show = true;
                        res = on_tabpage_selection(pnode);
                    }
                    else
                    {
                        res = index;
                    }
                }
                break;
            }
            if (!url_has_remote(pfile) && _tcsrchr(pfile, _T('/')))
            {
                TCHAR temp[MAX_BUFFER] = {0};
                _sntprintf(temp, MAX_BUFFER, _T("%s"), pfile);
                eu_wstr_replace(temp, MAX_BUFFER, _T("/"), _T("\\"));
                if (_tcsicmp(pnode->pathfile, temp) == 0)
                {
                    if (selection)
                    {
                        if (pnode->view && !eu_get_config()->eu_tab.slave_show)
                        {
                            eu_get_config()->eu_tab.slave_show = true;
                            res = on_tabpage_selection(pnode);
                        }
                        else
                        {
                            res = index;
                        }
                    }
                    break;
                }
            }
        }
    }
    return res;
}

void
on_file_active_condition(const HWND htab, const int index)
{
    if (htab)
    {
        const int count = TabCtrl_GetItemCount(htab);
        if (count < 1)
        {
            if (htab == HSLAVE_GET)
            {
                eu_tabpage *p = NULL;
                eu_get_config()->eu_tab.slave_show = false;
                on_proc_redraw(NULL);
                if ((p = on_tabpage_focus_at(HMAIN_GET)))
                {
                    on_tabpage_active_tab(p);
                }
            }
            else if (!eu_get_config()->m_exit)
            {
                on_file_new(htab, NULL);
            }
            else if (on_tabpage_other_empty(htab) && on_sql_sync_session() == SKYLARK_OK)
            {
                eu_logmsg("File: close last tab, skylark exit ...\n");
                SendMessage(eu_hwnd_self(), WM_BACKUP_OVER, 0, 0);
            }
        }
        else if (index >= 0)
        {
            switch (eu_get_config()->m_tab_active)
            {   // 激活另一个标签
                case IDM_VIEW_LEFT_TAB:
                    on_tabpage_select_index(htab, index > 0 ? index - 1 : count - 1);
                    break;
                case IDM_VIEW_RIGHT_TAB:
                    on_tabpage_select_index(htab, index > count - 1 ? 0 : index);
                    break;
                case IDM_VIEW_FAR_LEFT_TAB:
                    on_tabpage_select_index(htab, 0);
                    break;
                case IDM_VIEW_FAR_RIGHT_TAB:
                    on_tabpage_select_index(htab, count - 1);
                    break;
                default:
                    break;
            }
        }
        else
        {
            on_tabpage_select_index(htab, 0);
        }
    }
}

static void
on_file_active_other(eu_tabpage *pnode)
{
    HWND htab = on_tabpage_hwnd(pnode);
    if (htab)
    {
        const int count = TabCtrl_GetItemCount(htab);
        if (count < 1)
        {
            if (htab == HSLAVE_GET)
            {
                eu_get_config()->eu_tab.slave_show = false;
                on_proc_redraw(NULL);
                if ((pnode = on_tabpage_focus_at(HMAIN_GET)))
                {
                    on_tabpage_active_tab(pnode);
                }
            }
            else if (eu_get_config()->m_exit && on_tabpage_other_empty(htab))
            {   // 最后一个标签退出设置
                pnode->reason = TABS_MAYBE_EIXT;
            }
            else if (pnode->reason != TABS_MAYBE_RESERVE)
            {
                on_file_new(htab, NULL);
            }
        }
        else
        {
            on_file_active_condition(htab, pnode->tab_id);
        }
    }
}

void
on_file_before_open(eu_tabpage *pnode)
{
    if (pnode && !TAB_HEX_MODE(pnode) && !pnode->pmod)
    {
        on_sci_before_file(pnode, true);
        on_sci_call(pnode, SCI_CLEARALL, 0, 0);
        // 设置文档类型指针
        if (!pnode->doc_ptr && pnode->raw_size < (uint64_t)(eu_get_config()->m_limit * 1024 * 1024))
        {
            pnode->doc_ptr = on_doc_get_type(pnode->filename);
        }
        if (pnode->tab_focus)
        {   // 把工作目录设置在进程所在地
            util_set_working_dir(eu_module_path, NULL);
        }
    }
}

int
on_file_after_open(eu_tabpage *pnode)
{
    TCHAR *p = NULL;
    if (pnode)
    {
        on_sci_after_file(pnode, true);
        if (pnode->be_modify)
        {
            pnode->fn_modify = true;
        }
        if (!pnode->is_blank)
        {
            on_file_push_recent(pnode);
        }
        if ((p = _tcsrchr(pnode->bakpath, _T('\\'))) && p++)
        {
            const int len = (const int)_tcslen(p);
            if (util_isxdigit_string(p, len - 2) && p[len - 1] == _T('~') && p[len - 2] == _T('~'))
            {
                eu_logmsg("File: differential backup, recovering\n");
                pnode->bakpath[_tcslen(pnode->bakpath) - 2] = 0;
            }
        }
        if (TAB_HAS_TXT(pnode))
        {
            if (strlen(pnode->mark_id) > 0)
            {   // 恢复书签
                on_search_update_mark(pnode, pnode->mark_id);
            }
            if (strlen(pnode->fold_id) > 0)
            {   // 恢复折叠
                on_search_update_fold(pnode, pnode->fold_id);
            }
        }
        if (!pnode->plugin)
        {   // 恢复光标位置
            on_file_update_postion(pnode, NULL);
            on_navigate_list_add(pnode);
        }
        if (pnode->tab_focus > 0)
        {
            pnode->tab_id = on_tabpage_selection(pnode);
        }
        else
        {
            on_tabpage_size(NULL);
        }
        return SKYLARK_OK;
    }
    return EUE_POINT_NULL;
}

static time_t
on_file_max_date(file_backup *pbak)
{
    if (pbak)
    {
        int result = 0;
        struct _stat rel_t = {0};
        struct _stat bak_t = {0};
        if (STR_NOT_NUL(pbak->rel_path))
        {
            result = _tstat(pbak->rel_path, &rel_t);
        }
        if (STR_NOT_NUL(pbak->bak_path))
        {
            _tstat(pbak->bak_path, &bak_t);
        }
        if (rel_t.st_mtime > bak_t.st_mtime)
        {
            pbak->status = 0;
        }
        return (result == 0 && rel_t.st_mtime > 0 ? rel_t.st_mtime : bak_t.st_mtime);
    }
    return 0;
}

int
on_file_node_init(eu_tabpage **p, file_backup *pbak)
{
    if (p && pbak)
    {
        if (*p)
        {
            eu_logmsg("File: waning, node != NULL (should be NULL)\n");
        }
        if ((*p = (eu_tabpage *) calloc(1, sizeof(eu_tabpage))) == NULL)
        {
            return EUE_OUT_OF_MEMORY;
        }
        (*p)->begin_pos = -1;
        (*p)->nc_pos = pbak->postion;
        (*p)->tab_id = pbak->tab_id;
        (*p)->view = pbak->view;
        (*p)->tab_focus = pbak->focus;
        (*p)->hex_mode = pbak->hex;
        (*p)->is_blank = pbak->blank;
        (*p)->be_modify = !!pbak->status;
        (*p)->codepage = pbak->cp;
        (*p)->bakcp = pbak->bakcp;
        (*p)->eol = pbak->cp > 0 ? pbak->eol : -1;
        (*p)->x = pbak->x;
        (*p)->y = pbak->y;
        if (pbak->mark_id[0])
        {
            strcpy((*p)->mark_id, pbak->mark_id);
        }
        if (pbak->fold_id[0])
        {
            strcpy((*p)->fold_id, pbak->fold_id);
        }
        if (STR_NOT_NUL(pbak->rel_path))
        {   // 是远程文件, 查找服务器配置
            if (url_has_remote(pbak->rel_path))
            {
                remotefs *pserver = on_remote_list_find(pbak->rel_path);
                if (pserver)
                {
                    memcpy(&((*p)->fs_server), pserver, sizeof(remotefs));
                }
                _tsplitpath(pbak->rel_path, NULL, NULL, (*p)->filename, (*p)->extname);
                if (_tcslen((*p)->extname) > 0)
                {
                    _tcsncat((*p)->filename, (*p)->extname, MAX_PATH);
                }
                _tcsncpy((*p)->pathfile, pbak->rel_path, MAX_BUFFER);
            }
            else
            {
                on_file_splite_path(pbak->rel_path, (*p)->pathname, (*p)->filename, NULL, (*p)->extname);
                _tcsncpy((*p)->pathfile, pbak->rel_path, MAX_BUFFER);
            }
        }
        if (STR_NOT_NUL(pbak->bak_path))
        {
            _tcsncpy((*p)->bakpath, pbak->bak_path, MAX_BUFFER);
        }
        if (true)
        {
            (*p)->zoom_level = pbak->zoom;
            on_file_update_time((*p), on_file_max_date(pbak), false);
        }
        if ((*p)->extname[0])
        {
            np_plugins_lookup(NPP_PDFVIEW, (*p)->extname, &(*p)->pmod);
        }
        return SKYLARK_OK;
    }
    return EUE_POINT_NULL;
}

/**************************************************************************************
 * 文件打开函数, 需要自己初始化结构, 其中.focus为是否选中标签
 * 成功打开文件, 确保返回的是当前标签的序号
 * 如果失败, 返回一个负数错误值
 **************************************************************************************/
int
on_file_only_open(file_backup *pbak)
{
    eu_tabpage *pnode = NULL;
    int res = on_file_open_if(pbak->rel_path, true);
    const HWND htab = pnode ? (!pbak->view ? HMAIN_GET : HSLAVE_GET) : NULL;
    if (res < SKYLARK_NOT_OPENED)
    {
        return res;
    }
    if (res >= SKYLARK_OK && (pnode = on_tabpage_get_ptr(htab, res)) != NULL)
    {   // 如果文件已经打开, 根据新参数更新插入符位置
        on_file_update_postion(pnode, pbak);
        if (pnode->nc_pos >= 0)
        {
            eu_logmsg("File: we jump to %zd\n", pnode->nc_pos);
            on_search_jmp_pos(pnode);
        }
        if (!TAB_HEX_MODE(pnode))
        {
            res = pbak && pbak->hex == TYPES_HEX ? hexview_switch_mode(pnode) : res;
        }
        else if (pbak && pbak->hex == TYPES_TEXT && !eu_get_config()->m_instance)
        {
            res = hexview_switch_mode(pnode);
        }
        return res;
    }
    if ((res = on_file_node_init(&pnode, pbak)) != SKYLARK_OK)
    {
        return res;
    }
    if ((res = on_file_preload(pnode, pbak)) != SKYLARK_OK)
    {
        eu_safe_free(pnode);
        eu_logmsg("File: on_file_preload failed, err = %d\n", res);
        return res;
    }
    util_lock_v2(pnode);
    do
    {
        if ((res = on_tabpage_insert(pnode)) < SKYLARK_OK)
        {
            eu_logmsg("File: on_tabpage_insert failed, err = %d\n", res);
            break;
        }
        on_file_before_open(pnode);
        if (on_file_load(pnode, pbak, false))
        {
            res = EUE_WRITE_TAB_FAIL;
            break;
        }
        if (on_file_after_open(pnode) != SKYLARK_OK)
        {
            res = SKYLARK_NOT_OPENED;
        }
    } while(0);
    util_unlock_v2(pnode);
    if (res < 0)
    {
        if (res == EUE_WRITE_TAB_FAIL)
        {
            on_file_active_other(on_tabpage_remove(pnode, FILE_SHUTDOWN));
        }
        on_sci_free_tab(&pnode);
    }
    return res;
}

static int
on_file_open_bakcup(file_backup *pbak)
{
    return (on_file_only_open(pbak) >= 0 ? SKYLARK_OK : SKYLARK_NOT_OPENED);
}

/**************************************************************************************
 * 文件打开函数, 不带参数. 从资源管理器接受文件
 * 成功打开文件, 返回SKYLARK_OK
 * 否则, 返回非零值
 **************************************************************************************/
int
on_file_open(void)
{
    int err = SKYLARK_OK;
    int path_len = 0;
    file_backup bak = {-1, -1, 0, -1};
    cvector_vector_type(file_backup) vbak = NULL;
    TCHAR *p = NULL;
    TCHAR pathname[MAX_BUFFER] = {0};
    TCHAR *file_list = (TCHAR *)calloc(sizeof(TCHAR), BUFF_32K);
    if (file_list == NULL)
    {
        eu_logmsg("File: %s, memory allocation failed\n", __FUNCTION__);
        return EUE_OUT_OF_MEMORY;
    }
    if ((err = on_file_open_filename_dlg(NULL, file_list, BUFF_32K)) != SKYLARK_OK)
    {
        goto mem_clean;
    }
    if (!(GetFileAttributes(file_list) & FILE_ATTRIBUTE_DIRECTORY))
    {
        _tcsncpy(bak.rel_path, file_list, MAX_BUFFER);
        cvector_push_back(vbak, bak);
    }
    else
    {
        _tcsncpy(pathname, file_list, MAX_BUFFER - 1);
        path_len = (int) _tcslen(pathname);
        p = file_list + path_len + 1;
        while (*p)
        {
            _sntprintf(bak.rel_path, MAX_BUFFER, _T("%s\\%s"), pathname, p);
            cvector_push_back(vbak, bak);
            p += _tcslen(p) + 1;
        }
    }
    if ((path_len = (int)cvector_size(vbak)) > 0)
    {
        vbak[path_len - 1].focus = 1;
        err = on_file_redirect(vbak, (const size_t)path_len);
    }
mem_clean:
    if (file_list)
    {
        free(file_list);
    }
    cvector_free(vbak);
    return err;
}

/**************************************************************************************
 * 拖动标签时打开新窗口的函数,
 * htab 主/从tabbar句柄
 * index标签序号
 * 无论是否打开文件, 它总是弹出一个新窗口, 所以返回值总是SKYLARK_OK
 **************************************************************************************/
int
on_file_out_open(const HWND htab, const int index, uint32_t *pid)
{
    eu_tabpage *p = on_tabpage_get_ptr(htab, index);
    if (p && (!p->is_blank  || TAB_NOT_NUL(p)))
    {
        TCHAR process[MAX_BUFFER] = {0};
        if (GetModuleFileName(NULL , process , MAX_BUFFER) > 0)
        {
            int err = SKYLARK_NOT_OPENED;
            // FILE_REMOTE_CLOSE关闭时先保存标签正确的编号
            p->tab_id = (int)index;
            if (!eu_get_config()->m_session)
            {
                if (!p->is_blank)
                {
                    sptr_t pos = on_sci_call(p, SCI_GETCURRENTPOS, 0, 0);
                    sptr_t lineno = on_sci_call(p, SCI_LINEFROMPOSITION, pos, 0);
                    sptr_t row = on_sci_call(p, SCI_POSITIONFROMLINE, lineno, 0);
                    _sntprintf(process, MAX_BUFFER, _T("%s%s\"%s\" -n%zd -c%zd"), process, _T(" -noremote "), p->pathfile, lineno+1, pos-row+1);
                    if (!p->plugin && TAB_HEX_MODE(p))
                    {
                        _tcsncat(process, _T(" -hex"), MAX_BUFFER);
                    }
                    if (p->view)
                    {
                        _tcsncat(process, _T(" -v1"), MAX_BUFFER);
                    }
                    err = on_file_close(&p, FILE_ONLY_CLOSE);
                }
            }
            else
            {
                _sntprintf(process, MAX_BUFFER, _T("%s%s%s%d"), process, _T(" -noremote "), _T("#"), index);
                err = on_file_close(&p, FILE_REMOTE_CLOSE);
            }
            if (err == SKYLARK_OK)
            {
                CloseHandle(eu_new_process(process, NULL, NULL, 0, pid));
            }
        }
    }
    return SKYLARK_OK;
}

/**************************************************************************************
 * 多标签打开文件时调用的第一个函数
 * 成功打开文件返回SKYLARK_OK
 **************************************************************************************/
int
on_file_redirect(file_backup *pbak, const size_t vsize)
{
    int err = SKYLARK_NOT_OPENED;
    for (size_t i = 0; i < vsize; ++i)
    {
        if (pbak[i].rel_path[0])
        {
            if (!pbak[i].status && url_has_remote(pbak[i].rel_path))
            {
                err = (on_file_open_remote(NULL, &pbak[i]) >= 0 ? SKYLARK_OK : SKYLARK_NOT_OPENED);
            }
            else if (url_que_mark(pbak[i].rel_path))
            {
                pbak[i].rel_path[_tcslen(pbak[i].rel_path) - 1] = 0;
                on_treebar_locate_path(pbak[i].rel_path);
            }
            else
            {
                err = on_file_open_bakcup(&pbak[i]);
            }
        }
    }
    if (err != SKYLARK_OK)
    {   // 打开文件失败且标签小于1,则建立一个空白标签页
        const HWND htab = HMAIN_GET;
        if (htab && TabCtrl_GetItemCount(htab) < 1)
        {
            err = on_file_new(htab, NULL);
        }
    }
    else if (vsize > 1)
    {
        on_proc_redraw(NULL);
    }
    return err;
}

int
on_file_drop(HDROP hdrop)
{
    uint32_t attr = 0;
    file_backup bak = {-1, -1, 0, -1};
    cvector_vector_type(file_backup) vbak = NULL;
    int count = DragQueryFile(hdrop, 0xFFFFFFFF, NULL, 0);
    for (int index = 0; index < count; ++index)
    {
        memset(bak.rel_path, 0, sizeof(bak.rel_path));
        DragQueryFile(hdrop, index, bak.rel_path, MAX_BUFFER);
        attr = GetFileAttributes(bak.rel_path);
        if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
        {
            cvector_push_back(vbak, bak);
        }
        else
        {
            util_bfs_search(bak.rel_path, &vbak, &bak);
        }
    }
    DragFinish(hdrop);
    if ((count = (int)cvector_size(vbak)) > 0)
    {
        vbak[count - 1].focus = 1; 
        on_file_redirect(vbak, count);
    }
    eu_wine_dotool();
    SwitchToThisWindow(eu_hwnd_self(), TRUE);
    return SKYLARK_OK;
}

static size_t
on_file_read_remote(void *buffer, size_t size, size_t nmemb, void *stream)
{
    eu_tabpage *pnode = (eu_tabpage *) stream;
    char *data = (char *) buffer;
    size_t len;
    size_t offset = 0;
    len = size * nmemb;
    if (!pnode->bytes_remaining)
    {
        if (TAB_HEX_MODE(pnode) || !pnode->pmod)
        {
            pnode->codepage = eu_try_encoding(buffer, len, false, pnode->filename);
            if (pnode->codepage < IDM_UNI_UTF16LE)
            {
                pnode->eol = on_encoding_line_mode(buffer, len);
            }
            if (pnode->codepage < IDM_OTHER_BIN)
            {
                on_encoding_set_bom((const uint8_t *) buffer, pnode);
            }
            offset = pnode->pre_len;
        }
        else if (on_file_load_plugins(pnode, true))
        {
            eu_logmsg("File: %s, on_file_load_plugins failed\n", __FUNCTION__);
            return 0;
        }
    }
    else
    {
        offset = 0;
    }
    if (((pnode->codepage > IDM_UNI_UTF8B && pnode->codepage < IDM_OTHER_BIN) ||
       (pnode->codepage >= IDM_ICONV_001 && pnode->codepage <= IDM_ICONV_133)))
    {
        char *pdst = NULL;
        char *psrc = data + offset;
        size_t src_len = len - offset;
        size_t dst_len = 0;
        euconv_t evd = {0};
        evd.src_from = eu_query_encoding_name(pnode->codepage);
        evd.dst_to = "utf-8";
        size_t res = on_encoding_do_iconv(&evd, psrc, &src_len, &pdst, &dst_len);
        if (res == (size_t) -1)
        {
            return 0;
        }
        if (pnode->eol < 0)
        {
            pnode->eol = on_encoding_line_mode(pdst, dst_len);
        }
        on_sci_call(pnode, SCI_ADDTEXT, dst_len, (LPARAM)(pdst));
        free(pdst);
    }
    else
    {
        if (TAB_HEX_MODE(pnode) || !pnode->pmod)
        {
            on_sci_call(pnode, SCI_ADDTEXT, len - offset, (LPARAM)(data + offset));
        }
        else if (pnode->plugin && pnode->plugin->funcs.write)
        {
            pnode->plugin->funcs.write(&pnode->plugin->npp, &pnode->plugin->stream, eu_uint_cast(pnode->bytes_remaining), eu_uint_cast(len), buffer);
        }
    }
    pnode->bytes_remaining += len - offset;
    return len;
}

static int
on_file_remote_thread(eu_tabpage *p, file_backup *pbak, remotefs *premote)
{
    int ret = EUE_CURL_INIT_FAIL;
    if (pbak && STR_NOT_NUL(pbak->rel_path))
    {
        if (p->fs_server.networkaddr[0] == 0 && premote)
        {
            memcpy(&(p->fs_server), premote, sizeof(remotefs));
        }
        if ((p->reserved0 = (intptr_t)eu_utf16_utf8(pbak->rel_path, NULL)) != 0)
        {
            CURL *curl = on_remote_init_socket((const char *)p->reserved0, &p->fs_server);
            free((void *)p->reserved0);
            p->reserved0 = 0;
            if (curl)
            {
                eu_curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_file_read_remote);
                eu_curl_easy_setopt(curl, CURLOPT_WRITEDATA, p);
            #if APP_DEBUG
                eu_curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
            #endif
                eu_curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 6L);
                eu_curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
                eu_curl_easy_setopt(curl, CURLOPT_FILETIME, 1L);
                ret = eu_curl_easy_perform(curl);
                if (!ret)
                {
                    ret = eu_curl_easy_getinfo(curl, CURLINFO_FILETIME_T, &p->st_mtime);
                    eu_logmsg("File: pnode->st_mtime = %lld\n", p->st_mtime);
                }
                eu_curl_easy_cleanup(curl);
            }
        }
    }
    return ret;
}

/**************************************************************************************
 * 打开远程文件的函数
 * premote为远程服务器信息的结构体变量
 * pbak包含文件路径的一个结构体变量
 * 函数成功, 返回值为当前打开标签的序号, 失败则为负数
 **************************************************************************************/
int
on_file_open_remote(remotefs *premote, file_backup *pbak)
{
    eu_tabpage *pnode = NULL;
    const HWND htab = (pbak ? (!pbak->view ? HMAIN_GET : HSLAVE_GET) : NULL);
    int result = on_file_open_if(pbak->rel_path, true);
    if (result < SKYLARK_NOT_OPENED)
    {
        return result;
    }
    if (result >= SKYLARK_OK && (pnode = on_tabpage_get_ptr(htab, result)) != NULL)
    {   // 如果文件已经打开, 根据新参数更新插入符位置
        on_file_update_postion(pnode, pbak);
        if (pnode->nc_pos >= 0)
        {
            on_search_jmp_pos(pnode);
        }
        return result;
    }
    if ((result = on_file_node_init(&pnode, pbak)) != SKYLARK_OK)
    {
        return result;
    }
    if ((result = on_file_preload(pnode, pbak)) != SKYLARK_OK)
    {
        if (result == EUE_CURL_NETWORK_ERR)
        {   // 在数据库清理打开错误的文件
            on_sql_delete_backup_row(pnode);
        }
        eu_safe_free(pnode);
        eu_logmsg("File: on_file_preload failed, err = %d\n", result);
        return result;
    }
    util_lock_v2(pnode);
    do
    {
        if ((result = on_tabpage_insert(pnode)) < SKYLARK_OK)
        {
            eu_logmsg("File: on_tabpage_insert failed, err = %d\n", result);
            break;
        }
        on_file_before_open(pnode);
        if (on_file_remote_thread(pnode, pbak, premote) != CURLE_OK)
        {
            on_sql_delete_backup_row(pnode);
            result = EUE_CURL_NETWORK_ERR;
            break;
        }
        if (pnode->be_modify && (!pbak || !pbak->status))
        {   // add_text导致了SCN_SAVEPOINTLEFT消息
            // 重新修改回状态
            pnode->be_modify = false;
        }
        if (on_file_after_open(pnode) != SKYLARK_OK)
        {
            result = SKYLARK_NOT_OPENED;
        }
    } while(0);
    util_unlock_v2(pnode);
    if (result < 0)
    {
        if (result == EUE_CURL_NETWORK_ERR)
        {
            on_file_active_other(on_tabpage_remove(pnode, FILE_SHUTDOWN));
            MSG_BOX(IDC_MSG_ATTACH_FAIL, IDC_MSG_ERROR, MB_ICONERROR | MB_OK);
        }
        on_sci_free_tab(&pnode);
    }
    else
    {   // 远程二进制文件, 选择以插件或16进制编辑器打开
        if (!TAB_HEX_MODE(pnode) && pnode->pmod && pnode->plugin && pnode->plugin->funcs.write)
        {
            pnode->raw_size = pnode->bytes_remaining;
            pnode->plugin->funcs.destroystream(&pnode->plugin->npp, NULL, 0);
            pnode->hex_mode = TYPES_PLUGIN;
        }
        else if (!TAB_NOT_BIN(pnode) || TAB_HEX_MODE(pnode))
        {   // 以下是两个先决条件, 从文件切换到二进制
            pnode->hex_mode = TYPES_TEXT;
            pnode->codepage = IDM_OTHER_BIN;
            eu_logmsg("File: %s, hexview_switch_mode()\n", __FUNCTION__);
            result = hexview_switch_mode(pnode);
        }
    }
    return result;
}

static void
on_file_clean_tmp(eu_tabpage *pnode, const bool be_cache, int *pstatus)
{
    wchar_t tmp[MAX_BUFFER] = {0};
    if (!be_cache)
    {
        if (pnode->bakpath[0])
        {
            _sntprintf(tmp, MAX_BUFFER, _T("%s~~"), pnode->bakpath);
            if (eu_exist_file(pnode->bakpath))
            {
                util_delete_file(pnode->bakpath);
            }
            pnode->bakpath[0] = 0;
        }
        if (pstatus)
        {
            *pstatus = 0;
        }
    }
    if (eu_exist_file(tmp))
    {
        util_delete_file(tmp);
    }
}

static int
on_file_do_write(eu_tabpage *pnode, const TCHAR *pathfilename, const bool isbak, const bool save_as, int *pstatus)
{
    FILE *fp = NULL;
    int ret = SKYLARK_OK;
    bool be_ignore = false;
    bool be_cache = (bool)isbak;
    if (!TAB_HEX_MODE(pnode))
    {
        pnode->bytes_remaining = (size_t) on_sci_call(pnode, SCI_GETLENGTH, 0, 0);
        be_ignore = pnode->bytes_remaining > BUFF_200M && on_session_thread_id() == GetCurrentThreadId();
        if (be_ignore)
        {
            eu_logmsg("File: files are too large and are not backed up automatically\n");
            goto FILE_FINAL;
        }
        be_cache = isbak && (!eu_get_config()->m_limit || eu_get_config()->m_limit > eu_int_cast(pnode->bytes_remaining/1024/1024));
        if ((pnode->write_buffer = (uint8_t *)(on_sci_range_text(pnode, 0, pnode->bytes_remaining))) == NULL)
        {
            ret = EUE_POINT_NULL;
            goto FILE_FINAL;
        }
        if (isbak && !save_as && !be_cache)
        {   // 非二进制文件, 不产生备份, 直接写入原文件
            pathfilename = pnode->pathfile;
        }
    }
    else if (!TAB_NOT_BIN(pnode))  // 原生的16进制视图
    {   // 是否自动cache
        be_cache = isbak && (!eu_get_config()->m_limit || eu_get_config()->m_limit > eu_int_cast(pnode->phex->total_items/1024/1024));
        // 既不另存为,又不产生缓存, 则直接写入源文件
        ret = hexview_save_data(pnode, save_as || be_cache ? pathfilename : NULL);
        goto FILE_FINAL;
    }
    else
    {   // 超过文件大小限制, 不保存临时文件
        be_cache = isbak && (!eu_get_config()->m_limit || eu_get_config()->m_limit > eu_int_cast(pnode->phex->total_items/1024/1024));
        // hex_ascii为真,是原始的二进制,内存映射打开方式
        if (pnode->phex->hex_ascii && pnode->phex->hmap)
        {
            eu_logmsg("File: maybe binary encoding, do not convert this code\n");
            ret = hexview_save_data(pnode, save_as || be_cache ? pathfilename : NULL);
            goto FILE_FINAL;
        }
        // 转码utf8后生成的二进制编码, 流打开方式, 保存时需要转换回去
        pnode->write_buffer = hexview_strdup_data(pnode, &pnode->bytes_remaining);
        if (pnode->write_buffer == NULL)
        {   // 文件过大,没有足够内存
            ret = EUE_POINT_NULL;
            goto FILE_FINAL;
        }
        pathfilename = save_as || be_cache ? pathfilename : pnode->pathfile;
    }
    if (!be_cache &&
       ((pnode->codepage > IDM_UNI_UTF8B && pnode->codepage < IDM_OTHER_BIN) ||
       (pnode->codepage >= IDM_ICONV_001 && pnode->codepage <= IDM_ICONV_133)))
    {
        char *pdst = NULL;
        char *pbuf = (char *)pnode->write_buffer;
        size_t dst_len = 0;
        size_t src_len = pnode->bytes_remaining;
        euconv_t evd = {0};
        evd.src_from = "utf-8";
        evd.dst_to = eu_query_encoding_name(pnode->codepage);
        eu_logmsg("File: convert(%s) to (%s)\n", evd.src_from, evd.dst_to);
        size_t res = on_encoding_do_iconv(&evd, pbuf, &src_len, &pdst, &dst_len);
        if (res != (size_t) -1)
        {  // 释放之前的内存, 指向新的内存块
            free(pnode->write_buffer);
            pnode->write_buffer = (uint8_t *)pdst;
            pnode->bytes_remaining = dst_len;
        }
        else
        {
            ret = EUE_ICONV_FAIL;
            MSG_BOX(IDC_MSG_ICONV_FAIL1, IDC_MSG_ERROR, MB_ICONERROR | MB_OK);
            goto FILE_FINAL;
        }
    }
    if ((fp = _tfopen(pathfilename, _T("wb"))) == NULL)
    {
        ret = EUE_OPEN_FILE_ERR;
        MSG_BOX_ERR(IDC_MSG_OPEN_FAIL, IDC_MSG_ERROR, MB_ICONERROR | MB_OK);
        goto FILE_FINAL;
    }   // 文件备份时, 不转换回原编码
    if (!be_cache && pnode->pre_len > 0)
    {
        if (fwrite(pnode->pre_context, 1, pnode->pre_len, fp) < pnode->pre_len)
        {
            ret = EUE_WRITE_FILE_ERR;
            MSG_BOX(IDC_MSG_WRITE_FAIL, IDC_MSG_ERROR, MB_ICONERROR | MB_OK);
            goto FILE_FINAL;
        }
    }
    if (fwrite(pnode->write_buffer, pnode->bytes_remaining, 1, fp) == -1)
    {
        ret = EUE_WRITE_FILE_ERR;
        MSG_BOX(IDC_MSG_WRITE_FAIL, IDC_MSG_ERROR, MB_ICONERROR | MB_OK);
    }
FILE_FINAL:
    eu_safe_free(pnode->write_buffer);
    if (fp)
    {
        if (!ret && be_cache)
        {
            fflush(fp);
        }
        fclose(fp);
    }
    if (!ret && !be_ignore)
    {
        on_file_clean_tmp(pnode, be_cache, pstatus);
        if (!save_as && !be_cache)
        {
            on_file_update_time(pnode, 0, true);
        }
    }
    return ret;
}

static size_t
on_file_write_remote(void *buffer, size_t size, size_t nmemb, void *stream)
{
    size_t len;
    eu_tabpage *pnode = (eu_tabpage *) stream;
    if (pnode->bytes_written >= pnode->bytes_remaining)
    {
        return 0;
    }
    if (pnode->needpre)
    {
        memcpy(buffer, pnode->pre_context, pnode->pre_len);
        len = pnode->pre_len;
        pnode->needpre = false;
        return len;
    }
    len = size * nmemb;
    if (pnode->bytes_written + len > pnode->bytes_remaining)
    {
        len = pnode->bytes_remaining - pnode->bytes_written;
    }
    memcpy(buffer, pnode->write_buffer + pnode->bytes_written, len);
    pnode->bytes_written += len;
    return len;
}

static int
on_file_write_backup(const eu_tabpage *pnode)
{
    int ret = 0;
    if (pnode && !util_availed_char(pnode->fs_server.networkaddr[0]))
    {
        size_t len = _tcslen(pnode->pathfile);
        TCHAR *pbakup = len > 0 ? (TCHAR *)calloc(sizeof(TCHAR), len + 2) : NULL;
        if (pbakup)
        {
            _sntprintf(pbakup, len+1, _T("%s~"), pnode->pathfile);
            if (!CopyFile(pnode->pathfile, pbakup, false))
            {
                MSG_BOX(IDC_MSG_COPY_FAIL, IDC_MSG_ERROR, MB_ICONERROR|MB_OK);
                ret = EUE_COPY_FILE_ERR;
            }
            free(pbakup);
        }
    }
    return ret;
}

int
on_file_stream_upload(eu_tabpage *pnode, wchar_t *pmsg)
{
    int err = EUE_CURL_NETWORK_ERR;
    if (pnode)
    {
        char *cnv = NULL;
        CURL *curl = NULL;
        pnode->bytes_written = 0;
        pnode->needpre = false;
        if (pnode->pre_len > 0)
        {
            pnode->needpre = true;
        }
        do
        {
            if ((cnv = eu_utf16_utf8(pnode->pathfile, NULL)) == NULL)
            {
                err = EUE_API_CONV_FAIL;
                break;
            }
            if (!(curl = on_remote_init_socket(cnv, &pnode->fs_server)))
            {
                eu_logmsg("File: on_remote_init_socket return false\n");
                err = EUE_CURL_INIT_FAIL;
                break;
            }
            eu_curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
            eu_curl_easy_setopt(curl, CURLOPT_READFUNCTION, on_file_write_remote);
            eu_curl_easy_setopt(curl, CURLOPT_READDATA, pnode);
        #if defined(APP_DEBUG) && (APP_DEBUG > 0)
            eu_curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        #endif
            // 设置链接超时, 不要设置上传超时, 防止文件被截断
            eu_curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 6L);
            err = eu_curl_easy_perform(curl);
            if (err && pmsg)
            {
                util_make_u16(eu_curl_easy_strerror(err), pmsg, PERROR_LEN);
            }
            eu_curl_easy_cleanup(curl);
        } while(0);
        eu_safe_free(cnv);
    }
    return err;
}

int
on_file_save(eu_tabpage *pnode, const int save)
{
    int att = 0;
    int err = SKYLARK_OK;
    uint8_t *dup = NULL;
    char *ptext = NULL;
    npn_nmhdr nphdr = {0};
    bool saveas = save == SAVE_AS;
    if (!pnode)
    {
        return EUE_TAB_NULL;
    }
    if (!on_sci_doc_modified(pnode) && !file_force_saved(save))
    {
        err = EUE_UNEXPECTED_SAVE;
        goto SAVE_FINAL;
    }  // 插件打开的文件, 保存
    if (pnode->pmod && pnode->plugin && !file_force_saved(save))
    {
        nphdr.pnode = (void *)pnode;
        if (eu_get_config()->m_write_copy && on_file_write_backup(pnode))
        {
            err = EUE_WRITE_FILE_ERR;
            goto SAVE_FINAL;
        }
        npn_send_notify(pnode->hwnd_sc, NPP_DOC_MODIFY, OPERATE_SAVE, &nphdr);
        goto SAVE_FINAL;
    }  // 编辑器文件另存为
    if (pnode->is_blank || pnode->reserved0 > 0 || saveas)
    {
        TCHAR pcd[MAX_PATH] = {0};
        TCHAR full_path[MAX_BUFFER] = {0};
        TCHAR *p = NULL;
        if (pnode->is_blank || url_has_remote(pnode->pathfile))
        {
            GetEnvironmentVariable(_T("USERPROFILE"), pcd, MAX_PATH - 1);
        }
        if (pnode->reserved0 > 0)
        {
            if (L':' != ((TCHAR *)pnode->reserved0)[1])
            {
                if (pnode->is_blank || url_has_remote(pnode->pathfile))
                {
                    _sntprintf(full_path, MAX_BUFFER, _T("%s\\%s"), pcd, (TCHAR *)pnode->reserved0);
                }
                else
                {
                    _sntprintf(full_path, MAX_BUFFER, _T("%s%s"), pnode->pathname, (TCHAR *)pnode->reserved0);
                }
            }
            else if ((p = _tcsrchr((TCHAR *)pnode->reserved0, _T('\\'))) || (p = _tcsrchr((TCHAR *)pnode->reserved0, _T('/'))))
            {   // 目录不存在时, 试着创建目录
                _tcsncpy(full_path, (TCHAR *)pnode->reserved0, p - (TCHAR *)pnode->reserved0);
                if (!eu_exist_dir(full_path))
                {
                    eu_mk_dir(full_path);
                }
                _tcsncpy(full_path, (TCHAR *)pnode->reserved0, MAX_BUFFER);
            }
        }
        else
        {
            _tcsncpy(full_path, pnode->filename, MAX_BUFFER);
            if (on_file_get_filename_dlg(full_path, _countof(full_path), STR_NOT_NUL(pcd) ? pcd : pnode->pathname))
            {
                err = EUE_LOCAL_FILE_ERR;
                goto SAVE_FINAL;
            }
            saveas = true;
        }
        if (saveas)
        {
            _tsplitpath(full_path, NULL, NULL, pnode->filename, pnode->extname);
            if (_tcslen(pnode->extname) > 0)
            {
                _tcsncat(pnode->filename, pnode->extname, MAX_BUFFER);
            }
        }
        if (!pnode->pmod)
        {   // 非插件另存为
            if (on_file_do_write(pnode, full_path, false, true, NULL))
            {
                err = EUE_WRITE_FILE_ERR;
                goto SAVE_FINAL;
            }
            if (saveas)
            {
                _tcsncpy(pnode->pathfile, full_path, MAX_BUFFER);
                // 有可能是远程服务器文件, 清除网址
                pnode->fs_server.networkaddr[0] = 0;
                on_file_update_time(pnode, 0, true);
                util_set_title(pnode);
                pnode->doc_ptr = on_doc_get_type(pnode->filename);
                on_sci_before_file(pnode, false);
                on_sci_after_file(pnode, false);
                if (pnode->is_blank)
                {
                    pnode->is_blank = false;
                }
            }
        }
        else if (pnode->plugin)
        {   // 插件另存为
            np_plugins_savefileas(&pnode->plugin->funcs, &pnode->plugin->npp, full_path);
            if (saveas)
            {
                nphdr.pnode = (void *)pnode;
                npn_send_notify(pnode->hwnd_sc, NPP_DOC_MODIFY, OPERATE_SAVEAS, &nphdr);
            }
        }
    }
    else if (util_availed_char(pnode->fs_server.networkaddr[0]))
    {   // sftp文件保存
        size_t buf_len = 0;
        wchar_t msg[PERROR_LEN+1] = {0};
        if (TAB_NOT_BIN(pnode))
        {
            if (TAB_HEX_MODE(pnode))
            {
                ptext = (char *)hexview_strdup_data(pnode, &buf_len);
                if (ptext == NULL)
                {
                    err = EUE_POINT_NULL;
                    goto SAVE_FINAL;
                }
            }
            else if (!(ptext = util_strdup_content(pnode, &buf_len)))
            {
                err = EUE_POINT_NULL;
                goto SAVE_FINAL;
            }
            if (((pnode->codepage > IDM_UNI_UTF8B && pnode->codepage < IDM_OTHER_BIN) ||
               (pnode->codepage >= IDM_ICONV_001 && pnode->codepage <= IDM_ICONV_133)))
            {
                char *psrc = ptext;
                size_t src_len = buf_len;
                size_t dst_len = 0;
                euconv_t evd = {0};
                evd.src_from = "utf-8";
                evd.dst_to = eu_query_encoding_name(pnode->codepage);
                size_t res = on_encoding_do_iconv(&evd, psrc, &src_len, (char **)&pnode->write_buffer, &pnode->bytes_remaining);
                if (res == (size_t) -1)
                {
                    MSG_BOX(IDC_MSG_ICONV_FAIL1, IDC_MSG_ERROR, MB_ICONERROR | MB_OK);
                    err = EUE_ICONV_FAIL;
                    goto SAVE_FINAL;
                }
            }
            else
            {
                pnode->bytes_remaining = buf_len;
                pnode->write_buffer = (uint8_t *)ptext;
            }
        }
        else
        {
            pnode->write_buffer = hexview_strdup_data(pnode, &pnode->bytes_remaining);
            if (pnode->write_buffer == NULL)
            {
                err = EUE_POINT_NULL;
                goto SAVE_FINAL;
            }
        }
        if ((err = on_file_stream_upload(pnode, msg)) != SKYLARK_OK)
        {
            print_err_msg(IDC_MSG_ATTACH_ERRORS, msg);
            pnode->st_mtime = 0;
            goto SAVE_FINAL;
        }
        on_file_update_time(pnode, time(NULL), true);
    }
    else
    {
        if ((att = on_profiles_used(pnode)) > 0)
        {
            if (!on_profiles_warn())
            {
                att = 0;
                goto SAVE_FINAL;
            }
            else
            {
                eu_logmsg("File: this is profile\n");
            }
        }
        if (eu_get_config()->m_write_copy && on_file_write_backup(pnode))
        {
            err = EUE_WRITE_FILE_ERR;
            goto SAVE_FINAL;
        }
        if (on_file_do_write(pnode, pnode->pathfile, false, false, NULL))
        {
            err = EUE_WRITE_FILE_ERR;
        }
    }
SAVE_FINAL:
    // 防止重复释放内存
    if ((dup = (uint8_t *)pnode->write_buffer) && dup != (uint8_t *)ptext)
    {
        eu_safe_free(pnode->write_buffer);
    }
    if (ptext && dup != (uint8_t *)ptext)
    {
        free(ptext);
    }
    if (pnode->reserved0)
    {
        free((void *)pnode->reserved0);
        pnode->reserved0 = 0;
    }
    if (!err)
    {
        if (!pnode->pmod && save != SAVE_SIL)
        {   // 发送SCI_SETSAVEPOINT消息
            pnode->fn_modify = false;
            on_sci_point_reached(pnode);
            on_sci_call(pnode, SCI_SETSAVEPOINT, 0, 0);
            if (!pnode->is_blank && pnode->sym_show)
            {
                on_tabpage_symreload(pnode);
            }
        }
        if (!att)
        {
            if (save == SAVE_ALL)
            {
                util_redraw(on_tabpage_hwnd(pnode), true);
            }
            else
            {
                on_sci_refresh_ui(pnode);
            }
            on_file_filedb_update(pnode);
        }
        else
        {
            err = on_profiles_reload(pnode, att);
        }
        if (!err)
        {
            if (save == SAVE_ONLY)
            {
                on_script_loader_event(SKYLARK_FILESAVE, pnode);
            }
            else if (saveas)
            {
                on_script_loader_event(SKYLARK_FILESAVEAS, pnode);
            }
        }
    }
    return err;
}

int
on_file_save_as(eu_tabpage *pnode)
{
    return !pnode ? EUE_TAB_NULL : on_file_save(pnode, SAVE_AS);
}

int
on_file_all_save(void)
{
    eu_tabpage *p = NULL;
    HWND htab[2] = {HMAIN_GET, HSLAVE_SHOW ? HSLAVE_GET : NULL};
    for (int k = 0; k < 2 && htab[k]; ++k)
    {
        for (int index = 0, count = TabCtrl_GetItemCount(htab[k]); index < count; ++index)
        {
            if ((p = on_tabpage_get_ptr(htab[k], index)) && !(p->stat_id & TABS_DUPED))
            {
                on_file_save(p, false);
            }
        }
    }
    return SKYLARK_OK;
}

static void
on_file_guid(TCHAR *buf, int len)
{
    GUID  guid;
    if (S_OK == CoCreateGuid(&guid))
    {
        _sntprintf(buf,
                   len,
                   _T("%08x%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x"),
                   guid.Data1, guid.Data2, guid.Data3,
                   guid.Data4[0], guid.Data4[1],
                   guid.Data4[2], guid.Data4[3],
                   guid.Data4[4], guid.Data4[5],
                   guid.Data4[6], guid.Data4[7]);
    }
    else
    {
        eu_rand_str(buf, 32);
    }
}

static int
on_file_query_callback(void *data, int count, char **column, char **names)
{
    file_backup fbak = {0};
    for (int i = 0; i < count; ++i)
    {
        if (STRCMP(names[i], ==, "szBakPath"))
        {
            MultiByteToWideChar(CP_UTF8, 0, column[i], -1, fbak.bak_path, MAX_BUFFER);
        }
        else if (STRCMP(names[i], ==, "szStatus"))
        {
            fbak.status = atoi(column[i]);
        }
    }
    if (fbak.bak_path[0] && fbak.status)
    {
        TCHAR *p = _tcsrchr(fbak.bak_path, _T('\\'));
        if (p)
        {
            _sntprintf((wchar_t *)data, QW_SIZE - 1, _T("%s"), p + 1);
        }
    }
    return 0;
}

static bool
on_file_backup_name(eu_tabpage *pnode, wchar_t *pout)
{
    bool ret = false;
    char *path = NULL;
    if (pnode && pout)
    {
        *pout = 0;
        if (eu_exist_file(pnode->bakpath))
        {
            TCHAR *p = _tcsrchr(pnode->bakpath, _T('\\'));
            if (p)
            {
                _sntprintf(pout, QW_SIZE - 1, _T("%s"), p + 1);
                ret = true;
            }
        }
        else if ((path = eu_utf16_utf8(pnode->pathfile, NULL)) && path[0])
        {
            char sql[MAX_BUFFER+1] = {0};
            _snprintf(sql, MAX_BUFFER, "select szBakpath,szStatus from skylark_session where szRealpath='%s';", path);
            if (on_sql_mem_post(sql, on_file_query_callback, pout) == SKYLARK_OK)
            {
                ret = *pout ? true : false;
            }
        }
    }
    eu_safe_free(path);
    return ret;
}

void
on_file_npp_write(eu_tabpage *pnode, const wchar_t *cache_path, const bool isbak, int *pstatus)
{
#define TMP_SUFFX (L".$bak~")
    bool is_cache = isbak && (!eu_get_config()->m_limit || eu_get_config()->m_limit > eu_int_cast(pnode->raw_size/1024/1024));
    if (pnode && pnode->pmod && pnode->plugin)
    {
        if (is_cache)
        {
            int ret = 0;
            bool is_same = false;
            wchar_t *tmp_path = NULL;
            wchar_t pathfile[MAX_BUFFER] = {0};
            if (!np_plugins_getvalue(&pnode->plugin->funcs, &pnode->plugin->npp, NV_TABTITLE, (void **)&tmp_path) && STR_NOT_NUL(tmp_path))
            {   // 未保存之前, 把本地文件先复制到临时文件
                _snwprintf(pathfile, MAX_BUFFER, L"%s", tmp_path);
                // 本地文件也可能是之前的备份, 不用复制
                is_same = (STR_NOT_NUL(cache_path) && wcsicmp(pathfile, cache_path) == 0);
                if (!is_same)
                {
                    wcsncat(tmp_path, TMP_SUFFX, MAX_BUFFER);
                    ret = CopyFileW(pathfile, tmp_path, false);
                }
            }
            if ((ret || is_same) && STR_NOT_NUL(tmp_path))
            {   // 保存本地文件
                np_plugins_savefile(&pnode->plugin->funcs, &pnode->plugin->npp);
                if (!is_same)
                {   // 并移动到cache_path
                    ret = MoveFileW(pathfile, cache_path);
                    // 本地文件恢复未修改之前的状态
                    ret = MoveFileExW(tmp_path, pathfile, MOVEFILE_REPLACE_EXISTING|MOVEFILE_COPY_ALLOWED);
                }
            }
            eu_safe_free(tmp_path);
        }
        else if (!url_has_remote(pnode->pathfile))
        {   // no cache, 则自动保存
            np_plugins_savefile(&pnode->plugin->funcs, &pnode->plugin->npp);
            if (pstatus)
            {
                *pstatus = 0;
            }
        }
        else
        {   // 网络流文件的修改丢弃
            on_sql_delete_backup_row(pnode);
        }
    }
#undef TMP_SUFFX
}

static bool
on_file_cache_protect(eu_tabpage *pnode)
{   // 非cache目录内文件
    if (pnode && pnode->pathfile[0])
    {
        TCHAR cache[MAX_BUFFER] = {0};
        _sntprintf(cache, MAX_BUFFER - 1, _T("%s\\cache"), eu_config_path);
        return (_tcsnicmp(pnode->pathfile, cache, _tcslen(cache)) == 0);
    }
    return false;
}

bool
on_file_get_bakpath(eu_tabpage *pnode)
{
    TCHAR buf[QW_SIZE] = {0};
    if (pnode)
    {
        if (!on_file_backup_name(pnode, buf))
        {
            on_file_guid(buf, QW_SIZE - 1);
        }
    }
    if (buf[0])
    {
        _sntprintf(pnode->bakpath, MAX_BUFFER, _T("%s\\cache\\%s"), eu_config_path, buf);
    }
    return (buf[0] != 0);
}

static void
on_file_save_backup(eu_tabpage *pnode, const CLOSE_MODE mode)
{
    if (!file_click_close(mode) && !on_file_cache_protect(pnode))
    {
        if (!pnode->is_blank || TAB_NOT_NUL(pnode))
        {
            const bool autobak = TAB_NOT_BIN(pnode) && !TAB_HAS_PDF(pnode);
            file_backup filebak = {0};
            filebak.cp = pnode->codepage;
            filebak.bakcp = pnode->bakcp;
            filebak.view = pnode->view;
            if (pnode->be_modify)
            {
                TCHAR buf[QW_SIZE] = {0};
                filebak.status = 1;
                if (!on_file_backup_name(pnode, buf))
                {
                    on_file_guid(buf, QW_SIZE - 1);
                }
                if (!buf[0])
                {
                    eu_logmsg("File: %s, error, buf is null\n", __FUNCTION__);
                }
                else
                {
                    if (util_isxdigit_string(buf, eu_int_cast(_tcslen(buf) - 2)))
                    {
                        size_t buf_len = _tcslen(buf);
                        if (buf_len > 2 && buf[buf_len - 1] == _T('~') && buf[buf_len - 2] == _T('~'))
                        {
                            eu_logmsg("File: %s, File name may be incorrect\n", __FUNCTION__);
                            buf[buf_len - 2] = 0;
                        }
                    }
                    _sntprintf(filebak.bak_path, MAX_BUFFER, _T("%s\\cache\\%s"), eu_config_path, buf);
                }
                if (mode == FILE_SAVE_CLOSE)
                {
                    if (autobak)
                    {
                        on_file_do_write(pnode, filebak.bak_path, true, false, &filebak.status);
                    }
                }
                else if (TAB_HAS_PDF(pnode) && !TAB_HEX_MODE(pnode))
                {
                    on_file_npp_write(pnode, filebak.bak_path, true, &filebak.status);
                }
                else
                {
                    on_file_do_write(pnode, filebak.bak_path, true, false, &filebak.status);    
                }
            }
            if (true)
            {
                _tcscpy(filebak.rel_path, pnode->pathfile);
                filebak.tab_id = pnode->tab_id;
                filebak.eol = pnode->eol;
                filebak.blank = pnode->is_blank;
                filebak.hex = pnode->hex_mode;
                filebak.focus = pnode->tab_focus;
                filebak.zoom = pnode->zoom_level != SELECTION_ZOOM_LEVEEL ? pnode->zoom_level : 0;
                on_search_page_mark(pnode, filebak.mark_id, MAX_BUFFER-1);
                on_search_fold_kept(pnode, filebak.fold_id, MAX_BUFFER-1);
                filebak.postion = pnode->initial ? on_sci_call(pnode, SCI_GETCURRENTPOS, 0, 0) : (pnode->nc_pos >= 0 ? pnode->nc_pos : 0);
            }
            if (mode == FILE_REMOTE_CLOSE)
            {
                filebak.sync = (int)(url_has_remote(filebak.rel_path) ? 1 :
                               (TabCtrl_GetItemCount(on_tabpage_hwnd(pnode)) <= 1 && eu_get_config()->m_exit) ? 1 : 0);
                eu_update_backup_table(&filebak, DB_FILE);
                on_sql_delete_backup_row(pnode);
            }
            else if (mode == FILE_SAVE_CLOSE)
            {
                if (!pnode->bakpath[0] && filebak.bak_path[0])
                {
                    _sntprintf(pnode->bakpath, MAX_BUFFER, _T("%s"), filebak.bak_path);
                }
                if (eu_exist_file(filebak.bak_path))
                {
                    if (autobak)
                    {
                        TCHAR tmp[MAX_BUFFER] = {0};
                        _sntprintf(tmp, MAX_BUFFER, _T("%s~~"), filebak.bak_path);
                        util_copy_file(filebak.bak_path, tmp, false);
                    }
                    if (!_InterlockedCompareExchange(&pnode->lock_id, 1, 0))
                    {   // 写入文件数据库, 仅写入一次
                        if (autobak)
                        {
                            _tcsncat(filebak.bak_path, _T("~~"), MAX_BUFFER);
                            eu_update_backup_table(&filebak, DB_FILE);
                            filebak.bak_path[_tcslen(filebak.bak_path) - 2] = 0;
                        }
                        else
                        {
                            eu_update_backup_table(&filebak, DB_FILE);
                        }
                    }
                }
                eu_update_backup_table(&filebak, DB_MEM);
            }
            else
            {
                filebak.sync = 1;
                eu_update_backup_table(&filebak, DB_MEM);
            }
        }
        else
        {
            on_sql_delete_backup_row(pnode);
            on_file_clean_tmp(pnode, false, NULL);
        }
    }
    else
    {
        on_sql_delete_backup_row(pnode);
    }
}

void
on_file_auto_backup(void)
{
    eu_tabpage *p = NULL;
    const HWND htab = HMAIN_GET;
    for (int index = 0, count = TabCtrl_GetItemCount(htab); index < count; ++index)
    {
        if ((p = on_tabpage_get_ptr(htab, index)) && p->be_modify && !on_file_cache_protect(p))
        {
            util_lock_v2(p);
            on_file_save_backup(p, FILE_SAVE_CLOSE);
            util_unlock_v2(p);
        }
    }
}

int
on_file_close(eu_tabpage **ppnode, const CLOSE_MODE mode)
{
    int err = SKYLARK_OK;
    int ifocus = 0;
    HWND htab = NULL;
    eu_tabpage *p = 0;
    if (STR_IS_NUL(ppnode))
    {
        return EUE_POINT_NULL;
    }
    else 
    {
        htab = (*ppnode)->view ? HSLAVE_GET: HMAIN_GET;
        ifocus = TabCtrl_GetCurSel(htab);
        p = on_tabpage_get_ptr(htab, ifocus);
    }
    if (!htab)
    {
        return EUE_TAB_NULL;
    }
    if (!file_click_close(mode) && (eu_get_config()->m_session || mode == FILE_FORCE_CLOSE))
    {
        // do nothing
    }
    else if (on_sci_doc_modified(*ppnode))
    {
        TCHAR msg[100 + MAX_BUFFER];
        if ((*ppnode)->pathfile[0] == 0)
        {
            LOAD_I18N_RESSTR(IDC_MSG_SAVE_STR1, save_str);
            _sntprintf(msg, _countof(msg) - 1, save_str);
        }
        else
        {
            LOAD_I18N_RESSTR(IDC_MSG_SAVE_STR2, save_str);
            _sntprintf(msg, _countof(msg) - 1, save_str, (*ppnode)->pathfile);
        }
        LOAD_APP_RESSTR(IDS_APP_TITLE, title);
        /* 如果需要确认, 选中该标签 */
        on_tabpage_selection((*ppnode));
        int decision = eu_msgbox(eu_module_hwnd(), msg, title, MB_YESNOCANCEL);
        if (decision == IDCANCEL)
        {
            eu_logmsg("File: abort closing file\n");
            err = SKYLARK_OPENED;
        }
        else if (decision == IDYES)
        {
            err = on_file_save((*ppnode), SAVE_ONLY);
        }
        else
        {
            (*ppnode)->be_modify = false;
        }
    }
    if (!err)
    {
        if (eu_get_config()->m_session && mode != FILE_FORCE_CLOSE)
        {
            on_file_save_backup((*ppnode), mode);
        }
        else
        {
            on_sql_delete_backup_row((*ppnode));
        }
        /* 清理该文件的位置导航信息 */
        on_navigate_clean_this((*ppnode));
        /* 排序最近关闭文件的列表 */
        if (file_click_close(mode) && !(*ppnode)->is_blank)
        {
            on_file_push_recent((*ppnode));
        }
        /* 关闭标签后需要激活其他标签 */
        if (on_tabpage_remove((*ppnode), mode))
        {
            if (file_force_close(mode) || mode == FILE_ONLY_CLOSE)
            {
                if ((*ppnode)->tab_id == ifocus || file_force_close(mode))
                {
                    on_file_active_other((*ppnode));
                }
                else if (p)
                {
                    on_tabpage_selection(p);
                }
            }
            on_sci_free_tab(ppnode);
        }
        if (mode == FILE_REMOTE_CLOSE)
        {
            eu_logmsg("File: drag tabs?\n");
        }
    }
    return err;
}

int
on_file_all_close(void)
{
    int index = 0;
    int this_index = 0;
    eu_tabpage *p = NULL;
    HWND htab = NULL;
    int count = 0;
    if (HSLAVE_SHOW && (htab = HSLAVE_GET))
    {
        for (count = TabCtrl_GetItemCount(htab); index < count; ++index)
        {
            if ((p = on_tabpage_get_ptr(htab, 0)))
            {
                on_file_close(&p, FILE_ALL_CLOSE);
            }
        }
        eu_get_config()->eu_tab.slave_show = false;
    }
    for (index = 0, htab = HMAIN_GET, count = TabCtrl_GetItemCount(htab); index < count; ++index)
    {
        if ((p = on_tabpage_get_ptr(htab, this_index)) && on_file_close(&p, FILE_ALL_CLOSE))
        {
            ++this_index;
        }
    }
    if (!this_index)
    {   // 最后一个标签
        if (!eu_get_config()->m_exit)
        {
            file_backup bak = {0};
            on_file_redirect(&bak, 1);
        }
        else
        {
            eu_close_edit();
        }
    }
    else if (htab)
    {   // 如果有标签关闭失败
        on_tabpage_select_index(htab, 0);
    }
    return SKYLARK_OK;
}

int
on_file_left_close(void)
{
    eu_tabpage *pnode = on_tabpage_focused();
    const HWND htab = on_tabpage_hwnd(pnode);
    if (pnode && htab)
    {
        eu_tabpage *p = NULL;
        pnode->tab_id = TabCtrl_GetCurSel(htab);
        for (int index = 0; index < pnode->tab_id; ++index)
        {
            if ((p = on_tabpage_get_ptr(htab, 0)))
            {
                on_file_close(&p, FILE_EXCLUDE_CLOSE);
            }
        }
        on_tabpage_selection(pnode);
    }
    return SKYLARK_OK;
}

int
on_file_right_close(void)
{
    eu_tabpage *pnode = on_tabpage_focused();
    const HWND htab = on_tabpage_hwnd(pnode);
    if (pnode && htab)
    {
        eu_tabpage *p = NULL;
        int count = TabCtrl_GetItemCount(htab);
        int right = count - 1;
        pnode->tab_id = TabCtrl_GetCurSel(htab);
        for (; pnode->tab_id < right; --right)
        {
            if ((p = on_tabpage_get_ptr(htab, right)))
            {
                if (on_file_close(&p, FILE_EXCLUDE_CLOSE))
                {
                    continue;
                }
            }
        }
        if (count > pnode->tab_id + 1)
        {
            on_tabpage_selection(pnode);
        }
    }
    return SKYLARK_OK;
}

int
on_file_exclude_close(eu_tabpage *pnode)
{
    int this_index = 0;
    HWND htab = on_tabpage_hwnd(pnode);
    if (htab)
    {
        cvector_vector_type(int) v = NULL;
        if ((on_tabpage_sel_number(htab, &v, false)) > 0)
        {
            eu_tabpage *p = NULL;
            for (int index = 0, count = TabCtrl_GetItemCount(htab); index < count; ++index)
            {
                if (eu_cvector_at(v, index) >= 0)
                {
                    ++this_index;
                    continue;
                }
                if ((p = on_tabpage_get_ptr(htab, this_index)) && on_file_close(&p, FILE_EXCLUDE_CLOSE))
                {
                    ++this_index;
                }
            }
            on_tabpage_selection(pnode);
        }
        cvector_freep(&v);
    }
    return SKYLARK_OK;
}

int
on_file_unchange_close(eu_tabpage *pnode)
{
    HWND htab = on_tabpage_hwnd(pnode);
    if (htab)
    {
        int this_index = TabCtrl_GetCurSel(htab);
        const int count = TabCtrl_GetItemCount(htab);
        for (int index = count - 1; index >= 0; --index)
        {
            if ((pnode = on_tabpage_get_ptr(htab, index)) && !pnode->be_modify)
            {
                on_file_close(&pnode, FILE_EXCLUDE_CLOSE);
                if (index < this_index)
                {
                    --this_index;
                }
            }
        }
        on_file_active_condition(htab, this_index);
    }
    return SKYLARK_OK;
}

static unsigned __stdcall
on_file_check_save(void *lp)
{
    int err = 0;
    int index = 0;
    int count = 0;
    bool reset = false;
    HWND htab = HSLAVE_GET;
    eu_tabpage *p = NULL;
    int slave_focus = TabCtrl_GetCurSel(htab);
    int main_focus = TabCtrl_GetCurSel(HMAIN_GET);
    for (count = TabCtrl_GetItemCount(htab); index < count; ++index)
    {
        if ((p = on_tabpage_get_ptr(htab, 0)))
        {
            if (p->is_blank && !TAB_NOT_NUL(p) && !on_sci_doc_modified(p))
            {
                reset = true;
            }
            else
            {
                reset = false;
            }
            p->tab_id = index;
            p->zoom_level = p->zoom_level != SELECTION_ZOOM_LEVEEL ? (int) on_sci_call(p, SCI_GETZOOM, 0, 0) : 0;
            p->tab_focus = p->tab_id == slave_focus;
            on_file_close(&p, FILE_SHUTDOWN);
        }
    }
    if (reset || count < 1)
    {   // 如果都是空标签, 下次不再显示从视图
        eu_get_config()->eu_tab.slave_show = false;
    }
    for (index = 0, (htab = HMAIN_GET), count = TabCtrl_GetItemCount(htab); index < count; ++index)
    {
        if ((p = on_tabpage_get_ptr(htab, err)))
        {
            p->tab_id = index;
            p->zoom_level = p->zoom_level != SELECTION_ZOOM_LEVEEL ? (int) on_sci_call(p, SCI_GETZOOM, 0, 0) : 0;
            if (main_focus >= 0)
            {
                p->tab_focus = (int)(p->tab_id == main_focus);
            }
            if (on_file_close(&p, FILE_SHUTDOWN))
            {
                ++err;
                continue;
            }
        }
    }
    if (on_sql_sync_session())
    {
        eu_logmsg("File: on_sql_sync_session return false\n");
    }
    if (!err)
    {
        PostMessage(eu_module_hwnd(), WM_BACKUP_OVER, 0, 0);
    }
    _InterlockedExchange(&file_close_id, 0);
    return err;
}

static void
on_file_unsave_close(void)
{
    if (!file_close_id)
    {
        CloseHandle((HANDLE) _beginthreadex(NULL, 0, on_file_check_save, NULL, 0, (DWORD *)&file_close_id));
        if (!file_event_final)
        {
            share_envent_create_file_sem(&file_event_final);
        }
        else
        {
            SetEvent(file_event_final);
        }
    }
}

void
on_file_finish_wait(void)
{
    if (file_event_final)
    {   // we destroy windows, that it should wait for file close thread exit
        share_envent_wait_file_close_sem(&file_event_final);
    }
}

void
on_file_edit_exit(HWND hwnd)
{
    if (eu_get_config()->m_session)
    {
        on_file_unsave_close();
        if (hwnd)
        {
            ShowWindow(hwnd, SW_HIDE);
        }
        if (eu_get_search_hwnd())
        {
            ShowWindow(eu_get_search_hwnd(), SW_HIDE);
        }
    }
    else
    {
        on_file_check_save(NULL);
    }
}

void
on_file_edit_restart(HWND hwnd, const bool admin, const bool wait)
{
    HANDLE handle = NULL;
    TCHAR process[MAX_BUFFER] = {0};
    if (GetModuleFileName(NULL , process , MAX_BUFFER) > 0)
    {
        int len = 0;
        uint32_t pid = GetCurrentProcessId();
        _tcsncat(process, _T(" -restart "), MAX_BUFFER);
        len = (int)_tcslen(process);
        _sntprintf(process + len, MAX_BUFFER - len, _T("%lu"), pid);
        on_file_kill_tree(pid);
        if (!admin || on_reg_admin())
        {
            if (wait)
            {
                on_file_edit_exit(hwnd);
            }
            CloseHandle(eu_new_process(process, NULL, NULL, 0, NULL));
        }
        else
        {
            if (on_reg_admin_execute(process) && wait)
            {
                on_file_edit_exit(hwnd);
            }
        }
    }
}

void
on_file_new_eols(eu_tabpage *pnode, const int eol_mode)
{
    eu_get_config()->new_file_eol = eol_mode;
}

void
on_file_new_encoding(eu_tabpage *pnode, const int new_enc)
{
    eu_get_config()->new_file_enc = new_enc;
}

bool
on_file_auto_notify(void)
{
    bool ret = false;
    TCHAR input_chars[8] = {0};
    _sntprintf(input_chars, _countof(input_chars)-1, _T("%d"), eu_get_config()->m_up_notify);
    LOAD_I18N_RESSTR(IDC_MSG_INTERVAL_STR, ac_str);
    if (eu_input(ac_str, input_chars, _countof(input_chars)))
    {
        if (input_chars[0])
        {
            const int mt_max = INT_MAX/1000;
            int intervar = _tstoi(input_chars);
            if (intervar > 0 && intervar < 5)
            {
                eu_get_config()->m_up_notify = 5;
                ret = true;
            }
            else if (intervar < mt_max)
            {
                eu_get_config()->m_up_notify = intervar;
                ret = true;
            }
            else
            {
                eu_get_config()->m_up_notify = 0;
            }
        }
    }
    return ret;
}

static int
on_file_do_restore(void *data, int count, char **column, char **names)
{
    file_backup bak = {-1, -1, 0, -1, 1};
    for (int i = 0; i < count; ++i)
    {
        if (STRCMP(names[i], ==, "szName"))
        {
            if (MultiByteToWideChar(CP_UTF8, 0, column[i], -1, bak.rel_path, _countof(bak.rel_path)))
            {
                if (_tcsrchr(bak.rel_path, _T('&')))
                {
                    eu_wstr_replace(bak.rel_path, _countof(bak.rel_path), _T("&"), _T("&&"));
                }
            }
        }
        else if (STRCMP(names[i], ==, "szPos"))
        {
            bak.postion = _atoz(column[i]);
        }
        else if (STRCMP(names[i], ==, "szHex"))
        {
            bak.hex = atoi(column[i]);
        }
    }
    if (_tcslen(bak.rel_path) > 0 && on_file_open_if(bak.rel_path, false) < 0 && on_file_redirect(&bak, 1) == SKYLARK_OK)
    {
        // 文件成功打开, 结束回调
        return SKYLARK_SQL_END;
    }
    return SKYLARK_OK;
}

void
on_file_restore_recent(void)
{
    on_sql_mem_post("SELECT szName,szPos,szHex FROM file_recent ORDER BY szDate DESC;", on_file_do_restore, NULL);
}

void
on_file_reload_current(eu_tabpage *pnode)
{
    if (pnode && !TAB_HEX_MODE(pnode) && !url_has_remote(pnode->pathfile))
    {
        bool reload = true;
        bool modified = pnode->be_modify;
        if (modified && !eu_get_config()->inter_reserved_0)
        {
            LOAD_APP_RESSTR(IDS_APP_TITLE, title);
            LOAD_I18N_RESSTR(IDS_FILE_RELOAD_STR, msg);
            int dec = eu_msgbox(eu_module_hwnd(), msg, title, MB_YESNOALWAYS);
            switch (dec) {
                case IDALWAYS:
                    eu_get_config()->inter_reserved_0 = 1;
                    break;
                case IDYES:
                    break;
                default:
                    reload = false;
                    break;
            }
        }
        if (reload)
        {
            pnode->nc_pos = on_sci_call(pnode, SCI_GETCURRENTPOS, 0, 0);
            on_tabpage_reload_file(pnode, 2);
        }
    }
}

void
on_file_convert_array(const eu_tabpage *pnode)
{
    sptr_t len = on_sci_call(pnode, SCI_GETLENGTH, 0, 0);
    if (len > BUFF_32M)
    {
        MSG_BOX(IDC_MSG_MEM_NOT_AVAIL, IDC_MSG_ERROR, MB_ICONERROR|MB_OK);
    }
    else if (pnode->phex && pnode->phex->pbase && pnode->phex->total_items > 0 && _tcslen(pnode->filename) > 0)
    {
        TCHAR full_path[MAX_BUFFER] = {'f','o','o','.','c','\0'};
        if (on_file_get_filename_dlg(full_path, _countof(full_path), pnode->pathname) == SKYLARK_OK && _tcslen(full_path) > 0)
        {
            char *path = NULL;
            FILE *fp = _tfopen(full_path, _T("wb"));
            if (fp && (path = eu_utf16_utf8(pnode->filename, NULL)) != NULL)
            {
                size_t i = 0;
                char desc[MAX_BUFFER] = {0};
                const char *tail = "};\n";
                const char *header = "/* created by skylark editor from file %s */\nstatic const unsigned char %s[%zu] = {\n";
                _snprintf(desc, MAX_BUFFER - 1, header, path, "generate_resource", pnode->phex->total_items);
                free(path);
                fwrite(desc, 1, strlen(desc), fp);
                for (i = 0; i < pnode->phex->total_items; ++i)
                {
                    if (i == pnode->phex->total_items - 1)
                    {
                        if (!(i % 16))
                        {
                            fwrite("  ", 1, 2, fp);
                        }
                        sprintf(desc, "0x%02X", pnode->phex->pbase[i]);
                        fwrite(desc, 1, strlen(desc), fp);
                        fwrite("\n", 1, 1, fp);
                    }
                    else
                    {
                        if (!(i % 16))
                        {
                            fwrite("  ", 1, 2, fp);
                            sprintf(desc, "0x%02X", pnode->phex->pbase[i]);
                            fwrite(desc, 1, strlen(desc), fp);
                            fwrite(", ", 1, 2, fp);
                        }
                        else if ((i % 16) == 15)
                        {
                            sprintf(desc, "0x%02X", pnode->phex->pbase[i]);
                            fwrite(desc, 1, strlen(desc), fp);
                            if (i == pnode->phex->total_items - 1)
                            {
                                fwrite("\n", 1, 1, fp);
                            }
                            else
                            {
                                fwrite(",\n", 1, 2, fp);
                            }
                        }
                        else
                        {
                            sprintf(desc, "0x%02X", pnode->phex->pbase[i]);
                            fwrite(desc, 1, strlen(desc), fp);
                            fwrite(", ", 1, 2, fp);
                        }
                    }
                }
                fwrite(tail, 1, strlen(tail), fp);
            }
            if (fp)
            {
                fclose(fp);
            }
        }
    }
}
