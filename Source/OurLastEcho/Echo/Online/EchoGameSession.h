// Our Last Echo

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameSession.h"
#include "EchoGameSession.generated.h"

/**
 *  The game session of both game modes. The engine's version signs the player in to the *default* online
 *  service (EOS) whenever a game starts as a listen server; in a LAN or Play-In-Editor game nobody is signed in
 *  to EOS, so EOS opened the Epic sign-in page in a web browser every time a lobby opened. Signing in is
 *  UEchoLoginSubsystem's job (the login screen, before the main menu), so this skips that step.
 */
UCLASS()
class AEchoGameSession : public AGameSession
{
	GENERATED_BODY()

public:

	virtual bool ProcessAutoLogin() override;
};
