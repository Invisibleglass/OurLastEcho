// Our Last Echo

#include "EchoGameSession.h"

bool AEchoGameSession::ProcessAutoLogin()
{
	// Not waiting for an asynchronous login: the game mode registers the server straight away
	return false;
}
