// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterGame.h"
#include "ShooterGameSession.h"
#include "ShooterOnlineGameSettings.h"
#include "OnlineSubsystemSessionSettings.h"
#include "OnlineSubsystemUtils.h"

namespace
{
	const FString CustomMatchKeyword("Custom");
}

AShooterGameSession::AShooterGameSession(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		OnCreateSessionCompleteDelegate = IMSSessionManagerAPI::OpenAPISessionManagerV0Api::FCreateSessionV0Delegate::CreateUObject(this, &AShooterGameSession::OnCreateSessionComplete);
		OnDestroySessionCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &AShooterGameSession::OnDestroySessionComplete);

		OnFindSessionsCompleteDelegate = IMSSessionManagerAPI::OpenAPISessionManagerV0Api::FListSessionsV0Delegate::CreateUObject(this, &AShooterGameSession::OnFindSessionsComplete);
		OnJoinSessionCompleteDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &AShooterGameSession::OnJoinSessionComplete);

		OnStartSessionCompleteDelegate = FOnStartSessionCompleteDelegate::CreateUObject(this, &AShooterGameSession::OnStartOnlineGameComplete);

		RetryPolicy = IMSSessionManagerAPI::HttpRetryParams(RetryLimitCount, RetryTimeoutRelativeSeconds);
		SessionManagerAPI = MakeShared<IMSSessionManagerAPI::OpenAPISessionManagerV0Api>();
		CurrentSessionSearch = MakeShared<class SessionSearch>();
	}
}

/**
 * Delegate fired when a session start request has completed
 *
 * @param SessionName the name of the session this callback is for
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
void AShooterGameSession::OnStartOnlineGameComplete(FName InSessionName, bool bWasSuccessful)
{
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	if (OnlineSub)
	{
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
		if (Sessions.IsValid())
		{
			Sessions->ClearOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegateHandle);
		}
	}

	if (bWasSuccessful)
	{
		// tell non-local players to start online game
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			AShooterPlayerController* PC = Cast<AShooterPlayerController>(*It);
			if (PC && !PC->IsLocalPlayerController())
			{
				PC->ClientStartOnlineGame();
			}
		}
	}
}

/** Handle starting the match */
void AShooterGameSession::HandleMatchHasStarted()
{
	// start online game locally and wait for completion
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	if (OnlineSub)
	{
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
		if (Sessions.IsValid() && (Sessions->GetNamedSession(NAME_GameSession) != nullptr))
		{
			UE_LOG(LogOnlineGame, Log, TEXT("Starting session %s on server"), *FName(NAME_GameSession).ToString());
			OnStartSessionCompleteDelegateHandle = Sessions->AddOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegate);
			Sessions->StartSession(NAME_GameSession);
		}
	}
}

/** 
 * Ends a game session 
 *
 */
void AShooterGameSession::HandleMatchHasEnded()
{
	// end online game locally 
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	if (OnlineSub)
	{
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
		if (Sessions.IsValid() && (Sessions->GetNamedSession(NAME_GameSession) != nullptr))
		{
			// tell the clients to end
			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				AShooterPlayerController* PC = Cast<AShooterPlayerController>(*It);
				if (PC && !PC->IsLocalPlayerController())
				{
					PC->ClientEndOnlineGame();
				}
			}

			// server is handled here
			UE_LOG(LogOnlineGame, Log, TEXT("Ending session %s on server"), *FName(NAME_GameSession).ToString() );
			Sessions->EndSession(NAME_GameSession);
		}
	}
}

bool AShooterGameSession::IsBusy() const
{ 
	if (HostSettings.IsValid() || SearchSettings.IsValid())
	{
		return true;
	}

	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	if (OnlineSub)
	{
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
		if (Sessions.IsValid())
		{
			EOnlineSessionState::Type GameSessionState = Sessions->GetSessionState(NAME_GameSession);
			EOnlineSessionState::Type PartySessionState = Sessions->GetSessionState(NAME_PartySession);
			if (GameSessionState != EOnlineSessionState::NoSession || PartySessionState != EOnlineSessionState::NoSession)
			{
				return true;
			}
		}
	}

	return false;
}

/**
 * Delegate fired when a session create request has completed
 */
void AShooterGameSession::OnCreateSessionComplete(const IMSSessionManagerAPI::OpenAPISessionManagerV0Api::CreateSessionV0Response& Response)
{
	if (Response.IsSuccessful() && Response.Content.Address.IsSet() && Response.Content.Ports.IsSet())
	{
		FString IP = Response.Content.Address.GetValue();
		// Filtering the ports in the response for the "GamePort", this value should match what you have indicated in your allocation
		const IMSSessionManagerAPI::OpenAPIV0Port* GamePortResponse = Response.Content.Ports.GetValue().FindByPredicate([](IMSSessionManagerAPI::OpenAPIV0Port PortResponse) { return PortResponse.Name == "GamePort"; });

		if (GamePortResponse != nullptr)
		{
			FString SessionAddress = IP + ":" + FString::FromInt(GamePortResponse->Port);

			UE_LOG(LogOnlineGame, Display, TEXT("Successfully created a session. Connect to session address: '%s'"), *SessionAddress);
			OnCreateSessionComplete().Broadcast(SessionAddress, true);
		}
		else
		{
			UE_LOG(LogOnlineGame, Error, TEXT("Successfully created a session but could not find the Game Port."));
			OnCreateSessionComplete().Broadcast("", false);
		}
	}
	else
	{
		UE_LOG(LogOnlineGame, Display, TEXT("Failed to create a session."));
		OnCreateSessionComplete().Broadcast("", false);
	}
}

/**
 * Delegate fired when a destroying an online session has completed
 *
 * @param SessionName the name of the session this callback is for
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
void AShooterGameSession::OnDestroySessionComplete(FName InSessionName, bool bWasSuccessful)
{
	UE_LOG(LogOnlineGame, Verbose, TEXT("OnDestroySessionComplete %s bSuccess: %d"), *InSessionName.ToString(), bWasSuccessful);

	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	if (OnlineSub)
	{
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegateHandle);
		HostSettings = NULL;
	}
}

void AShooterGameSession::HostSession(const int32 MaxNumPlayers, const int32 BotsCount, const FString SessionTicket)
{
	// See the following doc for more information https://docs.ims.improbable.io/docs/ims-session-manager/guides/authetication
	SessionManagerAPI->AddHeaderParam("Authorization", "Bearer playfab/" + SessionTicket);

	IMSSessionManagerAPI::OpenAPISessionManagerV0Api::CreateSessionV0Request Request;
	Request.SetShouldRetry(RetryPolicy);
	Request.ProjectId = IMSProjectId;
	Request.SessionType = IMSSessionType;

	IMSSessionManagerAPI::OpenAPIV0CreateSessionRequestBody RequestBody;
	RequestBody.SessionConfig = CreateSessionConfigJson(MaxNumPlayers, BotsCount);
	Request.Body = RequestBody;

	UE_LOG(LogOnlineGame, Display, TEXT("Attempting to create a session..."));
	SessionManagerAPI->CreateSessionV0(Request, OnCreateSessionCompleteDelegate);

	FHttpModule::Get().GetHttpManager().Flush(false);
}

void AShooterGameSession::OnFindSessionsComplete(const IMSSessionManagerAPI::OpenAPISessionManagerV0Api::ListSessionsV0Response& Response)
{
	if (Response.IsSuccessful())
	{
		UE_LOG(LogOnlineGame, Display, TEXT("Successfully listed sessions."));

		TArray<Session> SearchResults;

		for (IMSSessionManagerAPI::OpenAPIV0Session SessionResult : Response.Content.Sessions)
		{
			if (SearchResults.Num() < CurrentSessionSearch->MaxSearchResults)
			{
				SearchResults.Add(Session(SessionResult));
			}
		}

		CurrentSessionSearch->SearchResults = SearchResults;
		CurrentSessionSearch->SearchState = SearchState::Done;

		OnFindSessionsComplete().Broadcast(true);
	}
	else
	{
		UE_LOG(LogOnlineGame, Display, TEXT("Failed to list sessions."));
		CurrentSessionSearch->SearchState = SearchState::Failed;
		OnFindSessionsComplete().Broadcast(false);
	}
}

void AShooterGameSession::ResetBestSessionVars()
{
	CurrentSessionParams.BestSessionIdx = -1;
}

void AShooterGameSession::ChooseBestSession()
{
	// Start searching from where we left off
	for (int32 SessionIndex = CurrentSessionParams.BestSessionIdx+1; SessionIndex < SearchSettings->SearchResults.Num(); SessionIndex++)
	{
		// Found the match that we want
		CurrentSessionParams.BestSessionIdx = SessionIndex;
		return;
	}

	CurrentSessionParams.BestSessionIdx = -1;
}

void AShooterGameSession::StartMatchmaking()
{
	ResetBestSessionVars();
	ContinueMatchmaking();
}

void AShooterGameSession::ContinueMatchmaking()
{	
	ChooseBestSession();
	if (CurrentSessionParams.BestSessionIdx >= 0 && CurrentSessionParams.BestSessionIdx < SearchSettings->SearchResults.Num())
	{
		IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
		if (OnlineSub)
		{
			IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
			if (Sessions.IsValid() && CurrentSessionParams.UserId.IsValid())
			{
				OnJoinSessionCompleteDelegateHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegate);
				Sessions->JoinSession(*CurrentSessionParams.UserId, CurrentSessionParams.SessionName, SearchSettings->SearchResults[CurrentSessionParams.BestSessionIdx]);
			}
		}
	}
	else
	{
		OnNoMatchesAvailable();
	}
}

void AShooterGameSession::OnNoMatchesAvailable()
{
	UE_LOG(LogOnlineGame, Verbose, TEXT("Matchmaking complete, no sessions available."));
	SearchSettings = NULL;
}

FString AShooterGameSession::CreateSessionConfigJson(const int32 MaxNumPlayers, const int32 BotsCount)
{
	UE_LOG(LogOnlineGame, Log, TEXT("Creating Session Config Json: MaxNumPlayers = %d, BotsCount = %d"), MaxNumPlayers, BotsCount);
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetNumberField("MaxNumPlayers", MaxNumPlayers);
	JsonObject->SetNumberField("BotsCount", BotsCount);

	FString SessionConfig;
	TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&SessionConfig);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	return SessionConfig;
}

const SearchState AShooterGameSession::GetSearchSessionsStatus() const
{
	return CurrentSessionSearch->SearchState;
}

const TArray<Session>& AShooterGameSession::GetSearchResults() const
{
	return CurrentSessionSearch->SearchResults;
}

void AShooterGameSession::FindSessions(FString SessionTicket)
{
	// See the following doc for more information https://docs.ims.improbable.io/docs/ims-session-manager/guides/authetication
	SessionManagerAPI->AddHeaderParam("Authorization", "Bearer playfab/" + SessionTicket);

	IMSSessionManagerAPI::OpenAPISessionManagerV0Api::ListSessionsV0Request Request;
	Request.SetShouldRetry(RetryPolicy);
	Request.ProjectId = IMSProjectId;
	Request.SessionType = IMSSessionType;

	UE_LOG(LogOnlineGame, Display, TEXT("Attempting to list sessions..."));
	CurrentSessionSearch->SearchState = SearchState::InProgress;
	SessionManagerAPI->ListSessionsV0(Request, OnFindSessionsCompleteDelegate);

	FHttpModule::Get().GetHttpManager().Flush(false);
}

bool AShooterGameSession::JoinSession(TSharedPtr<const FUniqueNetId> UserId, FName InSessionName, int32 SessionIndexInSearchResults)
{
	bool bResult = false;

	if (SessionIndexInSearchResults >= 0 && SessionIndexInSearchResults < SearchSettings->SearchResults.Num())
	{
		bResult = JoinSession(UserId, InSessionName, SearchSettings->SearchResults[SessionIndexInSearchResults]);
	}

	return bResult;
}

bool AShooterGameSession::JoinSession(TSharedPtr<const FUniqueNetId> UserId, FName InSessionName, const FOnlineSessionSearchResult& SearchResult)
{
	bool bResult = false;

	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	if (OnlineSub)
	{
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
		if (Sessions.IsValid() && UserId.IsValid())
		{
			OnJoinSessionCompleteDelegateHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegate);
			bResult = Sessions->JoinSession(*UserId, InSessionName, SearchResult);
		}
	}

	return bResult;
}

/**
 * Delegate fired when the joining process for an online session has completed
 *
 * @param SessionName the name of the session this callback is for
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
void AShooterGameSession::OnJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result)
{
	bool bWillTravel = false;

	UE_LOG(LogOnlineGame, Verbose, TEXT("OnJoinSessionComplete %s bSuccess: %d"), *InSessionName.ToString(), static_cast<int32>(Result));
	
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	if (OnlineSub)
	{
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
		if (Sessions.IsValid())
		{
			Sessions->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegateHandle);
		}
	}

	OnJoinSessionComplete().Broadcast(Result);
}

bool AShooterGameSession::TravelToSession(int32 ControllerId, FName InSessionName)
{
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	if (OnlineSub)
	{
		FString URL;
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
		if (Sessions.IsValid() && Sessions->GetResolvedConnectString(InSessionName, URL))
		{
			APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), ControllerId);
			if (PC)
			{
				PC->ClientTravel(URL, TRAVEL_Absolute);
				return true;
			}
		}
		else
		{
			UE_LOG(LogOnlineGame, Warning, TEXT("Failed to join session %s"), *SessionName.ToString());
		}
	}
#if !UE_BUILD_SHIPPING
	else
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), ControllerId);
		if (PC)
		{
			FString LocalURL(TEXT("127.0.0.1"));
			PC->ClientTravel(LocalURL, TRAVEL_Absolute);
			return true;
		}
	}
#endif //!UE_BUILD_SHIPPING

	return false;
}
