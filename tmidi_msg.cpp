/*
 * TMIDI Messages - SSP Plugin
 * Bridges TMIDI Player(s) to SSP via DDE + PLUGIN/2.0 Script injection.
 * Supports multiple TMIDI instances.
 * Compile: cl /LD /O2 /MT /Fe:tmidi_msg.dll tmidi_msg.cpp user32.lib shell32.lib
 * SSP is 32-bit; use the x86 toolchain (run vcvars32.bat)
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddeml.h>
#include <string>
#include <vector>
#include <string.h>
#include <ctype.h>

using namespace std;

static const string UNINIT = "\x01";

struct Inst {
    HCONV conv;
    string last;
};

static vector<Inst> g_inst;

static string g_dir;
static char  g_tmpl_m[8192];  /* MIMPIWRD */
static char  g_tmpl_s[8192];  /* SherryWRD */
static char  g_tmpl_n[8192];  /* NeoWRD */
static char  g_tmpl_x[8192];  /* NoWRD */
static char *g_tmpl;
static FILETIME g_tmpl_mtime;
static string g_script;
static DWORD g_dde;

static string dde_query(HCONV c, const string &item) {
    if (!c) return {};
    WCHAR wi[256];
    MultiByteToWideChar(CP_ACP, 0, item.c_str(), -1, wi, 256);
    HSZ hi = DdeCreateStringHandleW(g_dde, wi, CP_WINUNICODE);
    if (!hi) return {};
    string out;
    HDDEDATA hd = DdeClientTransaction(NULL, 0, c, hi, CF_TEXT, XTYP_REQUEST, 3000, NULL);
    if (hd) {
        DWORD sz = DdeGetData(hd, NULL, 0, 0);
        if (sz > 0) {
            out.resize(sz);
            DdeGetData(hd, (LPBYTE)out.data(), sz, 0);
            while (!out.empty() && out.back() == '\0') out.pop_back();
        }
        DdeFreeDataHandle(hd);
    }
    DdeFreeStringHandle(g_dde, hi);
    return out;
}

extern "C" HDDEDATA CALLBACK dde_cb(UINT t, UINT f, HCONV c, HSZ h1, HSZ h2, HDDEDATA d, ULONG_PTR dw1, ULONG_PTR dw2) {
    if (t == XTYP_REGISTER) {
        WCHAR name[256];
        if (h1 && DdeQueryStringW(g_dde, h1, name, 256, CP_WINUNICODE) && !wcscmp(name, L"TMIDI")) {
            HSZ svc = DdeCreateStringHandleW(g_dde, L"TMIDI", CP_WINUNICODE);
            HSZ top = DdeCreateStringHandleW(g_dde, L"TMIDI", CP_WINUNICODE);
            HCONV conv = DdeConnect(g_dde, svc, top, NULL);
            DdeFreeStringHandle(g_dde, svc);
            DdeFreeStringHandle(g_dde, top);
            if (conv) g_inst.push_back({conv});
        }
    }
    if (t == XTYP_DISCONNECT && c) {
        for (auto it = g_inst.begin(); it != g_inst.end(); ++it)
            if (it->conv == c) { g_inst.erase(it); break; }
    }
    return (HDDEDATA)NULL;
}

static void tmpl(const string &dir) {
    char *dst[4] = {g_tmpl_m, g_tmpl_s, g_tmpl_n, g_tmpl_x};
    const char *tag[4] = {"MIMPIWRD", "SherryWRD", "NeoWRD", "NoWRD"};
    g_tmpl_m[0] = g_tmpl_s[0] = g_tmpl_n[0] = g_tmpl_x[0] = 0;
    string p = dir + "\\sstp_sample.txt";
    HANDLE f = CreateFileA(p.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { strcpy_s(g_tmpl_x, 8192, "\\0\\s[0]$title\\n\\e"); g_tmpl = g_tmpl_x; return; }
    DWORD sz = GetFileSize(f, NULL); if (sz > 65536) sz = 65536;
    vector<char> t(sz + 1);
    ReadFile(f, t.data(), sz, &sz, NULL); t[sz] = 0; CloseHandle(f);
    string sec; char *q = t.data();
    while (*q) {
        char *ln = q; while (*q && *q != '\r' && *q != '\n') q++;
        char sv = *q; *q = 0;
        if (ln[0] == '#' && strncmp(ln, "#Header", 7) && strncmp(ln, "#Message", 8)) {
            sec.assign(ln + 1);
            size_t comma = sec.find(',');
            if (comma != string::npos) sec.resize(comma);
            while (!sec.empty() && sec[0] == ' ') sec.erase(0, 1);
        }
        if (strncmp(ln, "Script:", 7) == 0) {
            for (int i = 0; i < 4; i++) {
                if (sec == tag[i] && !dst[i][0]) {
                    strncpy_s(dst[i], 8192, ln + 7, 8191);
                    char *s = dst[i]; while (*s == ' ') s++;
                    if (s != dst[i]) memmove(dst[i], s, strlen(s)+1);
                }
            }
        }
        *q = sv; if (*q) q++; if (*q == '\n') q++;
    }
    if (!g_tmpl_x[0]) strcpy_s(g_tmpl_x, 8192, "\\0\\s[0]$title\\n\\e");
    g_tmpl = g_tmpl_x[0] ? g_tmpl_x : (g_tmpl_m[0] ? g_tmpl_m : g_tmpl_x);
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (GetFileAttributesExA(p.c_str(), GetFileExInfoStandard, &fa))
        g_tmpl_mtime = fa.ftLastWriteTime;
    else
        memset(&g_tmpl_mtime, 0, sizeof(g_tmpl_mtime));
}

static void check_tmpl(void) {
    string p = g_dir + "\\sstp_sample.txt";
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (GetFileAttributesExA(p.c_str(), GetFileExInfoStandard, &fa)) {
        if (CompareFileTime(&fa.ftLastWriteTime, &g_tmpl_mtime) != 0)
            tmpl(g_dir);
    }
}

static void select_tmpl(const string &path) {
    string base = path;
    size_t dot = base.rfind('.');
    if (dot == string::npos) { g_tmpl = g_tmpl_x[0] ? g_tmpl_x : g_tmpl_m; return; }
    base.resize(dot);
    string test = base + ".nrd";
    if (GetFileAttributesA(test.c_str()) != INVALID_FILE_ATTRIBUTES && g_tmpl_n[0])
        { g_tmpl = g_tmpl_n; return; }
    test = base + ".sry";
    if (GetFileAttributesA(test.c_str()) != INVALID_FILE_ATTRIBUTES && g_tmpl_s[0])
        { g_tmpl = g_tmpl_s; return; }
    test = base + ".wrd";
    if (GetFileAttributesA(test.c_str()) != INVALID_FILE_ATTRIBUTES && g_tmpl_m[0])
        { g_tmpl = g_tmpl_m; return; }
    g_tmpl = g_tmpl_x[0] ? g_tmpl_x : g_tmpl_m;
}

static string acp2utf8(const string &in) {
    if (in.empty()) return "Unknown";
    WCHAR w[256] = {0};
    MultiByteToWideChar(CP_ACP, 0, in.c_str(), -1, w, 256);
    char out[256] = {0};
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out, 256, NULL, NULL);
    return out;
}

static string fmt_ext(HCONV c, const string &pf) {
    string fn = dde_query(c, "getfilename " + pf);
    if (fn.empty()) return "MIDI";
    size_t dot = fn.rfind('.');
    if (dot == string::npos) return "MIDI";
    string ext = fn.substr(dot + 1);
    for (auto &ch : ext) ch = (char)toupper((unsigned char)ch);
    if (ext == "MID" || ext == "SMF") return "SMF";
    if (ext == "RCP") return "RCP";
    if (ext == "R36") return "R36";
    if (ext == "G36") return "G36";
    if (ext == "G18") return "G18";
    if (ext == "WAV") return "WAV";
    if (ext == "MP3") return "MP3";
    return ext;
}

static string build_script(const string &title, const string &format) {
    string out;
    out.reserve(8192);
    const char *t = g_tmpl;
    while (*t) {
        if (!strncmp(t, "$title", 6)) {
            for (const char *u = title.c_str(); *u; u++) {
                if (*u == '\\') out += "\\\\";
                else out += *u;
            }
            t += 6;
        } else if (!strncmp(t, "$format", 7)) {
            out += format;
            t += 7;
        } else {
            out += *t++;
        }
    }
    return out;
}

static void poll(void) {
    g_script.clear();
    check_tmpl();

    for (auto &inst : g_inst) {
        string st = dde_query(inst.conv, "getstatus");
        if (st.empty()) { DdeDisconnect(inst.conv); inst.conv = NULL; continue; }
        if (st == "play") {
            string tr = dde_query(inst.conv, "getplayfile");
            if (tr.empty()) tr = "0";
            string fn = dde_query(inst.conv, "getfilename " + tr);
            select_tmpl(fn);
            if (inst.last == UNINIT) {
                inst.last = fn;
                continue;
            }
            if (fn != inst.last) {
                string ra = dde_query(inst.conv, "gettitle " + tr);
                string ti = acp2utf8(ra);
                string fmt = fmt_ext(inst.conv, tr);
                g_script = build_script(ti, fmt);
                inst.last = fn;
                break;
            }
        } else if (st == "stop") {
            inst.last.clear();
        }
    }
    // Compact dead entries
    for (auto it = g_inst.begin(); it != g_inst.end(); ) {
        if (it->conv) ++it;
        else it = g_inst.erase(it);
    }
}

extern "C" __declspec(dllexport) BOOL __cdecl load(HGLOBAL h, long len) {
    if (h && len > 0) {
        const char *p = (const char*)h;
        int l = len < MAX_PATH - 1 ? len : MAX_PATH - 1;
        g_dir.assign(p, l);
        while (!g_dir.empty() && g_dir.back() == '\\') g_dir.pop_back();
    }
    if (h) GlobalFree(h);
    tmpl(g_dir);
    if (DdeInitializeW(&g_dde, dde_cb, APPCLASS_STANDARD, 0L) != DMLERR_NO_ERROR) return TRUE;
    {
        HSZ svc = DdeCreateStringHandleW(g_dde, L"TMIDI", CP_WINUNICODE);
        HSZ top = DdeCreateStringHandleW(g_dde, L"TMIDI", CP_WINUNICODE);
        HCONVLIST list = DdeConnectList(g_dde, svc, top, NULL, NULL);
        if (list) {
            HCONV prev = NULL;
            while (1) {
                HCONV conv = DdeQueryNextServer(list, prev);
                if (!conv) break;
                g_inst.push_back({conv, UNINIT});
                prev = conv;
            }
        }
        DdeFreeStringHandle(g_dde, svc);
        DdeFreeStringHandle(g_dde, top);
    }
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL __cdecl unload(void) {
    for (auto &inst : g_inst) if (inst.conv) DdeDisconnect(inst.conv);
    g_inst.clear();
    if (g_dde) { DdeUninitialize(g_dde); g_dde = 0; }
    return TRUE;
}

extern "C" __declspec(dllexport) HGLOBAL __cdecl request(HGLOBAL h, long *len) {
    if (!h) return NULL;
    char *req = (char*)h; int rl = len ? *len : (int)strlen(req);
    string id;
    char *p = req;
    while (p - req < rl && *p != '\r') p++;
    if (p - req < rl && *p == '\r') p += 2;
    while (p - req < rl && *p != '\r') {
        char *le = p; while (le - req < rl && *le != '\r') le++;
        if (le - req >= rl) break;
        char sv = *le; *le = 0;
        const char *col = strchr(p, ':');
        if (col) {
            int kl = (int)(col - p); if (kl > 127) kl = 127;
            string k(p, kl);
            if (k == "ID") { const char *v = col + 1; while (*v == ' ' || *v == '\t') v++; id = v; }
        }
        *le = sv; p = le; if (*p == '\r') p += 2;
    }
    GlobalFree(h);
    if (id == "OnSecondChange") poll();
    string rs;
    if (id == "version")
        rs = "PLUGIN/2.0 200 OK\r\nCharset: UTF-8\r\nValue: 1.0.0\r\n\r\n";
    else if (id == "OnSecondChange" && !g_script.empty()) {
        rs = "PLUGIN/2.0 200 OK\r\nCharset: UTF-8\r\nScript: " + g_script + "\r\n\r\n";
        g_script.clear();
    } else
        rs = "PLUGIN/2.0 200 OK\r\nCharset: UTF-8\r\n\r\n";
    int sz = (int)rs.size() + 1;
    HGLOBAL ret = GlobalAlloc(GMEM_FIXED, sz);
    if (ret) memcpy(ret, rs.c_str(), sz);
    if (len) *len = sz;
    return ret;
}

extern "C" BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID v) { return TRUE; }
