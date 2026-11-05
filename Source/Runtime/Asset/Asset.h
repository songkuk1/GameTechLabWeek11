#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"

class UAsset : public UObject
{
	DECLARE_CLASS(UAsset, UObject)

public:
	virtual ~UAsset() = default;

	void SetPath(FString Path) { AssetPath = Path; }
	const FString& GetPath() const { return AssetPath; }

private:
	FString AssetPath;
};

