#include "SQLiteBridge.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "SQLiteDatabase.h"
#include "SQLitePreparedStatement.h"

FResultadoSQL USQLiteBridge::ExecutarQuery(FString QueryTexto)
{
	FResultadoSQL Retorno;
	// Assumimos que falhou até provar o contrário
	Retorno.bSucesso = false;

	// ---------------------------------------------------------
	// BLINDAGEM 1: Evitar Crash por texto vazio ou espaços
	// ---------------------------------------------------------
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
		Retorno.TextoTerminal = TEXT("ERRO FATAL: Nao foi possivel conectar ao banco de dados.");
		return Retorno;
	}

	FSQLitePreparedStatement Statement;

	// ---------------------------------------------------------
	// BLINDAGEM 2: Se o SQLite falhar ao criar o statement
	// ---------------------------------------------------------
	if (!Statement.Create(Database, *QueryLimpa))
	{
		FString Erro = Database.GetLastError();
		Database.Close();
		Retorno.TextoTerminal = FString::Printf(TEXT("ERRO DE SINTAXE SQL:\n%s"), *Erro);
		return Retorno;
	}

	// ---------------------------------------------------------
	// BLINDAGEM 3: Impedir o Assertion (O erro que crashou a engine)
	// ---------------------------------------------------------
	if (!Statement.IsValid())
	{
		Statement.Destroy();
		Database.Close();
		Retorno.TextoTerminal = TEXT("ERRO DE EXECUCAO: Comando invalido ou em branco detectado.");
		return Retorno;
	}

	// Executa a Query e formata a string (Camada 1)
	if (QueryLimpa.StartsWith(TEXT("SELECT"), ESearchCase::IgnoreCase))
	{
		TArray<FString> NomesColunas = Statement.GetColumnNames();
		int32 ContadorRegistros = 1;

		ResultadoFinal += TEXT("================================================\n");
		ResultadoFinal += TEXT("RESULTADOS DA CONSULTA:\n");
		ResultadoFinal += TEXT("================================================\n\n");

		while (Statement.Step() == ESQLitePreparedStatementStepResult::Row)
		{
			ResultadoFinal += FString::Printf(TEXT("--- [ REGISTRO %d ] ---\n"), ContadorRegistros);

			for (int32 i = 0; i < NomesColunas.Num(); i++)
			{
				FString ValorColuna;
				Statement.GetColumnValueByIndex(i, ValorColuna);

				// Formata como "nome_coluna: valor"
				ResultadoFinal += NomesColunas[i] + TEXT(": ") + ValorColuna + TEXT("\n");
			}
			ResultadoFinal += TEXT("\n"); // Espaço extra entre os registros
			ContadorRegistros++;
		}

		// Caso não retorne nenhuma linha
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
			// A execução de DML/DDL (INSERT, UPDATE, CREATE) foi bem sucedida!
			Retorno.bSucesso = true;
		}
		else
		{
			ResultadoFinal = FString::Printf(TEXT("FALHA NA EXECUCAO:\n%s"), *Database.GetLastError());
			// Se caiu aqui, a sintaxe estava certa, mas deu erro de integridade (ex: tabela não existe). bSucesso continua false.
		}
	}

	Statement.Destroy();
	Database.Close();

	Retorno.TextoTerminal = ResultadoFinal;

	// ---------------------------------------------------------
	// Camada 2: Validação e Progressão
	// ---------------------------------------------------------
	FString QueryMin = QueryLimpa.ToLower();

	if (QueryMin.Contains(TEXT("select")) && QueryMin.Contains(TEXT("suspicion_level")) && QueryMin.Contains(TEXT("4")))
	{
		Retorno.bEncontrouPistaJose = true;
	}

	return Retorno;
}