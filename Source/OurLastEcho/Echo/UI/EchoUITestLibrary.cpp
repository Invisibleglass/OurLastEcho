// Our Last Echo

#include "EchoUITestLibrary.h"
#include "Components/Widget.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Input/Events.h"
#include "Containers/Ticker.h"

bool UEchoUITestLibrary::SendKey(FKey Key)
{
	if (!FSlateApplication::IsInitialized() || !Key.IsValid())
	{
		return false;
	}

	FSlateApplication& Slate = FSlateApplication::Get();
	const FKeyEvent Down(Key, FModifierKeysState(), 0, false, 0, 0);
	const FKeyEvent Up(Key, FModifierKeysState(), 0, false, 0, 0);
	const bool bHandled = Slate.ProcessKeyDownEvent(Down);
	Slate.ProcessKeyUpEvent(Up);
	return bHandled;
}

namespace
{
	bool GetCentre(UWidget* Widget, FVector2D& OutCentre)
	{
		if (!Widget || !Widget->GetCachedWidget().IsValid())
		{
			return false;
		}
		const FGeometry& Geometry = Widget->GetCachedGeometry();
		OutCentre = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
		return true;
	}

	FPointerEvent MouseEvent(const FVector2D& Position, const FVector2D& Last, const TSet<FKey>& Pressed, FKey Button)
	{
		return FPointerEvent(FSlateApplication::Get().CursorPointerIndex, Position, Last, Pressed, Button, 0.0f, FModifierKeysState());
	}
}

bool UEchoUITestLibrary::HoverWidget(UWidget* Widget)
{
	FVector2D Centre;
	if (!FSlateApplication::IsInitialized() || !GetCentre(Widget, Centre))
	{
		return false;
	}
	// Two moves, like a real mouse: CommonUI ignores the first move after gamepad input (and any move with no
	// distance), and only switches to mouse mode on a move it counts
	FSlateApplication& Slate = FSlateApplication::Get();
	const FVector2D Near = Centre + FVector2D(4.0f, 3.0f);
	const FVector2D Last = Slate.GetCursorPos();
	Slate.SetCursorPos(Near);
	Slate.ProcessMouseMoveEvent(MouseEvent(Near, Last, {}, EKeys::Invalid));
	Slate.SetCursorPos(Centre);
	return Slate.ProcessMouseMoveEvent(MouseEvent(Centre, Near, {}, EKeys::Invalid));
}

bool UEchoUITestLibrary::ClickWidget(UWidget* Widget)
{
	FVector2D Centre;
	if (!FSlateApplication::IsInitialized() || !GetCentre(Widget, Centre))
	{
		return false;
	}
	HoverWidget(Widget);
	FSlateApplication& Slate = FSlateApplication::Get();
	TSet<FKey> Pressed = { EKeys::LeftMouseButton };
	const bool bDown = Slate.ProcessMouseButtonDownEvent(nullptr, MouseEvent(Centre, Centre, Pressed, EKeys::LeftMouseButton));
	Slate.ProcessMouseButtonUpEvent(MouseEvent(Centre, Centre, {}, EKeys::LeftMouseButton));
	return bDown;
}

void UEchoUITestLibrary::QueueClickWidget(UWidget* Widget)
{
	TWeakObjectPtr<UWidget> WeakWidget(Widget);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWidget](float)
	{
		if (UWidget* Target = WeakWidget.Get())
		{
			ClickWidget(Target);
		}
		return false;
	}));
}

void UEchoUITestLibrary::QueueKey(FKey Key)
{
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Key](float)
	{
		SendKey(Key);
		return false;
	}));
}

bool UEchoUITestLibrary::HasFocus(UWidget* Widget)
{
	return Widget && (Widget->HasAnyUserFocus() || Widget->HasFocusedDescendants());
}

FString UEchoUITestLibrary::GetFocusedWidgetDescription()
{
	if (!FSlateApplication::IsInitialized())
	{
		return FString();
	}
	const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(0);
	return Focused.IsValid() ? Focused->GetTypeAsString() + TEXT(" ") + Focused->GetReadableLocation() : TEXT("nothing");
}
