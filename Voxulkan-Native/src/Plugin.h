#pragma once

#include <string>

#define SAFE_DELETE_ARR(s) if(s) delete[] s; s = nullptr
#define SAFE_DELETE(s) if(s) delete s; s = nullptr
#define SAFE_DELETE_RES(s) if(s){ s->Release(); delete s;} s = nullptr
#define EXPORT extern "C" __declspec(dllexport)

void Log(const std::string& msg);

#ifdef NDEBUG
#define LOG(msg) ((void)0)
#define VK_CALL(call) call
#else
#define LOG(msg) Log(msg) 
#define VK_CALL(call) if(call != VK_SUCCESS){LOG(std::string(#call) + ": Vulkan call failed on line " + std::to_string(__LINE__) + " in file " + std::string(__FILE__));} 
#endif