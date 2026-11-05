#pragma once

#include "Component/PrimitiveComponent.h"

// 선택 외곽선을 그릴 대상 목록. 다중 선택이면 대상이 여러 개다.
class FOutline
{
public:
	// 대상 하나만 표시한다. nullptr이면 아무것도 표시하지 않는다.
	void SetTarget(UPrimitiveComponent* InTarget)
	{
		Targets.Reset();
		if (InTarget)
			Targets.Add(InTarget);
	}
	void SetTargets(const TArray<UPrimitiveComponent*>& InTargets) { Targets = InTargets; }

	// 첫 대상. 단일 선택을 쓰는 곳(Benchmark 등)과의 호환용이다.
	UPrimitiveComponent* GetTarget() const { return Targets.IsEmpty() ? nullptr : Targets[0]; }
	const TArray<UPrimitiveComponent*>& GetTargets() const { return Targets; }
private:
	TArray<UPrimitiveComponent*> Targets;

};
