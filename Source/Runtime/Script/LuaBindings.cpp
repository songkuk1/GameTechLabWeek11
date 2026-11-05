#include "EnginePCH.h"
#include "LuaBindings.h"
#include "Engine/GameViewportClient.h"
#include "UI/WidgetTree.h"
#include "UI/CanvasWidget.h"
#include "UI/TextWidget.h"
#include "UI/ImageWidget.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Level.h"

#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Actor/PlayerActor.h"
#include "GameFramework/Actor/EnemyActor.h"
#include "Game/CoinLauncher.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ScriptComponent.h"
#include "Component/SphereComponent.h"
#include "Camera/CameraComponent.h"

#include "Math/Vector.h"
#include "Math/Vector2.h"
#include "Math/Transform.h"

#include "LuaCoroutineScheduler.h"

#include "Input/KeyCode.h"
#include "Input/InputSystem.h"

#include "Collision/HitResult.h"
#include "Game/CoinRain.h"
#include "Game/Bullet.h"
#include "Sound/SoundSystem.h"

void FLuaBindings::RegisterLuaBindings(sol::state& Lua, FLuaCoroutineScheduler* CoroutinScheduler)
{
	// --------------------- Key ---------------------
	Lua.new_enum("KeyCode",
		"LButton", EKeyCode::LButton,
		"RButton", EKeyCode::RButton,
		"MButton", EKeyCode::MButton,
		"Backspace", EKeyCode::Backspace,
		"Tab", EKeyCode::Tab,
		"Clear", EKeyCode::Clear,
		"Enter", EKeyCode::Enter,
		"Shift", EKeyCode::Shift,
		"Control", EKeyCode::Control,
		"Alt", EKeyCode::Alt,
		"Pause", EKeyCode::Pause,
		"CapsLock", EKeyCode::CapsLock,
		"Escape", EKeyCode::Escape,
		"Space", EKeyCode::Space,
		"PageUp", EKeyCode::PageUp,
		"PageDown", EKeyCode::PageDown,
		"End", EKeyCode::End,
		"Home", EKeyCode::Home,
		"Left", EKeyCode::Left,
		"Up", EKeyCode::Up,
		"Right", EKeyCode::Right,
		"Down", EKeyCode::Down,
		"PrintScreen", EKeyCode::PrintScreen,
		"Insert", EKeyCode::Insert,
		"Delete", EKeyCode::Delete,
		"Num0", EKeyCode::Num0,
		"Num1", EKeyCode::Num1,
		"Num2", EKeyCode::Num2,
		"Num3", EKeyCode::Num3,
		"Num4", EKeyCode::Num4,
		"Num5", EKeyCode::Num5,
		"Num6", EKeyCode::Num6,
		"Num7", EKeyCode::Num7,
		"Num8", EKeyCode::Num8,
		"Num9", EKeyCode::Num9,
		"A", EKeyCode::A,
		"B", EKeyCode::B,
		"C", EKeyCode::C,
		"D", EKeyCode::D,
		"E", EKeyCode::E,
		"F", EKeyCode::F,
		"G", EKeyCode::G,
		"H", EKeyCode::H,
		"I", EKeyCode::I,
		"J", EKeyCode::J,
		"K", EKeyCode::K,
		"L", EKeyCode::L,
		"M", EKeyCode::M,
		"N", EKeyCode::N,
		"O", EKeyCode::O,
		"P", EKeyCode::P,
		"Q", EKeyCode::Q,
		"R", EKeyCode::R,
		"S", EKeyCode::S,
		"T", EKeyCode::T,
		"U", EKeyCode::U,
		"V", EKeyCode::V,
		"W", EKeyCode::W,
		"X", EKeyCode::X,
		"Y", EKeyCode::Y,
		"Z", EKeyCode::Z,
		"LWin", EKeyCode::LWin,
		"RWin", EKeyCode::RWin,
		"NumPad0", EKeyCode::NumPad0,
		"NumPad1", EKeyCode::NumPad1,
		"NumPad2", EKeyCode::NumPad2,
		"NumPad3", EKeyCode::NumPad3,
		"NumPad4", EKeyCode::NumPad4,
		"NumPad5", EKeyCode::NumPad5,
		"NumPad6", EKeyCode::NumPad6,
		"NumPad7", EKeyCode::NumPad7,
		"NumPad8", EKeyCode::NumPad8,
		"NumPad9", EKeyCode::NumPad9,
		"Multiply", EKeyCode::Multiply,
		"Add", EKeyCode::Add,
		"Separator", EKeyCode::Separator,
		"Subtract", EKeyCode::Subtract,
		"Decimal", EKeyCode::Decimal,
		"Divide", EKeyCode::Divide,
		"F1", EKeyCode::F1,
		"F2", EKeyCode::F2,
		"F3", EKeyCode::F3,
		"F4", EKeyCode::F4,
		"F5", EKeyCode::F5,
		"F6", EKeyCode::F6,
		"F7", EKeyCode::F7,
		"F8", EKeyCode::F8,
		"F9", EKeyCode::F9,
		"F10", EKeyCode::F10,
		"F11", EKeyCode::F11,
		"F12", EKeyCode::F12,
		"NumLock", EKeyCode::NumLock,
		"ScrollLock", EKeyCode::ScrollLock,
		"LShift", EKeyCode::LShift,
		"RShift", EKeyCode::RShift,
		"LControl", EKeyCode::LControl,
		"RControl", EKeyCode::RControl,
		"LAlt", EKeyCode::LAlt,
		"RAlt", EKeyCode::RAlt,
		"Semicolon", EKeyCode::Semicolon,
		"Equals", EKeyCode::Equals,
		"Comma", EKeyCode::Comma,
		"Minus", EKeyCode::Minus,
		"Period", EKeyCode::Period,
		"Slash", EKeyCode::Slash,
		"Tilde", EKeyCode::Tilde,
		"LeftBracket", EKeyCode::LeftBracket,
		"Backslash", EKeyCode::Backslash,
		"RightBracket", EKeyCode::RightBracket,
		"Quote", EKeyCode::Quote
	);

	sol::table Input = Lua.create_named_table("Input");
	Input.set_function("IsKeyPressed", &FInputSystem::IsKeyPressed);
	Input.set_function("IsKeyDown", &FInputSystem::IsKeyDown);
	Input.set_function("IsMouseDown", &FInputSystem::IsMouseDown);
	Input.set_function("GetMouseDelta", []()
	{
		return std::make_tuple(FInputSystem::GetMouseDeltaX(), FInputSystem::GetMouseDeltaY());
	});

	Lua.new_enum("MouseButton", "Left", EMouseButton::Left, "Right", EMouseButton::Right, "Middle", EMouseButton::Middle);
	Input.set_function("IsMousePressed", &FInputSystem::IsMousePressed);

	// --------------------- Type ---------------------
	Lua.new_usertype<FVector2>(
		"Vector2",
		sol::constructors<FVector2(), FVector2(float, float)>(),
		"X", &FVector2::X,
		"Y", &FVector2::Y
	);

    Lua.new_usertype<FVector>(
		"Vector",
        sol::constructors<FVector(), FVector(float, float, float)>(),
        "x", &FVector::X,
        "y", &FVector::Y,
        "z", &FVector::Z,
		"Normalized", &FVector::Normalized,
        sol::meta_function::addition, [](const FVector& a, const FVector& b) { return FVector(a.X + b.X, a.Y + b.Y, a.Z + b.Z); },
        sol::meta_function::subtraction, [](const FVector& a, const FVector& b) { return FVector(a.X - b.X, a.Y - b.Y, a.Z - b.Z); },
        sol::meta_function::multiplication, [](const FVector& v, float f) { return v * f; }
    );

	Lua.new_usertype<FRotator>(
		"Rotator",
		sol::constructors<FRotator(), FRotator(float, float, float)>()
	);

	Lua.new_usertype<FTransform>(
		"Transform",
		sol::constructors<FTransform(), FTransform(FVector, FVector, FRotator)>(),
		"Location", &FTransform::Location,
		"Scale", &FTransform::Scale,
		"Rotation", &FTransform::Rotation
	);

	Lua.new_usertype<FLinearColor>(
		"LinearColor",
		sol::constructors<FLinearColor(), FLinearColor(float, float, float, float)>(),
		"R", &FLinearColor::R,
		"G", &FLinearColor::G,
		"B", &FLinearColor::B,
		"A", &FLinearColor::A,
		sol::meta_function::addition, &FLinearColor::operator+,
		sol::meta_function::multiplication, &FLinearColor::operator*
	);

	Lua.new_usertype<FWidgetLayout>(
		"WidgetLayout",
		sol::constructors<FWidgetLayout(), FWidgetLayout(const FVector2&, const FVector2&, const FVector2&, const FVector2&, const FVector2&)>(),
		"AnchorMin", &FWidgetLayout::AnchorMin,
		"AnchorMax", &FWidgetLayout::AnchorMax,
		"Pivot", &FWidgetLayout::Pivot,
		"AnchoredPosition", &FWidgetLayout::AnchoredPosition,
		"SizeDelta", &FWidgetLayout::SizeDelta
	);

	Lua.new_usertype<UObject>(
		"Object",
		sol::no_constructor,
		"GetName", &UObject::GetName
	);

	Lua.new_usertype<UWorld>(
		"World",
		sol::no_constructor,
		sol::base_classes, sol::bases<UObject>(),
		"SpawnActor", sol::overload(
			sol::resolve<UWorld&, UClass*, sol::string_view, const FTransform*>(&FLuaBindings::SpawnActor),
			sol::resolve<UWorld&, UClass*, const FTransform*>(&FLuaBindings::SpawnActor)
		)
	);

	Lua.new_usertype<AActor>(
		"Actor",
		sol::no_constructor,
		sol::base_classes, sol::bases<UObject>(),
		"GetActorLocation", &AActor::GetActorLocation,
		"SetActorLocation", &AActor::SetActorLocation,
		"GetWorld", &AActor::GetWorld,
		"FindComponentByName", &FLuaBindings::FindComponentByName,
		"FindComponentByClass", &FLuaBindings::FindComponentByClass,
		"Destroy", [](AActor* Actor)
		{
			return Actor && Actor->GetWorld() ? Actor->GetWorld()->RequestDestroyActor(Actor) : false;
		}
    );

	Lua.new_usertype<APawn>(
		"APawn",
		sol::no_constructor,
		sol::base_classes, sol::bases<AActor, UObject>(),
		"GetActorLocation", &AActor::GetActorLocation,
		"SetActorLocation", &AActor::SetActorLocation,
		"FindComponentByName", &FLuaBindings::FindComponentByName,
		"FindComponentByClass", &FLuaBindings::FindComponentByClass,
		"IsInputEnabled", &APawn::IsPlayerInputEnabled,
		"AddYawInput", &APawn::AddControllerYawInput,
		"AddPitchInput", &APawn::AddControllerPitchInput
	);

	Lua.new_usertype<UActorComponent>(
		"ActorComponent",
		sol::no_constructor,
		sol::base_classes, sol::bases<UObject>(),
		"GetOwner", &UActorComponent::GetOwner
	);

	Lua.new_usertype<USceneComponent>(
		"SceneComponent",
		sol::no_constructor,
		sol::base_classes, sol::bases<UActorComponent, UObject>(),
		"StaticClass", &USceneComponent::StaticClass,
		"GetRelativeRotation", &USceneComponent::GetRelativeRotation,
		"SetRelativeRotation", &USceneComponent::SetRelativeRotation
	);

	Lua.new_usertype<UPrimitiveComponent>(
		"PrimitiveComponent",
		sol::no_constructor,
		sol::base_classes, sol::bases<USceneComponent, UActorComponent, UObject>(),
		"StaticClass", &UPrimitiveComponent::StaticClass,
		"GetName", &UObject::GetName,
		"GetOwner", &UActorComponent::GetOwner
	);	

	Lua.new_usertype<UScriptComponent>(
		"ScriptComponent",
		sol::no_constructor,
		sol::base_classes, sol::bases<UActorComponent, UObject>(),
		"StaticClass", &UScriptComponent::StaticClass,
		"GetLuaTable", &UScriptComponent::GetLuaTable
	);

	Lua.new_usertype<UCameraComponent>(
		"CameraComponent",
		sol::no_constructor,
		sol::base_classes, sol::bases<USceneComponent, UActorComponent, UObject>(),
		"StaticClass", &UCameraComponent::StaticClass,
		"GetAspectRatio", &UCameraComponent::GetAspectRatio,
		"SetAspectRatio", &UCameraComponent::SetAspectRatio,
		"GetConstrainAspectRatio", &UCameraComponent::GetConstrainAspectRatio,
		"SetConstrainAspectRatio", &UCameraComponent::SetConstrainAspectRatio
	);

	Lua.new_usertype<FWaitForSeconds>(
		"WaitForSeconds",
		sol::constructors<FWaitForSeconds(double)>(),
		sol::base_classes, sol::bases<FWaitCondition>(),
		"Seconds", &FWaitForSeconds::Seconds
	);

	Lua.new_usertype<FWaitUntil>(
		"WaitUntil",
		sol::constructors<FWaitUntil(sol::protected_function)>(),
		sol::base_classes, sol::bases<FWaitCondition>(),
		"Predicate", &FWaitUntil::Predicate
	);

	Lua.new_usertype<FWaitNextFrame>(
		"WaitNextFrame",
		sol::constructors<FWaitNextFrame()>(),
		sol::base_classes, sol::bases<FWaitCondition>()
	);

	Lua.new_usertype<FWaitForRealSeconds>(
		"WaitForRealSeconds",
		sol::constructors<FWaitForRealSeconds(double)>(),
		sol::base_classes, sol::bases<FWaitCondition>()
	);

	Lua.new_usertype<FHitResult>(
		"HitResult",
		sol::no_constructor,

		"Location", & FHitResult::Location,      // 멈춘 위치 (내 중심)
		"ImpactPoint", & FHitResult::ImpactPoint,   // 접점
		"Normal", & FHitResult::Normal,
		"ImpactNormal", & FHitResult::ImpactNormal,
		"Time", & FHitResult::Time,
		"Distance", & FHitResult::Distance,
		"bBlockingHit", & FHitResult::bBlockingHit,
		"bStartPenetrating", & FHitResult::bStartPentrating,
		"HitComponent", & FHitResult::HitComponent
	);

	Lua.new_usertype<SWidget>(
		"Widget",
		sol::no_constructor,
		"SetLayout", &SWidget::SetLayout,
		"GetLayout", [](SWidget& Widget) -> FWidgetLayout { return Widget.GetLayout(); }
	);

	Lua.new_usertype<SCanvasWidget>(
		"CanvasWidget",
		sol::base_classes, sol::bases<SWidget>()
	);

	Lua.new_usertype<STextWidget>(
		"TextWidget",
		sol::base_classes, sol::bases<SWidget>(),
		"SetText", &STextWidget::SetText,
		"SetColor", &STextWidget::SetColor
	);

	Lua.new_usertype<SImageWidget>(
		"ImageWidget",
		sol::base_classes, sol::bases<SWidget>(),
		"SetColor", &SImageWidget::SetColor
	);

	Lua.new_usertype<FWidgetTree>(
		"WidgetTree",
		"FindWidget", &FLuaBindings::FindWidget
	);

	// Lua에 함수 바인딩
    Lua.set_function("Log", [](const std::string& Message) { HTR_LOG(Info, "[Lua] {}", Message); });
	Lua.set_function("StartCoroutine", sol::resolve<sol::protected_function, sol::variadic_args>(&FLuaCoroutineScheduler::StartCoroutine), CoroutinScheduler);
	Lua.set_function("StopCoroutine", &FLuaCoroutineScheduler::StopCoroutine, CoroutinScheduler);

	sol::table GamePlayStatics = Lua.create_named_table("GamePlayStatics");
	GamePlayStatics.set_function("OpenLevel", &FLuaBindings::OpenLevel);
	GamePlayStatics.set_function("PlaySound", [](const FString& Path, sol::optional<float> Volume)
	{
		FSoundSystem::Get().PlaySound(Path, Volume.value_or(1.0f));
	});
	GamePlayStatics.set_function("PlayMusic", [](const FString& Path, sol::optional<float> Volume)
	{
		FSoundSystem::Get().PlayMusic(Path, Volume.value_or(1.0f));
	});
	GamePlayStatics.set_function("StopMusic", []
	{
		FSoundSystem::Get().StopMusic();
	});
	GamePlayStatics.set_function("GetActorOfClass", &FLuaBindings::GetActorOfClass);
	GamePlayStatics.set_function("GetViewportSize", []
	{
		return GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetViewportSize() : FVector2(0.0f, 0.0f);
	});
	GamePlayStatics.set_function("SetGlobalTimeDilation", [](AActor* WorldContextObject, float NewDilation)
	{
		WorldContextObject->GetWorld()->SetGlobalTimeDilation(NewDilation);
	});
	// 시퀀스 카메라 컷이 끝난 뒤 화면을 다시 플레이어 카메라로 돌릴 때 쓴다
	GamePlayStatics.set_function("SetViewTarget", [](AActor* WorldContextObject, AActor* NewViewTarget)
	{
		if (APlayerController* PC = WorldContextObject->GetWorld()->GetPlayerController())
		{
			PC->SetViewTarget(NewViewTarget);
		}
	});
	// 플레이어 카메라(스프링암)가 바라볼 방향. 컷신이 끝날 때 마지막 컷 카메라와 같은 방향으로 맞춘다
	GamePlayStatics.set_function("SetControlRotation", [](AActor* WorldContextObject, const FRotator& NewRotation)
	{
		if (APlayerController* PC = WorldContextObject->GetWorld()->GetPlayerController())
		{
			PC->SetControlRotation(NewRotation);
		}
	});

	sol::table UI = Lua.create_named_table("UI");
	UI.set_function("CreateWidgetTree", &FLuaBindings::CreateWidgetTree);
	UI.set_function("DestroyWidgetTree", &FLuaBindings::DetroyWidgetTree);
	UI.set_function("AddToViewport", &FLuaBindings::AddToViewport);
	UI.set_function("RemoveFromViewport", &FLuaBindings::RemoveFromViewport);

	// --------------------- Player ---------------------
	Lua.new_usertype<APlayerActor>(
		"Player",
		sol::no_constructor,
		sol::base_classes, sol::bases<APawn, AActor, UObject>(),
		"StaticClass", &APlayerActor::StaticClass,
		"Move", &APlayerActor::Move,
		"Jump", &APlayerActor::Jump,
		"PlayerShot", &APlayerActor::PlayerProjectile,
		"PlayDamageEffect", &APlayerActor::PlayDamageEffect,
		"ApplyKnockback", &APlayerActor::ApplyKnockback,
		"StartDeathFall", &APlayerActor::StartDeathFall,
		"StartWandSwing", &APlayerActor::StartWandSwing,
		"SetMagnetRadius", &APlayerActor::SetMagnetRadius
	);

	// --------------------- Enemy ---------------------
	Lua.new_usertype<AEnemyActor>(
		"Enemy",
		sol::base_classes, sol::bases<APawn, AActor, UObject>(),
		"StaticClass", &AEnemyActor::StaticClass,
		"ProjectileAtPlayer", &AEnemyActor::EnemyProjectileAtPlayer,
		"PlayHitSquash", &AEnemyActor::PlayHitSquash,
		"PlaySlamCameraShake", &AEnemyActor::PlaySlamCameraShake,
		"Jump", &AEnemyActor::Jump,
		"IsGrounded", &AEnemyActor::IsGrounded
	);

	// --------------------- Bullet ---------------------
	Lua.new_usertype<ABullet>(
		"Bullet",
		sol::base_classes, sol::bases<AActor, UObject>(),
		"StaticClass", &ABullet::StaticClass,
		"SetTarget", &ABullet::SetTarget,
		"SetDirection", &ABullet::SetDirection
	);

	// --------------------- CoinLauncher ---------------------
	sol::table CoinLauncher = Lua.create_named_table("CoinLauncher");
	CoinLauncher.set_function("Launch", [](AActor* Actor, uint32 Amount, float Speed)
	{
		if (ACoinLauncher* Launcher = Cast<ACoinLauncher>(Actor))
		{
			Launcher->Launch(Amount, Speed);
		}
	});
	// 아무 액터 위(HeightOffset)에서 위쪽으로 코인을 쏟아낸다. 슬라임 타격 보상에 쓴다.
	CoinLauncher.set_function("Burst", [](AActor* Actor, uint32 Amount, float Speed, sol::optional<float> HeightOffset)
	{
		if (!Actor)
		{
			return;
		}
		const FVector Origin = Actor->GetActorLocation() + FVector(0.0f, 0.0f, HeightOffset.value_or(2.5f));
		ACoinLauncher::LaunchCoins(Actor->GetWorld(), Origin, FVector::UpVector, 35.0f, Amount, Speed);
	});

	// --------------------- CoinRain ---------------------
	sol::table CoinRain = Lua.create_named_table("CoinRain");
	CoinRain.set_function("GetInterval", [](AActor* Actor)
	{
		if (ACoinRain* Rain = Cast<ACoinRain>(Actor))
		{
			return Rain->GetInterval();
		}
	});
	CoinRain.set_function("Spawn", [](AActor* Actor)
	{
		if (ACoinRain* Rain = Cast<ACoinRain>(Actor))
		{
			Rain->Spawn();
		}	
	});
};

static sol::object ConvertActorToActual(AActor* Actor)
{
	if (Actor->GetClass() == APlayerActor::StaticClass())
	{
		return sol::make_object(FLuaScriptSystem::Get().GetLuaState(), Cast<APlayerActor>(Actor));
	}
	else if (Actor->GetClass() == AEnemyActor::StaticClass())
	{
		return sol::make_object(FLuaScriptSystem::Get().GetLuaState(), Cast<AEnemyActor>(Actor));
	}
	else if (Actor->GetClass() == ABullet::StaticClass())
	{
		return sol::make_object(FLuaScriptSystem::Get().GetLuaState(), Cast<ABullet>(Actor));
	}

	return sol::object();
}

static sol::object ConvertComponentToActual(UActorComponent* Component)
{
	if (!Component)
	{
		return sol::object();
	}

	if (Component->GetClass() == UScriptComponent::StaticClass())
	{
		return sol::make_object(FLuaScriptSystem::Get().GetLuaState(), Cast<UScriptComponent>(Component));
	}

	if (UCameraComponent* Camera = Cast<UCameraComponent>(Component))
	{
		return sol::make_object(FLuaScriptSystem::Get().GetLuaState(), Camera);
	}

	return sol::object();
}

sol::object FLuaBindings::FindComponentByName(const AActor& Actor, sol::string_view NameStr)
{
	return ConvertComponentToActual(Actor.FindComponentByName(FName(NameStr.data(), NameStr.size())));
}

sol::object FLuaBindings::FindComponentByClass(const AActor& Actor, const UClass* Clss)
{
	return ConvertComponentToActual(Actor.FindComponentByClass(Clss));
}

sol::object FLuaBindings::SpawnActor(UWorld& World, UClass* Class, const FTransform* Transform)
{
	return ConvertActorToActual(World.SpawnActor(Class, NAME_None, Transform));
}

sol::object FLuaBindings::SpawnActor(UWorld& World, UClass* Class, sol::string_view NameStr, const FTransform* Transform)
{
	return ConvertActorToActual(World.SpawnActor(Class, FName(NameStr.data(), NameStr.size()), Transform));
}

void FLuaBindings::OpenLevel(AActor* WorldContextObject, sol::string_view NextURL)
{
	GEngine->SetClientTravel(WorldContextObject->GetWorld(), FString(NextURL.data(), NextURL.size()));
}

AActor* FLuaBindings::GetActorOfClass(AActor* WorldContextObject, const UClass* Clss)
{
	return WorldContextObject->GetWorld()->GetCurrentLevel()->GetActorOfClass(Clss);
}

AActor* FLuaBindings::GetActorByName(AActor* WorldContextObject, sol::string_view Name)
{
	for (AActor* Actor : WorldContextObject->GetWorld()->GetCurrentLevel()->GetActors())
	{
		if (Actor && Actor->GetName() == FString(Name.data(), Name.size()))
		{
			return Actor;
		}
	}
	return nullptr;
}

FWidgetTree* FLuaBindings::CreateWidgetTree(sol::string_view JsonSource)
{
	return FWidgetTree::CreateFromJson(FString(JsonSource.data(), JsonSource.size()));
}

void FLuaBindings::DetroyWidgetTree(FWidgetTree* Tree)
{
	if (!Tree)
	{
		return;
	}

	delete Tree;
}

sol::object FLuaBindings::FindWidget(FWidgetTree& Tree, FWidgetID ID)
{
	SWidget* Widget = Tree.FindWidget(ID);
	if (STextWidget* TextWidget = dynamic_cast<STextWidget*>(Widget))
	{
		return sol::make_object(FLuaScriptSystem::Get().GetLuaState(), TextWidget);
	}
	else if (SImageWidget* ImageWidget = dynamic_cast<SImageWidget*>(Widget))
	{
		return sol::make_object(FLuaScriptSystem::Get().GetLuaState(), ImageWidget);
	}
	else if (SCanvasWidget* CanvasWidget = dynamic_cast<SCanvasWidget*>(Widget))
	{
		return sol::make_object(FLuaScriptSystem::Get().GetLuaState(), CanvasWidget);
	}

	return sol::make_object(FLuaScriptSystem::Get().GetLuaState(), Widget);
}

void FLuaBindings::AddToViewport(FWidgetTree* Tree)
{
	UGameViewportClient* GameViewport = GEngine->GameViewport;
	if (!GameViewport)
	{
		return;
	}

	GameViewport->AddToViewport(Tree);
}

void FLuaBindings::RemoveFromViewport(FWidgetTree* Tree)
{
	UGameViewportClient* GameViewport = GEngine->GameViewport;
	if (!GameViewport)
	{
		return;
	}

	if (GameViewport->GetWidgetTree() == Tree)
	{
		GameViewport->AddToViewport(nullptr);
	}
}
