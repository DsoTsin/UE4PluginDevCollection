#pragma once

#include "CoreMinimal.h"

class IP4
{
public:

	virtual ~IP4() {}

	virtual FString GetCurrentWorkspace() const = 0;
	virtual FString GetCurrentStream() const = 0;
	virtual uint64	GetLocalCurrentChangelist() const = 0;
};