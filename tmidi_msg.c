/*
 * TMIDI Messages - SSP Plugin
 * Bridges TMIDI Player(s) to SSP via DDE + PLUGIN/2.0 Script injection.
 * Supports multiple TMIDI instances.
 * Compile: cl /LD /O2 /MD /Fe:tmidi_msg.dll tmidi_msg.c user32.lib
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddeml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

__declspec(dllexport) BOOL  __cdecl load(HGLOBAL h, long len);
__declspec(dllexport) BOOL  __cdecl unload(void);
__declspec(dllexport) HGLOBAL __cdecl request(HGLOBAL h, long *len);

#define MAX_INST 8

static char  g_dir[MAX_PATH];
static char  g_tmpl[8192];
static char  g_script[8192];
static DWORD g_dde;
static HCONV g_c[MAX_INST];
static int   g_n;
static int   g_was[MAX_INST];
static int   g_ok;

static HDDEDATA CALLBACK cb(UINT t, UINT f, HCONV c, HSZ h1, HSZ h2, HDDEDATA d, ULONG_PTR dw1, ULONG_PTR dw2)
    { return (HDDEDATA)NULL; }

static int dde(HCONV c, const char *item, char *out, int max) {
    out[0] = 0; if (!c) return 0;
    WCHAR wi[256]; MultiByteToWideChar(CP_ACP, 0, item, -1, wi, 256);
    HSZ hi = DdeCreateStringHandleW(g_dde, wi, CP_WINUNICODE);
    if (!hi) return 0;
    HDDEDATA hd = DdeClientTransaction(NULL, 0, c, hi, CF_TEXT, XTYP_REQUEST, 3000, NULL);
    if (hd) { DdeGetData(hd, (LPBYTE)out, max-1, 0); DdeFreeDataHandle(hd); }
    DdeFreeStringHandle(g_dde, hi);
    return out[0] ? 1 : 0;
}

static void refresh(void) {
    for (int i = 0; i < g_n; i++) if (g_c[i]) DdeDisconnect(g_c[i]);
    g_n = 0; memset(g_c, 0, sizeof(g_c)); memset(g_was, 0, sizeof(g_was));
    HSZ svc = DdeCreateStringHandleW(g_dde, L"TMIDI", CP_WINUNICODE);
    HSZ top = DdeCreateStringHandleW(g_dde, L"TMIDI", CP_WINUNICODE);
    HCONVLIST list = DdeConnectList(g_dde, svc, top, NULL, NULL);
    if (list) {
        HCONV prev = NULL;
        while (g_n < MAX_INST) {
            HCONV conv = DdeQueryNextServer(list, prev);
            if (!conv) break;
            g_c[g_n++] = conv; prev = conv;
        }
    }
    DdeFreeStringHandle(g_dde, svc);
    DdeFreeStringHandle(g_dde, top);
}

static void tmpl(const char *dir) {
    char p[MAX_PATH]; sprintf_s(p, sizeof(p), "%s\\sstp_sample.txt", dir);
    HANDLE f = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { strcpy_s(g_tmpl, sizeof(g_tmpl), "\\0\\s[0]$title\\n\\e"); return; }
    DWORD sz = GetFileSize(f, NULL); if (sz > 65536) sz = 65536;
    char *t = (char*)malloc(sz + 1); if (!t) { CloseHandle(f); return; }
    ReadFile(f, t, sz, &sz, NULL); t[sz] = 0; CloseHandle(f);
    char sec[64] = {0}, *q = t;
    while (*q) {
        char *ln = q; while (*q && *q != '\r' && *q != '\n') q++;
        char sv = *q; *q = 0;
        if (ln[0] == '#' && strncmp(ln, "#Header", 7) && strncmp(ln, "#Message", 8)) {
            strncpy_s(sec, sizeof(sec), ln + 1, 63);
            char *c = strchr(sec, ','); if (c) *c = 0;
            char *ts = sec; while (*ts == ' ') ts++;
            if (ts != sec) memmove(sec, ts, strlen(ts)+1);
        }
        if (strncmp(ln, "Script:", 7) == 0 && strcmp(sec, "MIMPIWRD") == 0) {
            strncpy_s(g_tmpl, sizeof(g_tmpl), ln + 7, 8191);
            char *s = g_tmpl; while (*s == ' ') s++;
            if (s != g_tmpl) memmove(g_tmpl, s, strlen(s)+1);
        }
        *q = sv; if (*q) q++; if (*q == '\n') q++;
    }
    free(t);
    if (!g_tmpl[0]) strcpy_s(g_tmpl, sizeof(g_tmpl), "\\0\\s[0]$title\\n\\e");
}

static void acp2utf8(const char *in, char *out, int max) {
    if (!in[0]) { strcpy_s(out, max, "Unknown"); return; }
    WCHAR w[256] = {0};
    MultiByteToWideChar(CP_ACP, 0, in, -1, w, 256);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out, max, NULL, NULL);
}

static void build_script(const char *title) {
    char *s = g_script, *t = g_tmpl;
    memset(g_script, 0, sizeof(g_script));
    while (*t && (size_t)(s - g_script) < sizeof(g_script) - 1) {
        if (!strncmp(t, "$title", 6))
            { strcpy_s(s, sizeof(g_script)-(s-g_script), title); s += strlen(title); t += 6; }
        else if (!strncmp(t, "$format", 7))
            { strcpy_s(s, sizeof(g_script)-(s-g_script), "MIDI"); s += 4; t += 7; }
        else if (!strncmp(t, "$target", 7) || !strncmp(t, "$module", 7))
            { t += 7; }
        else if (!strncmp(t, "%me", 3))
            { strcpy_s(s, sizeof(g_script)-(s-g_script), "Emily"); s += 5; t += 3; }
        else *s++ = *t++;
    }
    *s = 0;
}

static void poll(void) {
    g_script[0] = 0;
    if (!g_ok) return;
    if (g_n == 0) refresh();
    for (int i = 0; i < g_n; i++) {
        if (!g_c[i]) continue;
        char st[256] = {0};
        if (dde(g_c[i], "getstatus", st, sizeof(st)) && !strcmp(st, "play")) {
            if (!g_was[i]) {
                char tr[16] = {0}; dde(g_c[i], "getplayfile", tr, sizeof(tr));
                char rq[64]; sprintf_s(rq, sizeof(rq), "gettitle %s", tr[0] ? tr : "0");
                char ra[256] = {0}, ti[256]; dde(g_c[i], rq, ra, sizeof(ra));
                acp2utf8(ra, ti, sizeof(ti));
                build_script(ti); g_was[i] = 1;
                break;
            }
        } else if (!strcmp(st, "stop")) { g_was[i] = 0; }
        /* Dead connection? */
        if (!st[0]) { DdeDisconnect(g_c[i]); g_c[i] = NULL; }
    }
    /* Compact */
    int j = 0;
    for (int i = 0; i < g_n; i++) if (g_c[i]) {
        if (i != j) { g_c[j] = g_c[i]; g_was[j] = g_was[i]; }
        j++;
    }
    g_n = j;
}

BOOL __cdecl load(HGLOBAL h, long len) {
    if (h && len > 0) {
        int l = len < MAX_PATH-1 ? len : MAX_PATH-1;
        memcpy(g_dir, (char*)h, l); g_dir[l] = 0;
        int sl = (int)strlen(g_dir); while (sl > 0 && g_dir[sl-1] == '\\') g_dir[--sl] = 0;
    }
    if (h) GlobalFree(h);
    tmpl(g_dir);
    if (DdeInitializeW(&g_dde, cb, APPCLASS_STANDARD, 0L) != DMLERR_NO_ERROR) return TRUE;
    g_ok = 1;
    return TRUE;
}

BOOL __cdecl unload(void) {
    g_ok = 0;
    for (int i = 0; i < g_n; i++) if (g_c[i]) DdeDisconnect(g_c[i]);
    g_n = 0;
    if (g_dde) { DdeUninitialize(g_dde); g_dde = 0; }
    return TRUE;
}

HGLOBAL __cdecl request(HGLOBAL h, long *len) {
    if (!h) return NULL;
    char *req = (char*)h; int rl = len ? *len : (int)strlen(req);
    char id[128] = {0};
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
            char k[128]; memcpy(k, p, kl); k[kl] = 0;
            if (!strcmp(k, "ID")) { const char *v = col + 1; while (*v == ' ' || *v == '\t') v++; strcpy_s(id, sizeof(id), v); }
        }
        *le = sv; p = le; if (*p == '\r') p += 2;
    }
    GlobalFree(h);
    if (!strcmp(id, "OnSecondChange")) poll();
    char rs[10240];
    if (!strcmp(id, "version"))
        sprintf_s(rs, sizeof(rs), "PLUGIN/2.0 200 OK\r\nCharset: UTF-8\r\nValue: 1.0.0\r\n\r\n");
    else if (!strcmp(id, "OnSecondChange") && g_script[0]) {
        sprintf_s(rs, sizeof(rs), "PLUGIN/2.0 200 OK\r\nCharset: UTF-8\r\nScript: %s\r\n\r\n", g_script);
        g_script[0] = 0;
    } else
        sprintf_s(rs, sizeof(rs), "PLUGIN/2.0 200 OK\r\nCharset: UTF-8\r\n\r\n");
    int sz = (int)strlen(rs) + 1;
    HGLOBAL ret = GlobalAlloc(GMEM_FIXED, sz);
    if (ret) memcpy(ret, rs, sz);
    if (len) *len = sz;
    return ret;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID v) { return TRUE; }
