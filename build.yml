/*
================================================================================
  RPG DEBUGGER  –  single-file edition
  Attach to your game process and run Lua 5.4 scripts against its memory.

  BUILD (one command, no CMake needed):
  ──────────────────────────────────────────────────────────────────────────────
  STEP 1 – Get Lua 5.4 headers + amalgamation:
    Download lua-5.4.7.tar.gz from https://www.lua.org/ftp/
    Extract so that lua-5.4.7\ is next to this file.

  STEP 2a – MSVC (Developer Command Prompt x64):
    cl /nologo /std:c++17 /O2 /W3 /MD /EHsc ^
       /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX ^
       /D_CRT_SECURE_NO_WARNINGS /DLUA_CORE /DLUA_LIB ^
       /I lua-5.4.7\src ^
       main.cpp ^
       /link /SUBSYSTEM:WINDOWS /MANIFESTUAC:"level='requireAdministrator'" ^
       psapi.lib comctl32.lib comdlg32.lib gdi32.lib user32.lib kernel32.lib

  STEP 2b – MinGW-w64 (MSYS2):
    g++ -std=c++17 -O2 -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX \
        -DLUA_CORE -DLUA_LIB -DLUA_USE_WINDOWS \
        -I lua-5.4.7/src \
        main.cpp \
        -mwindows -static-libgcc -static-libstdc++ \
        -lpsapi -lcomctl32 -lcomdlg32 -lgdi32 -luser32 -lkernel32 -o debugger.exe

  LUA API (use in the script editor, press F5 to run):
  ──────────────────────────────────────────────────────────────────────────────
    mem.read(addr, type)            → value | nil  (types: int32 int64 float double byte string)
    mem.write(addr, value, type)    → true | false
    mem.scan(value, type, lo, hi)   → address | nil
    mem.pointer(base, off1, ...)    → resolved address | nil

    process.base([name])            → module base address
    process.pid()                   → pid
    process.name()                  → exe name

    gui.log(msg [, color])          → print to output pane
    gui.clear()                     → clear output
    gui.status(msg)                 → status bar

    util.tohex(n)                   → "0xABCD"
    util.sleep(ms)

    COLOR_OK  COLOR_ERR  COLOR_WARN  COLOR_INFO  (colour constants)
================================================================================
*/

// ── Lua amalgamation (include all Lua 5.4 .c files as one TU) ────────────────
// These must appear before any Windows headers because Lua defines its own
// minimal set of macros that conflict with winsock if included after.
extern "C" {
#include "lzio.c"
#include "lctype.c"
#include "lopcodes.c"
#include "lmem.c"
#include "lobject.c"
#include "ltm.c"
#include "lstring.c"
#include "ltable.c"
#include "lstate.c"
#include "llex.c"
#include "lparser.c"
#include "lcode.c"
#include "ldebug.c"
#include "ldo.c"
#include "lfunc.c"
#include "lgc.c"
#include "lvm.c"
#include "lapi.c"
#include "lauxlib.c"
#include "lbaselib.c"
#include "lcorolib.c"
#include "ldblib.c"
#include "liolib.c"
#include "lmathlib.c"
#include "loadlib.c"
#include "loslib.c"
#include "lstrlib.c"
#include "ltablib.c"
#include "lutf8lib.c"
#include "linit.c"
}

// ── Windows / system headers ──────────────────────────────────────────────────
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <richedit.h>

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cstdio>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "gdi32.lib")

// ─────────────────────────────────────────────────────────────────────────────
//  Colours
// ─────────────────────────────────────────────────────────────────────────────
#define CLR_BG         RGB(15,  15,  20)
#define CLR_PANEL      RGB(22,  22,  32)
#define CLR_EDITOR_BG  RGB(11,  11,  16)
#define CLR_OUTPUT_BG  RGB( 8,   8,  12)
#define CLR_ACCENT     RGB(140,  90, 255)
#define CLR_ACCENT2    RGB( 70, 210, 180)
#define CLR_TEXT       RGB(215, 215, 235)
#define CLR_TEXT_DIM   RGB(110, 110, 140)
#define CLR_SUCCESS    RGB( 80, 220, 100)
#define CLR_ERROR      RGB(255,  75,  75)
#define CLR_WARNING    RGB(255, 175,  45)

// ─────────────────────────────────────────────────────────────────────────────
//  Control IDs
// ─────────────────────────────────────────────────────────────────────────────
enum {
    ID_PROC_LIST = 1001, ID_ATTACH, ID_REFRESH, ID_DETACH,
    ID_SCRIPT,           ID_RUN,    ID_CLR_SCR, ID_CLR_OUT,
    ID_OUTPUT,
    ID_ADDR,             ID_TYPE,   ID_VAL,
    ID_MEM_READ,         ID_MEM_WRITE, ID_BOOKMARK,
    ID_MEM_TABLE,
    ID_EXAMPLE_COMBO,    ID_LOAD_EX,
    ID_STATUS,
};

// ─────────────────────────────────────────────────────────────────────────────
//  App state (all global, single-instance tool)
// ─────────────────────────────────────────────────────────────────────────────
struct Bookmark { std::wstring label; uintptr_t address; std::wstring type; std::wstring val; };

static HWND g_hwnd, g_hProc, g_hScript, g_hOutput, g_hStatus;
static HWND g_hAddr, g_hType, g_hVal, g_hMemTbl, g_hExample;
static HANDLE    g_hProcess  = nullptr;
static DWORD     g_pid       = 0;
static std::wstring g_procName;
static lua_State*   g_L      = nullptr;
static std::vector<std::wstring>  g_procNames;
static std::vector<DWORD>         g_procPids;
static std::vector<Bookmark>      g_bookmarks;
static HBRUSH g_brBg    = nullptr;
static HFONT  g_fMono   = nullptr;
static HFONT  g_fUI     = nullptr;
static HFONT  g_fBold   = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
static std::wstring GetWndText(HWND h) {
    int n = GetWindowTextLengthW(h); if (!n) return {};
    std::wstring s(n+1,0); GetWindowTextW(h,s.data(),n+1); s.resize(n); return s;
}
static std::wstring WideFromUtf8(const char* s, int len=-1) {
    if (!s||!*s) return {};
    int n = MultiByteToWideChar(CP_UTF8,0,s,len,nullptr,0);
    std::wstring w(n,0); MultiByteToWideChar(CP_UTF8,0,s,len,w.data(),n);
    if (!w.empty()&&w.back()==0) w.pop_back();
    return w;
}
static std::string Utf8FromWide(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),nullptr,0,nullptr,nullptr);
    std::string s(n,0); WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),s.data(),n,nullptr,nullptr);
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Output pane
// ─────────────────────────────────────────────────────────────────────────────
static void AppendOutput(const std::wstring& text, COLORREF col = CLR_TEXT) {
    if (!g_hOutput) return;
    CHARRANGE cr{-1,-1};
    SendMessageW(g_hOutput, EM_EXSETSEL, 0, (LPARAM)&cr);
    CHARFORMATW cf{}; cf.cbSize=sizeof(cf);
    cf.dwMask = CFM_COLOR|CFM_FACE|CFM_SIZE;
    cf.crTextColor = col; cf.yHeight = 180;
    wcscpy_s(cf.szFaceName, L"Consolas");
    SendMessageW(g_hOutput, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    SendMessageW(g_hOutput, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
    SendMessageW(g_hOutput, WM_VSCROLL, SB_BOTTOM, 0);
}
static void SetStatus(const std::wstring& s) {
    if (g_hStatus) SetWindowTextW(g_hStatus, s.c_str());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Memory R/W
// ─────────────────────────────────────────────────────────────────────────────
static bool MemRawRead(uintptr_t addr, void* buf, size_t sz) {
    if (!g_hProcess) return false;
    SIZE_T r=0;
    return ReadProcessMemory(g_hProcess,(LPCVOID)addr,buf,sz,&r) && r==sz;
}
static bool MemRawWrite(uintptr_t addr, const void* buf, size_t sz) {
    if (!g_hProcess) return false;
    DWORD old=0;
    VirtualProtectEx(g_hProcess,(LPVOID)addr,sz,PAGE_EXECUTE_READWRITE,&old);
    SIZE_T w=0;
    bool ok = WriteProcessMemory(g_hProcess,(LPVOID)addr,buf,sz,&w) && w==sz;
    if (old) VirtualProtectEx(g_hProcess,(LPVOID)addr,sz,old,&old);
    return ok;
}
template<typename T> static bool MemRead (uintptr_t a, T& v) { return MemRawRead (a,&v,sizeof(T)); }
template<typename T> static bool MemWrite(uintptr_t a, T  v) { return MemRawWrite(a,&v,sizeof(T)); }

static std::string MemReadStr(uintptr_t addr, size_t maxLen=256) {
    if (!g_hProcess) return {};
    std::string s(maxLen,'\0'); SIZE_T r=0;
    ReadProcessMemory(g_hProcess,(LPCVOID)addr,s.data(),maxLen,&r);
    s.resize(strnlen(s.c_str(),r)); return s;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Process list
// ─────────────────────────────────────────────────────────────────────────────
static void RefreshProcs() {
    g_procNames.clear(); g_procPids.clear();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if (snap==INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe{sizeof(pe)};
    if (Process32FirstW(snap,&pe)) do {
        g_procNames.push_back(pe.szExeFile);
        g_procPids .push_back(pe.th32ProcessID);
    } while (Process32NextW(snap,&pe));
    CloseHandle(snap);
    // Sort by name
    std::vector<size_t> idx(g_procPids.size());
    std::iota(idx.begin(),idx.end(),0);
    std::sort(idx.begin(),idx.end(),[](size_t a,size_t b){
        return _wcsicmp(g_procNames[a].c_str(),g_procNames[b].c_str())<0;
    });
    std::vector<std::wstring> sn; std::vector<DWORD> sp;
    for (auto i:idx){ sn.push_back(g_procNames[i]); sp.push_back(g_procPids[i]); }
    g_procNames=sn; g_procPids=sp;
    // Fill listbox
    SendMessageW(g_hProc,LB_RESETCONTENT,0,0);
    for (size_t i=0;i<g_procPids.size();i++) {
        std::wstring e = g_procNames[i]+L"  ["+std::to_wstring(g_procPids[i])+L"]";
        int j=(int)SendMessageW(g_hProc,LB_ADDSTRING,0,(LPARAM)e.c_str());
        SendMessageW(g_hProc,LB_SETITEMDATA,j,(LPARAM)g_procPids[i]);
    }
}
static void Detach() {
    if (g_hProcess) { CloseHandle(g_hProcess); g_hProcess=nullptr; }
    g_pid=0; g_procName.clear();
}
static bool Attach(DWORD pid) {
    Detach();
    HANDLE h = OpenProcess(PROCESS_VM_READ|PROCESS_VM_WRITE|PROCESS_VM_OPERATION|
                           PROCESS_QUERY_INFORMATION,FALSE,pid);
    if (!h) {
        AppendOutput(L"[ERROR] OpenProcess failed (err="+std::to_wstring(GetLastError())+L")\r\n",CLR_ERROR);
        return false;
    }
    g_hProcess=h; g_pid=pid;
    for (size_t i=0;i<g_procPids.size();i++)
        if (g_procPids[i]==pid) { g_procName=g_procNames[i]; break; }
    AppendOutput(L"[OK] Attached to "+g_procName+L" (PID "+std::to_wstring(pid)+L")\r\n",CLR_SUCCESS);
    SetStatus(L"Attached: "+g_procName+L" [PID "+std::to_wstring(pid)+L"]");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Bookmark table
// ─────────────────────────────────────────────────────────────────────────────
static void RefreshBookmarks() {
    ListView_DeleteAllItems(g_hMemTbl);
    for (int i=0;i<(int)g_bookmarks.size();i++) {
        auto& b=g_bookmarks[i];
        LVITEMW lvi{}; lvi.mask=LVIF_TEXT; lvi.iItem=i;
        lvi.pszText=const_cast<LPWSTR>(b.label.c_str());
        ListView_InsertItem(g_hMemTbl,&lvi);
        std::wostringstream oss; oss<<L"0x"<<std::hex<<std::uppercase<<b.address;
        ListView_SetItemText(g_hMemTbl,i,1,const_cast<LPWSTR>(oss.str().c_str()));
        ListView_SetItemText(g_hMemTbl,i,2,const_cast<LPWSTR>(b.type.c_str()));
        ListView_SetItemText(g_hMemTbl,i,3,const_cast<LPWSTR>(b.val.c_str()));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Lua bindings
// ─────────────────────────────────────────────────────────────────────────────
static int L_print(lua_State* L) {
    int n=lua_gettop(L); std::wstring line;
    lua_getglobal(L,"tostring");
    for (int i=1;i<=n;i++) {
        lua_pushvalue(L,-1); lua_pushvalue(L,i); lua_call(L,1,1);
        size_t len=0; const char* s=lua_tolstring(L,-1,&len);
        if (i>1) line+=L"\t";
        line+=WideFromUtf8(s?s:"(nil)",(int)(s?len:-1));
        lua_pop(L,1);
    }
    lua_pop(L,1);
    AppendOutput(line+L"\r\n",CLR_TEXT);
    return 0;
}

// mem table
static int L_mem_read(lua_State* L) {
    if (!g_hProcess){ lua_pushnil(L); lua_pushstring(L,"not attached"); return 2; }
    uintptr_t addr=(uintptr_t)lua_tointeger(L,1);
    const char* t=luaL_optstring(L,2,"int32");
    #define TRY(T,push) { T v{}; if(!MemRead(addr,v)){lua_pushnil(L);lua_pushstring(L,"read failed");return 2;} push(L,v); return 1; }
    if (!strcmp(t,"int32"))  TRY(int32_t, lua_pushinteger)
    if (!strcmp(t,"int64"))  TRY(int64_t, lua_pushinteger)
    if (!strcmp(t,"float"))  TRY(float,   lua_pushnumber)
    if (!strcmp(t,"double")) TRY(double,  lua_pushnumber)
    if (!strcmp(t,"byte"))   { uint8_t v{}; if(!MemRead(addr,v)){lua_pushnil(L);lua_pushstring(L,"read failed");return 2;} lua_pushinteger(L,v); return 1; }
    if (!strcmp(t,"string")) {
        size_t mx=(size_t)luaL_optinteger(L,3,256);
        auto s=MemReadStr(addr,mx); lua_pushlstring(L,s.c_str(),s.size()); return 1;
    }
    #undef TRY
    lua_pushnil(L); lua_pushfstring(L,"unknown type: %s",t); return 2;
}
static int L_mem_write(lua_State* L) {
    if (!g_hProcess){ lua_pushboolean(L,0); lua_pushstring(L,"not attached"); return 2; }
    uintptr_t addr=(uintptr_t)lua_tointeger(L,1);
    const char* t=luaL_optstring(L,3,"int32");
    bool ok=false;
    if      (!strcmp(t,"int32"))  ok=MemWrite<int32_t>(addr,(int32_t)lua_tointeger(L,2));
    else if (!strcmp(t,"int64"))  ok=MemWrite<int64_t>(addr,(int64_t)lua_tointeger(L,2));
    else if (!strcmp(t,"float"))  ok=MemWrite<float>  (addr,(float)lua_tonumber(L,2));
    else if (!strcmp(t,"double")) ok=MemWrite<double> (addr,lua_tonumber(L,2));
    else if (!strcmp(t,"byte"))   ok=MemWrite<uint8_t>(addr,(uint8_t)lua_tointeger(L,2));
    else if (!strcmp(t,"string")) {
        size_t len=0; const char* s=luaL_checklstring(L,2,&len);
        ok=MemRawWrite(addr,s,len+1);
    }
    lua_pushboolean(L,ok?1:0); return 1;
}
static int L_mem_scan(lua_State* L) {
    if (!g_hProcess){ lua_pushnil(L); lua_pushstring(L,"not attached"); return 2; }
    const char* t=luaL_optstring(L,2,"int32");
    uintptr_t lo=(uintptr_t)luaL_optinteger(L,3,0x10000);
    uintptr_t hi=(uintptr_t)luaL_optinteger(L,4,0x7FFFFFFF0000LL);
    uint8_t needle[8]; size_t nsz=0;
    if      (!strcmp(t,"int32"))  { int32_t v=(int32_t)lua_tointeger(L,1); memcpy(needle,&v,4); nsz=4; }
    else if (!strcmp(t,"float"))  { float   v=(float)lua_tonumber(L,1);    memcpy(needle,&v,4); nsz=4; }
    else if (!strcmp(t,"byte"))   { needle[0]=(uint8_t)lua_tointeger(L,1); nsz=1; }
    else { lua_pushnil(L); lua_pushstring(L,"unsupported scan type"); return 2; }
    AppendOutput(L"[SCAN] Searching...\r\n",CLR_TEXT_DIM);
    MEMORY_BASIC_INFORMATION mbi{}; uintptr_t cur=lo;
    while (cur<hi && VirtualQueryEx(g_hProcess,(LPCVOID)cur,&mbi,sizeof(mbi))) {
        if (mbi.State==MEM_COMMIT &&
            (mbi.Protect&(PAGE_READWRITE|PAGE_EXECUTE_READWRITE|PAGE_WRITECOPY)) &&
           !(mbi.Protect&PAGE_GUARD)) {
            std::vector<uint8_t> buf(mbi.RegionSize); SIZE_T rd=0;
            if (ReadProcessMemory(g_hProcess,mbi.BaseAddress,buf.data(),mbi.RegionSize,&rd)&&rd>=nsz)
                for (size_t i=0;i<=rd-nsz;i++)
                    if (!memcmp(buf.data()+i,needle,nsz)) {
                        uintptr_t found=(uintptr_t)mbi.BaseAddress+i;
                        std::wostringstream oss; oss<<L"[SCAN] Found: 0x"<<std::hex<<std::uppercase<<found<<L"\r\n";
                        AppendOutput(oss.str(),CLR_SUCCESS);
                        lua_pushinteger(L,(lua_Integer)found); return 1;
                    }
        }
        cur=(uintptr_t)mbi.BaseAddress+mbi.RegionSize;
        if (!mbi.RegionSize) break;
    }
    AppendOutput(L"[SCAN] Not found\r\n",CLR_WARNING);
    lua_pushnil(L); return 1;
}
static int L_mem_pointer(lua_State* L) {
    if (!g_hProcess){ lua_pushnil(L); return 1; }
    uintptr_t addr=(uintptr_t)lua_tointeger(L,1);
    for (int i=2;i<=lua_gettop(L);i++) {
        uintptr_t next=0;
        if (!MemRead(addr,next)){ lua_pushnil(L); return 1; }
        addr=next+(uintptr_t)lua_tointeger(L,i);
    }
    lua_pushinteger(L,(lua_Integer)addr); return 1;
}

// process table
static int L_proc_base(lua_State* L) {
    if (!g_hProcess){ lua_pushnil(L); return 1; }
    const char* mod=luaL_optstring(L,1,nullptr);
    HMODULE mods[1024]; DWORD needed=0;
    if (!EnumProcessModules(g_hProcess,mods,sizeof(mods),&needed)){ lua_pushnil(L); return 1; }
    for (DWORD i=0;i<needed/sizeof(HMODULE);i++) {
        char name[MAX_PATH]{};
        GetModuleBaseNameA(g_hProcess,mods[i],name,MAX_PATH);
        if (!mod||_stricmp(name,mod)==0){ lua_pushinteger(L,(lua_Integer)(uintptr_t)mods[i]); return 1; }
    }
    lua_pushnil(L); return 1;
}
static int L_proc_pid (lua_State* L){ lua_pushinteger(L,(lua_Integer)g_pid); return 1; }
static int L_proc_name(lua_State* L){ auto s=Utf8FromWide(g_procName); lua_pushlstring(L,s.c_str(),s.size()); return 1; }

// gui table
static int L_gui_log(lua_State* L) {
    size_t len=0; const char* s=luaL_tolstring(L,1,&len);
    COLORREF col=CLR_TEXT;
    if (!lua_isnoneornil(L,2)) col=(COLORREF)lua_tointeger(L,2);
    AppendOutput(WideFromUtf8(s,(int)len)+L"\r\n",col);
    return 0;
}
static int L_gui_clear (lua_State* L){ SetWindowTextW(g_hOutput,L""); return 0; }
static int L_gui_status(lua_State* L){
    size_t len=0; const char* s=luaL_tolstring(L,1,&len);
    SetStatus(WideFromUtf8(s,(int)len)); return 0;
}

// util table
static int L_util_hex  (lua_State* L){ char b[24]; snprintf(b,sizeof(b),"0x%llX",(unsigned long long)(uintptr_t)lua_tointeger(L,1)); lua_pushstring(L,b); return 1; }
static int L_util_sleep(lua_State* L){ Sleep((DWORD)luaL_optinteger(L,1,100)); return 0; }

static void reg_table(lua_State* L,const char* name,const luaL_Reg* f) {
    lua_newtable(L);
    for (;f->name;f++){ lua_pushcfunction(L,f->func); lua_setfield(L,-2,f->name); }
    lua_setglobal(L,name);
}

static void LuaInit() {
    if (g_L){ lua_close(g_L); g_L=nullptr; }
    g_L=luaL_newstate(); luaL_openlibs(g_L);
    lua_pushcfunction(g_L,L_print); lua_setglobal(g_L,"print");

    static const luaL_Reg mem_f[]={{"read",L_mem_read},{"write",L_mem_write},{"scan",L_mem_scan},{"pointer",L_mem_pointer},{nullptr,nullptr}};
    static const luaL_Reg proc_f[]={{"base",L_proc_base},{"pid",L_proc_pid},{"name",L_proc_name},{nullptr,nullptr}};
    static const luaL_Reg gui_f[]={{"log",L_gui_log},{"clear",L_gui_clear},{"status",L_gui_status},{nullptr,nullptr}};
    static const luaL_Reg util_f[]={{"tohex",L_util_hex},{"sleep",L_util_sleep},{nullptr,nullptr}};
    reg_table(g_L,"mem",mem_f);
    reg_table(g_L,"process",proc_f);
    reg_table(g_L,"gui",gui_f);
    reg_table(g_L,"util",util_f);

    lua_pushinteger(g_L,CLR_SUCCESS); lua_setglobal(g_L,"COLOR_OK");
    lua_pushinteger(g_L,CLR_ERROR);   lua_setglobal(g_L,"COLOR_ERR");
    lua_pushinteger(g_L,CLR_WARNING); lua_setglobal(g_L,"COLOR_WARN");
    lua_pushinteger(g_L,CLR_ACCENT2); lua_setglobal(g_L,"COLOR_INFO");
}

static bool LuaRun(const std::string& code) {
    if (!g_L) LuaInit();
    int top=lua_gettop(g_L);
    if (luaL_loadstring(g_L,code.c_str())) {
        AppendOutput(L"[SYNTAX] "+WideFromUtf8(lua_tostring(g_L,-1))+L"\r\n",CLR_ERROR);
        lua_settop(g_L,top); return false;
    }
    if (lua_pcall(g_L,0,LUA_MULTRET,0)) {
        AppendOutput(L"[ERROR] "+WideFromUtf8(lua_tostring(g_L,-1))+L"\r\n",CLR_ERROR);
        lua_settop(g_L,top); return false;
    }
    lua_settop(g_L,top); return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Built-in example scripts
// ─────────────────────────────────────────────────────────────────────────────
struct Example { const wchar_t* name; const char* code; };
static const Example k_ex[] = {
{ L"Process info", R"(
print("Name : " .. process.name())
print("PID  : " .. tostring(process.pid()))
local b = process.base()
gui.log(b and ("Base : " .. util.tohex(b)) or "Base : (no process)", b and COLOR_OK or COLOR_ERR)
)" },
{ L"Read float", R"(
-- Change the offset to match your game
local base = process.base()
if not base then gui.log("Not attached", COLOR_ERR) return end
local addr = base + 0x1A4       -- ← your offset
local v = mem.read(addr, "float")
gui.log(v and string.format("Value: %.3f  @ %s", v, util.tohex(addr))
          or  "Read failed – wrong offset?", v and COLOR_OK or COLOR_ERR)
)" },
{ L"Write float (health)", R"(
local base = process.base()
if not base then gui.log("Not attached", COLOR_ERR) return end
local addr  = base + 0x1A4     -- ← your offset
local value = 9999.0
local ok = mem.write(addr, value, "float")
gui.log(ok and ("Wrote " .. tostring(value) .. " → " .. util.tohex(addr))
           or  "Write failed!", ok and COLOR_OK or COLOR_ERR)
)" },
{ L"Teleport XYZ", R"(
local base = process.base()
if not base then gui.log("Not attached", COLOR_ERR) return end
local pos = base + 0x3C0       -- ← base of your position struct { float x,y,z; }
local x,y,z = 512.0, 0.0, 256.0
mem.write(pos+0x0, x, "float")
mem.write(pos+0x4, y, "float")
mem.write(pos+0x8, z, "float")
gui.log(string.format("Teleported → (%.1f, %.1f, %.1f)", x,y,z), COLOR_OK)
)" },
{ L"Pointer chain", R"(
local base = process.base()
if not base then gui.log("Not attached", COLOR_ERR) return end
-- Resolves: [base+0x50] → +0x18 → +0x4  → value
local addr = mem.pointer(base+0x50, 0x18, 0x4)
if not addr then gui.log("Pointer walk failed", COLOR_ERR) return end
gui.log("Resolved: " .. util.tohex(addr), COLOR_INFO)
local v = mem.read(addr, "float")
gui.log("Value: " .. tostring(v), v and COLOR_OK or COLOR_ERR)
)" },
{ L"Scan for int32", R"(
local base = process.base() or 0x10000
gui.log("Scanning for 100 ...", COLOR_INFO)
local addr = mem.scan(100, "int32", base, base + 0x800000)
if addr then
    gui.log("Found at " .. util.tohex(addr), COLOR_OK)
    mem.write(addr, 9999, "int32")
    gui.log("Patched to 9999", COLOR_OK)
else
    gui.log("Not found – try running in-game first so the value exists", COLOR_WARN)
end
)" },
{ L"Hex dump (64 bytes)", R"(
local base = process.base()
if not base then gui.log("Not attached", COLOR_ERR) return end
local line = ""
for i = 0, 63 do
    local b = mem.read(base+i, "byte")
    line = line .. string.format("%02X ", b or 0)
    if (i+1) % 16 == 0 then gui.log(line, COLOR_INFO); line="" end
end
)" },
{ L"Godmode loop (5s)", R"(
local base = process.base()
if not base then gui.log("Not attached", COLOR_ERR) return end
local addr = base + 0x1A4     -- ← health float offset
gui.log("God mode for 5 seconds...", COLOR_WARN)
for i = 1, 50 do
    mem.write(addr, 9999.0, "float")
    util.sleep(100)
end
gui.log("Done.", COLOR_OK)
)" },
};
static void LoadExample(int i) {
    if (i<0||i>=(int)(sizeof(k_ex)/sizeof(*k_ex))) return;
    auto w=WideFromUtf8(k_ex[i].code);
    // Strip leading newline
    if (!w.empty()&&w[0]==L'\n') w=w.substr(1);
    SetWindowTextW(g_hScript,w.c_str());
}

// ─────────────────────────────────────────────────────────────────────────────
//  GUI – memory inspector helpers
// ─────────────────────────────────────────────────────────────────────────────
static void DoMemRead() {
    if (!g_hProcess){ AppendOutput(L"[WARN] Not attached\r\n",CLR_WARNING); return; }
    uintptr_t addr=(uintptr_t)wcstoull(GetWndText(g_hAddr).c_str(),nullptr,16);
    wchar_t tb[32]{}; SendMessageW(g_hType,CB_GETLBTEXT,SendMessageW(g_hType,CB_GETCURSEL,0,0),(LPARAM)tb);
    std::wstring t=tb, result;
    std::wostringstream rv;
    if (t==L"int32")  { int32_t v{}; MemRead(addr,v); rv<<v; }
    else if(t==L"int64") { int64_t v{}; MemRead(addr,v); rv<<v; }
    else if(t==L"float") { float   v{}; MemRead(addr,v); rv<<v; }
    else if(t==L"double"){ double  v{}; MemRead(addr,v); rv<<v; }
    else if(t==L"byte")  { uint8_t v{}; MemRead(addr,v); rv<<(int)v; }
    else if(t==L"string"){ rv<<WideFromUtf8(MemReadStr(addr).c_str()); }
    SetWindowTextW(g_hVal,rv.str().c_str());
    std::wostringstream log; log<<L"[R] 0x"<<std::hex<<std::uppercase<<addr<<L" ("<<t<<L") = "<<rv.str()<<L"\r\n";
    AppendOutput(log.str(),CLR_ACCENT2);
    for (auto& b:g_bookmarks) if (b.address==addr) b.val=rv.str();
    RefreshBookmarks();
}
static void DoMemWrite() {
    if (!g_hProcess){ AppendOutput(L"[WARN] Not attached\r\n",CLR_WARNING); return; }
    uintptr_t addr=(uintptr_t)wcstoull(GetWndText(g_hAddr).c_str(),nullptr,16);
    wchar_t tb[32]{}; SendMessageW(g_hType,CB_GETLBTEXT,SendMessageW(g_hType,CB_GETCURSEL,0,0),(LPARAM)tb);
    std::wstring t=tb; std::wstring vs=GetWndText(g_hVal);
    bool ok=false;
    if      (t==L"int32")  ok=MemWrite<int32_t>(addr,(int32_t)_wtoi(vs.c_str()));
    else if (t==L"int64")  ok=MemWrite<int64_t>(addr,(int64_t)_wtoi64(vs.c_str()));
    else if (t==L"float")  ok=MemWrite<float>  (addr,(float)_wtof(vs.c_str()));
    else if (t==L"double") ok=MemWrite<double> (addr,_wtof(vs.c_str()));
    else if (t==L"byte")   ok=MemWrite<uint8_t>(addr,(uint8_t)_wtoi(vs.c_str()));
    else if (t==L"string") { auto s=Utf8FromWide(vs); ok=MemRawWrite(addr,s.c_str(),s.size()+1); }
    std::wostringstream log; log<<(ok?L"[W OK] ":L"[W FAIL] ")<<L"0x"<<std::hex<<std::uppercase<<addr<<L" ← "<<vs<<L"\r\n";
    AppendOutput(log.str(),ok?CLR_SUCCESS:CLR_ERROR);
}

// ─────────────────────────────────────────────────────────────────────────────
//  WndProc
// ─────────────────────────────────────────────────────────────────────────────
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_CREATE: {
        HINSTANCE hi=((CREATESTRUCT*)lp)->hInstance;

        // Fonts
        g_fMono=CreateFontW(-14,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FIXED_PITCH|FF_MODERN,L"Consolas");
        g_fUI  =CreateFontW(-13,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        g_fBold=CreateFontW(-13,0,0,0,FW_BOLD,  0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        g_brBg=CreateSolidBrush(CLR_BG);

        auto F=[&](HFONT f,HWND h){ SendMessageW(h,WM_SETFONT,(WPARAM)f,FALSE); return h; };

        // Left column: process list
        g_hProc=F(g_fMono,CreateWindowExW(WS_EX_CLIENTEDGE,L"LISTBOX",nullptr,
            WS_CHILD|WS_VISIBLE|LBS_NOTIFY|WS_VSCROLL|LBS_HASSTRINGS,
            8,30,308,190,hwnd,(HMENU)ID_PROC_LIST,hi,nullptr));

        auto Btn=[&](const wchar_t* t,int x,int y,int w,int h,int id)->HWND{
            return F(g_fUI,CreateWindowExW(0,L"BUTTON",t,WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_FLAT,
                x,y,w,h,hwnd,(HMENU)(intptr_t)id,hi,nullptr));};

        Btn(L"⟳",8,228,40,24,ID_REFRESH); Btn(L"Attach",52,228,80,24,ID_ATTACH);
        Btn(L"Detach",136,228,70,24,ID_DETACH);

        // Memory inspector
        F(g_fUI,CreateWindowExW(0,L"STATIC",L"Address",WS_CHILD|WS_VISIBLE,8,262,60,14,hwnd,nullptr,hi,nullptr));
        g_hAddr=F(g_fMono,CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"0x00000000",
            WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,8,278,148,22,hwnd,(HMENU)ID_ADDR,hi,nullptr));

        F(g_fUI,CreateWindowExW(0,L"STATIC",L"Type",WS_CHILD|WS_VISIBLE,162,262,36,14,hwnd,nullptr,hi,nullptr));
        g_hType=F(g_fUI,CreateWindowExW(0,L"COMBOBOX",nullptr,
            WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|CBS_HASSTRINGS,162,278,88,120,hwnd,(HMENU)ID_TYPE,hi,nullptr));
        for (auto* s:{L"int32",L"int64",L"float",L"double",L"byte",L"string"})
            SendMessageW(g_hType,CB_ADDSTRING,0,(LPARAM)s);
        SendMessageW(g_hType,CB_SETCURSEL,0,0);

        F(g_fUI,CreateWindowExW(0,L"STATIC",L"Value",WS_CHILD|WS_VISIBLE,256,262,48,14,hwnd,nullptr,hi,nullptr));
        g_hVal=F(g_fMono,CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",nullptr,
            WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,256,278,60,22,hwnd,(HMENU)ID_VAL,hi,nullptr));

        Btn(L"Read",8,306,70,22,ID_MEM_READ);
        Btn(L"Write",82,306,70,22,ID_MEM_WRITE);
        Btn(L"+ Bkmk",156,306,80,22,ID_BOOKMARK);

        // Bookmark ListView
        F(g_fUI,CreateWindowExW(0,L"STATIC",L"Bookmarks",WS_CHILD|WS_VISIBLE,8,332,120,14,hwnd,nullptr,hi,nullptr));
        g_hMemTbl=F(g_fMono,CreateWindowExW(WS_EX_CLIENTEDGE,WC_LISTVIEWW,nullptr,
            WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_FULLROWSELECT|LVS_SHOWSELALWAYS,
            8,348,308,110,hwnd,(HMENU)ID_MEM_TABLE,hi,nullptr));
        ListView_SetExtendedListViewStyle(g_hMemTbl,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER);
        {
            LVCOLUMNW c{}; c.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;
            auto ac=[&](const wchar_t* h,int w,int s){ c.pszText=const_cast<LPWSTR>(h); c.cx=w; c.iSubItem=s; ListView_InsertColumn(g_hMemTbl,s,&c); };
            ac(L"Label",82,0); ac(L"Address",92,1); ac(L"Type",56,2); ac(L"Value",72,3);
        }

        // Right column: script editor
        g_hExample=F(g_fUI,CreateWindowExW(0,L"COMBOBOX",nullptr,
            WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|CBS_HASSTRINGS,
            322,8,200,200,hwnd,(HMENU)ID_EXAMPLE_COMBO,hi,nullptr));
        for (auto& e:k_ex) SendMessageW(g_hExample,CB_ADDSTRING,0,(LPARAM)e.name);
        SendMessageW(g_hExample,CB_SETCURSEL,0,0);
        Btn(L"Load",526,8,50,22,ID_LOAD_EX);

        g_hScript=F(g_fMono,CreateWindowExW(WS_EX_CLIENTEDGE,RICHEDIT_CLASSW,nullptr,
            WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_WANTRETURN|ES_AUTOVSCROLL|WS_VSCROLL|WS_HSCROLL,
            322,34,644,262,hwnd,(HMENU)ID_SCRIPT,hi,nullptr));
        SendMessageW(g_hScript,EM_SETBKGNDCOLOR,0,(LPARAM)CLR_EDITOR_BG);
        SendMessageW(g_hScript,EM_SETLIMITTEXT,0,0);
        {
            CHARFORMATW cf{}; cf.cbSize=sizeof(cf); cf.dwMask=CFM_COLOR|CFM_FACE|CFM_SIZE;
            cf.crTextColor=CLR_TEXT; cf.yHeight=200; wcscpy_s(cf.szFaceName,L"Consolas");
            SendMessageW(g_hScript,EM_SETCHARFORMAT,SCF_ALL,(LPARAM)&cf);
        }

        Btn(L"▶  Run  [F5]",322,302,130,26,ID_RUN);
        Btn(L"Clear Script",458,302,100,26,ID_CLR_SCR);
        Btn(L"Clear Output",562,302,100,26,ID_CLR_OUT);

        // Output pane
        g_hOutput=F(g_fMono,CreateWindowExW(WS_EX_CLIENTEDGE,RICHEDIT_CLASSW,nullptr,
            WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,
            322,334,644,268,hwnd,(HMENU)ID_OUTPUT,hi,nullptr));
        SendMessageW(g_hOutput,EM_SETBKGNDCOLOR,0,(LPARAM)CLR_OUTPUT_BG);
        SendMessageW(g_hOutput,EM_SETLIMITTEXT,0,0);
        {
            CHARFORMATW cf{}; cf.cbSize=sizeof(cf); cf.dwMask=CFM_COLOR|CFM_FACE|CFM_SIZE;
            cf.crTextColor=CLR_TEXT; cf.yHeight=180; wcscpy_s(cf.szFaceName,L"Consolas");
            SendMessageW(g_hOutput,EM_SETCHARFORMAT,SCF_ALL,(LPARAM)&cf);
        }

        // Status bar
        g_hStatus=F(g_fUI,CreateWindowExW(0,L"STATIC",L"Not attached",
            WS_CHILD|WS_VISIBLE|SS_LEFT,0,612,1200,20,hwnd,(HMENU)ID_STATUS,hi,nullptr));

        LuaInit();
        RefreshProcs();
        LoadExample(0);
        AppendOutput(L"RPG Debugger ready. Attach a process, then press F5.\r\n\r\n",CLR_ACCENT2);
        return 0;
    }

    case WM_COMMAND: {
        int id=LOWORD(wp);
        if (id==ID_REFRESH){ RefreshProcs(); break; }
        if (id==ID_ATTACH) {
            int i=(int)SendMessageW(g_hProc,LB_GETCURSEL,0,0);
            if (i!=LB_ERR) Attach((DWORD)SendMessageW(g_hProc,LB_GETITEMDATA,i,0));
            else AppendOutput(L"[WARN] Select a process first.\r\n",CLR_WARNING);
            break;
        }
        if (id==ID_DETACH){ Detach(); SetStatus(L"Detached"); AppendOutput(L"[INFO] Detached.\r\n",CLR_TEXT_DIM); break; }
        if (id==ID_RUN) {
            auto ws=GetWndText(g_hScript);
            auto code=Utf8FromWide(ws);
            AppendOutput(L"── run ──────────────────────────────────\r\n",CLR_TEXT_DIM);
            bool ok=LuaRun(code);
            AppendOutput(ok?L"── ok ───────────────────────────────────\r\n"
                           :L"── error ────────────────────────────────\r\n",
                         ok?CLR_TEXT_DIM:CLR_ERROR);
            break;
        }
        if (id==ID_CLR_SCR){ SetWindowTextW(g_hScript,L""); break; }
        if (id==ID_CLR_OUT){ SetWindowTextW(g_hOutput,L""); break; }
        if (id==ID_MEM_READ ){ DoMemRead();  break; }
        if (id==ID_MEM_WRITE){ DoMemWrite(); break; }
        if (id==ID_BOOKMARK) {
            std::wstring addrTxt=GetWndText(g_hAddr);
            uintptr_t addr=(uintptr_t)wcstoull(addrTxt.c_str(),nullptr,16);
            wchar_t tb[32]{}; SendMessageW(g_hType,CB_GETLBTEXT,SendMessageW(g_hType,CB_GETCURSEL,0,0),(LPARAM)tb);
            Bookmark b; b.address=addr; b.type=tb;
            b.label=L"bm"+std::to_wstring(g_bookmarks.size()+1);
            b.val=GetWndText(g_hVal);
            g_bookmarks.push_back(b); RefreshBookmarks(); break;
        }
        if (id==ID_LOAD_EX){ LoadExample((int)SendMessageW(g_hExample,CB_GETCURSEL,0,0)); break; }
        break;
    }

    case WM_NOTIFY: {
        NMHDR* nm=(NMHDR*)lp;
        if (nm->idFrom==ID_MEM_TABLE) {
            if (nm->code==NM_DBLCLK) {
                NMITEMACTIVATE* nm2=(NMITEMACTIVATE*)lp;
                if (nm2->iItem>=0&&nm2->iItem<(int)g_bookmarks.size()) {
                    auto& b=g_bookmarks[nm2->iItem];
                    std::wostringstream a; a<<L"0x"<<std::hex<<std::uppercase<<b.address;
                    SetWindowTextW(g_hAddr,a.str().c_str());
                    int cnt=(int)SendMessageW(g_hType,CB_GETCOUNT,0,0);
                    for (int i=0;i<cnt;i++){
                        wchar_t buf[32]{}; SendMessageW(g_hType,CB_GETLBTEXT,i,(LPARAM)buf);
                        if (b.type==buf){ SendMessageW(g_hType,CB_SETCURSEL,i,0); break; }
                    }
                }
            }
            if (nm->code==NM_CUSTOMDRAW) {
                NMLVCUSTOMDRAW* cd=(NMLVCUSTOMDRAW*)lp;
                if (cd->nmcd.dwDrawStage==CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
                if (cd->nmcd.dwDrawStage==CDDS_ITEMPREPAINT) {
                    cd->clrText=CLR_TEXT;
                    cd->clrTextBk=(cd->nmcd.dwItemSpec%2==0)?CLR_PANEL:CLR_EDITOR_BG;
                    return CDRF_NEWFONT;
                }
            }
        }
        return 0;
    }

    case WM_SIZE: {
        int W=LOWORD(lp), H=HIWORD(lp);
        if (g_hScript) MoveWindow(g_hScript,322,34,W-330,262,TRUE);
        if (g_hOutput) MoveWindow(g_hOutput,322,334,W-330,H-358,TRUE);
        if (g_hMemTbl) MoveWindow(g_hMemTbl,8,348,308,H-478,TRUE);
        if (g_hStatus) MoveWindow(g_hStatus,0,H-20,W,20,TRUE);
        return 0;
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc=(HDC)wp; SetTextColor(hdc,CLR_TEXT); SetBkColor(hdc,CLR_EDITOR_BG);
        return (LRESULT)CreateSolidBrush(CLR_EDITOR_BG);
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc=(HDC)wp; SetTextColor(hdc,CLR_TEXT_DIM); SetBkColor(hdc,CLR_BG);
        return (LRESULT)g_brBg;
    }
    case WM_ERASEBKGND: {
        RECT r; GetClientRect(hwnd,&r); FillRect((HDC)wp,&r,g_brBg); return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc=BeginPaint(hwnd,&ps);
        // Accent strip
        RECT top={0,0,9999,2}; HBRUSH ab=CreateSolidBrush(CLR_ACCENT);
        FillRect(hdc,&top,ab); DeleteObject(ab);
        // Section headings
        SelectObject(hdc,g_fBold); SetBkMode(hdc,TRANSPARENT);
        SetTextColor(hdc,CLR_ACCENT);
        TextOutW(hdc,8, 12,L"PROCESS",7);
        TextOutW(hdc,8,250,L"MEMORY WATCH",12);
        TextOutW(hdc,8,330,L"BOOKMARKS",9);
        TextOutW(hdc,322,18,L"SCRIPT EDITOR",13);
        TextOutW(hdc,322,318,L"OUTPUT",6);
        EndPaint(hwnd,&ps); return 0;
    }
    case WM_DESTROY:
        Detach();
        if (g_L){ lua_close(g_L); g_L=nullptr; }
        if (g_brBg) DeleteObject(g_brBg);
        if (g_fMono) DeleteObject(g_fMono);
        if (g_fUI)   DeleteObject(g_fUI);
        if (g_fBold) DeleteObject(g_fBold);
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}

// ─────────────────────────────────────────────────────────────────────────────
//  WinMain
// ─────────────────────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    LoadLibraryW(L"Msftedit.dll");
    INITCOMMONCONTROLSEX icc{sizeof(icc),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{};
    wc.cbSize=sizeof(wc); wc.style=CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc=WndProc; wc.hInstance=hInst;
    wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.lpszClassName=L"RPGDbg";
    wc.hIconSm=wc.hIcon;
    RegisterClassExW(&wc);

    g_hwnd=CreateWindowExW(WS_EX_APPWINDOW,L"RPGDbg",
        L"⚔  RPG Debugger  —  Memory & Lua Console",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,990,660,
        nullptr,nullptr,hInst,nullptr);
    ShowWindow(g_hwnd,nShow); UpdateWindow(g_hwnd);

    MSG msg{};
    while (GetMessageW(&msg,nullptr,0,0)) {
        if (msg.message==WM_KEYDOWN && msg.wParam==VK_F5)
            PostMessageW(g_hwnd,WM_COMMAND,ID_RUN,0);
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
