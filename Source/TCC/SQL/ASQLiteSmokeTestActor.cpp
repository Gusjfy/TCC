// Fill out your copyright notice in the Description page of Project Settings.


#include "ASQLiteSmokeTestActor.h"

#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"

#include "SQLiteDatabase.h"
#include "SQLitePreparedStatement.h"

// Sets default values
AASQLiteSmokeTestActor::AASQLiteSmokeTestActor()
{
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void AASQLiteSmokeTestActor::BeginPlay()
{
	Super::BeginPlay();
	const bool bSuccess = RunSQLiteSmokeTest();
	UE_LOG(LogTemp, Warning, TEXT("SQLite smoke test final: %s"), bSuccess ? TEXT("SUCESSO") : TEXT("FALHOU"));
	
}

// Called every frame
void AASQLiteSmokeTestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

bool AASQLiteSmokeTestActor::RunSQLiteSmokeTest()
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	const FString DbFolder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Databases"));
	const FString DbPath = FPaths::Combine(DbFolder, TEXT("sql_test.db"));

	if (!PlatformFile.DirectoryExists(*DbFolder))
	{
		PlatformFile.CreateDirectoryTree(*DbFolder);
	}

	// Para realmente testar "do zero", apaga o arquivo antigo se existir
	if (PlatformFile.FileExists(*DbPath))
	{
		PlatformFile.DeleteFile(*DbPath);
	}

	UE_LOG(LogTemp, Warning, TEXT("DB Path: %s"), *DbPath);

	FSQLiteDatabase Database;

	// Se o arquivo não existir, o modo de criação deve gerar o banco novo
	if (!Database.Open(*DbPath, ESQLiteDatabaseOpenMode::ReadWriteCreate))
	{
		UE_LOG(LogTemp, Error, TEXT("Falha ao abrir/criar DB: %s"), *Database.GetLastError());
		return false;
	}

	// 1) CREATE TABLE
	{
		FSQLitePreparedStatement Statement;

		const TCHAR* Sql = TEXT(
			"CREATE TABLE IF NOT EXISTS people ("
			"id INTEGER PRIMARY KEY, "
			"name TEXT NOT NULL, "
			"suspicion_level INTEGER NOT NULL"
			");"
		);

		if (!Statement.Create(Database, Sql))
		{
			UE_LOG(LogTemp, Error, TEXT("Falha ao preparar CREATE TABLE"));
			Database.Close();
			return false;
		}

		if (!Statement.Execute())
		{
			UE_LOG(LogTemp, Error, TEXT("Falha ao executar CREATE TABLE: %s"), *Database.GetLastError());
			Statement.Destroy();
			Database.Close();
			return false;
		}

		Statement.Destroy();

	}

	// 2) INSERT 1
	{
		FSQLitePreparedStatement Statement;

		const TCHAR* Sql = TEXT(
			"INSERT INTO people (id, name, suspicion_level) "
			"VALUES (1, 'Pedro', 4);"
		);

		if (!Statement.Create(Database, Sql))
		{
			UE_LOG(LogTemp, Error, TEXT("Falha ao preparar INSERT 1"));
			Database.Close();
			return false;
		}

		if (!Statement.Execute())
		{
			UE_LOG(LogTemp, Error, TEXT("Falha ao executar INSERT 1: %s"), *Database.GetLastError());
			Statement.Destroy();
			Database.Close();
			return false;
		}

		Statement.Destroy();
	}
	
	// 3) INSERT 2
	{
		FSQLitePreparedStatement Statement;

		const TCHAR* Sql = TEXT(
			"INSERT INTO people (id, name, suspicion_level) "
			"VALUES (2, 'Jose', 8);"
		);

		if (!Statement.Create(Database, Sql))
		{
			UE_LOG(LogTemp, Error, TEXT("Falha ao preparar INSERT 2"));
			Database.Close();
			return false;
		}

		if (!Statement.Execute())
		{
			UE_LOG(LogTemp, Error, TEXT("Falha ao executar INSERT 2: %s"), *Database.GetLastError());
			Statement.Destroy();
			Database.Close();
			return false;
		}

		Statement.Destroy();
	}


	// 4) SELECT
	{
		FSQLitePreparedStatement Statement;

		const TCHAR* Sql = TEXT(
			"SELECT id, name, suspicion_level "
			"FROM people "
			"ORDER BY id;"
		);

		if (!Statement.Create(Database, Sql))
		{
			UE_LOG(LogTemp, Error, TEXT("Falha ao preparar SELECT"));
			Database.Close();
			return false;
		}

		while (true)
		{
			const ESQLitePreparedStatementStepResult StepResult = Statement.Step();

			if (StepResult == ESQLitePreparedStatementStepResult::Row)
			{
				int32 Id = 0;
				FString Name;
				int32 Suspicion = 0;

				Statement.GetColumnValueByIndex(0, Id);
				Statement.GetColumnValueByIndex(1, Name);
				Statement.GetColumnValueByIndex(2, Suspicion);

				UE_LOG(LogTemp, Warning, TEXT("ROW -> id=%d | name=%s | suspicion=%d"), Id, *Name, Suspicion);
			}
			else if (StepResult == ESQLitePreparedStatementStepResult::Done)
			{
				break;
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Erro ao iterar SELECT: %s"), *Database.GetLastError());
				Statement.Destroy();
				Database.Close();
				return false;
			}
		}

		Statement.Destroy();
	}

	if (!Database.Close())
	{
		UE_LOG(LogTemp, Error, TEXT("Falha ao fechar DB: %s"), *Database.GetLastError());
		return false;
	}


	return true;
}

