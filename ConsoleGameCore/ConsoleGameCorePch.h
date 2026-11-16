#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif



#include <Windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

// 안정적인 공통 인터페이스만 포함한다. 구체 클래스와 생성 데이터는 .cpp에서 포함한다.
#include "Objects/IObject.h"

#include "Input/InputTypes.h"
#include "Input/IInputSource.h"

#include "UI/Common/IUI.h"

#include "UI/IUiInputTarget.h"
#include "Scene/FSceneLifetime.h"
