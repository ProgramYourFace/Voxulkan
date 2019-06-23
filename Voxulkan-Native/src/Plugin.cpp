#include "Plugin.h"
#include "IUnityInterface.h"
#include "IUnityGraphics.h"
#include "Engine.h"
#include <ctime>
#include <sstream>

#pragma warning(disable : 4996)

typedef void(*logMSG_t) (const char* msg);
logMSG_t logMsg;
std::string fallback;
void Log(const std::string& msg) {
	if (logMsg)
	{
		(*logMsg)((fallback + msg).c_str());
		fallback = "";
	}
	else
	{
		std::time_t result = std::time(nullptr);
		tm* lt = std::localtime(&result);
		std::stringstream ss;
		ss << "[" << lt->tm_hour << ":" << lt->tm_min << ":" << lt->tm_sec << "] ";
		fallback += ss.str() + msg + "\n";
	}
}

static IUnityGraphics* s_Graphics = NULL;

EXPORT bool IsInitialized()
{
	return Engine::Get().IsInitialized();
}

EXPORT void RegisterLogCallback(logMSG_t logCallback)
{
	if (logCallback)
	{
		logMsg = logCallback;
		LOG("LOG REGISTERED");
	}
	else
	{
		LOG("LOG UNREGISTERED");
		logMsg = nullptr;
	}
}

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	switch (eventType)
	{
	case kUnityGfxDeviceEventInitialize:
	{
		Engine::Get().OnDeviceInitialize();
		break;
	}
	case kUnityGfxDeviceEventShutdown:
	{
		Engine::Get().OnDeviceDeinitialize();
		break;
	}
	default:
		break;
	}
}

// Unity plugin load event
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	s_Graphics = unityInterfaces->Get<IUnityGraphics>();
	
	Engine::Get().Initialize(unityInterfaces);
	s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

// Unity plugin unload event
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
	Engine::Get().Deinitialize();
}