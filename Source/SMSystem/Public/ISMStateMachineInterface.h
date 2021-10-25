// Copyright Recursoft LLC 2019-2020. All Rights Reserved.
#pragma once

#include "UObject/Interface.h"
#include "SMNode_Base.h"
#include "ISMStateMachineInterface.generated.h"


UENUM(BlueprintType)
enum ESMNetworkConfigurationType
{
	SM_Client			UMETA(DisplayName = "Client"),
	SM_Server			UMETA(DisplayName = "Server"),
	SM_ClientAndServer	UMETA(DisplayName = "ClientAndServer")
};

UINTERFACE(BlueprintType)
class SMSYSTEM_API USMInstanceInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class SMSYSTEM_API ISMInstanceInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	virtual UObject* GetContext() const;
};

UINTERFACE(BlueprintType)
class SMSYSTEM_API USMStateMachineInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class SMSYSTEM_API ISMStateMachineInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	/** Initialize bound functions and load in the context. */
	virtual void Initialize(UObject* Context = nullptr);

	/** Start the root state machine. */
	virtual void Start();

	/** Manual way of updating the root state machine if tick is disabled. */
	virtual void Update(float DeltaSeconds);

	/** This will complete the state machine's current state and force the machine to end regardless of if the state is an end state. */
	virtual void Stop();

	/** Shutdown this instance. Calls Stop.*/
	virtual void Shutdown();
};

UINTERFACE(BlueprintType)
class SMSYSTEM_API USMStateMachineNetworkedInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class SMSYSTEM_API ISMStateMachineNetworkedInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	virtual void ProcessTransaction(const TArray<FSMNetworkedTransaction>& Transactions);
	virtual bool ShouldReplicateStates() const;
};