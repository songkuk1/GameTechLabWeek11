#pragma once

class UWorld;
class AActor;
class AStaticMeshActor;
class ADirectionalLightActor;
class AAmbientLightActor;

class FPreviewScene
{
public:
	FPreviewScene();
	~FPreviewScene();

	FPreviewScene(const FPreviewScene&) = delete;
	FPreviewScene& operator=(const FPreviewScene&) = delete;
	UWorld* GetWorld() const { return PreviewWorld; }
	void HideIcons(AActor* Actor);

private:
	//미리보기 씬에서만 쓰는 월드. 레벨 에디터의 월드와는 다르다.
	UWorld* PreviewWorld = nullptr;


	// 월드 안의 액터라 월드가 지워질 때 같이 지워진다. 여기서는 참조만 한다
	AStaticMeshActor* Floor = nullptr;
	ADirectionalLightActor* DirectionalLight = nullptr;
	AAmbientLightActor* AmbientLight = nullptr;

};