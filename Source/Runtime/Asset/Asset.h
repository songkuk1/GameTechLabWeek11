#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Asset.generated.h"

UCLASS()
class UAsset : public UObject
{
	GENERATED_BODY()

public:
	virtual ~UAsset() = default;

	void SetPath(FString Path) { AssetPath = Path; }
	const FString& GetPath() const { return AssetPath; }

private:
	FString AssetPath;
};

