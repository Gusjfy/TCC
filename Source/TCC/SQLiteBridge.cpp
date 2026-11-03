#include "SQLiteBridge.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "SQLiteDatabase.h"
#include "SQLitePreparedStatement.h"

// ---------------------------------------------------------------------------
// Caminhos
// ---------------------------------------------------------------------------

FString USQLiteBridge::CaminhoBanco()
{
	// Saved/ e gravavel tanto no editor quanto no build empacotado
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Databases"), TEXT("sql_test.db"));
}

FString USQLiteBridge::CaminhoScript()
{
	// Precisa de DirectoriesToAlwaysStageAsNonUFS=(Path="SQL") no DefaultGame.ini
	// para o arquivo ir solto no build empacotado
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("SQL"), TEXT("caso_maria.sql"));
}

bool USQLiteBridge::EhConsulta(const FString& Query)
{
	const FString Q = Query.TrimStartAndEnd();
	return Q.StartsWith(TEXT("SELECT"), ESearchCase::IgnoreCase) || Q.StartsWith(TEXT("WITH"), ESearchCase::IgnoreCase);
}

// ---------------------------------------------------------------------------
// Banco: recriar a partir do script
// ---------------------------------------------------------------------------

TArray<FString> USQLiteBridge::DividirScript(const FString& Script)
{
	// Separa por ';' ignorando os que estao dentro de '...' e comentarios "-- ..."
	TArray<FString> Comandos;
	FString Atual;
	bool bDentroDeTexto = false;

	for (int32 i = 0; i < Script.Len(); i++)
	{
		const TCHAR C = Script[i];

		if (!bDentroDeTexto && C == TEXT('-') && i + 1 < Script.Len() && Script[i + 1] == TEXT('-'))
		{
			// pula ate o fim da linha
			while (i < Script.Len() && Script[i] != TEXT('\n'))
			{
				i++;
			}
			Atual += TEXT('\n');
			continue;
		}

		if (C == TEXT('\''))
		{
			// '' dentro de texto vira dois toggles seguidos, entao continua correto
			bDentroDeTexto = !bDentroDeTexto;
		}

		if (C == TEXT(';') && !bDentroDeTexto)
		{
			const FString Limpo = Atual.TrimStartAndEnd();
			if (!Limpo.IsEmpty())
			{
				Comandos.Add(Limpo);
			}
			Atual.Reset();
			continue;
		}

		Atual += C;
	}

	const FString Resto = Atual.TrimStartAndEnd();
	if (!Resto.IsEmpty())
	{
		Comandos.Add(Resto);
	}

	return Comandos;
}

bool USQLiteBridge::RecriarBanco(FString& MensagemErro)
{
	MensagemErro.Reset();

	FString Script;
	if (!FFileHelper::LoadFileToString(Script, *CaminhoScript()))
	{
		MensagemErro = FString::Printf(TEXT("Script do banco nao encontrado: %s"), *CaminhoScript());
		UE_LOG(LogTemp, Error, TEXT("%s"), *MensagemErro);
		return false;
	}

	const FString DbPath = CaminhoBanco();
	IFileManager& FM = IFileManager::Get();
	FM.MakeDirectory(*FPaths::GetPath(DbPath), true);

	if (FM.FileExists(*DbPath) && !FM.Delete(*DbPath, false, true, true))
	{
		MensagemErro = TEXT("Nao foi possivel apagar o banco antigo (arquivo em uso?).");
		UE_LOG(LogTemp, Error, TEXT("%s"), *MensagemErro);
		return false;
	}
	FM.Delete(*(DbPath + TEXT("-journal")), false, true, true);

	FSQLiteDatabase Database;
	if (!Database.Open(*DbPath, ESQLiteDatabaseOpenMode::ReadWriteCreate))
	{
		MensagemErro = TEXT("Nao foi possivel criar o banco: ") + Database.GetLastError();
		UE_LOG(LogTemp, Error, TEXT("%s"), *MensagemErro);
		return false;
	}

	auto Rodar = [&Database](const FString& Sql, FString& Erro) -> bool
	{
		FSQLitePreparedStatement Stmt;
		if (!Stmt.Create(Database, *Sql) || !Stmt.IsValid())
		{
			Erro = Database.GetLastError();
			Stmt.Destroy();
			return false;
		}
		const bool bOk = Stmt.Execute();
		if (!bOk)
		{
			Erro = Database.GetLastError();
		}
		Stmt.Destroy();
		return bOk;
	};

	const TArray<FString> Comandos = DividirScript(Script);
	FString Erro;

	// Uma transacao so: deixa a criacao instantanea
	Rodar(TEXT("BEGIN TRANSACTION"), Erro);

	for (int32 i = 0; i < Comandos.Num(); i++)
	{
		if (!Rodar(Comandos[i], Erro))
		{
			MensagemErro = FString::Printf(TEXT("Erro no comando %d do script: %s\n%s"), i + 1, *Erro, *Comandos[i].Left(200));
			UE_LOG(LogTemp, Error, TEXT("%s"), *MensagemErro);
			Rodar(TEXT("ROLLBACK"), Erro);
			Database.Close();
			return false;
		}
	}

	Rodar(TEXT("COMMIT"), Erro);
	Database.Close();

	UE_LOG(LogTemp, Log, TEXT("Banco recriado com %d comandos."), Comandos.Num());
	return true;
}

bool USQLiteBridge::GarantirBanco(FString& MensagemErro)
{
	MensagemErro.Reset();
	if (IFileManager::Get().FileExists(*CaminhoBanco()))
	{
		return true;
	}
	return RecriarBanco(MensagemErro);
}

// ---------------------------------------------------------------------------
// Consultas
// ---------------------------------------------------------------------------

bool USQLiteBridge::RodarConsulta(const FString& Query, FResultadoSQL& Saida)
{
	// Banco somente leitura: o gabarito nunca altera nada
	FSQLiteDatabase Database;
	if (!Database.Open(*CaminhoBanco(), ESQLiteDatabaseOpenMode::ReadOnly))
	{
		return false;
	}

	FSQLitePreparedStatement Statement;
	if (!Statement.Create(Database, *Query) || !Statement.IsValid())
	{
		Statement.Destroy();
		Database.Close();
		return false;
	}

	Saida.NomesColunas = Statement.GetColumnNames();
	Saida.Registros.Reset();

	while (Statement.Step() == ESQLitePreparedStatementStepResult::Row)
	{
		FLinhaSQL Linha;
		for (int32 i = 0; i < Saida.NomesColunas.Num(); i++)
		{
			FString Valor;
			Statement.GetColumnValueByIndex(i, Valor);
			Linha.Valores.Add(Valor);
		}
		Saida.Registros.Add(Linha);
	}

	Statement.Destroy();
	Database.Close();
	Saida.bSucesso = true;
	Saida.bFoiConsulta = true;
	return true;
}

FResultadoSQL USQLiteBridge::ExecutarQuery(FString QueryTexto)
{
	FResultadoSQL Retorno;
	Retorno.bSucesso = false;

	// BLINDAGEM 1: texto vazio ou so espacos
	const FString QueryLimpa = QueryTexto.TrimStartAndEnd();
	if (QueryLimpa.IsEmpty())
	{
		Retorno.TextoTerminal = TEXT("ERRO: Nenhuma query inserida. Digite um comando SQL.");
		return Retorno;
	}

	// BLINDAGEM 2: se o banco sumiu, recria antes de rodar
	FString ErroBanco;
	if (!GarantirBanco(ErroBanco))
	{
		Retorno.TextoTerminal = TEXT("ERRO FATAL: Banco indisponivel.\n") + ErroBanco;
		return Retorno;
	}

	FSQLiteDatabase Database;
	if (!Database.Open(*CaminhoBanco(), ESQLiteDatabaseOpenMode::ReadWrite))
	{
		Retorno.TextoTerminal = TEXT("ERRO FATAL: Nao foi possivel conectar ao banco de dados.\n") + Database.GetLastError();
		return Retorno;
	}

	FSQLitePreparedStatement Statement;

	// BLINDAGEM 3: erro de sintaxe
	if (!Statement.Create(Database, *QueryLimpa))
	{
		const FString Erro = Database.GetLastError();
		Database.Close();
		Retorno.TextoTerminal = FString::Printf(TEXT("ERRO DE SINTAXE SQL:\n%s"), *Erro);
		return Retorno;
	}

	// BLINDAGEM 4: statement invalido (evita assertion)
	if (!Statement.IsValid())
	{
		Statement.Destroy();
		Database.Close();
		Retorno.TextoTerminal = TEXT("ERRO DE EXECUCAO: Comando invalido ou em branco detectado.");
		return Retorno;
	}

	FString ResultadoFinal;

	if (EhConsulta(QueryLimpa))
	{
		Retorno.bFoiConsulta = true;
		Retorno.NomesColunas = Statement.GetColumnNames();

		int32 ContadorRegistros = 1;

		ResultadoFinal += TEXT("================================================\n");
		ResultadoFinal += TEXT("RESULTADOS DA CONSULTA:\n");
		ResultadoFinal += TEXT("================================================\n\n");

		while (Statement.Step() == ESQLitePreparedStatementStepResult::Row)
		{
			ResultadoFinal += FString::Printf(TEXT("--- [ REGISTRO %d ] ---\n"), ContadorRegistros);

			FLinhaSQL NovaLinha;
			for (int32 i = 0; i < Retorno.NomesColunas.Num(); i++)
			{
				FString ValorColuna;
				Statement.GetColumnValueByIndex(i, ValorColuna);

				NovaLinha.Valores.Add(ValorColuna);
				ResultadoFinal += Retorno.NomesColunas[i] + TEXT(": ") + ValorColuna + TEXT("\n");
			}
			Retorno.Registros.Add(NovaLinha);

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
	return Retorno;
}

// ---------------------------------------------------------------------------
// Progressao
// ---------------------------------------------------------------------------

bool USQLiteBridge::ValidarComGabarito(const FResultadoSQL& ResultadoJogador, FString QueryGabarito)
{
	if (!ResultadoJogador.bSucesso || !ResultadoJogador.bFoiConsulta)
	{
		return false;
	}

	if (!EhConsulta(QueryGabarito))
	{
		UE_LOG(LogTemp, Warning, TEXT("Gabarito precisa ser SELECT: %s"), *QueryGabarito);
		return false;
	}

	FResultadoSQL Gabarito;
	if (!RodarConsulta(QueryGabarito.TrimStartAndEnd(), Gabarito))
	{
		UE_LOG(LogTemp, Warning, TEXT("Gabarito com erro: %s"), *QueryGabarito);
		return false;
	}

	if (Gabarito.Registros.Num() != ResultadoJogador.Registros.Num())
	{
		return false;
	}

	// Cada linha do gabarito precisa casar com uma linha diferente do jogador
	// que contenha todos os valores dela (contando repeticoes)
	TArray<bool> LinhaUsada;
	LinhaUsada.Init(false, ResultadoJogador.Registros.Num());

	for (const FLinhaSQL& LinhaGabarito : Gabarito.Registros)
	{
		bool bAchou = false;

		for (int32 j = 0; j < ResultadoJogador.Registros.Num() && !bAchou; j++)
		{
			if (LinhaUsada[j])
			{
				continue;
			}

			TMap<FString, int32> Disponiveis;
			for (const FString& V : ResultadoJogador.Registros[j].Valores)
			{
				Disponiveis.FindOrAdd(V.TrimStartAndEnd())++;
			}

			bool bContemTudo = true;
			for (const FString& V : LinhaGabarito.Valores)
			{
				int32* Qtd = Disponiveis.Find(V.TrimStartAndEnd());
				if (Qtd == nullptr || *Qtd <= 0)
				{
					bContemTudo = false;
					break;
				}
				(*Qtd)--;
			}

			if (bContemTudo)
			{
				LinhaUsada[j] = true;
				bAchou = true;
			}
		}

		if (!bAchou)
		{
			return false;
		}
	}

	return true;
}

bool USQLiteBridge::ChecarEstadoBanco(FString QueryVerificacao)
{
	if (!EhConsulta(QueryVerificacao))
	{
		UE_LOG(LogTemp, Warning, TEXT("Verificacao precisa ser SELECT: %s"), *QueryVerificacao);
		return false;
	}

	FResultadoSQL Saida;
	return RodarConsulta(QueryVerificacao.TrimStartAndEnd(), Saida) && Saida.Registros.Num() > 0;
}
