// pch.h
#pragma once
#define _WIN32_WINNT 0x0601 // windows 7+

#include <windows.h>
#include <wtsapi32.h>
#include <tlhelp32.h>
#include <rpc.h>
#include <rpcdce.h>

#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <algorithm>
