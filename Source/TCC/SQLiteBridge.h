#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SQLiteBridge.generated.h"

// Struct que representa uma unica linha da tabela
USTRUCT(BlueprintType)
struct FLinhaSQL
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SQL")
	TArray<FString> Valores;
};

USTRUCT(BlueprintType)
struct FResultadoSQL
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SQL")
	bool bSucesso = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Motor SQL")
	FString TextoTerminal = TEXT("");

	// LEGADO: mantido para nao quebrar Blueprints antigos. Use ValidarComGabarito.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Progressao")
	bool bEncontrouPistaJose = false;

	// Dados para a tabela SGBD
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Motor SQL")
	TArray<FString> NomesColunas;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Motor SQL")
	TArray<FLinhaSQL> Registros;

	// true quando a query era uma consulta (SELECT / WITH)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Motor SQL")
	bool bFoiConsulta = false;
};

UCLASS()
class TCC_API USQLiteBridge : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Executa a query do jogador e devolve texto + colunas + linhas
	UFUNCTION(BlueprintCallable, Category = "Motor SQL")
	static FResultadoSQL ExecutarQuery(FString QueryTexto);

	// Apaga o banco do jogador e recria do zero a partir de Content/SQL/caso_maria.sql.
	// As tabelas criadas pelo jogador (ex: provas_pedro) sao copiadas para o banco novo.
	// Chamar em "Iniciar nova instancia" e no "Restaurar backup" do SYS_CONFIG.
	UFUNCTION(BlueprintCallable, Category = "Motor SQL")
	static bool RecriarBanco(FString& MensagemErro);

	// Recria o banco somente se o arquivo ainda nao existir (chamar ao carregar um save).
	UFUNCTION(BlueprintCallable, Category = "Motor SQL")
	static bool GarantirBanco(FString& MensagemErro);

	// Roda um script de Content/SQL/ no banco atual (ex: "glitch_dia3.sql").
	// Use para eventos da historia que alteram o banco.
	UFUNCTION(BlueprintCallable, Category = "Motor SQL")
	static bool ExecutarScriptSQL(FString NomeArquivo, FString& MensagemErro);

	// Compara o resultado do jogador com o resultado da query-gabarito do objetivo.
	// Passa se: mesmo numero de linhas E cada linha do gabarito tem todos os seus valores
	// numa linha diferente do jogador. Assim SELECT * e SELECT nome valem igual,
	// e a ordem das linhas e das colunas nao importa.
	UFUNCTION(BlueprintCallable, Category = "Progressao")
	static bool ValidarComGabarito(const FResultadoSQL& ResultadoJogador, FString QueryGabarito);

	// Para objetivos de UPDATE/INSERT: roda uma consulta de verificacao e passa se ela
	// devolver pelo menos uma linha. Ex: SELECT 1 FROM suspeitos WHERE id = 3 AND status = 'Livre'
	UFUNCTION(BlueprintCallable, Category = "Progressao")
	static bool ChecarEstadoBanco(FString QueryVerificacao);

private:
	static FString CaminhoBanco();
	static FString CaminhoScript(const FString& NomeArquivo);
	static bool EhConsulta(const FString& Query);
	static bool DanificaTabelaDoCaso(const FString& Query);
	static void CopiarTabelasDoJogador(const FString& DbAnterior, const FString& DbNovo);
	static bool RodarConsulta(const FString& Query, FResultadoSQL& Saida);
	static TArray<FString> DividirScript(const FString& Script);
	static bool RodarScript(const FString& DbPath, const FString& NomeArquivo, bool bCriarArquivo, FString& MensagemErro);
};
