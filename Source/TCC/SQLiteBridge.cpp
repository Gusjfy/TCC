#include "SQLiteBridge.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "SQLiteDatabase.h"
#include "SQLitePreparedStatement.h"

FResultadoSQL USQLiteBridge::ExecutarQuery(FString QueryTexto)
{
	FResultadoSQL Retorno;
	Retorno.bSucesso = false;

	// BLINDAGEM 1: Evitar Crash por texto vazio ou espaços
	FString QueryLimpa = QueryTexto.TrimStartAndEnd();

	if (QueryLimpa.IsEmpty())
	{
		Retorno.TextoTerminal = TEXT("ERRO: Nenhuma query inserida. Digite um comando SQL.");
		return Retorno;
	}

	FString ResultadoFinal = TEXT("");

	const FString DbFolder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Databases"));
	const FString DbPath = FPaths::Combine(DbFolder, TEXT("sql_test.db"));

	FSQLiteDatabase Database;

	if (!Database.Open(*DbPath, ESQLiteDatabaseOpenMode::ReadWriteCreate))
	{
		Retorno.TextoTerminal = TEXT("ERRO FATAL: Nao foi possivel conectar ao banco de dados.\n") + Database.GetLastError();
		return Retorno;
	}

	FSQLitePreparedStatement Statement;

	// BLINDAGEM 2: Se o SQLite falhar ao criar o statement
	if (!Statement.Create(Database, *QueryLimpa))
	{
		FString Erro = Database.GetLastError();
		Database.Close();
		Retorno.TextoTerminal = FString::Printf(TEXT("ERRO DE SINTAXE SQL:\n%s"), *Erro);
		return Retorno;
	}

	// BLINDAGEM 3: Impedir o Assertion
	if (!Statement.IsValid())
	{
		Statement.Destroy();
		Database.Close();
		Retorno.TextoTerminal = TEXT("ERRO DE EXECUCAO: Comando invalido ou em branco detectado.");
		return Retorno;
	}

	// Executa a Query e formata a string e a Struct (Camada 1)
	if (QueryLimpa.StartsWith(TEXT("SELECT"), ESearchCase::IgnoreCase))
	{
		TArray<FString> NomesColunas = Statement.GetColumnNames();

		// SALVA OS NOMES DAS COLUNAS PARA A UI
		Retorno.NomesColunas = NomesColunas;

		int32 ContadorRegistros = 1;

		ResultadoFinal += TEXT("================================================\n");
		ResultadoFinal += TEXT("RESULTADOS DA CONSULTA:\n");
		ResultadoFinal += TEXT("================================================\n\n");

		while (Statement.Step() == ESQLitePreparedStatementStepResult::Row)
		{
			ResultadoFinal += FString::Printf(TEXT("--- [ REGISTRO %d ] ---\n"), ContadorRegistros);

			FLinhaSQL NovaLinha; // Cria a linha para armazenar as células separadas

			for (int32 i = 0; i < NomesColunas.Num(); i++)
			{
				FString ValorColuna;
				Statement.GetColumnValueByIndex(i, ValorColuna);

				NovaLinha.Valores.Add(ValorColuna); // Salva o valor puro na célula
				ResultadoFinal += NomesColunas[i] + TEXT(": ") + ValorColuna + TEXT("\n");
			}
			Retorno.Registros.Add(NovaLinha); // Adiciona a linha pronta no Array final

			ResultadoFinal += TEXT("\n");
			ContadorRegistros++;
		}

		if (ContadorRegistros == 1)
		{
			ResultadoFinal += TEXT("Nenhum registro encontrado.\n");
		}

		Retorno.bSucesso = true;
	}
	else
	{
		if (Statement.Execute())
		{
			ResultadoFinal = TEXT("COMANDO EXECUTADO COM SUCESSO.\nLinhas afetadas verificadas no sistema remoto.");
			Retorno.bSucesso = true;
		}
		else
		{
			ResultadoFinal = FString::Printf(TEXT("FALHA NA EXECUCAO:\n%s"), *Database.GetLastError());
		}
	}

	Statement.Destroy();
	Database.Close();

	Retorno.TextoTerminal = ResultadoFinal;

	// Camada 2: Validação e Progressão
	FString QueryMin = QueryLimpa.ToLower();

	if (QueryMin.Contains(TEXT("select")) && QueryMin.Contains(TEXT("suspicion_level")) && QueryMin.Contains(TEXT("4")))
	{
		Retorno.bEncontrouPistaJose = true;
	}

	return Retorno;
}