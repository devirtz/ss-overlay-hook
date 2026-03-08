#pragma once
#include <Windows.h>

inline const char* logPath()
{
    static char path[MAX_PATH];
    if (!path[0]) { GetTempPathA(MAX_PATH, path); lstrcatA(path, "hook_log.txt"); }
    return path;
}

inline void logWrite(const char* s, DWORD len = 0)
{
    if (!len) len = (DWORD)lstrlenA(s);
    static HANDLE hMutex = CreateMutexA(nullptr, FALSE, "Global\\hook_log_mtx");
    WaitForSingleObject(hMutex, INFINITE);
    HANDLE hf = CreateFileA(logPath(), GENERIC_WRITE, FILE_SHARE_READ,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf != INVALID_HANDLE_VALUE)
    {
        SetFilePointer(hf, 0, nullptr, FILE_END);
        DWORD w; WriteFile(hf, s, len, &w, nullptr);
        CloseHandle(hf);
    }
    ReleaseMutex(hMutex);
}

struct Log
{
    char   buf[512] = {};
    int    pos      = 0;
    bool   hex      = false;

    ~Log() { logWrite(buf, (DWORD)pos); }

    Log& operator<<(const char* s)
    {
        if (!s) return *this;
        int n = lstrlenA(s);
        if (pos + n < (int)sizeof(buf) - 1) { memcpy(buf + pos, s, n); pos += n; }
        return *this;
    }

    Log& operator<<(char c)
    {
        if (pos + 1 < (int)sizeof(buf) - 1) buf[pos++] = c;
        return *this;
    }

    // Numeric overloads — manual conversion, no CRT printf needed
    Log& putUInt(unsigned long long v)
    {
        char t[24] = {}; int i = 22;
        if (hex) {
            const char* d = "0123456789abcdef";
            if (!v) t[i--] = '0';
            else do { t[i--] = d[v & 0xF]; v >>= 4; } while (v);
        } else {
            if (!v) t[i--] = '0';
            else do { t[i--] = (char)('0' + v % 10); v /= 10; } while (v);
        }
        return *this << (t + i + 1);
    }
    Log& putInt(long long v)
    {
        if (hex) return putUInt((unsigned long long)v);
        char t[24] = {}; int i = 22;
        bool neg = v < 0;
        unsigned long long u = neg ? ~(unsigned long long)v + 1 : (unsigned long long)v;
        if (!u) t[i--] = '0';
        else do { t[i--] = (char)('0' + u % 10); u /= 10; } while (u);
        if (neg) t[i--] = '-';
        return *this << (t + i + 1);
    }

    Log& operator<<(int v)               { return putInt(v); }
    Log& operator<<(unsigned int v)      { return putUInt(v); }
    Log& operator<<(long v)              { return putInt(v); }
    Log& operator<<(unsigned long v)     { return putUInt(v); }
    Log& operator<<(long long v)         { return putInt(v); }
    Log& operator<<(unsigned long long v){ return putUInt(v); }
    Log& operator<<(void* v)             { char t[32]; wsprintfA(t, "%p", v); return *this << t; }

    // Hex/dec toggle
    struct _Hex{};
    struct _Dec{};
    Log& operator<<(_Hex) { hex = true;  return *this; }
    Log& operator<<(_Dec) { hex = false; return *this; }
};

inline Log::_Hex LogHex() { return {}; }
inline Log::_Dec LogDec() { return {}; }
