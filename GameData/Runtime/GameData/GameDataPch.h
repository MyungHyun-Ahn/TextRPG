#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <dpapi.h>
#include <bcrypt.h>

// 공통 Windows SDK의 고정 링크 의존성도 PCH에서 한 번 선언한다.
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

// 타깃별 PCH는 안정적인 표준/SDK 정의만 공유한다. 주입과 구체 타입은 소비 .cpp에 둔다.
#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <new>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
