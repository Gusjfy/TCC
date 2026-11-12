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

// Tira os comentarios do comeco ("-- ..." e "/* ... */"). Sem isso, uma consulta
// que comeca com comentario nao e reconhecida como SELECT, e um DROP TABLE
// depois de um comentario escaparia da protecao das tabelas do caso.
static FString PularComentariosIniciais(const FString& Query)
{
	FString Q = Query.TrimStartAndEnd();
	while (true)
	{
		if (Q.StartsWith(TEXT("--")))
		{
			int32 Fim = INDEX_NONE;
			Q = Q.FindChar(TEXT('\n'), Fim) ? Q.Mid(Fim + 1).TrimStart() : FString();
		}
		else if (Q.StartsWith(TEXT("/*")))
		{
			const int32 Fim = Q.Find(TEXT("*/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 2);
			Q = Fim != INDEX_NONE ? Q.Mid(Fim + 2).TrimStart() : FString();
		}
		else
		{
			return Q;
		}
	}
}

bool USQLiteBridge::EhConsulta(const FString& Query)
{
	const FString Q = PularComentariosIniciais(Query);
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
	FString Q = PularComentariosIniciais(Query).ToUpper();
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

bool USQLiteBridge::NovoBancoLimpo(FString& MensagemErro)
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

	// Jogo novo: nada do banco antigo sobrevive, nem as tabelas do jogador
	FM.Delete(*(DbPath + TEXT(".anterior")), false, true, true);
	FM.Delete(*(DbPath + TEXT("-journal")), false, true, true);
	if (FM.FileExists(*DbPath) && !FM.Delete(*DbPath, false, true, true))
	{
		MensagemErro = TEXT("Nao foi possivel apagar o banco antigo (arquivo em uso?).");
		UE_LOG(LogTemp, Error, TEXT("%s"), *MensagemErro);
		return false;
	}

	if (!RodarScript(DbPath, TEXT("caso_maria.sql"), true, MensagemErro))
	{
		FM.Delete(*DbPath, false, true, true);
		return false;
	}
	return true;
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
// Avisos amigaveis: traduzem os erros do SQLite para dicas de iniciante
// ---------------------------------------------------------------------------

namespace AvisosSQL
{
	// Palavras-chave que o jogador costuma digitar errado. A dica sugere uma destas.
	static const TCHAR* PalavrasChave[] = {
		TEXT("SELECT"), TEXT("FROM"), TEXT("WHERE"), TEXT("JOIN"), TEXT("INNER"), TEXT("LEFT"),
		TEXT("ON"), TEXT("AND"), TEXT("OR"), TEXT("NOT"), TEXT("ORDER"), TEXT("GROUP"), TEXT("BY"),
		TEXT("HAVING"), TEXT("LIMIT"), TEXT("DISTINCT"), TEXT("INSERT"), TEXT("INTO"), TEXT("VALUES"),
		TEXT("UPDATE"), TEXT("SET"), TEXT("DELETE"), TEXT("CREATE"), TEXT("TABLE"), TEXT("ALTER"),
		TEXT("ADD"), TEXT("COLUMN"), TEXT("DROP"), TEXT("LIKE"), TEXT("BETWEEN"), TEXT("NULL"),
		TEXT("ASC"), TEXT("DESC"), TEXT("UNION"), TEXT("OFFSET"), TEXT("EXISTS"),
		TEXT("COUNT"), TEXT("SUM"), TEXT("AVG"), TEXT("MIN"), TEXT("MAX")
	};

	// Palavras validas do SQL que nunca devem ser "corrigidas"
	static const TCHAR* OutrasPalavrasValidas[] = {
		TEXT("AS"), TEXT("IS"), TEXT("IN"), TEXT("IF"), TEXT("ALL"), TEXT("ANY"), TEXT("CASE"), TEXT("WHEN"),
		TEXT("THEN"), TEXT("ELSE"), TEXT("END"), TEXT("OUTER"), TEXT("RIGHT"), TEXT("FULL"), TEXT("CROSS"),
		TEXT("NATURAL"), TEXT("USING"), TEXT("INTERSECT"), TEXT("EXCEPT"), TEXT("WITH"), TEXT("RECURSIVE"),
		TEXT("PRIMARY"), TEXT("KEY"), TEXT("DEFAULT"), TEXT("REFERENCES"), TEXT("FOREIGN"), TEXT("UNIQUE"),
		TEXT("CHECK"), TEXT("CONSTRAINT"), TEXT("COLLATE"), TEXT("NOCASE"), TEXT("CAST"), TEXT("GLOB"),
		TEXT("ESCAPE"), TEXT("RENAME"), TEXT("TO"), TEXT("REPLACE"), TEXT("INDEX"), TEXT("VIEW"),
		TEXT("TRUE"), TEXT("FALSE"), TEXT("INTEGER"), TEXT("INT"), TEXT("TEXT"), TEXT("REAL"), TEXT("BLOB"),
		TEXT("NUMERIC"), TEXT("VARCHAR"), TEXT("CHAR"), TEXT("BOOLEAN"), TEXT("DATE"), TEXT("TIME"),
		TEXT("DATETIME"), TEXT("STRFTIME"), TEXT("LOWER"), TEXT("UPPER"), TEXT("LENGTH"), TEXT("SUBSTR"),
		TEXT("TRIM"), TEXT("ROUND"), TEXT("ABS"), TEXT("COALESCE"), TEXT("IFNULL"), TEXT("INSTR"),
		TEXT("TOTAL"), TEXT("GROUP_CONCAT"), TEXT("MAIN"), TEXT("ROWID"), TEXT("BEGIN"), TEXT("COMMIT"),
		TEXT("ROLLBACK"), TEXT("TRANSACTION"), TEXT("PRAGMA"), TEXT("TEMP"), TEXT("NULLS"), TEXT("FIRST"),
		TEXT("LAST"), TEXT("ESCAPE")
	};

	// Funcoes que o jogador pode usar (para a dica de "no such function")
	static const TCHAR* Funcoes[] = {
		TEXT("COUNT"), TEXT("SUM"), TEXT("AVG"), TEXT("MIN"), TEXT("MAX"), TEXT("LOWER"), TEXT("UPPER"),
		TEXT("LENGTH"), TEXT("SUBSTR"), TEXT("TRIM"), TEXT("REPLACE"), TEXT("ROUND"), TEXT("ABS"),
		TEXT("COALESCE"), TEXT("IFNULL"), TEXT("INSTR"), TEXT("DATE"), TEXT("TIME"), TEXT("DATETIME"),
		TEXT("STRFTIME"), TEXT("GROUP_CONCAT"), TEXT("TOTAL")
	};

	struct FPalavra
	{
		FString Texto;          // como o jogador digitou
		FString Maiuscula;      // para comparar
		int32 Profundidade = 0; // dentro de quantos parenteses
	};

	struct FAnalise
	{
		TArray<FPalavra> Palavras;  // palavras fora de textos e comentarios
		int32 SaldoParenteses = 0;  // > 0: faltou fechar; < 0: sobrou ')'
		bool bAspaAberta = false;   // uma aspa simples foi aberta e nao foi fechada
		bool bTemAspasDuplas = false;
		bool bTemTexto = false;     // tem algum '...' na query
	};

	struct FEsquema
	{
		TArray<FString> Tabelas;
		TArray<FString> Colunas;                  // sem repetir
		TMap<FString, TArray<FString>> ColunasDe; // tabela (maiuscula) -> colunas
		TSet<FString> Nomes;                      // tabelas e colunas, em maiuscula
	};

	static FAnalise Analisar(const FString& Q)
	{
		FAnalise A;
		const int32 N = Q.Len();
		int32 i = 0;
		while (i < N)
		{
			const TCHAR C = Q[i];

			// comentario de linha
			if (C == TEXT('-') && i + 1 < N && Q[i + 1] == TEXT('-'))
			{
				while (i < N && Q[i] != TEXT('\n'))
				{
					i++;
				}
				continue;
			}

			// comentario de bloco
			if (C == TEXT('/') && i + 1 < N && Q[i + 1] == TEXT('*'))
			{
				i += 2;
				while (i + 1 < N && !(Q[i] == TEXT('*') && Q[i + 1] == TEXT('/')))
				{
					i++;
				}
				i += 2;
				continue;
			}

			// texto '...' ou identificador "..." / `...`
			if (C == TEXT('\'') || C == TEXT('"') || C == TEXT('`'))
			{
				if (C == TEXT('\''))
				{
					A.bTemTexto = true;
				}
				if (C == TEXT('"'))
				{
					A.bTemAspasDuplas = true;
				}

				const TCHAR Fecha = C;
				bool bFechou = false;
				i++;
				while (i < N)
				{
					if (Q[i] == Fecha)
					{
						if (i + 1 < N && Q[i + 1] == Fecha)
						{
							i += 2; // '' dentro do texto
							continue;
						}
						bFechou = true;
						i++;
						break;
					}
					i++;
				}
				if (!bFechou && C == TEXT('\''))
				{
					A.bAspaAberta = true;
				}
				continue;
			}

			if (C == TEXT('('))
			{
				A.SaldoParenteses++;
				i++;
				continue;
			}
			if (C == TEXT(')'))
			{
				A.SaldoParenteses--;
				i++;
				continue;
			}

			if (FChar::IsAlpha(C) || C == TEXT('_'))
			{
				const int32 Inicio = i;
				while (i < N && (FChar::IsAlnum(Q[i]) || Q[i] == TEXT('_')))
				{
					i++;
				}
				FPalavra P;
				P.Texto = Q.Mid(Inicio, i - Inicio);
				P.Maiuscula = P.Texto.ToUpper();
				P.Profundidade = FMath::Max(A.SaldoParenteses, 0);
				A.Palavras.Add(P);
				continue;
			}

			if (FChar::IsDigit(C))
			{
				// numero (pula letras grudadas, ex: 1e5, para nao virar palavra)
				while (i < N && (FChar::IsAlnum(Q[i]) || Q[i] == TEXT('.')))
				{
					i++;
				}
				continue;
			}

			i++;
		}
		return A;
	}

	static TArray<FString> LerColuna(FSQLiteDatabase& Db, const FString& Sql)
	{
		TArray<FString> Saida;
		FSQLitePreparedStatement Stmt;
		if (Stmt.Create(Db, *Sql) && Stmt.IsValid())
		{
			while (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
			{
				FString Valor;
				Stmt.GetColumnValueByIndex(0, Valor);
				Saida.Add(Valor);
			}
		}
		Stmt.Destroy();
		return Saida;
	}

	static FEsquema LerEsquema(FSQLiteDatabase& Db)
	{
		FEsquema E;
		E.Tabelas = LerColuna(Db, TEXT("SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%' ORDER BY rowid"));
		for (const FString& Tabela : E.Tabelas)
		{
			E.Nomes.Add(Tabela.ToUpper());
			const FString NomeAspas = Tabela.Replace(TEXT("'"), TEXT("''"));
			const TArray<FString> Cols = LerColuna(Db, FString::Printf(TEXT("SELECT name FROM pragma_table_info('%s')"), *NomeAspas));
			E.ColunasDe.Add(Tabela.ToUpper(), Cols);
			for (const FString& Col : Cols)
			{
				E.Nomes.Add(Col.ToUpper());
				E.Colunas.AddUnique(Col);
			}
		}
		return E;
	}

	// Distancia de edicao (com troca de duas letras vizinhas contando como 1 erro)
	static int32 Distancia(const FString& A, const FString& B)
	{
		const int32 LA = A.Len();
		const int32 LB = B.Len();
		TArray<int32> D;
		D.SetNumZeroed((LA + 1) * (LB + 1));
		auto At = [&D, LB](int32 i, int32 j) -> int32& { return D[i * (LB + 1) + j]; };

		for (int32 i = 0; i <= LA; i++) { At(i, 0) = i; }
		for (int32 j = 0; j <= LB; j++) { At(0, j) = j; }

		for (int32 i = 1; i <= LA; i++)
		{
			for (int32 j = 1; j <= LB; j++)
			{
				const int32 Custo = (A[i - 1] == B[j - 1]) ? 0 : 1;
				int32 V = FMath::Min3(At(i - 1, j) + 1, At(i, j - 1) + 1, At(i - 1, j - 1) + Custo);
				if (i > 1 && j > 1 && A[i - 1] == B[j - 2] && A[i - 2] == B[j - 1])
				{
					V = FMath::Min(V, At(i - 2, j - 2) + 1);
				}
				At(i, j) = V;
			}
		}
		return At(LA, LB);
	}

	static int32 PrefixoComum(const FString& A, const FString& B)
	{
		int32 i = 0;
		while (i < A.Len() && i < B.Len() && A[i] == B[i])
		{
			i++;
		}
		return i;
	}

	// Quantos erros de digitacao aceitar para um nome desse tamanho
	static int32 Tolerancia(int32 Tamanho)
	{
		// baixa de proposito: 'Andre' nao pode virar a coluna 'andar'
		return Tamanho <= 6 ? 1 : 2;
	}

	// Palavra-chave mais parecida com Palavra (ja em maiuscula). Vazio se nenhuma.
	static FString PalavraChaveParecida(const FString& Palavra)
	{
		if (Palavra.Len() < 3)
		{
			return FString();
		}

		FString Melhor;
		int32 MelhorDist = MAX_int32;
		int32 MelhorPrefixo = -1;

		for (const TCHAR* K : PalavrasChave)
		{
			const FString Chave(K);
			if (Palavra == Chave)
			{
				return FString();
			}

			int32 Dist = 0;
			if (Chave.Len() <= 3)
			{
				// palavras curtas (AND, NOT, SET...): so aceita letras trocadas de lugar, ex: ADN
				if (Palavra.Len() != Chave.Len())
				{
					continue;
				}
				TArray<TCHAR> L1 = Palavra.GetCharArray();
				TArray<TCHAR> L2 = Chave.GetCharArray();
				L1.Sort();
				L2.Sort();
				if (L1 != L2)
				{
					continue;
				}
				Dist = 1;
			}
			else
			{
				const int32 Limite = Chave.Len() >= 6 ? 2 : 1;
				if (FMath::Abs(Palavra.Len() - Chave.Len()) > Limite)
				{
					continue;
				}
				Dist = Distancia(Palavra, Chave);
				// com 2 erros, exige pelo menos a primeira letra certa
				if (Dist > Limite || (Dist == 2 && Palavra[0] != Chave[0]))
				{
					continue;
				}
			}

			const int32 Prefixo = PrefixoComum(Palavra, Chave);
			if (Dist < MelhorDist || (Dist == MelhorDist && Prefixo > MelhorPrefixo))
			{
				Melhor = Chave;
				MelhorDist = Dist;
				MelhorPrefixo = Prefixo;
			}
		}
		return Melhor;
	}

	// Nome mais parecido numa lista (tabelas, colunas, funcoes). Vazio se nenhum.
	static FString NomeParecido(const FString& Nome, const TArray<FString>& Candidatos)
	{
		const FString Alvo = Nome.ToUpper();
		FString Melhor;
		int32 MelhorDist = Tolerancia(Alvo.Len()) + 1;
		for (const FString& C : Candidatos)
		{
			const int32 Dist = Distancia(Alvo, C.ToUpper());
			if (Dist < MelhorDist)
			{
				Melhor = C;
				MelhorDist = Dist;
			}
		}
		return Melhor;
	}

	static bool EhPalavraValida(const FString& Maiuscula)
	{
		for (const TCHAR* K : PalavrasChave)
		{
			if (Maiuscula == K)
			{
				return true;
			}
		}
		for (const TCHAR* K : OutrasPalavrasValidas)
		{
			if (Maiuscula == K)
			{
				return true;
			}
		}
		return false;
	}

	static bool TemPalavra(const FAnalise& A, const TCHAR* Palavra)
	{
		for (const FPalavra& P : A.Palavras)
		{
			if (P.Maiuscula == Palavra)
			{
				return true;
			}
		}
		return false;
	}

	// Texto que vem depois de Marcador na mensagem do SQLite (ex: "no such column: ")
	static FString DepoisDe(const FString& Erro, const TCHAR* Marcador)
	{
		const int32 Pos = Erro.Find(Marcador, ESearchCase::IgnoreCase);
		if (Pos == INDEX_NONE)
		{
			return FString();
		}
		FString Resto = Erro.Mid(Pos + FCString::Strlen(Marcador)).TrimStartAndEnd();
		Resto.TrimCharInline(TEXT('"'), nullptr);
		return Resto;
	}

	// Duas consultas coladas sem ';' (ex: o jogador nao apagou a anterior)
	static bool TemDuasConsultas(const FAnalise& A)
	{
		if (A.Palavras.Num() == 0 || (A.Palavras[0].Maiuscula != TEXT("SELECT") && A.Palavras[0].Maiuscula != TEXT("WITH")))
		{
			return false;
		}
		int32 Selects = 0;
		for (int32 i = 0; i < A.Palavras.Num(); i++)
		{
			const FPalavra& P = A.Palavras[i];
			if (P.Profundidade != 0 || P.Maiuscula != TEXT("SELECT"))
			{
				continue;
			}
			const FString Anterior = i > 0 ? A.Palavras[i - 1].Maiuscula : FString();
			if (Anterior == TEXT("UNION") || Anterior == TEXT("ALL") || Anterior == TEXT("INTERSECT") || Anterior == TEXT("EXCEPT"))
			{
				continue;
			}
			Selects++;
		}
		return Selects >= 2;
	}

	// Virgula logo antes do FROM: SELECT nome, FROM ...
	static bool VirgulaAntesDoFrom(const FString& Q)
	{
		const FString U = Q.ToUpper();
		int32 Pos = U.Find(TEXT("FROM"));
		while (Pos != INDEX_NONE)
		{
			int32 j = Pos - 1;
			while (j >= 0 && FChar::IsWhitespace(U[j]))
			{
				j--;
			}
			if (j >= 0 && U[j] == TEXT(','))
			{
				return true;
			}
			Pos = U.Find(TEXT("FROM"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Pos + 4);
		}
		return false;
	}

	// Monta as dicas para um erro do SQLite (de preparo ou de execucao)
	static TArray<FString> DicasDoErro(const FString& Query, const FString& Erro, FSQLiteDatabase& Db)
	{
		TArray<FString> Dicas;
		const FAnalise A = Analisar(Query);
		const FEsquema E = LerEsquema(Db);
		const FString ErroMin = Erro.ToLower();

		// --- Consultas coladas -------------------------------------------------
		if (TemDuasConsultas(A))
		{
			Dicas.Add(TEXT("Parece que ha duas consultas no console. Ele executa uma consulta por vez: apague a anterior e deixe so a que voce quer rodar."));
		}

		// --- Palavras-chave digitadas errado -----------------------------------
		TSet<FString> JaAvisadas;
		// a funcao com nome errado ja ganha a dica propria, mais abaixo
		JaAvisadas.Add(DepoisDe(Erro, TEXT("no such function:")).ToUpper());
		for (int32 i = 0; i < A.Palavras.Num(); i++)
		{
			const FPalavra& P = A.Palavras[i];
			if (EhPalavraValida(P.Maiuscula) || E.Nomes.Contains(P.Maiuscula) || JaAvisadas.Contains(P.Maiuscula))
			{
				continue;
			}
			// apelido escolhido pelo jogador (... AS ordem) nao e erro
			if (i > 0 && A.Palavras[i - 1].Maiuscula == TEXT("AS"))
			{
				continue;
			}
			const FString Certa = PalavraChaveParecida(P.Maiuscula);
			if (!Certa.IsEmpty())
			{
				JaAvisadas.Add(P.Maiuscula);
				Dicas.Add(FString::Printf(TEXT("'%s' esta digitado errado. O certo e: %s"), *P.Texto, *Certa));
			}
		}

		// --- Aspas e parenteses -----------------------------------------------
		if (A.bAspaAberta)
		{
			Dicas.Add(TEXT("Uma aspa simples ' foi aberta e nao foi fechada. Todo texto precisa de aspa no comeco e no fim, ex: 'Jose'."));
		}
		if (A.SaldoParenteses > 0)
		{
			Dicas.Add(TEXT("Faltou fechar um parentese: para cada ( precisa existir um )."));
		}
		else if (A.SaldoParenteses < 0)
		{
			Dicas.Add(TEXT("Tem um ) sobrando, sem o ( correspondente."));
		}

		// --- Mensagens especificas do SQLite -----------------------------------
		if (ErroMin.Contains(TEXT("no such table")))
		{
			const FString Nome = DepoisDe(Erro, TEXT("no such table:"));
			const FString Parecida = NomeParecido(Nome, E.Tabelas);
			FString Dica = FString::Printf(TEXT("A tabela '%s' nao existe."), *Nome);
			if (!Parecida.IsEmpty())
			{
				Dica += FString::Printf(TEXT(" Voce quis dizer '%s'?"), *Parecida);
			}
			Dica += TEXT("\n  Tabelas do banco: ") + FString::Join(E.Tabelas, TEXT(", "));
			Dicas.Add(Dica);
		}
		else if (ErroMin.Contains(TEXT("no such column")))
		{
			const FString Nome = DepoisDe(Erro, TEXT("no such column:"));
			FString Prefixo;
			FString Coluna;
			if (!Nome.Split(TEXT("."), &Prefixo, &Coluna, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
			{
				Prefixo.Reset();
				Coluna = Nome;
			}
			const FString Parecida = NomeParecido(Coluna, E.Colunas);
			const bool bSemFrom = !TemPalavra(A, TEXT("FROM")) && A.Palavras.Num() > 0 && A.Palavras[0].Maiuscula == TEXT("SELECT");

			if (bSemFrom)
			{
				Dicas.Add(TEXT("Faltou o FROM: diga de qual tabela vem a coluna. Ex: SELECT nome FROM suspeitos"));
			}
			else if (!Parecida.IsEmpty() && !Parecida.Equals(Coluna, ESearchCase::IgnoreCase))
			{
				FString Dica = FString::Printf(TEXT("A coluna '%s' nao existe. Voce quis dizer '%s'?"), *Coluna, *Parecida);
				if (Prefixo.IsEmpty())
				{
					Dica += FString::Printf(TEXT("\n  (Se '%s' e um valor de texto, escreva entre aspas simples: '%s')"), *Coluna, *Coluna);
				}
				Dicas.Add(Dica);
			}
			else if (!Prefixo.IsEmpty())
			{
				Dicas.Add(FString::Printf(TEXT("Em '%s', a tabela (ou apelido) '%s' precisa aparecer no FROM ou no JOIN da consulta."), *Nome, *Prefixo));
			}
			else
			{
				FString Dica = FString::Printf(TEXT("A coluna '%s' nao existe. Se '%s' e um valor de texto, escreva entre aspas simples: '%s'"), *Nome, *Nome, *Nome);
				if (A.bTemAspasDuplas)
				{
					Dica += TEXT("\n  Use aspas simples ' para textos. Aspas duplas \" sao para nomes de colunas.");
				}
				Dica += TEXT("\n  As colunas de cada tabela estao no Diagrama Relacional.");
				Dicas.Add(Dica);
			}
		}
		else if (ErroMin.Contains(TEXT("ambiguous column name")))
		{
			const FString Nome = DepoisDe(Erro, TEXT("ambiguous column name:"));
			FString Exemplo;
			for (const FPalavra& P : A.Palavras)
			{
				const TArray<FString>* Cols = E.ColunasDe.Find(P.Maiuscula);
				if (Cols && Cols->ContainsByPredicate([&Nome](const FString& C) { return C.Equals(Nome, ESearchCase::IgnoreCase); }))
				{
					Exemplo = P.Texto + TEXT(".") + Nome;
					break;
				}
			}
			FString Dica = FString::Printf(TEXT("A coluna '%s' existe em mais de uma tabela do JOIN. Diga de qual tabela ela e: tabela.%s"), *Nome, *Nome);
			if (!Exemplo.IsEmpty())
			{
				Dica += FString::Printf(TEXT(" (ex: %s)"), *Exemplo);
			}
			Dicas.Add(Dica);
		}
		else if (ErroMin.Contains(TEXT("no such function")))
		{
			const FString Nome = DepoisDe(Erro, TEXT("no such function:"));
			TArray<FString> ListaFuncoes;
			for (const TCHAR* F : Funcoes)
			{
				ListaFuncoes.Add(F);
			}
			const FString Parecida = NomeParecido(Nome, ListaFuncoes);
			FString Dica = FString::Printf(TEXT("A funcao '%s' nao existe."), *Nome);
			if (!Parecida.IsEmpty())
			{
				Dica += FString::Printf(TEXT(" Voce quis dizer %s?"), *Parecida);
			}
			Dicas.Add(Dica);
		}
		else if (ErroMin.Contains(TEXT("misuse of aggregate")))
		{
			Dicas.Add(TEXT("COUNT, SUM, AVG, MIN e MAX nao podem ficar no WHERE. Para filtrar pelo resultado deles, use GROUP BY ... HAVING."));
		}
		else if (ErroMin.Contains(TEXT("values for")) && ErroMin.Contains(TEXT("columns")))
		{
			Dicas.Add(TEXT("A quantidade de valores no VALUES nao bate com a quantidade de colunas listadas. Conte as virgulas dos dois lados."));
		}
		else if (ErroMin.Contains(TEXT("not null constraint failed")))
		{
			FString Tabela;
			FString Coluna = DepoisDe(Erro, TEXT("constraint failed:"));
			Coluna.Split(TEXT("."), &Tabela, &Coluna, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			Dicas.Add(FString::Printf(TEXT("A coluna '%s' e obrigatoria: o INSERT precisa ter um valor para ela."), *Coluna));
		}
		else if (ErroMin.Contains(TEXT("unique constraint failed")))
		{
			Dicas.Add(TEXT("Ja existe uma linha com esse valor (normalmente o id). Use outro id, ou um UPDATE para mudar a linha que ja existe."));
		}
		else if (ErroMin.Contains(TEXT("already exists")))
		{
			Dicas.Add(TEXT("Ja existe uma tabela com esse nome. Escolha outro nome, ou apague a sua tabela antiga com DROP TABLE."));
		}
		else if (ErroMin.Contains(TEXT("incomplete input")) && !A.bAspaAberta && A.SaldoParenteses <= 0)
		{
			Dicas.Add(TEXT("A query terminou no meio. Confira o final: falta um valor depois do = ou uma parte do comando?"));
		}
		else if (ErroMin.Contains(TEXT("unrecognized token")) && !A.bAspaAberta)
		{
			Dicas.Add(FString::Printf(TEXT("O SQL nao reconhece o simbolo %s. Apague ele ou confira se foi digitado sem querer."), *DepoisDe(Erro, TEXT("unrecognized token:"))));
		}

		if (ErroMin.Contains(TEXT("syntax error")) && VirgulaAntesDoFrom(Query))
		{
			Dicas.Add(TEXT("Tem uma virgula sobrando antes do FROM. A ultima coluna da lista nao leva virgula."));
		}

		// SELECT FROM ... (faltou dizer as colunas)
		for (int32 i = 0; i + 1 < A.Palavras.Num(); i++)
		{
			if (A.Palavras[i].Maiuscula == TEXT("SELECT") && A.Palavras[i + 1].Maiuscula == TEXT("FROM"))
			{
				Dicas.Add(TEXT("Faltou dizer quais colunas mostrar entre o SELECT e o FROM. Ex: SELECT * FROM suspeitos (todas) ou SELECT nome FROM suspeitos"));
				break;
			}
		}

		// Nada especifico: aponta onde o SQLite se perdeu
		if (Dicas.Num() == 0 && ErroMin.Contains(TEXT("syntax error")))
		{
			const int32 Inicio = Erro.Find(TEXT("near \""));
			if (Inicio != INDEX_NONE)
			{
				const int32 Fim = Erro.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Inicio + 6);
				if (Fim != INDEX_NONE)
				{
					const FString Perto = Erro.Mid(Inicio + 6, Fim - Inicio - 6);
					Dicas.Add(FString::Printf(TEXT("O SQL se perdeu perto de '%s'. Confira o que vem logo antes: uma palavra digitada errado ou uma virgula faltando?"), *Perto));
				}
			}
		}

		return Dicas;
	}

	static FString FormatarDicas(const TArray<FString>& Dicas)
	{
		FString Texto;
		for (const FString& Dica : Dicas)
		{
			Texto += TEXT("\n> DICA: ") + Dica;
		}
		return Texto.IsEmpty() ? Texto : TEXT("\n") + Texto;
	}

	// Avisos que impedem a query de rodar (vazio = pode rodar)
	static FString AvisoAntesDeRodar(const FString& Query, int32 NumComandos)
	{
		if (Query.Contains(TEXT("___")))
		{
			return TEXT("AVISO: a query ainda tem o ___ do exemplo.\nO ___ nao e para digitar: apague ele e coloque no lugar o valor que falta.\nEx: WHERE id = ___   ->   WHERE id = 3");
		}

		// Aspas inclinadas (copiadas de editor de texto) e acento agudo no lugar da aspa
		for (int32 i = 0; i < Query.Len(); i++)
		{
			const TCHAR C = Query[i];
			if (C == 0x2018 || C == 0x2019 || C == 0x201C || C == 0x201D || C == 0x00B4)
			{
				return TEXT("AVISO: a query tem uma aspa inclinada ou um acento agudo no lugar da aspa.\nO SQL so entende a aspa simples reta '. Apague e digite de novo com a tecla de aspas.");
			}
		}

		if (NumComandos == 0)
		{
			return TEXT("AVISO: a query so tem comentarios (linhas com --). Escreva um comando SQL.");
		}

		if (NumComandos > 1)
		{
			return FString::Printf(TEXT("AVISO: este console executa uma consulta por vez, e encontrei %d comandos separados por ;\nApague as consultas anteriores e deixe so a que voce quer rodar."), NumComandos);
		}

		return FString();
	}

	// A consulta rodou, mas uma palavra-chave errada logo depois da tabela virou apelido dela.
	// Ex: SELECT * FROM suspeitos WERE  -> o SQLite entende WERE como apelido de suspeitos
	static FString AvisoApelidoSuspeito(const FString& Query, FSQLiteDatabase& Db)
	{
		const FAnalise A = Analisar(Query);
		bool bTemCandidato = false;
		for (const FPalavra& P : A.Palavras)
		{
			if (!EhPalavraValida(P.Maiuscula) && !PalavraChaveParecida(P.Maiuscula).IsEmpty())
			{
				bTemCandidato = true;
				break;
			}
		}
		if (!bTemCandidato)
		{
			return FString(); // caso comum: nem le o esquema
		}

		const FEsquema E = LerEsquema(Db);
		for (int32 i = 1; i < A.Palavras.Num(); i++)
		{
			const FPalavra& P = A.Palavras[i];
			const FString& Anterior = A.Palavras[i - 1].Maiuscula;
			if (EhPalavraValida(P.Maiuscula) || E.Nomes.Contains(P.Maiuscula) || !E.ColunasDe.Contains(Anterior))
			{
				continue;
			}
			const FString Certa = PalavraChaveParecida(P.Maiuscula);
			if (Certa == TEXT("WHERE") || Certa == TEXT("JOIN") || Certa == TEXT("ORDER") || Certa == TEXT("GROUP")
				|| Certa == TEXT("LIMIT") || Certa == TEXT("INNER") || Certa == TEXT("LEFT"))
			{
				return FString::Printf(TEXT("A consulta rodou, mas '%s' parece um %s digitado errado.\n  O SQL entendeu '%s' como um apelido da tabela %s e ignorou essa parte. Corrija para %s."),
					*P.Texto, *Certa, *P.Texto, *A.Palavras[i - 1].Texto, *Certa);
			}
		}
		return FString();
	}

	// SELECT que voltou vazio
	static FString DicaConsultaVazia(const FString& Query)
	{
		const FAnalise A = Analisar(Query);
		FString SemEspacos = Query.ToUpper();
		SemEspacos.ReplaceInline(TEXT(" "), TEXT(""));
		SemEspacos.ReplaceInline(TEXT("\t"), TEXT(""));
		SemEspacos.ReplaceInline(TEXT("\n"), TEXT(""));
		SemEspacos.ReplaceInline(TEXT("\r"), TEXT(""));

		if (SemEspacos.Contains(TEXT("=NULL")) || SemEspacos.Contains(TEXT("<>NULL")))
		{
			return TEXT("Para procurar valores vazios use IS NULL (ou IS NOT NULL). O = NULL nunca encontra nada.");
		}
		if ((A.bTemTexto || A.bTemAspasDuplas) && TemPalavra(A, TEXT("WHERE")))
		{
			return TEXT("Com =, o texto precisa ser identico ao do banco, inclusive maiusculas e acentos ('Jose' e diferente de 'jose').\n  Para buscar parte do texto, use LIKE, ex: WHERE nome LIKE '%Jos%'");
		}
		return FString();
	}
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

	// BLINDAGEM 1.5: erros de iniciante que nem chegam a rodar (___ do exemplo, varias consultas)
	const FString AvisoPrevio = AvisosSQL::AvisoAntesDeRodar(QueryLimpa, DividirScript(QueryLimpa).Num());
	if (!AvisoPrevio.IsEmpty())
	{
		Retorno.TextoTerminal = AvisoPrevio;
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
		Statement.Destroy();
		const FString Dicas = AvisosSQL::FormatarDicas(AvisosSQL::DicasDoErro(QueryLimpa, Erro, Database));
		Database.Close();
		Retorno.TextoTerminal = FString::Printf(TEXT("ERRO DE SINTAXE SQL:\n%s%s"), *Erro, *Dicas);
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
			const FString Dica = AvisosSQL::DicaConsultaVazia(QueryLimpa);
			if (!Dica.IsEmpty())
			{
				ResultadoFinal += TEXT("\n> DICA: ") + Dica + TEXT("\n");
			}
		}

		const FString AvisoApelido = AvisosSQL::AvisoApelidoSuspeito(QueryLimpa, Database);
		if (!AvisoApelido.IsEmpty())
		{
			ResultadoFinal += TEXT("\n> AVISO: ") + AvisoApelido + TEXT("\n");
		}

		Retorno.bSucesso = true;
	}
	else
	{
		if (Statement.Execute())
		{
			Statement.Destroy();
			ResultadoFinal = TEXT("COMANDO EXECUTADO COM SUCESSO.");
			Retorno.bSucesso = true;

			// INSERT / UPDATE / DELETE: mostra quantas linhas mudaram e avisa os casos perigosos
			const AvisosSQL::FAnalise Analise = AvisosSQL::Analisar(QueryLimpa);
			const FString Comando = Analise.Palavras.Num() > 0 ? Analise.Palavras[0].Maiuscula : FString();
			if (Comando == TEXT("INSERT") || Comando == TEXT("UPDATE") || Comando == TEXT("DELETE"))
			{
				const TArray<FString> Mudancas = AvisosSQL::LerColuna(Database, TEXT("SELECT changes()"));
				const int32 Linhas = Mudancas.Num() > 0 ? FCString::Atoi(*Mudancas[0]) : -1;
				if (Linhas >= 0)
				{
					ResultadoFinal += FString::Printf(TEXT("\nLinhas afetadas: %d"), Linhas);
				}

				bool bTemWhere = false;
				for (const AvisosSQL::FPalavra& P : Analise.Palavras)
				{
					if (P.Profundidade == 0 && P.Maiuscula == TEXT("WHERE"))
					{
						bTemWhere = true;
						break;
					}
				}

				if (Comando != TEXT("INSERT") && Linhas == 0)
				{
					ResultadoFinal += TEXT("\n\n> AVISO: nenhuma linha bateu com o WHERE, entao nada mudou.\n  Confira o valor procurado (maiusculas, acentos e aspas).");
				}
				else if (Comando != TEXT("INSERT") && !bTemWhere && Linhas > 1)
				{
					ResultadoFinal += FString::Printf(TEXT("\n\n> ATENCAO: o comando nao tinha WHERE, entao TODAS as %d linhas da tabela foram alteradas.\n  Se foi sem querer, use o Restaurar backup no SYS_CONFIG."), Linhas);
				}
			}
		}
		else
		{
			const FString Erro = Database.GetLastError();
			Statement.Destroy();
			ResultadoFinal = FString::Printf(TEXT("FALHA NA EXECUCAO:\n%s%s"), *Erro, *AvisosSQL::FormatarDicas(AvisosSQL::DicasDoErro(QueryLimpa, Erro, Database)));
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

bool USQLiteBridge::DossieContem(FString Condicao, FString& NomeTabela)
{
	int32 NumLinhas = 0;
	if (!BuscarTabelaDoJogador(TEXT(""), NomeTabela, NumLinhas) || NumLinhas == 0)
	{
		return false;
	}

	const FString NomeSql = NomeTabela.Replace(TEXT("\""), TEXT("\"\""));
	const FString Cond = Condicao.TrimStartAndEnd();
	const FString Q = Cond.IsEmpty()
		? FString::Printf(TEXT("SELECT 1 FROM \"%s\""), *NomeSql)
		: FString::Printf(TEXT("SELECT 1 FROM \"%s\" WHERE %s"), *NomeSql, *Cond);

	// Se o jogador criou o dossie com outras colunas, a consulta falha e conta como false
	FResultadoSQL Saida;
	return RodarConsulta(Q, Saida) && Saida.Registros.Num() > 0;
}
