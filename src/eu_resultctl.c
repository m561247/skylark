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

#include "framework.h"

static HWND hwnd_rst = NULL;

int
on_result_append_text(eu_tabpage *pnode, const TCHAR *format, ...)
{
    va_list valist;
    SYSTEMTIME systime;
    int l, len = 0;
    TCHAR *buf = NULL;
    char *utf_buf = NULL;
    if (!RESULT_SHOW(pnode))
    {
        return 1;
    }
    va_start(valist, format);
    int buf_len = _vsctprintf(format, valist);
    buf = buf_len > 0 ? (TCHAR *)calloc(sizeof(TCHAR), buf_len += 68) : NULL;
    if (!buf)
    {
        va_end(valist);
        return 1;

    }
    if (on_sci_call(pnode->presult, SCI_GETLENGTH, 0, 0) < 1)
    {
        char *u8 = NULL;
        LOAD_I18N_RESSTR(IDS_LOADLIBRARY_SQL, mlib);
        if ((u8 = eu_utf16_utf8(mlib, NULL)) != NULL)
        {
            on_sci_call(pnode->presult, SCI_ADDTEXT, strlen(u8), (sptr_t)u8);
            free(u8);
        }
    }
    else
    {
        GetLocalTime(&systime);
        l = _sntprintf(buf + len,
                       buf_len - len,
                       _T("%04d-%02d-%02d %02d:%02d:%02d.%06d | "),
                       systime.wYear,
                       systime.wMonth,
                       systime.wDay,
                       systime.wHour,
                       systime.wMinute,
                       systime.wSecond,
                       systime.wMilliseconds * 1000);
        if (l > 0)
        {
            len += l;
        }
    }
    l = _vsntprintf(buf + len, buf_len - 1 - len, format, valist);
    if (l > 0)
    {
        len += l;
    }
    va_end(valist);
    l = _sntprintf(buf + len, buf_len - 1 - len, _T("\n"));
    if (l > 0)
    {
        len += l;
    }
    if ((utf_buf = eu_utf16_utf8(buf, NULL)) != NULL)
    {
        on_sci_call(pnode->presult, SCI_ADDTEXT, (sptr_t)strlen(utf_buf), (sptr_t)utf_buf);
        free(utf_buf);
    }
    free(buf);
    return 0;
}

int
on_result_append_text_utf8(eu_tabpage *p, const char *format, ...)
{
    va_list valist;
    char *buf = NULL;
    int l, len = 0;
    bool cmds = false;
    if (!p || !p->presult || !p->presult->hwnd_sc)
    {
        return 1;
    }
    va_start(valist, format);
    int buf_len = _vscprintf(format, valist);
    buf = buf_len > 0 ? (char *)calloc(1, buf_len += 68) : NULL;
    if (!buf)
    {
        va_end(valist);
        return 1;
    }
    l = _vsnprintf(buf + len, buf_len - 1 - len, format, valist);
    if (l > 0)
    {
        len += l;
    }
    va_end(valist);
    l = _snprintf(buf + len, buf_len - 1 - len, "\n");
    if (l > 0)
    {
        len += l;
    }
    if ((on_sci_call(p->presult, SCI_GETCARETSTYLE, 0, 0) & CARETSTYLE_LINE))
    {
        cmds = true;
    }
    if (!cmds)
    {
        on_sci_call(p->presult, SCI_SETREADONLY, 0, 0);
        on_sci_call(p->presult, SCI_CLEARALL, 0, 0);
        on_sci_call(p->presult, SCI_ADDTEXT, (sptr_t)strlen(buf), (sptr_t)buf);
        on_sci_call(p->presult, SCI_SETREADONLY, 1, 0);
        on_sci_call(p->presult, SCI_GOTOLINE, 1, 0);
    }
    else
    {
        if (p->presult->tab_id == AI_CMD_MODEL)
        {
            p->presult->tab_id = 0;
            on_sci_call(p->presult, SCI_SETREADONLY, 0, 0);
            on_sci_call(p->presult, SCI_ADDTEXT, (sptr_t)strlen(buf), (sptr_t)buf);
            on_sci_call(p->presult, SCI_SETREADONLY, 1, 0);
        }
        else
        {
            on_sci_call(p->presult, SCI_ADDTEXT, (sptr_t)1, (sptr_t)"\n");
            on_sci_call(p->presult, SCI_ADDTEXT, (sptr_t)(strlen(buf) - 1), (sptr_t)buf);
        }
    }
    free(buf);
    return 0;
}

static void
on_result_menu_callback(HMENU hpop, void *param)
{
    eu_tabpage *p = (eu_tabpage *)param;
    if (RESULT_SHOW(p) && hpop)
    {
        util_set_menu_item(hpop, IDM_RESULT_WRAPLINE, on_sci_call(p->presult, SCI_GETWRAPMODE, 0, 0));
    }
}

static eu_tabpage *
on_result_other_tab(int line, result_vec *vec_strings)
{
    if ((int)cvector_size(vec_strings) >= line)
    {
        for (int i = line - 1; i >= 0; --i)
        {
            if (vec_strings[i].line == -1)
            {
                return (eu_tabpage *)(vec_strings[i].mark._no);
            }
        }
    }
    return NULL;
}

static int
on_result_dbclick(eu_tabpage *pnode)
{
    if (pnode && pnode->presult && pnode->ret_vec)
    {
        const sptr_t pos = on_sci_call(pnode->presult, SCI_GETCURRENTPOS, 0, 0);
        const sptr_t line = on_sci_call(pnode->presult, SCI_LINEFROMPOSITION, pos, 0);
        on_sci_call(pnode->presult, SCI_SETEMPTYSELECTION, pos, 0);
        if (line > 0 && cvector_size(pnode->ret_vec) > 0)
        {
            const sptr_t current_line = on_sci_call(pnode, SCI_LINEFROMPOSITION, on_sci_call(pnode, SCI_GETCURRENTPOS, 0, 0), 0);
            eu_tabpage *p = on_result_other_tab((int)line, pnode->ret_vec);
            if (!p)
            {
                on_search_jmp_line(pnode, pnode->ret_vec[line - 1].line, current_line);
                on_sci_call(pnode, SCI_SETSELECTION, pnode->ret_vec[line - 1].mark.start, pnode->ret_vec[line - 1].mark.end);
                SetFocus(pnode->hwnd_sc);
            }
            else if (on_tabpage_selection(p) >= 0)
            {
                on_search_jmp_line(p, pnode->ret_vec[line - 1].line, current_line);
                on_sci_call(p, SCI_SETSEL, pnode->ret_vec[line - 1].mark.start, pnode->ret_vec[line - 1].mark.end);
            }
        }
    }
    return 1;
}

static void
on_result_command(eu_tabpage *p, const HWND hwnd, const WORD low, intptr_t lp)
{
    switch(low)
    {
        case IDM_RESULT_COPY:
        {
            SendMessage(hwnd, WM_COPY, 0, 0);
            break;
        }
        case IDM_RESULT_SETSEL:
        {
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            break;
        }
        case IDM_RESULT_UNSETSEL:
        {
            SendMessage(hwnd, EM_SETSEL, 0, 0);
            break;
        }
        case IDM_RESULT_WRAPLINE:
        {
            if (RESULT_SHOW(p))
            {
                int mode = (int)on_sci_call(p->presult, SCI_GETWRAPMODE, 0, 0);
                !mode ? on_sci_call(p->presult, SCI_SETWRAPMODE, 2, 0) : on_sci_call(p->presult, SCI_SETWRAPMODE, 0, 0);
            }
            break;
        }
        case IDM_RESULT_CLEARALL:
        {
            if (RESULT_SHOW(p))
            {
                on_sci_call(p->presult, SCI_SETREADONLY, 0, 0);
                on_sci_call(p->presult, SCI_CLEARALL, 0, 0);
                if (p->presult->pwant != on_command_light)
                {
                    on_sci_call(p->presult, SCI_SETREADONLY, 1, 0);
                }
            }
            break;
        }
        case IDM_RESULT_CLOSE:
        {
            on_result_destroy(p);
            break;
        }
        case IDC_MSG_ATTACH_FAIL3:
        case IDS_OPENAI_FORMAT_ERR:
        case IDS_OPENAI_AUTH_FAILD:
        case IDS_INSUFFICIENT_BAL:
        case IDS_OPENAI_PARAMETE_ERR:
        case IDS_OPENAI_TMP_ERR:
        case IDS_OPENAI_SERV_FAILURE:
        case IDS_OPENAI_SERV_BUSY:
        case IDS_OPENAI_SERV_404:
        case IDS_OPENAI_DATA_ERR:
        {
            p->presult->tab_id = AI_CMD_MODEL;
            on_result_output(p, low);
            break;
        }
        case IDS_OPENAI_SERV_THINK:
        {
            if (AI_EOT == lp)
            {
                on_sci_call(p->presult, SCI_SETREADONLY, 0, 0);
                on_sci_call(p->presult, SCI_ADDTEXT, (sptr_t)1, (sptr_t)("\n"));
                on_sci_call(p->presult, SCI_SETREADONLY, 1, 0);
                on_sci_call(p->presult, SCI_GOTOPOS, on_sci_call(p->presult, SCI_GETLENGTH, 0, 0), 0);
            }
            else if (!lp)
            {
                p->presult->tab_id = AI_CMD_MODEL;
                on_result_output(p, low);
            }
            else
            {
                char *mem = (char *)lp;
                on_sci_call(p->presult, SCI_SETREADONLY, 0, 0);
                on_sci_call(p->presult, SCI_ADDTEXT, (sptr_t)(strlen(mem)), (sptr_t)mem);
                on_sci_call(p->presult, SCI_SETREADONLY, 1, 0);
                on_sci_call(p->presult, SCI_GOTOPOS, on_sci_call(p->presult, SCI_GETLENGTH, 0, 0), 0);
                free(mem);
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

static LRESULT CALLBACK
on_result_edit_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_KEYDOWN:
        {
            if ((wParam == VK_ESCAPE || wParam == VK_RETURN) && KEY_UP(VK_SHIFT) && KEY_UP(VK_CONTROL))
            {
                eu_tabpage *pnode = on_tabpage_from_handle(hwnd, on_tabpage_resultctl);
                if (RESULT_SHOW(pnode) && pnode->presult->pwant == on_command_light)
                {
                    if (wParam == VK_ESCAPE)
                    {
                        on_sci_call(pnode->presult, SCI_CLEARALL, 0, 0);
                    }
                    else
                    {
                        on_script_loader_event(SKYLARK_COMMANDS, pnode->presult);
                    }
                }
            }
            break;
        }
        case WM_LBUTTONDBLCLK:
        {
            return on_result_dbclick(on_tabpage_from_handle(hwnd, on_tabpage_resultctl));
        }
        case WM_RBUTTONUP:
        {
            eu_tabpage *p = on_tabpage_from_handle(hwnd, on_tabpage_resultctl);
            if (RESULT_SHOW(p))
            {
                return menu_pop_track(hwnd, IDR_RESULT_MENU, 0, -1, on_result_menu_callback, p);
            }
            return 1;
        }
        case WM_COMMAND:
        {
            WORD low = LOWORD(wParam);
            if (HIWORD(wParam) == 0)
            {
                on_result_command(on_tabpage_from_handle(hwnd, on_tabpage_resultctl), hwnd, low, lParam);
                return 0;
            }
            break;
        }
        case WM_THEMECHANGED:
        {
            break;
        }
        case WM_DESTROY:
        {
            break;
        }
    }
    return CallWindowProc((WNDPROC)eu_edit_wnd, hwnd, message, wParam, lParam);
}

void
on_result_output(eu_tabpage *p, const int res)
{
    char msg[MAX_PATH] = {0};
    LOAD_I18N_RESSTR(res, info);
    on_result_append_text_utf8(p, util_make_u8(info, msg, MAX_PATH));
}

void
on_result_destroy(eu_tabpage *p)
{
    if (RESULT_SHOW(p))
    {
        on_openai_cancel(p->presult);
        SendMessage(p->presult->hwnd_sc, WM_CLOSE, 0, 0);
        p->presult->hwnd_sc = NULL;
        p->result_show = false;
        eu_safe_free(p->presult);
        eu_window_resize();
    }
}

void
on_result_lexer(eu_tabpage *pedit)
{
    if (pedit)
    {
        on_sci_default_theme(pedit, DEFAULTSPACE);
        // disable margin
        on_sci_call(pedit, SCI_SETMARGINS, 0, 0);
        // 强制使用unix回车符
        on_sci_call(pedit, SCI_SETEOLMODE, SC_EOL_LF, 0);
        // 不显示插入符
        on_sci_call(pedit, SCI_SETCARETSTYLE, CARETSTYLE_INVISIBLE, 0);
        // 需要时显示水平滚动条
        on_sci_call(pedit, SCI_SETSCROLLWIDTH, 1, 0);
        on_sci_call(pedit, SCI_SETSCROLLWIDTHTRACKING, 1, 0);
        // 加载词语解析器
        on_doc_init_after_scilexer(pedit, "result");
        on_doc_default_light(pedit, SCE_RESULT_COMMENT, 0x768465, -1, true);
        on_doc_default_light(pedit, SCE_RESULT_HEADER , eu_get_theme()->item.results.color, -1, true);
        on_doc_default_light(pedit, SCE_RESULT_KEYWORD, eu_get_theme()->item.results.bgcolor, -1, true);
        if (pedit->pwant)
        {
            pedit->pwant(pedit);
        }
    }
}

bool
on_result_launch(eu_tabpage *pnode)
{
    bool ret = false;
    if (pnode)
    {
        bool split = true;
        const HWND h = eu_hwnd_self();
        const HWND htab = on_tabpage_hwnd(pnode);
        if (h && htab)
        {
            if (!g_splitter_editbar)
            {
                split = on_splitter_init_editbar(h);
            }
            if (!g_splitter_editbar2)
            {
                split = on_splitter_editbar_slave(h);
            }
        }
        if (split && !(ret ^= (pnode->presult != NULL)))
        {
            const int flags = WS_CHILD | WS_CLIPSIBLINGS | WS_VSCROLL | WS_CLIPCHILDREN | WS_EX_RTLREADING;
            if ((pnode->presult = (eu_tabpage *)calloc(1, sizeof(eu_tabpage))))
            {
                if (on_sci_create(pnode->presult, h, flags, on_result_edit_proc) == SKYLARK_OK)
                {
                    on_sci_default_theme(pnode->presult, -1);
                    ret = true;
                }
            }
        }
    }
    return ret;
}

HWND
eu_result_hwnd(eu_tabpage *pnode)
{
    if (pnode && pnode->presult)
    {
        return (pnode->presult->hwnd_sc);
    }
    return NULL;
}
