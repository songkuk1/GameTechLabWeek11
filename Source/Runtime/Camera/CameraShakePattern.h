#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"

// 패턴이 한 프레임에 만든 흔들림. 카메라 로컬 공간 기준 (X 앞, Y 오른쪽, Z 위)
struct FCameraShakeUpdateResult
{
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator(0.f, 0.f, 0.f);
	float FOV = 0.f;
};

class UCameraShakePattern : public UObject
{
	DECLARE_CLASS(UCameraShakePattern, UObject)
public:
	UCameraShakePattern();
	virtual ~UCameraShakePattern();
	

public:	
	void StartShake();

	// 블렌드 가중치까지 곱한 결과를 Out에 쓴다
	void UpdateShake(float DeltaTime, FCameraShakeUpdateResult& Out);
	void StopShake(bool bImmediate);
	bool IsFinished() const;

protected:
	// 하위 클래스는 "가중치 1일 때의 흔들림"만 계산한다
	virtual void OnStart() {}
	virtual void OnUpdate(float DeltaTime, FCameraShakeUpdateResult& Out) {}

public:
	// 0 이하면 StopShake 전까지 무한
	float Duration = 0.5f;       
	float BlendInTime = 0.05f;
	float BlendOutTime = 0.2f;

protected:
	float ElapsedTime = 0.f;
	// StopShake(false) 후 블렌드 아웃 중
	bool bStopping = false;      
	float StopElapsed = 0.f;
	bool bFinished = false;
};

