#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <algorithm>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

static HANDLE g_process = nullptr;
static DWORD g_pid = 0;
static HWND g_log = nullptr;
static HWND g_editor = nullptr;
static HWND g_processList = nullptr;
static HWND g_status = nullptr;
static HBRUSH g_bg = nullptr;
static HBRUSH g_panel = nullptr;
static HBRUSH g_editorBg = nullptr;
static HFONT g_uiFont = nullptr;
static HFONT g_monoFont = nullptr;

static const COLORREF BG = RGB(10, 10, 14);
static const COLORREF PANEL = RGB(17, 17, 24);
static const COLORREF EDITOR = RGB(12, 12, 18);
static const COLORREF PURPLE = RGB(145, 80, 255);
static const COLORREF TEXT = RGB(235, 232, 245);
static const COLORREF MUTED = RGB(145, 140, 160);

static std::wstring Widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}

static std::string Narrow(const std::wstring& s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n, nullptr, nullptr);
    return out;
}

static void Log(const std::wstring& s) {
    if (!g_log) return;
    int len = GetWindowTextLengthW(g_log);
    SendMessageW(g_log, EM_SETSEL, len, len);
    SendMessageW(g_log, EM_REPLACESEL, FALSE, (LPARAM)(s + L"\r\n").c_str());
}

static bool ParseAddress(lua_State* L, int idx, uintptr_t& out) {
    if (lua_isinteger(L, idx)) { out = (uintptr_t)lua_tointeger(L, idx); return true; }
    if (lua_isstring(L, idx)) {
        const char* p = lua_tostring(L, idx); if (!p) return false;
        char* end = nullptr; unsigned long long v = strtoull(p, &end, 0);
        if (end && *end == '\0') { out = (uintptr_t)v; return true; }
    }
    return false;
}

static int LuaReadI32(lua_State* L) {
    uintptr_t addr; if (!ParseAddress(L,1,addr)) return luaL_error(L,"read_i32: invalid address");
    int32_t value{}; SIZE_T got{};
    if (!g_process || !ReadProcessMemory(g_process,(LPCVOID)addr,&value,sizeof(value),&got) || got != sizeof(value)) return luaL_error(L,"ReadProcessMemory failed");
    lua_pushinteger(L,value); return 1;
}
static int LuaReadF32(lua_State* L) {
    uintptr_t addr; if (!ParseAddress(L,1,addr)) return luaL_error(L,"read_f32: invalid address");
    float value{}; SIZE_T got{};
    if (!g_process || !ReadProcessMemory(g_process,(LPCVOID)addr,&value,sizeof(value),&got) || got != sizeof(value)) return luaL_error(L,"ReadProcessMemory failed");
    lua_pushnumber(L,value); return 1;
}
static int LuaReadU64(lua_State* L) {
    uintptr_t addr; if (!ParseAddress(L,1,addr)) return luaL_error(L,"read_u64: invalid address");
    uint64_t value{}; SIZE_T got{};
    if (!g_process || !ReadProcessMemory(g_process,(LPCVOID)addr,&value,sizeof(value),&got) || got != sizeof(value)) return luaL_error(L,"ReadProcessMemory failed");
    lua_pushinteger(L,(lua_Integer)value); return 1;
}
static int LuaWriteI32(lua_State* L) {
    uintptr_t addr; if (!ParseAddress(L,1,addr)) return luaL_error(L,"write_i32: invalid address");
    int32_t value=(int32_t)luaL_checkinteger(L,2); SIZE_T wrote{};
    if (!g_process || !WriteProcessMemory(g_process,(LPVOID)addr,&value,sizeof(value),&wrote) || wrote != sizeof(value)) return luaL_error(L,"WriteProcessMemory failed");
    lua_pushboolean(L,1); return 1;
}
static int LuaWriteF32(lua_State* L) {
    uintptr_t addr; if (!ParseAddress(L,1,addr)) return luaL_error(L,"write_f32: invalid address");
    float value=(float)luaL_checknumber(L,2); SIZE_T wrote{};
    if (!g_process || !WriteProcessMemory(g_process,(LPVOID)addr,&value,sizeof(value),&wrote) || wrote != sizeof(value)) return luaL_error(L,"WriteProcessMemory failed");
    lua_pushboolean(L,1); return 1;
}
static int LuaWriteU64(lua_State* L) {
    uintptr_t addr; if (!ParseAddress(L,1,addr)) return luaL_error(L,"write_u64: invalid address");
    uint64_t value=(uint64_t)luaL_checkinteger(L,2); SIZE_T wrote{};
    if (!g_process || !WriteProcessMemory(g_process,(LPVOID)addr,&value,sizeof(value),&wrote) || wrote != sizeof(value)) return luaL_error(L,"WriteProcessMemory failed");
    lua_pushboolean(L,1); return 1;
}
static int LuaReadBytes(lua_State* L) {
    uintptr_t addr; if (!ParseAddress(L,1,addr)) return luaL_error(L,"read_bytes: invalid address");
    lua_Integer n=luaL_checkinteger(L,2); if(n<0 || n>1024*1024) return luaL_error(L,"invalid length");
    std::string data((size_t)n,'\0'); SIZE_T got{};
    if (!g_process || !ReadProcessMemory(g_process,(LPCVOID)addr,data.data(),(SIZE_T)n,&got)) return luaL_error(L,"ReadProcessMemory failed");
    lua_pushlstring(L,data.data(),got); return 1;
}
static int LuaWriteBytes(lua_State* L) {
    uintptr_t addr; if (!ParseAddress(L,1,addr)) return luaL_error(L,"write_bytes: invalid address");
    size_t n{}; const char* data=luaL_checklstring(L,2,&n); SIZE_T wrote{};
    if(n>1024*1024) return luaL_error(L,"data too large");
    if(!g_process || !WriteProcessMemory(g_process,(LPVOID)addr,data,n,&wrote) || wrote!=n) return luaL_error(L,"WriteProcessMemory failed");
    lua_pushboolean(L,1); return 1;
}
static int LuaLog(lua_State* L) { Log(Widen(luaL_optstring(L,1,""))); return 0; }

static void RegisterLua(lua_State* L) {
    luaL_openlibs(L); lua_newtable(L);
    lua_pushcfunction(L,LuaReadI32); lua_setfield(L,-2,"read_i32");
    lua_pushcfunction(L,LuaReadF32); lua_setfield(L,-2,"read_f32");
    lua_pushcfunction(L,LuaReadU64); lua_setfield(L,-2,"read_u64");
    lua_pushcfunction(L,LuaReadBytes); lua_setfield(L,-2,"read_bytes");
    lua_pushcfunction(L,LuaWriteI32); lua_setfield(L,-2,"write_i32");
    lua_pushcfunction(L,LuaWriteF32); lua_setfield(L,-2,"write_f32");
    lua_pushcfunction(L,LuaWriteU64); lua_setfield(L,-2,"write_u64");
    lua_pushcfunction(L,LuaWriteBytes); lua_setfield(L,-2,"write_bytes");
    lua_pushcfunction(L,LuaLog); lua_setfield(L,-2,"log"); lua_setglobal(L,"mem");
}

static void RunScript() {
    if(!g_process){ Log(L"[!] Attach to your development game first."); return; }
    int len=GetWindowTextLengthW(g_editor); std::wstring ws(len,L'\0'); GetWindowTextW(g_editor,ws.data(),len+1);
    std::string script=Narrow(ws); lua_State* L=luaL_newstate(); RegisterLua(L);
    int rc=luaL_loadbuffer(L,script.data(),script.size(),"editor") || lua_pcall(L,0,LUA_MULTRET,0);
    if(rc!=LUA_OK){ const char* err=lua_tostring(L,-1); Log(L"[Lua Error] "+Widen(err?err:"unknown error")); }
    else Log(L"[OK] Script executed successfully.");
    lua_close(L);
}

static std::vector<std::pair<DWORD,std::wstring>> EnumerateProcesses() {
    std::vector<std::pair<DWORD,std::wstring>> out; HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snap==INVALID_HANDLE_VALUE) return out; PROCESSENTRY32W pe{sizeof(pe)};
    if(Process32FirstW(snap,&pe)) do{out.emplace_back(pe.th32ProcessID,pe.szExeFile);}while(Process32NextW(snap,&pe));
    CloseHandle(snap); std::sort(out.begin(),out.end(),[](auto&a,auto&b){return a.second<b.second;}); return out;
}
static void RefreshProcesses(){
    SendMessageW(g_processList,CB_RESETCONTENT,0,0); auto ps=EnumerateProcesses();
    for(auto&p:ps){int idx=(int)SendMessageW(g_processList,CB_ADDSTRING,0,(LPARAM)p.second.c_str()); SendMessageW(g_processList,CB_SETITEMDATA,idx,p.first);}
    Log(L"[System] Process list refreshed.");
}
static void AttachSelected(){
    int idx=(int)SendMessageW(g_processList,CB_GETCURSEL,0,0); if(idx==CB_ERR){Log(L"[!] Select a process.");return;}
    DWORD pid=(DWORD)SendMessageW(g_processList,CB_GETITEMDATA,idx,0);
    HANDLE h=OpenProcess(PROCESS_VM_READ|PROCESS_VM_WRITE|PROCESS_VM_OPERATION|PROCESS_QUERY_INFORMATION,FALSE,pid);
    if(!h){Log(L"[!] OpenProcess failed.");return;} if(g_process) CloseHandle(g_process); g_process=h; g_pid=pid;
    wchar_t name[260]{}; SendMessageW(g_processList,CB_GETLBTEXT,idx,(LPARAM)name);
    SetWindowTextW(g_status,(L"●  ATTACHED  PID "+std::to_wstring(pid)).c_str()); Log(L"[+] Attached to "+std::wstring(name)+L"  PID "+std::to_wstring(pid));
}
static void Disconnect(){
    if(g_process){CloseHandle(g_process);g_process=nullptr;g_pid=0;} if(g_status) SetWindowTextW(g_status,L"○  NOT ATTACHED"); Log(L"[System] Detached.");
}

static HWND Button(HWND p,int id,const wchar_t* text,int x,int y,int w,int h){
    return CreateWindowW(L"BUTTON",text,WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,x,y,w,h,p,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
}
static void SetCtrlFont(HWND h, HFONT f){ SendMessageW(h,WM_SETFONT,(WPARAM)f,TRUE); }

static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_ERASEBKGND:{RECT r;GetClientRect(h,&r);FillRect((HDC)w,&r,g_bg);return 1;}
    case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: case WM_CTLCOLORBTN:{
        HDC dc=(HDC)w; HWND ctrl=(HWND)l; SetBkMode(dc,TRANSPARENT); SetTextColor(dc,TEXT);
        if(ctrl==g_editor || ctrl==g_log){SetBkColor(dc,EDITOR);return (LRESULT)g_editorBg;}
        SetBkColor(dc,PANEL);return (LRESULT)g_panel;
    }
    case WM_DRAWITEM:{
        DRAWITEMSTRUCT* d=(DRAWITEMSTRUCT*)l; if(!d || !d->hwndItem) break;
        RECT r=d->rcItem; HBRUSH b=CreateSolidBrush((d->itemState&ODS_SELECTED)?RGB(115,55,215):RGB(32,25,45));
        FillRect(d->hDC,&r,b); DeleteObject(b); FrameRect(d->hDC,&r,CreateSolidBrush(PURPLE));
        wchar_t text[128]{}; GetWindowTextW(d->hwndItem,text,128); SetBkMode(d->hDC,TRANSPARENT); SetTextColor(d->hDC,TEXT); SelectObject(d->hDC,g_uiFont);
        DrawTextW(d->hDC,text,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE); return TRUE;
    }
    case WM_COMMAND:
        switch(LOWORD(w)){case 1001:RefreshProcesses();break;case 1002:AttachSelected();break;case 1003:Disconnect();break;case 1004:RunScript();break;}
        break;
    case WM_KEYDOWN: if(w==VK_F5){RunScript();return 0;} break;
    case WM_DESTROY: Disconnect(); if(g_bg)DeleteObject(g_bg);if(g_panel)DeleteObject(g_panel);if(g_editorBg)DeleteObject(g_editorBg);if(g_uiFont)DeleteObject(g_uiFont);if(g_monoFont)DeleteObject(g_monoFont);PostQuitMessage(0);break;
    }
    return DefWindowProcW(h,m,w,l);
}

int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int show){
    INITCOMMONCONTROLSEX icc{sizeof(icc),ICC_STANDARD_CLASSES}; InitCommonControlsEx(&icc);
    g_bg=CreateSolidBrush(BG); g_panel=CreateSolidBrush(PANEL); g_editorBg=CreateSolidBrush(EDITOR);
    g_uiFont=CreateFontW(15,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    g_monoFont=CreateFontW(15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FIXED_PITCH,L"Cascadia Mono");
    WNDCLASSW wc{}; wc.hInstance=inst;wc.lpfnWndProc=WndProc;wc.lpszClassName=L"NeonDebuggerWindow";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=g_bg;RegisterClassW(&wc);
    HWND win=CreateWindowW(wc.lpszClassName,L"NeonDebugger  •  Developer Sandbox",WS_OVERLAPPEDWINDOW|WS_VISIBLE,80,70,1200,760,nullptr,nullptr,inst,nullptr);
    if(!win)return 0;
    SetClassLongPtrW(win,GCLP_HBRBACKGROUND,(LONG_PTR)g_bg);

    HWND sidebar=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,0,0,185,760,win,nullptr,inst,nullptr);SetCtrlFont(sidebar,g_uiFont);
    HWND title=CreateWindowW(L"STATIC",L"NEON\nDEBUGGER",WS_CHILD|WS_VISIBLE,25,25,145,55,win,nullptr,inst,nullptr);SetCtrlFont(title,g_uiFont);
    HWND sub=CreateWindowW(L"STATIC",L"DEVELOPER SANDBOX",WS_CHILD|WS_VISIBLE,25,80,145,25,win,nullptr,inst,nullptr);SetCtrlFont(sub,g_uiFont);
    Button(win,1101,L"▣   EDITOR",18,140,149,38); Button(win,1102,L"▤   SCRIPTS",18,185,149,38); Button(win,1103,L"▥   CONSOLE",18,230,149,38); Button(win,1104,L"⚙   SETTINGS",18,275,149,38);
    HWND sideInfo=CreateWindowW(L"STATIC",L"LOCAL DEVELOPMENT\r\nLua 5.4\r\nWindows x64",WS_CHILD|WS_VISIBLE,25,650,145,70,win,nullptr,inst,nullptr);SetCtrlFont(sideInfo,g_uiFont);

    HWND top=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,185,0,1015,70,win,nullptr,inst,nullptr);
    HWND label=CreateWindowW(L"STATIC",L"SCRIPT EDITOR",WS_CHILD|WS_VISIBLE,210,18,220,25,win,nullptr,inst,nullptr);SetCtrlFont(label,g_uiFont);
    g_processList=CreateWindowW(L"COMBOBOX",L"Select process...",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,585,18,250,34,win,(HMENU)1000,inst,nullptr);SetCtrlFont(g_processList,g_uiFont);
    Button(win,1001,L"REFRESH",845,18,85,34);Button(win,1002,L"ATTACH",938,18,85,34);Button(win,1003,L"DETACH",1031,18,85,34);

    g_status=CreateWindowW(L"STATIC",L"○  NOT ATTACHED",WS_CHILD|WS_VISIBLE,210,78,300,30,win,nullptr,inst,nullptr);SetCtrlFont(g_status,g_uiFont);
    g_editor=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"-- NeonDebugger Lua sandbox\r\n-- Press F5 or click RUN to execute\r\n\r\nmem.log('Hello from Lua')\r\n",WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_AUTOHSCROLL|ES_WANTRETURN,210,115,930,405,win,nullptr,inst,nullptr);SetCtrlFont(g_editor,g_monoFont);
    Button(win,1004,L"▶  RUN LUA   F5",210,535,150,40);
    HWND hint=CreateWindowW(L"STATIC",L"OUTPUT / CONSOLE",WS_CHILD|WS_VISIBLE,380,543,250,25,win,nullptr,inst,nullptr);SetCtrlFont(hint,g_uiFont);
    g_log=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"[System] NeonDebugger ready.\r\n",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY,210,580,930,90,win,nullptr,inst,nullptr);SetCtrlFont(g_log,g_monoFont);
    RefreshProcesses();
    ShowWindow(win,show);UpdateWindow(win);
    MSG msg;while(GetMessageW(&msg,nullptr,0,0)){TranslateMessage(&msg);DispatchMessageW(&msg);}return 0;
}
