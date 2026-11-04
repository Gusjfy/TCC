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

FString USQLiteBridge::CaminhoScript(const FString& NomeArquivo)
{
	// Precisa de DirectoriesToAlwaysStageAsNonUFS=(Path="SQL") no DefaultGame.ini
	// para os arquivos irem soltos no build empacotado
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("SQL"), NomeArquivo);
}

bool USQLiteBridge::EhConsulta(const FString& Query)
{
	const FString Q = Query.TrimStartAndEnd();
	return Q.StartsWith(TEXT("SELECT"), ESearchCase::IgnoreCase) || Q.StartsWith(TEXT("WITH"), ESearchCase::IgnoreCase);
}

// Tabelas criadas pelo caso_maria.sql. As outras tabelas sao do jogador.
static const TCHAR* TabelasDoCaso[] = {
	TEXT("SUSPEITOS"), TEXT("AMBIENTES"), TEXT("MOVIMENTACAO"),
	TEXT("EVIDENCIAS"), TEXT("HISTORICO_MENSAGENS"), TEXT("SYS_AUDIT_LOG")
};

bool USQLiteBridge::DanificaTabelaDoCaso(const FString& Query)
{
	// Bloqueia DROP TABLE e ALTER TABLE (RENAME / DROP COLUMN) nas tabelas do caso.
	// O jogador pode criar, alterar e apagar as proprias tabelas (ex: provas_pedro)
	// e pode fazer ALTER TABLE ... ADD COLUMN em qualquer tabela.
	FString Q = Query.ToUpper();
	for (const TCHAR* Sinal : { TEXT("\""), TEXT("'"), TEXT("`"), TEXT("["), TEXT("]"), TEXT(";"), TEXT("("), TEXT(")"), TEXT("\t"), TEXT("\r"), TEXT("\n") })
	{
		Q.ReplaceInline(Sinal, TEXT(" "));
	}

	TArray<FString> Palavras;
	Q.ParseIntoArrayWS(Palavras);
	if (Palavras.Num() < 3 || Palavras[1] != TEXT("TABLE"))
	{
		return false;
	}

	const bool bDrop = Palavras[0] == TEXT("DROP");
	const bool bAlter = Palavras[0] == TEXT("ALTER");
	if (!bDrop && !bAlter)
	{
		return false;
	}

	// Nome da tabela: pula "IF EXISTS" e um eventual prefixo "main."
	int32 IndiceNome = 2;
	if (bDrop && Palavras.Num() > 4 && Palavras[2] == TEXT("IF") && Palavras[3] == TEXT("EXISTS"))
	{
		IndiceNome = 4;
	}
	FString Nome = Palavras[IndiceNome];
	Nome.RemoveFromStart(TEXT("MAIN."));

	bool bTabelaDoCaso = false;
	for (const TCHAR* Tabela : TabelasDoCaso)
	{
		if (Nome == Tabela)
		{
			bTabelaDoCaso = true;
			break;
		}
	}
	if (!bTabelaDoCaso)
	{
		return false;
	}

	if (bDrop)
	{
		return true;
	}

	// ALTER TABLE <tabela> ADD [COLUMN] ... e permitido
	return !(Palavras.Num() > IndiceNome + 1 && Palavras[IndiceNome + 1] == TEXT("ADD"));
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

bool USQLiteBridge::RodarScript(const FString& DbPath, const FString& NomeArquivo, bool bCriarArquivo, FString& MensagemErro)
{
	FString Script;
	if (!FFileHelper::LoadFileToString(Script, *CaminhoScript(NomeArquivo)))
	{
		MensagemErro = FString::Printf(TEXT("Script nao encontrado: %s"), *CaminhoScript(NomeArquivo));
		UE_LOG(LogTemp, Error, TEXT("%s"), *MensagemErro);
		return false;
	}

	FSQLiteDatabase Database;
	const ESQLiteDatabaseOpenMode Modo = bCriarArquivo ? ESQLiteDatabaseOpenMode::ReadWriteCreate : ESQLiteDatabaseOpenMode::ReadWrite;
	if (!Database.Open(*DbPath, Modo))
	{
		MensagemErro = TEXT("Nao foi possivel abrir o banco: ") + Database.GetLastError();
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

	// Uma transacao so: tudo ou nada, e bem mais rapido
	Rodar(TEXT("BEGIN TRANSACTION"), Erro);

	for (int32 i = 0; i < Comandos.Num(); i++)
	{
		if (!Rodar(Comandos[i], Erro))
		{
			MensagemErro = FString::Printf(TEXT("Erro no comando %d de %s: %s\n%s"), i + 1, *NomeArquivo, *Erro, *Comandos[i].Left(200));
			UE_LOG(LogTemp, Error, TEXT("%s"), *MensagemErro);
			Rodar(TEXT("ROLLBACK"), Erro);
			Database.Close();
			return false;
		}
	}

	Rodar(TEXT("COMMIT"), Erro);
	Database.Close();

	UE_LOG(LogTemp, Log, TEXT("%s: %d comandos executados."), *NomeArquivo, Comandos.Num());
	return true;
}

bool USQLiteBridge::RecriarBanco(FString& MensagemErro)
{
	MensagemErro.Reset();

	if (!IFileManager::Get().FileExists(*CaminhoScript(TEXT("caso_maria.sql"))))
	{
		MensagemErro = FString::Printf(TEXT("Script do banco nao encontrado: %s"), *CaminhoScript(TEXT("caso_maria.sql")));
		UE_LOG(LogTemp, Error, TEXT("%s"), *MensagemErro);
		return false;
	}

	const FString DbPath = CaminhoBanco();
	IFileManager& FM = IFileManager::Get();
	FM.MakeDirectory(*FPaths::GetPath(DbPath), true);

	// O banco antigo vira uma copia temporaria, para devolver as tabelas do jogador depois
	const FString DbAnterior = DbPath + TEXT(".anterior");
	FM.Delete(*DbAnterior, false, true, true);
	const bool bTinhaBanco = FM.FileExists(*DbPath);
	if (bTinhaBanco && !FM.Move(*DbAnterior, *DbPath, true, true))
	{
		MensagemErro = TEXT("Nao foi possivel apagar o banco antigo (arquivo em uso?).");
		UE_LOG(LogTemp, Error, TEXT("%s"), *MensagemErro);
		return false;
	}
	FM.Delete(*(DbPath + TEXT("-journal")), false, true, true);

	if (!RodarScript(DbPath, TEXT("caso_maria.sql"), true, MensagemErro))
	{
		// Falhou: devolve o banco antigo para o jogo nao ficar sem banco
		FM.Delete(*DbPath, false, true, true);
		if (bTinhaBanco)
		{
			FM.Move(*DbPath, *DbAnterior, true, true);
		}
		return false;
	}

	if (bTinhaBanco)
	{
		CopiarTabelasDoJogador(DbAnterior, DbPath);
		FM.Delete(*DbAnterior, false, true, true);
	}
	return true;
}

void USQLiteBridge::CopiarTabelasDoJogador(const FString& DbAnterior, const FString& DbNovo)
{
	// Melhor esforco: se o banco antigo estiver estragado, so nao copia nada
	FSQLiteDatabase Database;
	if (!Database.Open(*DbNovo, ESQLiteDatabaseOpenMode::ReadWrite))
	{
		return;
	}

	auto Rodar = [&Database](const FString& Sql) -> bool
	{
		FSQLitePreparedStatement Stmt;
		if (!Stmt.Create(Database, *Sql) || !Stmt.IsValid())
		{
			Stmt.Destroy();
			return false;
		}
		const bool bOk = Stmt.Execute();
		Stmt.Destroy();
		return bOk;
	};

	const FString CaminhoSql = DbAnterior.Replace(TEXT("'"), TEXT("''"));
	if (!Rodar(FString::Printf(TEXT("ATTACH DATABASE '%s' AS antigo"), *CaminhoSql)))
	{
		UE_LOG(LogTemp, Warning, TEXT("Restaurar: nao foi possivel ler o banco antigo. %s"), *Database.GetLastError());
		Database.Close();
		return;
	}

	// Lista as tabelas do jogador (tudo que nao e do caso nem interno do SQLite)
	TArray<FString> Nomes;
	TArray<FString> Criacoes;
	{
		FSQLitePreparedStatement Lista;
		if (Lista.Create(Database, TEXT("SELECT name, sql FROM antigo.sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%'")) && Lista.IsValid())
		{
			while (Lista.Step() == ESQLitePreparedStatementStepResult::Row)
			{
				FString Nome, Sql;
				Lista.GetColumnValueByIndex(0, Nome);
				Lista.GetColumnValueByIndex(1, Sql);

				bool bDoCaso = false;
				for (const TCHAR* Tabela : TabelasDoCaso)
				{
					if (Nome.Equals(Tabela, ESearchCase::IgnoreCase))
					{
						bDoCaso = true;
						break;
					}
				}
				if (!bDoCaso && !Sql.IsEmpty())
				{
					Nomes.Add(Nome);
					Criacoes.Add(Sql);
				}
			}
		}
		Lista.Destroy();
	}

	for (int32 i = 0; i < Nomes.Num(); i++)
	{
		const FString NomeSql = Nomes[i].Replace(TEXT("\""), TEXT("\"\""));
		// O sql guardado e o CREATE TABLE original do jogador; roda no banco novo (main)
		if (Rodar(Criacoes[i]))
		{
			Rodar(FString::Printf(TEXT("INSERT INTO main.\"%s\" SELECT * FROM antigo.\"%s\""), *NomeSql, *NomeSql));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Restaurar: nao foi possivel recriar a tabela %s. %s"), *Nomes[i], *Database.GetLastError());
		}
	}

	Rodar(TEXT("DETACH DATABASE antigo"));
	Database.Close();
}

bool USQLiteBridge::ExecutarScriptSQL(FString NomeArquivo, FString& MensagemErro)
{
	MensagemErro.Reset();
	if (!GarantirBanco(MensagemErro))
	{
		return false;
	}
	return RodarScript(CaminhoBanco(), NomeArquivo, false, MensagemErro);
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

	// BLINDAGEM 2: o jogador nao pode apagar nem renomear as tabelas do caso
	if (DanificaTabelaDoCaso(QueryLimpa))
	{
		Retorno.TextoTerminal = TEXT("ACESSO NEGADO: tabela protegida pelo sistema da policia.\nVoce pode criar, alterar e apagar apenas as suas proprias tabelas.");
		return Retorno;
	}

	// BLINDAGEM 3: se o banco sumiu, recria antes de rodar
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

	// BLINDAGEM 4: erro de sintaxe
	if (!Statement.Create(Database, *QueryLimpa))
	{
		const FString Erro = Database.GetLastError();
		Database.Close();
		Retorno.TextoTerminal = FString::Printf(TEXT("ERRO DE SINTAXE SQL:\n%s"), *Erro);
		return Retorno;
	}

	// BLINDAGEM 5: statement invalido (evita assertion)
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

bool USQLiteBridge::BuscarTabelaDoJogador(FString ColunaExigida, FString& NomeTabela, int32& NumLinhas)
{
	NomeTabela.Reset();
	NumLinhas = 0;

	FResultadoSQL Tabelas;
	if (!RodarConsulta(TEXT("SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%' ORDER BY rowid"), Tabelas))
	{
		return false;
	}

	const FString Coluna = ColunaExigida.TrimStartAndEnd().Replace(TEXT("'"), TEXT("''"));
	bool bAchou = false;

	for (const FLinhaSQL& Linha : Tabelas.Registros)
	{
		if (Linha.Valores.Num() == 0)
		{
			continue;
		}
		const FString& Nome = Linha.Valores[0];

		bool bDoCaso = false;
		for (const TCHAR* Tabela : TabelasDoCaso)
		{
			if (Nome.Equals(Tabela, ESearchCase::IgnoreCase))
			{
				bDoCaso = true;
				break;
			}
		}
		if (bDoCaso)
		{
			continue;
		}

		const FString NomeAspas = Nome.Replace(TEXT("'"), TEXT("''"));
		if (!Coluna.IsEmpty())
		{
			FResultadoSQL TemColuna;
			const FString Q = FString::Printf(TEXT("SELECT 1 FROM pragma_table_info('%s') WHERE name = '%s' COLLATE NOCASE"), *NomeAspas, *Coluna);
			if (!RodarConsulta(Q, TemColuna) || TemColuna.Registros.Num() == 0)
			{
				continue;
			}
		}

		FResultadoSQL Contagem;
		const FString NomeSql = Nome.Replace(TEXT("\""), TEXT("\"\""));
		int32 Linhas = 0;
		if (RodarConsulta(FString::Printf(TEXT("SELECT COUNT(*) FROM \"%s\""), *NomeSql), Contagem)
			&& Contagem.Registros.Num() > 0 && Contagem.Registros[0].Valores.Num() > 0)
		{
			Linhas = FCString::Atoi(*Contagem.Registros[0].Valores[0]);
		}

		// Se houver mais de uma tabela do jogador, fica com a que tem mais linhas
		if (!bAchou || Linhas > NumLinhas)
		{
			NomeTabela = Nome;
			NumLinhas = Linhas;
			bAchou = true;
		}
	}

	return bAchou;
}
