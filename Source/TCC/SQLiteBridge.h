#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SQLiteBridge.generated.h"

// Nosso pacote de dados (Struct) que será enviado para o Blueprint
USTRUCT(BlueprintType)
struct FResultadoSQL
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Motor SQL")
	FString TextoTerminal;

	UPROPERTY(BlueprintReadOnly, Category = "Progressao")
	bool bEncontrouPistaJose = false;

	UPROPERTY(BlueprintReadWrite, Category = "SQL")
	bool bSucesso = false;
};

UCLASS()
class TCC_API USQLiteBridge : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// A função agora devolve a Struct em vez de apenas uma FString
	UFUNCTION(BlueprintCallable, Category = "Motor SQL")
	static FResultadoSQL ExecutarQuery(FString QueryTexto);
};