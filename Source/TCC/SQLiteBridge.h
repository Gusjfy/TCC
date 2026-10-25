#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SQLiteBridge.generated.h"

// Struct que representa uma única linha da tabela
USTRUCT(BlueprintType)
struct FLinhaSQL
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SQL")
	TArray<FString> Valores;
};

// Atualize sua FResultadoSQL para ficar assim:
USTRUCT(BlueprintType)
struct FResultadoSQL
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SQL")
	bool bSucesso = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Motor SQL")
	FString TextoTerminal = TEXT("");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Progressao")
	bool bEncontrouPistaJose = false;

	// NOVAS VARIÁVEIS PARA A TABELA SGBD:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Motor SQL")
	TArray<FString> NomesColunas;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Motor SQL")
	TArray<FLinhaSQL> Registros;
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