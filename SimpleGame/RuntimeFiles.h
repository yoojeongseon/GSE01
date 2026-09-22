#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <string>

inline std::wstring RuntimeDirectory(const wchar_t* name)
{
    wchar_t executable[32768] = {};
    DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    if (!length || length >= 32768)
    {
        return {};
    }
    std::wstring path(executable, length);
    path = path.substr(0, path.find_last_of(L"\\/") + 1) + name;
    if (!CreateDirectoryW(path.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        return {};
    }
    return path;
}
