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

	// Apaga o banco inteiro (inclusive as tabelas criadas pelo jogador) e recria do zero
	// a partir de Content/SQL/caso_maria.sql. Chamar no "Novo jogo".
	UFUNCTION(BlueprintCallable, Category = "Motor SQL")
	static bool NovoBancoLimpo(FString& MensagemErro);

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

	// Para a DDL do Dia 3: procura uma tabela criada pelo jogador (qualquer nome que nao
	// seja das 6 do caso). Se ColunaExigida nao for vazia, so aceita tabela com essa coluna
	// (ex: "observacao" para o ALTER). Devolve o nome e quantas linhas ela tem.
	// Objetivos: CREATE = retorno true. INSERT = true e NumLinhas > 0.
	UFUNCTION(BlueprintCallable, Category = "Progressao")
	static bool BuscarTabelaDoJogador(FString ColunaExigida, FString& NomeTabela, int32& NumLinhas);

	// Roda "SELECT 1 FROM <dossie do jogador> WHERE <Condicao>" e passa se achar linha.
	// Usar na acusacao. Ex: LOWER(TRIM(tabela_origem)) = 'sys_audit_log' AND registro_id IN
	// (SELECT id FROM sys_audit_log WHERE terminal = 'PREF-TI-07')
	UFUNCTION(BlueprintCallable, Category = "Progressao")
	static bool DossieContem(FString Condicao, FString& NomeTabela);

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
