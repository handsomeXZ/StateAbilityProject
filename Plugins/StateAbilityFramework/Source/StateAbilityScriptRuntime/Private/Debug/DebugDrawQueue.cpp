// Copyright Epic Games, Inc. All Rights Reserved.
#include "Debug/DebugDrawQueue.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeLock.h"

#if WITH_EDITOR
FCFrameDebugDrawQueue& FCFrameDebugDrawQueue::GetInstance()
{
	static FCFrameDebugDrawQueue* PSingleton = nullptr;
	if (PSingleton == nullptr)
	{
		static FCFrameDebugDrawQueue Singleton;
		PSingleton = &Singleton;
	}
	return *PSingleton;
}
#endif
