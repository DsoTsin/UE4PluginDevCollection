// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "SQLitePCH.h"
#include "Classes/DataBase.h"
#include "Classes/Cursor.h"
#include "sqlite3.h"

FDataBase::FDataBase(FString const& DBPath)
: DB(NULL), PathDB(DBPath)
{
    int Rc = sqlite3_open(TCHAR_TO_UTF8(*DBPath), &DB);
    if(SQLITE_OK != Rc)
    {
        UE_LOG(SQLiteDB, Error, TEXT("Open DB Path: %s Failed!"), *DBPath);
    }
}

void FDataBase::Close()
{
    if(DB)
    {
       sqlite3_close(DB);
       DB = nullptr; 
    }
}

FDataBase::~FDataBase()
{
    Close();
}

bool FDataBase::Execute(const TCHAR * Command)
{
    if (!DB)
    {
        UE_LOG(SQLiteDB, Error, TEXT("DB was not Opened: %s Failed!"), *PathDB);
        return false;
    }
    char* ErrorMsg = nullptr;
    int Rc = sqlite3_exec(DB, TCHAR_TO_UTF8(Command), NULL, NULL, &ErrorMsg);
    if (SQLITE_OK != Rc)
    {
        UE_LOG(SQLiteDB, Error, TEXT("DB Executed: %s Failed!, %s !"), Command, UTF8_TO_TCHAR(ErrorMsg));
        return false;
    }

    return true;
}

FDBCursor* FDataBase::RawQuery(const TCHAR* QueryString)
{
    if (!DB)
    {
        UE_LOG(SQLiteDB, Error, TEXT("DB was not Opened: %s Failed!"), *PathDB);
        return nullptr;
    }

    sqlite3_stmt* PreparedStatement;
    int32 PrepareStatus = sqlite3_prepare_v2(DB, TCHAR_TO_UTF8(QueryString), -1, &PreparedStatement, NULL);
    if (PrepareStatus == SQLITE_OK)
    {
        return new FDBCursor(PreparedStatement);
    }
    return nullptr;
}