#include "db_mgr.h"
#include <iostream>
#include <string>
#include <QString>
#include "my_utility.h"
#include "logger.h"

namespace db
{

//common funcs
bool prepareInsert(QSqlQuery& query, std::map<std::string, std::string>& queryMap)
{
    if ( queryMap.size() <= 1)
    {
        LG_ERR_RET_FALSE; //<-----------------
    }

    auto iter = queryMap.find("tablename");
    if (queryMap.end() == iter) LG_ERR_RET_FALSE;

    std::string tableName = iter->second;
    queryMap.erase("tablename");

    //"INSERT INTO tablename (colname1, colname2, colname3) VALUES (?, ?, ?);"
    std::string templ = "INSERT INTO ";
    templ += tableName; 
	
    std::string fields = " (";
    iter = queryMap.begin();
    fields += iter->first;
    ++iter;
    for (; iter != queryMap.end(); ++iter)
    {
        fields += ", ";
        fields += iter->first;
    }
    fields += ") VALUES (?";
    for (unsigned i = 0; i < (queryMap.size() - 1); ++ i)
    {
        fields += ", ?";
    }
    fields += ");";
    templ += fields;

    if ( !query.prepare(templ.c_str()) )
    {
        std::cerr << "InsertUserLog: Problem in prearing sql-request ";
        std::cerr << query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    //place value to query
   for (auto iter2 = queryMap.begin(); iter2 != queryMap.end(); ++ iter2)
   {
       query.addBindValue(iter2->second.c_str());
   }
    return true;
}
bool prepareSelect(QSqlQuery& query, std::map<std::string, std::string>& queryMap, std::string* orderColumn = nullptr, unsigned int limit = 0)
{
    auto iter = queryMap.find("tablename");
    if ( queryMap.end() == iter )
    {
        //no tablename in map!!!
        LG_ERR_RET_FALSE;
    }

    std::string tableName = iter->second;
    queryMap.erase("tablename");
    std::string templ = "SELECT * FROM ";
    templ += tableName;
    if ( 0 != queryMap.size() )          // == SELECT * FROM `usr_log` WHERE ...
    {
        templ += " WHERE "; //be careful with whitespaces
        auto iter = queryMap.begin();
        templ += iter->first;
        templ += " = \"";
        templ += iter->second;
        templ += "\" ";
        ++ iter;
        for ( ; iter != queryMap.end(); ++ iter)
        {
            templ += " AND ";
            templ += iter->first;
            templ += " = \"";
            templ += iter->second;
            templ += "\" ";
        }
    }
    else // == SELECT * FROM `tablename`
    {
        //do nothing
    }

    if (nullptr != orderColumn) //ORDER BY `column_name`
    {
        templ += " ORDER BY ";
        templ += *orderColumn;
        templ += " DESC ";
    }

    if (limit) //LIMIT number
    {
        templ += " LIMIT ";
        templ += std::to_string(limit);
    }
    templ += ";";

    if ( ! query.prepare(templ.c_str()) )
    {
        std::cerr << "prepareSelect: RTFM SQL. " << templ;
        std::cerr <<  query.lastError().text().toStdString() << std::endl;
        LG_ERR_RET_FALSE;
    }

    return true;
}
bool prepareUpdate(QSqlQuery& query, std::map<std::string, std::string>& setMap,
                   std::map<std::string, std::string>& whereMap)
{
    //UPDATE `tablename`
    auto iter_f = setMap.find("tablename");
    if (setMap.end() == iter_f) LG_ERR_RET_FALSE;
    std::string tableName = iter_f->second;

    setMap.erase("tablename");
    if (setMap.size() < 1)
    {
        std::cerr << "no data to update" << std::endl;
        LG_ERR_RET_FALSE;
    }
    whereMap.erase("tablename");
    std::string templ = "UPDATE ";
    templ += "`";
    templ += tableName;
    templ += "`";

    //SET column = expression
    templ += " SET ";
    auto iter = setMap.begin();
    templ += iter->first;
    templ += " = \"";
    templ += iter->second;
    templ += "\" ";
    ++ iter;
    for ( ; iter != setMap.end(); ++ iter)
    {
        templ += " , ";
        templ += iter->first;
        templ += " = \"";
        templ += iter->second;
        templ += "\" ";
    }

    //WHERE key = value
    if (whereMap.size() > 0) //if conditions exist
    {
        templ += " WHERE "; //be careful with whitespaces
        auto iter = whereMap.begin();
        templ += iter->first;
        templ += " = \"";
        templ += iter->second;
        templ += "\" ";
        ++ iter;
        for ( ; iter != whereMap.end(); ++ iter)
        {
            templ += " AND ";
            templ += iter->first;
            templ += " = \"";
            templ += iter->second;
            templ += "\" ";
        }
    }


    templ += ";";

    //prepare query
    if ( ! query.prepare(templ.c_str()) )
    {
        std::cerr << "preapreUpdate: RTFM SQL **" << templ;
        std::cerr <<  query.lastError().text().toStdString() << std::endl;
        LG_ERR_RET_FALSE;
    }

    return true;
}

//**********************************************************************************
DbMgr::DbMgr()
{
    sdb_ = QSqlDatabase::addDatabase("QSQLITE"); //set driver
}

bool DbMgr::checkQueryError(QSqlQuery& query_to_check, std::string* result = NULL)
{
    if (query_to_check.lastError().type() != QSqlError::NoError)
    {
        lastError_ = query_to_check.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    if (NULL != result) //if need result information
    {
        if (query_to_check.isSelect())
            *result = std::string("Query OK.");
        else
            *result = std::string("Query OK, number of affected rows: ")+ std::to_string(query_to_check.numRowsAffected());
    }
    return true;
}

bool DbMgr::Createdb(std::string &db_path)
{
    //need check if db exists
    Opendb(db_path);
    std::vector<std::string> sqlScript;
{
    sqlScript.push_back( //Table `so`
        "CREATE TABLE `so` (`id` INTEGER PRIMARY KEY AUTOINCREMENT, \
        `name` VARCHAR(45) NOT NULL UNIQUE, \
        `ip` VARCHAR(45) NOT NULL,\
        `address` VARCHAR(512) NOT NULL,\
        `vendor` VARCHAR(45) NOT NULL,\
        `passport_uri` VARCHAR(256) NULL,\
        `district_id` INTEGER NOT NULL,\
        `latitude` VARCHAR(45) NOT NULL,\
        `longitude` VARCHAR(45) NOT NULL,\
        CONSTRAINT `fk_so_district1`\
        FOREIGN KEY (`district_id`)\
        REFERENCES `district` (`id`)\
        ON DELETE NO ACTION \
        ON UPDATE NO ACTION);" );

    sqlScript.push_back( //Table `district`
         "CREATE TABLE `district` (\
         `id` INTEGER PRIMARY KEY AUTOINCREMENT,\
         `name` VARCHAR(45) NOT NULL UNIQUE,\
         `description` VARCHAR(245) NULL);"     );

    sqlScript.push_back( //Table `error_so`
         "CREATE TABLE `error_so` ( \
         `id` INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,\
         `error_rise_datetime` DATETIME NOT NULL,\
         `message` VARCHAR(256) NOT NULL,\
         `so_id` INTEGER NOT NULL,\
         `error_fix_datetime` DATETIME NULL DEFAULT NULL,\
          CONSTRAINT `fk_error_so_so1`\
          FOREIGN KEY (`so_id`)\
          REFERENCES `so` (`id`)\
          ON DELETE NO ACTION\
          ON UPDATE NO ACTION);"       );

    sqlScript.push_back( //Table `users`
          "CREATE TABLE `users` (\
          `id` INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,\
          `user_name` VARCHAR(45) NOT NULL UNIQUE,\
          `pwd` VARCHAR(45) NOT NULL,\
          `lvl` INTEGER NOT NULL);"      );

    sqlScript.push_back(//Table `user_log`
           "CREATE TABLE `user_log` (\
           `id_user_log` INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,\
           `log_datetime` INTEGER NOT NULL,\
           `action` VARCHAR(245) NOT NULL,\
           `user_id` INTEGER NOT NULL,\
           CONSTRAINT `fk_usr_log_user1`\
           FOREIGN KEY (`user_id`)\
           REFERENCES `users` (`id`)\
           ON DELETE NO ACTION\
           ON UPDATE NO ACTION); "  );

    sqlScript.push_back(//Table `action_so`
           "CREATE TABLE `action_so` ( \
           `id` INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, \
           `action_datetime` DATETIME NOT NULL,\
           `action` VARCHAR(256) NOT NULL,\
           `user_id` INTEGER NOT NULL,\
           `so_id` INTEGER NOT NULL,\
           CONSTRAINT `fk_actions_on_so_users`\
           FOREIGN KEY (`user_id`)\
           REFERENCES `users` (`id`)\
           ON DELETE NO ACTION\
           ON UPDATE NO ACTION,\
           CONSTRAINT `fk_actions_on_so_so1`\
           FOREIGN KEY (`so_id`)\
           REFERENCES `so` (`id`)\
           ON DELETE NO ACTION\
           ON UPDATE NO ACTION);"             );

    sqlScript.push_back( //Table `detector`
           "CREATE TABLE `detector` ( \
           `id` INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, \
           `vendor` VARCHAR(255) NULL DEFAULT 'noname', \
           `serial_number` VARCHAR(255) NULL DEFAULT 'sn' UNIQUE);"    );

    sqlScript.push_back(//Table `so_detector`
           "CREATE TABLE `so_detector` ( \
           `so_id` INTEGER NOT NULL, \
           `number_on_so` INTEGER NOT NULL, \
           `detector_id` INTEGER NOT NULL UNIQUE, \
           PRIMARY KEY (`so_id`, `number_on_so`), \
           CONSTRAINT `fk_detector_so1` \
           FOREIGN KEY (`so_id`) \
           REFERENCES `so` (`id`) \
           ON DELETE NO ACTION \
           ON UPDATE NO ACTION, \
           CONSTRAINT `fk_so_has_detector_detector1` \
           FOREIGN KEY (`detector_id`) \
           REFERENCES `detector` (`id`) \
           ON DELETE NO ACTION \
           ON UPDATE NO ACTION);"   );

    sqlScript.push_back( //Table `plan`
           "CREATE TABLE `plan` ( \
            `so_id` INTEGER NOT NULL, \
            `number_on_so` INTEGER NOT NULL, \
            `description` VARCHAR(1024) NULL, \
            PRIMARY KEY (`so_id`, `number_on_so`), \
            CONSTRAINT `fk_plan_so1` \
            FOREIGN KEY (`so_id`) \
            REFERENCES `so` (`id`) \
            ON DELETE NO ACTION \
            ON UPDATE NO ACTION); "     );

    sqlScript.push_back( //Table `phase`
            "CREATE TABLE `phase` ( \
            `so_id` INTEGER NOT NULL, \
            `number_on_so` INTEGER NOT NULL, \
            `img_uri` VARCHAR(1024) NOT NULL, \
            PRIMARY KEY (`so_id`, `number_on_so`), \
            CONSTRAINT `fk_phase_so1` \
            FOREIGN KEY (`so_id`) \
            REFERENCES `so` (`id`) \
            ON DELETE NO ACTION \
            ON UPDATE NO ACTION); "     );

    sqlScript.push_back( //Table `plan_has_phase`
            "CREATE TABLE `plan_has_phase` ( \
            `so_id` INTEGER NOT NULL, \
            `plan_number_on_so` INTEGER NOT NULL, \
            `phase_number_on_so` INTEGER NOT NULL, \
            `main_time` INTEGER NULL, \
            `min_time` INTEGER NULL, \
            `max_time` INTEGER NULL, \
            PRIMARY KEY (`so_id`, `plan_number_on_so`, `phase_number_on_so`), \
            CONSTRAINT `fk_plan_has_phase_plan1` \
            FOREIGN KEY (`so_id` , `plan_number_on_so`) \
            REFERENCES `plan` (`so_id` , `number_on_so`) \
            ON DELETE NO ACTION \
            ON UPDATE NO ACTION, \
            CONSTRAINT `fk_plan_has_phase_phase1` \
            FOREIGN KEY (`phase_number_on_so` , `so_id`) \
            REFERENCES `phase` (`number_on_so` , `so_id`) \
            ON DELETE NO ACTION \
            ON UPDATE NO ACTION); "    );

    sqlScript.push_back( //Table `cc_zone`
            "CREATE TABLE `cc_zone` (\
            `id` INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, \
            `name` VARCHAR(45) NOT NULL, \
            `description` VARCHAR(245) NULL);"  );

    sqlScript.push_back( //Table `cc_zone_has_so`
             "CREATE TABLE `cc_zone_has_so` ( \
             `so_id` INTEGER NOT NULL, \
             `cc_zone_id` INTEGER NOT NULL, \
             PRIMARY KEY (`so_id`, `cc_zone_id`), \
             CONSTRAINT `fk_so_has_district_cc_so1` \
             FOREIGN KEY (`so_id`) \
             REFERENCES `so` (`id`) \
             ON DELETE NO ACTION \
             ON UPDATE NO ACTION, \
             CONSTRAINT `fk_so_has_district_cc_district_cc1` \
             FOREIGN KEY (`cc_zone_id`) \
             REFERENCES `cc_zone` (`id`) \
             ON DELETE NO ACTION \
             ON UPDATE NO ACTION);"    );

    sqlScript.push_back( //Table `cc_plan`
             "CREATE TABLE `cc_plan` ( \
             `id` INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, \
             `name` VARCHAR(245) NULL, \
             `description` VARCHAR(245) NULL, \
             `cycle_time` INTEGER NOT NULL, \
             `map_img_uri` VARCHAR(245) NOT NULL, \
             `cycle_img_uri` VARCHAR(245) NOT NULL, \
             `cc_zone_id` INTEGER NOT NULL, \
             CONSTRAINT `fk_coordination_plan_zone_cc1` \
             FOREIGN KEY (`cc_zone_id`) \
             REFERENCES `cc_zone` (`id`) \
             ON DELETE NO ACTION \
             ON UPDATE NO ACTION); ");

     sqlScript.push_back( //Table `cc_plan_has_phase
                         "CREATE TABLE `cc_plan_has_phase` ( \
                         `id` INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, \
                         `cc_plan_id` INTEGER NOT NULL, \
                         `so_id` INTEGER NOT NULL, \
                         `phase_number_on_so` INTEGER NOT NULL, \
                         `time_on` INTEGER NOT NULL, \
                         CONSTRAINT `fk_cc_plan_has_phase_cc_plan1` \
                           FOREIGN KEY (`cc_plan_id`) \
                           REFERENCES `cc_plan` (`id`) \
                           ON DELETE NO ACTION \
                           ON UPDATE NO ACTION, \
                         CONSTRAINT `fk_cc_plan_has_phase_phase1` \
                           FOREIGN KEY (`so_id` , `phase_number_on_so`) \
                           REFERENCES `phase` (`so_id` , `number_on_so`) \
                           ON DELETE NO ACTION \
                           ON UPDATE NO ACTION);"
    );

    sqlScript.push_back( //Table `traffic`
              "CREATE TABLE `traffic` ( \
              `id` INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, \
              `traffic_datetime` INTEGER NOT NULL, \
              `traffic_data` INTEGER NOT NULL, \
              `so_id` INTEGER NOT NULL, \
              `detector_number_on_so` INTEGER NOT NULL, \
              CONSTRAINT `fk_traffic_data_detector1` \
              FOREIGN KEY (`so_id` , `detector_number_on_so`) \
              REFERENCES `so_detector` (`so_id` , `number_on_so`) \
              ON DELETE NO ACTION \
              ON UPDATE NO ACTION);"
                );

    sqlScript.push_back( //Table `settings`
                         "CREATE TABLE `settings` ( \
                         `id` INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, \
                         `name` VARCHAR(128) NOT NULL, \
                         `val` VARCHAR(512) NOT NULL);"
                );
    sqlScript.push_back( // Table `logbook`
                         "CREATE TABLE IF NOT EXISTS `logbook` ( \
                         `id` INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, \
                         `so_id` INTEGER NOT NULL, \
                         `entry` VARCHAR(1024) NOT NULL, \
                         CONSTRAINT `fk_logbooks_so1` \
                           FOREIGN KEY (`so_id`) \
                           REFERENCES `so` (`id`) \
                           ON DELETE NO ACTION \
                           ON UPDATE NO ACTION);"
                );
}
    for (size_t i = 0; i < sqlScript.size(); ++i)
    {
        QSqlQuery query;
        if ( !query.prepare(sqlScript[i].c_str()) )
        {
            lastError_ = "InsertUserLog: smth wrong in execution query ";
            lastError_ += query.lastError().text().toStdString();
            LG_ERR_RET_FALSE;
        }
        //exec query
        if ( !query.exec() )
        {
            lastError_ = "InsertUserLog: smth wrong in execution query ";
            lastError_ += query.lastError().text().toStdString();
            LG_ERR_RET_FALSE;
        }
        //check for query errors
        std::string info;
        if ( !checkQueryError(query, &info ) )
            LG_ERR_RET_FALSE;

        std::cerr << info << std::endl; // aux, info about success
        }

    return true;
}

bool DbMgr::Opendb(std::string& db_path)
{
    if ( !CheckFile(db_path))
    {
        lastError_ = "database not exists";
        LG_ERR_RET_FALSE;
    }
    sdb_.setDatabaseName(db_path.c_str());
    if (!sdb_.open()) {//open db
        std::cout << sdb_.lastError().text().toStdString(); //aux
        lastError_ = sdb_.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }
	makeSODictionaries();
    return true;
}

bool DbMgr::GetSetting(const std::string& settingName, std::string& settingValue)
{
    if (settingName.empty())
    {
        lastError_ = "GetSetting: Invalid setting's name";
        LG_ERR_RET_FALSE;
    }
    settingValue.clear();

    // prepare query map
    std::map<std::string, std::string> queryMap;
    queryMap["tablename"] = "settings";
    queryMap["name"] = settingName;

    //prepare query
    QSqlQuery query;
    if (!prepareSelect(query, queryMap))
    {
        lastError_ = "GetSetting: smth wrong in preparing query ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    //send Q
    if ( !query.exec() )
    {
        lastError_ = "GetSetting: smth wrong in execution query ";
        lastError_ += query.lastError().text().toStdString();
       LG_ERR_RET_FALSE;
    }

    //check errors in Q
    std::string info;
    if ( !checkQueryError(query, &info ) )
        LG_ERR_RET_FALSE;

    std::cerr << info << std::endl; // aux, info about success query

    //return answer
    QSqlRecord rec = query.record();
    if (query.next())
    {
        settingValue = query.value(rec.indexOf("val")).toString().toStdString();
    }
    else
    {
        lastError_ = "GetSetting: No such setting in db";
        LG_ERR_RET_FALSE;
    }

    return true;
}
bool DbMgr::InsertSetting(const std::string& settingNameToInsert, const std::string& settingValueToInsert)
{
    //prepare query
    QSqlQuery query;
    if ( !query.prepare("INSERT INTO `settings` (name, val)"
                        "VALUES (?, ?);") )
    {
        lastError_ = "InsertSetting: Problem in prearing sql-request ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    //place value to query
    query.bindValue(0, settingNameToInsert.c_str()); //name
    query.bindValue(1, settingValueToInsert.c_str()); //value


    //exec query
    if ( !query.exec() )
    {
        lastError_ = "InsertSetting: smth wrong in execution query ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    //check for query errors
    std::string info;
    if ( !checkQueryError(query, &info ) )
        LG_ERR_RET_FALSE;

    std::cerr << info << std::endl; // aux, info about success
    return true;
}

bool DbMgr::InsertErrorSO(ErrorsSOData& errorToInsert)
{
    //prepare query
    QSqlQuery query;
    if ( !query.prepare("INSERT INTO error_so (error_rise_datetime, message, so_id)"
                        "VALUES (?, ?, ?);") )
    {
        lastError_ = "InsertErrorSO: Problem in prearing sql-request ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    //place value to query
    query.bindValue(0, std::to_string(errorToInsert.riseDatetime_).c_str()); //error_datetime
    query.bindValue(1,errorToInsert.msg_.c_str()); //message
    query.bindValue(2, std::to_string(errorToInsert.soId_).c_str()); //so_id

    //exec query
    if ( !query.exec() )
    {
        lastError_ = "InsertErrorSO: smth wrong in execution query ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    //check for query errors
    std::string info;
    if ( !checkQueryError(query, &info ) )
        LG_ERR_RET_FALSE;

    std::cerr << info << std::endl; // aux, info about success
    return true;
}
bool DbMgr::SelectErrorSO(std::vector<ErrorsSOData>& result, ErrorsSOQuery* pErrorToSelect = NULL)
{
    //Has no check on empty struct@AU!
    QSqlQuery query;
    //prepare Q
    if (NULL == pErrorToSelect) // == SELECT * FROM `error_so`
    {
        if ( ! query.prepare("SELECT * FROM `error_so`") )
        {
            lastError_ = "SelectErrorSO: RTFM SQL";
            lastError_ += query.lastError().text().toStdString();
            LG_ERR_RET_FALSE;
        }
    }
    else // == SELECT * FROM `errors_so` WHERE ...
    {
        std::string queryBuf = "SELECT * FROM `error_so` WHERE "; //be careful with whitespaces
        bool needAND = false;
        if ( NULL != pErrorToSelect->pId_)
        {
            queryBuf += "id = ";
            queryBuf += std::to_string(*pErrorToSelect->pId_);
            needAND = true;
        }

        if ( NULL != pErrorToSelect->pSoId_)
        {
            if (needAND)
                queryBuf += " AND";
            queryBuf += "so_id = ";
            queryBuf += std::to_string(*pErrorToSelect->pSoId_);
            needAND = true;
        }
        if ( NULL != pErrorToSelect->pMsg_)
        {
            if (needAND)
                queryBuf += " AND";
            queryBuf += " message = ";
            queryBuf += *pErrorToSelect->pMsg_;
            needAND = true;
        }
        if ( NULL != pErrorToSelect->pRiseDatetimeStart_)
        {
            if (needAND)
                queryBuf += " AND";
            queryBuf += " error_rise_datetime >= ";
            queryBuf += std::to_string(*pErrorToSelect->pRiseDatetimeStart_);
            needAND = true;
        }
        if ( NULL != pErrorToSelect->pRiseDatetimeEnd_)
        {
            if (needAND)
                queryBuf += " AND";
            queryBuf += " error_rise_datetime <= ";
            queryBuf += std::to_string(*pErrorToSelect->pRiseDatetimeEnd_);
            needAND = true;
        }

        if ( NULL != pErrorToSelect->pFixDatetimeStart_)
        {
            if (needAND)
                queryBuf += " AND";
            queryBuf += " error_fix_datetime >= ";
            queryBuf += std::to_string(*pErrorToSelect->pFixDatetimeStart_);
            needAND = true;
        }
        if ( NULL != pErrorToSelect->pFixDatetimeEnd_)
        {
            if (needAND)
                queryBuf += " AND";
            queryBuf += " error_fix_datetime <= ";
            queryBuf += std::to_string(*pErrorToSelect->pFixDatetimeEnd_);
            needAND = true;
        }

        queryBuf += ";";
        if ( ! query.prepare(queryBuf.c_str()) )
        {
            lastError_ = "SelectErrorSO: RTFM SQL";
            lastError_ += query.lastError().text().toStdString();
            LG_ERR_RET_FALSE;
        }
    }
    //send Q
    if ( !query.exec() )
    {
        lastError_ = "SelectErrorsSO: smth wrong in execution query ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }
    //check errors in Q
    std::string info;
    if ( !checkQueryError(query, &info ) )
        LG_ERR_RET_FALSE;

    std::cerr << "SelectErrorSO: "<< info << std::endl; // aux, info about success query

    //parsing result
    QSqlRecord rec = query.record();
    TObjId id;
    time_t datetimeRise;
    std::string msg;
    TObjId soId;
    time_t datetimeFix;

    while (query.next())
    {
        id = query.value(rec.indexOf("id")).toInt();
        datetimeRise = query.value(rec.indexOf("error_rise_datetime")).toInt();
        msg = query.value(rec.indexOf("message")).toString().toStdString();
        soId = query.value(rec.indexOf("so_id")).toInt();
        datetimeFix = query.value(rec.indexOf("error_fix_datetime")).toInt();
        result.push_back(ErrorsSOData(id, datetimeRise, msg, soId, datetimeFix));
    }

    return true;
}
bool DbMgr::ResetErrorSO(ErrorsSOQuery *errorToReset, time_t fixTime)
{
    //get request
    std::string queryStr = "UPDATE `error_so` SET error_fix_datetime = ";
    queryStr += std::to_string(fixTime);
    if (nullptr != errorToReset) //conditions exist
    {
        bool flagNeedAnd = false;
        //WHERE ...
        std::string buf = " WHERE ";

        if (nullptr != errorToReset->pId_)
        {
            if (flagNeedAnd)
                buf += " AND ";
            buf += "id = ";
            buf += std::to_string(*errorToReset->pId_);
            flagNeedAnd = true;
        }

        if (nullptr != errorToReset->pRiseDatetimeStart_)
        {
            if (flagNeedAnd)
                buf += " AND ";
            buf += "error_rise_datetime >= ";
            buf += std::to_string(*errorToReset->pRiseDatetimeStart_);
            flagNeedAnd = true;
        }

        if (nullptr != errorToReset->pRiseDatetimeEnd_)
        {
            if (flagNeedAnd)
                buf += " AND ";
            buf += " error_rise_datetime <= ";
            buf += std::to_string(*errorToReset->pRiseDatetimeEnd_);
            flagNeedAnd = true;
        }

        if (nullptr != errorToReset->pMsg_)
        {
            if (flagNeedAnd)
                buf += " AND ";
            buf += " message = ";
            buf += "\"";
            buf += *errorToReset->pMsg_;
            buf += "\"";
            flagNeedAnd = true;
        }

        if (nullptr != errorToReset->pSoId_)
        {
            if (flagNeedAnd)
                buf += " AND ";
            buf += "so_id = ";
            buf += std::to_string(*errorToReset->pSoId_);
            flagNeedAnd = true;
        }

        if (flagNeedAnd)
            buf += " AND ";
        buf += " error_fix_datetime IS NULL "; //not to change already fixed errors
        queryStr += buf;
    }

    //preare Q
    QSqlQuery query;
    if ( !query.prepare(queryStr.c_str()))
    {
        lastError_ = "ResetErrorSO: Problem in prearing sql-request ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }
    //exec query
    if ( !query.exec() )
    {
        lastError_ = "ResetErrorSO: smth wrong in execution query ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    //check for query errors
    std::string info;
    if ( !checkQueryError(query, &info ) )
        LG_ERR_RET_FALSE;
    std::cerr << info << std::endl; // aux, info about success

    return true;
}

bool DbMgr::InsertActioSO(ActionSOData& actionToInsert)
{
    //prepare query
    QSqlQuery query;
    if ( !query.prepare("INSERT INTO `action_so` (action_datetime, action, user_id, so_id)"
                        "VALUES (?, ?, ?, ?);") )
    {
        lastError_ = "InsertActioSO: Problem in prearing sql-request ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    //place value to query
    query.bindValue(0, std::to_string(actionToInsert.actionDatetime_).c_str()); //action_datetime
    query.bindValue(1,actionToInsert.action_.c_str()); //action
    query.bindValue(2, std::to_string(actionToInsert.userId_).c_str()); //user id
    query.bindValue(3, std::to_string(actionToInsert.soId_).c_str()); //so_id

    //exec query
    if ( !query.exec() )
    {
        lastError_ = "InsertActioSO: smth wrong in execution query ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    //check for query errors
    std::string info;
    if ( !checkQueryError(query, &info ) ) LG_ERR_RET_FALSE;

    std::cerr << info << std::endl; // aux, info about success
    return true;
}
bool DbMgr::SelectActionSO(std::vector<ActionSOData>& result, ActionSOQuery* actionToSelect = NULL)
{
    QSqlQuery query;
    //prepare Q
    if (NULL == actionToSelect) // == SELECT * FROM `action_so`
    {
        if ( ! query.prepare("SELECT * FROM `action_so`") )
        {
            lastError_ = "SelectActionSO: RTFM SQL";
            lastError_ += query.lastError().text().toStdString();
            return false;
        }
    }
    else // == SELECT * FROM `action_so` WHERE ...
    {
        std::string queryBuf = "SELECT * FROM `action_so` WHERE "; //be careful with whitespaces
        bool needAND = false;
        if ( NULL != actionToSelect->pActionDatetimeStart_)
        {
            queryBuf += "action_datetime >= ";
            queryBuf += std::to_string(*actionToSelect->pActionDatetimeStart_);
            needAND = true;
        }
        if ( NULL != actionToSelect->pActionDatetimeEnd_)
        {
            if (needAND)
                queryBuf += " AND";
            queryBuf += " action_datetime <= ";
            queryBuf += std::to_string(*actionToSelect->pActionDatetimeEnd_);
            needAND = true;
        }
        if ( NULL != actionToSelect->pAction_)
        {
            if (needAND)
                queryBuf += " AND";
            queryBuf += " action = ";
            queryBuf += *actionToSelect->pAction_;
            needAND = true;
        }
        if ( NULL != actionToSelect->pUserId_)
        {
            if (needAND)
                queryBuf += " AND";
            queryBuf += " user_id = ";
            queryBuf += std::to_string(*actionToSelect->pUserId_);
            needAND = true;
        }
        if ( NULL != actionToSelect->pSoId_)
        {
            if (needAND)
                queryBuf += " AND";
            queryBuf += " so_id = ";
            queryBuf += std::to_string(*actionToSelect->pSoId_);
            needAND = true;
        }
        queryBuf += ";";
        if ( ! query.prepare(queryBuf.c_str()) )
        {
            lastError_ = "SelectActionSO: RTFM SQL";
            lastError_ += query.lastError().text().toStdString();
            LG_ERR_RET_FALSE;
        }
    }
    //send Q
    if ( !query.exec() )
    {
        lastError_ = "SelectActionSO: smth wrong in execution query ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }
    //check errors in Q
    std::string info;
    if ( !checkQueryError(query, &info ) ) LG_ERR_RET_FALSE;

    std::cerr << info << std::endl; // aux, info about success query

    //parsing result
    QSqlRecord rec = query.record();
    TObjId id = 0; //generated by db; do not touch
    time_t actionDatetime = 0;
    std::string action;
    TObjId userId = 0; //foreighn key on User
    TObjId soId = 0; //foreign key on SO

    while (query.next())
    {
        id = query.value(rec.indexOf("id")).toInt();
        actionDatetime = query.value(rec.indexOf("action_datetime")).toInt();
        action = query.value(rec.indexOf("action")).toString().toStdString();
        userId = query.value(rec.indexOf("user_id")).toInt();
        soId = query.value(rec.indexOf("so_id")).toInt();

        result.push_back(ActionSOData(id, actionDatetime, action, userId, soId));
    }

    return true;
}

bool DbMgr::InsertUserLog(UserLogData& userLogToInsert)
{
    //prepare query
    QSqlQuery query;
    if ( !query.prepare("INSERT INTO `user_log` (log_datetime, action, user_id)"
                        "VALUES (?, ?, ?);") )
    {
        lastError_ = "InsertActioSO: Problem in prearing sql-request ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    //place value to query
    query.bindValue(0, std::to_string(userLogToInsert.logDatetime_).c_str()); //log_datetime
    query.bindValue(1,userLogToInsert.action_.c_str()); //action
    query.bindValue(2, std::to_string(userLogToInsert.userId_).c_str()); //user id


    //exec query
    if ( !query.exec() )
    {
        lastError_ = "InsertUserLog: smth wrong in execution query ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    //check for query errors
    std::string info;
    if ( !checkQueryError(query, &info ) ) LG_ERR_RET_FALSE;

    std::cerr << info << std::endl; // aux, info about success
    return true;
}
bool DbMgr::SelectUserLog(std::vector<UserLogData>& result, UserLogQuery* userToSelect = nullptr)
{
    QSqlQuery query;
    //prepare Q
    if (NULL == userToSelect) // == SELECT * FROM `user_log`
    {
        if ( ! query.prepare("SELECT * FROM `user_log`") )
        {
            lastError_ = "SelectUserLog: RTFM SQL ";
            lastError_ += query.lastError().text().toStdString();
            LG_ERR_RET_FALSE;
        }
    }
    else // == SELECT * FROM `user_log` WHERE ...
    {
        std::string queryBuf = "SELECT * FROM `user_log` WHERE "; //be careful with whitespaces
        bool needAND = false;

        if ( NULL != userToSelect->pId_)
        {
            queryBuf += "id = ";
            queryBuf += std::to_string(*userToSelect->pId_);
            needAND = true;
        }
        if ( NULL != userToSelect->pLogDatetimeStart_)
        {
            queryBuf += "log_datetime >= ";
            queryBuf += std::to_string(*userToSelect->pLogDatetimeStart_);
            needAND = true;
        }
        if ( NULL != userToSelect->pLogDatetimeEnd_)
        {
            if (needAND)
                queryBuf += " AND";
            queryBuf += " log_datetime <= ";
            queryBuf += std::to_string(*userToSelect->pLogDatetimeEnd_);
            needAND = true;
        }
        if ( NULL != userToSelect->pAction_)
        {
            if (needAND)
                queryBuf += " AND";
            queryBuf += " action = ";
            queryBuf += *userToSelect->pAction_;
            needAND = true;
        }
        if ( NULL != userToSelect->pUserId_)
        {
            if (needAND)
                queryBuf += " AND";
            queryBuf += " user_id = ";
            queryBuf += std::to_string(*userToSelect->pUserId_);
            needAND = true;
        }

        queryBuf += ";";
        if ( ! query.prepare(queryBuf.c_str()) )
        {
            lastError_ = "SelectUserLog: RTFM SQL";
            lastError_ += query.lastError().text().toStdString();
            LG_ERR_RET_FALSE;
        }
    }
    //send Q
    if ( !query.exec() )
    {
        lastError_ = "SelectUserLog: smth wrong in execution query ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }
    //check errors in Q
    std::string info;
    if ( !checkQueryError(query, &info ) ) LG_ERR_RET_FALSE;

    std::cerr << info << std::endl; // aux, info about success query

    //parsing result


    QSqlRecord rec = query.record();
    TObjId id = 0;
    time_t logDatetime = 0;
    std::string action;
    TObjId userId = 0;
    while (query.next())
    {
        id = query.value(rec.indexOf("id")).toInt();
        logDatetime = query.value(rec.indexOf("log_datetime")).toInt();
        action = query.value(rec.indexOf("action")).toString().toStdString();
        userId = query.value(rec.indexOf("user_id")).toInt();

        result.push_back(UserLogData(id, logDatetime, action, userId));
    }

    return true;
}

bool DbMgr::InsertTraffic(TrafficData& trafficToInsert)
{
    //prepare query
    QSqlQuery query;
    if ( !query.prepare("INSERT INTO `traffic` (traffic_datetime, traffic_data, so_id, detector_number_on_so)"
                        "VALUES (?, ?, ?, ?);") )
    {
        lastError_ = "InsertTraffic: Problem in prearing sql-request ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    //place value to query
    query.bindValue(0, std::to_string(trafficToInsert.datetime_).c_str()); //traffic_datetime
    query.bindValue(1, std::to_string(trafficToInsert.data_).c_str()); //traffic_data
    query.bindValue(2, std::to_string(trafficToInsert.soId_).c_str()); //so_id
    query.bindValue(3, std::to_string(trafficToInsert.detectorNum_).c_str()); //detector_number_on_so

    //exec query
    if ( !query.exec() )
    {
        lastError_ = "InsertTraffic: smth wrong in execution query ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    //check for query errors
    std::string info;
    if ( !checkQueryError(query, &info ) ) LG_ERR_RET_FALSE;

    std::cerr << info << std::endl; // aux, info about success
    return true;
}
bool DbMgr::SelectTraffic(std::vector<TrafficData>& result, TrafficQuery* trafficToSelect)
{
    QSqlQuery query;
    //prepare Q
    if (NULL == trafficToSelect) // == SELECT * FROM `traffic`
    {
        if ( ! query.prepare("SELECT * FROM `traffic`") )
        {
            lastError_ = "SelectTraffic: RTFM SQL ";
            lastError_ += query.lastError().text().toStdString();
            LG_ERR_RET_FALSE;
        }
    }
    else // == SELECT * FROM `traffic` WHERE ...
    {
        std::string queryBuf = "SELECT * FROM `traffic` WHERE "; //be careful with whitespaces
        bool needAND = false;
        if ( NULL != trafficToSelect->pDatetimeStart_)
        {
            queryBuf += " traffic_datetime >= ";
            queryBuf += std::to_string(*trafficToSelect->pDatetimeStart_);
            needAND = true;
        }
        if ( NULL != trafficToSelect->pDatetimeEnd_)
        {
            if (needAND)
                queryBuf += " AND";
            queryBuf += " traffic_datetime <= ";
            queryBuf += std::to_string(*trafficToSelect->pDatetimeEnd_);
            needAND = true;
        }
        if ( NULL != trafficToSelect->pData_)
        {
            if (needAND)
                queryBuf += " AND";
            queryBuf += " traffic_data = ";
            queryBuf += std::to_string(*trafficToSelect->pData_);
            needAND = true;
        }
        if ( NULL != trafficToSelect->pSoId_)
        {
            if (needAND)
                queryBuf += " AND";
            queryBuf += " so_id = ";
            queryBuf += std::to_string(*trafficToSelect->pSoId_);
            needAND = true;
        }
        if ( NULL != trafficToSelect->pDetectorNum_)
        {
            if (needAND)
                queryBuf += " AND";
            queryBuf += " detector_namber_on_so = ";
            queryBuf += std::to_string(*trafficToSelect->pDetectorNum_);
            needAND = true;
        }
        queryBuf += ";";

        if ( ! query.prepare(queryBuf.c_str()) )
        {
            lastError_ = "SelectTraffic: RTFM SQL";
            lastError_ += query.lastError().text().toStdString();
            LG_ERR_RET_FALSE;
        }
    }
    //send Q
    if ( !query.exec() )
    {
        lastError_ = "SelectActionSO: smth wrong in execution query ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }
    //check errors in Q
    std::string info;
    if ( !checkQueryError(query, &info ) ) LG_ERR_RET_FALSE;

    std::cerr << info << std::endl; // aux, info about success query

    //parsing result
    QSqlRecord rec = query.record();
    TObjId id = 0; //
    time_t datetime = 0;
    unsigned int data;
    TObjId soId = 0;
    unsigned int detectorNum_ = 0;

    while (query.next())
    {
        id = query.value(rec.indexOf("id")).toInt();
        datetime = query.value(rec.indexOf("traffic_datetime")).toInt();
        data = query.value(rec.indexOf("traffic_data")).toInt();
        soId = query.value(rec.indexOf("so_id")).toInt();
        detectorNum_ = query.value(rec.indexOf("detector_number_on_so")).toInt();


        result.push_back(TrafficData(id, datetime, data, soId, detectorNum_));
    }
    return true;
}

/*******************************************************************************/
//specific funcs:
//Query -> Map<string, string>; Use to Select and for Where to Update
bool pStructToMap(CCPlanQuery &ccPlanToSelect, std::map<std::string, std::string>& queryMap)
{
    queryMap.clear();
    queryMap["tablename"] = "cc_plan";
    if (nullptr != ccPlanToSelect.pId_)
    {
        queryMap["id"] = std::to_string(*ccPlanToSelect.pId_);
    }
    if (nullptr != ccPlanToSelect.pName_)
    {
        queryMap["name"] = *ccPlanToSelect.pName_;
    }
    if (nullptr != ccPlanToSelect.pDescription_)
    {
        queryMap["description"] = *ccPlanToSelect.pDescription_;
    }
    if (nullptr != ccPlanToSelect.pCycleTime_)
    {
        queryMap["cycle_time"] = std::to_string(*ccPlanToSelect.pCycleTime_);
    }
    if (nullptr != ccPlanToSelect.pMapImgUri_)
    {
        queryMap["map_img_uri"] = *ccPlanToSelect.pMapImgUri_;
    }
    if (nullptr != ccPlanToSelect.pCycleImgUri_)
    {
        queryMap["cycle_img_uri"] = *ccPlanToSelect.pCycleImgUri_;
    }
    if (nullptr != ccPlanToSelect.pZoneCCId_)
    {
        queryMap["cc_zone_id"] = std::to_string(*ccPlanToSelect.pZoneCCId_);
    }
    return true;
}
bool pStructToMap(CCPlanPhaseQuery &ccPlanPhaseToSelect, std::map<std::string, std::string> &queryMap)
{
    queryMap.clear();
    queryMap["tablename"] = "cc_plan_has_phase";
    if (nullptr != ccPlanPhaseToSelect.pCCPlanId_)
    {
        queryMap["cc_plan_id"] = std::to_string(*ccPlanPhaseToSelect.pCCPlanId_);
    }
    if (nullptr != ccPlanPhaseToSelect.pSoId_)
    {
        queryMap["so_id"] = std::to_string(*ccPlanPhaseToSelect.pSoId_);
    }
    if (nullptr != ccPlanPhaseToSelect.pPhaseNum_)
    {
        queryMap["phase_number_on_so"] = std::to_string(*ccPlanPhaseToSelect.pPhaseNum_);
    }
    if (nullptr != ccPlanPhaseToSelect.pTimeOn_)
    {
        queryMap["time_on"] = std::to_string(*ccPlanPhaseToSelect.pTimeOn_);
    }
    return true;
}
bool pStructToMap(CCZoneQuery& ccZoneToSelect, std::map<std::string, std::string>& queryMap)
{
    queryMap.clear();
    queryMap["tablename"] = "cc_zone";
    if (nullptr != ccZoneToSelect.pId_)
    {
        queryMap["id"] = std::to_string(*ccZoneToSelect.pId_);
    }
    if (nullptr != ccZoneToSelect.pName_)
    {
        queryMap["name"] = *ccZoneToSelect.pName_;
    }
    if (nullptr != ccZoneToSelect.pDescription_)
    {
        queryMap["description"] = *ccZoneToSelect.pDescription_;
    }

    return true;
}
bool pStructToMap(CCZoneSOQuery& ccZoneSOToSelect, std::map<std::string, std::string>& queryMap)
{
    queryMap.clear();
    queryMap["tablename"] = "cc_zone_has_so";
    if (nullptr != ccZoneSOToSelect.pSoId_)
    {
        queryMap["so_id"] = std::to_string(*ccZoneSOToSelect.pSoId_);
    }
    if (nullptr != ccZoneSOToSelect.pCCZoneId_)
    {
        queryMap["cc_zone_id"] = std::to_string(*ccZoneSOToSelect.pCCZoneId_);
    }
    return true;
}
bool pStructToMap(DetectorQuery& detectorToSelect, std::map<std::string, std::string>& queryMap)
{
    queryMap.clear();
    queryMap["tablename"] = "detector";
    if (nullptr != detectorToSelect.pId_)
    {
        queryMap["id"] = std::to_string(*detectorToSelect.pId_);
    }
    if (nullptr != detectorToSelect.pVendor_)
    {
        queryMap["vendor"] = *detectorToSelect.pVendor_ ;
    }
    if (nullptr != detectorToSelect.pSerialN_)
    {
        queryMap["serial_number"] = *detectorToSelect.pSerialN_ ;
    }
    return true;
}
bool pStructToMap(DistrictQuery &districtToSelect, std::map<std::string, std::string> &queryMap)
{
    queryMap.clear();
    queryMap["tablename"] = "district";
    if (nullptr != districtToSelect.pId_)
    {
        queryMap["id"] = std::to_string(*districtToSelect.pId_);
    }
    if (nullptr != districtToSelect.pName_)
    {
        queryMap["name"] = *districtToSelect.pName_;
    }
    if (nullptr != districtToSelect.pDescription_)
    {
        queryMap["description"] = *districtToSelect.pDescription_;
    }
    return true;
}
bool pStructToMap(LogbookQuery& logbookQuery, std::map<std::string, std::string>& queryMap)
{
    queryMap.clear();
    queryMap["tablename"] = "logbook";
    if (nullptr != logbookQuery.pSoId_)
    {
        queryMap["so_id"] = std::to_string(*logbookQuery.pSoId_);
    }
    if (nullptr != logbookQuery.pEntry_)
    {
        queryMap["entry"] = *logbookQuery.pEntry_;
    }
    return true;
}
bool pStructToMap(PhaseQuery& phaseToSelect,std::map<std::string, std::string>& queryMap)
{
    queryMap.clear();
    queryMap["tablename"] = "phase";
    if (nullptr != phaseToSelect.pSoId_)
    {
        queryMap["so_id"] = std::to_string(*phaseToSelect.pSoId_);
    }
    if (nullptr != phaseToSelect.pNumOnSO_)
    {
        queryMap["number_on_so"] = std::to_string(*phaseToSelect.pNumOnSO_);
    }
    if (nullptr != phaseToSelect.pImgUri_)
    {
        queryMap["img_uri"] = *phaseToSelect.pImgUri_;
    }
    return true;
}
bool pStructToMap(PlanQuery& planToSelect, std::map<std::string, std::string>& queryMap)
{
    queryMap.clear();
    queryMap["tablename"] = "plan";

    if (nullptr != planToSelect.pSoId_)
    {
        queryMap["so_id"] = std::to_string(*planToSelect.pSoId_);
    }
    if (nullptr != planToSelect.pNumOnSO_)
    {
        queryMap["number_on_so"] = std::to_string(*planToSelect.pNumOnSO_);
    }
    if (nullptr != planToSelect.pDescription_)
    {
        queryMap["description"] = *planToSelect.pDescription_;
    }
    return true;
}
bool pStructToMap(PlanPhaseQuery& planPhaseToSelect, std::map<std::string, std::string>& queryMap)
{
    queryMap.clear();
    queryMap["tablename"] = "plan_has_phase";
    if (nullptr != planPhaseToSelect.pSoId_)
    {
        queryMap["so_id"] = std::to_string(*planPhaseToSelect.pSoId_);
    }
    if (nullptr != planPhaseToSelect.pPlanNum_)
    {
        queryMap["plan_number_on_so"] = std::to_string(*planPhaseToSelect.pPlanNum_);
    }
    if (nullptr != planPhaseToSelect.pPhaseNum_)
    {
        queryMap["phase_number_on_so"] = std::to_string(*planPhaseToSelect.pPhaseNum_);
    }
    if (nullptr != planPhaseToSelect.pMainTime_)
    {
        queryMap["main_time"] = std::to_string(*planPhaseToSelect.pMainTime_);
    }
    if (nullptr != planPhaseToSelect.pMaxTime_)
    {
        queryMap["max_time"] = std::to_string(*planPhaseToSelect.pMaxTime_);
    }
    if (nullptr != planPhaseToSelect.pMinTime_)
    {
        queryMap["min_time"] = std::to_string(*planPhaseToSelect.pMinTime_);
    }
    return true;
}
bool pStructToMap(SOQuery &soToSelect, std::map<std::string, std::string> &queryMap)
{
    queryMap.clear();
    queryMap["tablename"] = "so";
    if (nullptr != soToSelect.pId_)
    {
        queryMap["id"] = std::to_string(*soToSelect.pId_);
    }
    if (nullptr != soToSelect.pName_)
    {
        queryMap["name"] = *soToSelect.pName_;
    }
    if (nullptr != soToSelect.pIp_)
    {
        queryMap["ip"] = *soToSelect.pIp_;
    }
    if (nullptr != soToSelect.pAdress_)
    {
        queryMap["address"] = *soToSelect.pAdress_;
    }
    if (nullptr != soToSelect.pVendor_)
    {
        queryMap["vendor"] = *soToSelect.pVendor_;
    }
    if (nullptr != soToSelect.pPassportUri_)
    {
        queryMap["passport_uri"] = *soToSelect.pPassportUri_;
    }
    if (nullptr != soToSelect.pDistrictId_)
    {
        queryMap["district_id"] = std::to_string(*soToSelect.pDistrictId_);
    }
    if (nullptr != soToSelect.pLatitude_)
    {
        queryMap["latitude"] = *soToSelect.pLatitude_;
    }
    if (nullptr != soToSelect.pLongitude_)
    {
        queryMap["longitude"] = *soToSelect.pLongitude_;
    }
    return true;
}
bool pStructToMap(SODetectorQuery& soDetectorToSelect, std::map<std::string, std::string>& queryMap)
{
    queryMap.clear();
    queryMap["tablename"] = "so_detector";
    if (nullptr != soDetectorToSelect.pSoId_)
    {
        queryMap["so_id"] = std::to_string(*soDetectorToSelect.pSoId_);
    }
    if (nullptr != soDetectorToSelect.pNumOnSO_)
    {
        queryMap["number_on_so"] = std::to_string(*soDetectorToSelect.pNumOnSO_);
    }
    if (nullptr != soDetectorToSelect.pDetectorId_)
    {
        queryMap["detector_id"] = std::to_string(*soDetectorToSelect.pDetectorId_);
    }
    return true;
}
bool pStructToMap(UserQuery &userToSelect, std::map<std::string, std::string> &queryMap)
{
    queryMap.clear();
    queryMap["tablename"] = "users";
    if (nullptr != userToSelect.pId_)
    {
        queryMap["id"] = std::to_string(*userToSelect.pId_);
    }
    if (nullptr != userToSelect.pUserName_)
    {
        queryMap["user_name"] = *userToSelect.pUserName_;
    }
    if (nullptr != userToSelect.pPwd_)
    {
        queryMap["pwd"] = *userToSelect.pPwd_;
    }
    if (nullptr != userToSelect.pLvl_)
    {
        queryMap["lvl"] = std::to_string(*userToSelect.pLvl_);
    }
    return true;
}

//Data -> Map<string, string>; Use to Insert and for Set to Update
bool structToMap(CCPlanData& ccPlanData, std::map<std::string, std::string> &dataMap)
{
    dataMap.clear();
    dataMap["tablename"] = "cc_plan";

    dataMap["name"] = ccPlanData.name_;
    dataMap["description"] = ccPlanData.description_;
    dataMap["cycle_time"] = std::to_string(ccPlanData.cycleTime_);
    dataMap["map_img_uri"] = ccPlanData.mapImgUri_;
    dataMap["cycle_img_uri"] = ccPlanData.cycleImgUri_;
    dataMap["cc_zone_id"] = std::to_string(ccPlanData.ccZoneId_);

    return true;
}
bool structToMap(CCPlanPhaseData& ccPlanPhaseData, std::map<std::string, std::string> &dataMap)
{
    dataMap.clear();
    dataMap["tablename"] = "cc_plan_has_phase";
    dataMap["cc_plan_id"] = std::to_string(ccPlanPhaseData.ccPlanId_);
    dataMap["so_id"] = std::to_string(ccPlanPhaseData.soId_);
    dataMap["phase_number_on_so"] = std::to_string(ccPlanPhaseData.phaseNum_);
    dataMap["time_on"] = std::to_string(ccPlanPhaseData.timeOn_);
    return true;
}
bool structToMap(CCZoneData& ccZoneData, std::map<std::string, std::string>& dataMap)
{
    dataMap.clear();
    dataMap["tablename"] = "cc_zone";

    dataMap["name"] = ccZoneData.name_;
    dataMap["description"] = ccZoneData.description_;
    return true;
}
bool structToMap(CCZoneSOData& ccZoneSOData, std::map<std::string, std::string>& dataMap)
{
    dataMap.clear();
    dataMap["tablename"] = "cc_zone_has_so";
    dataMap["so_id"] = std::to_string(ccZoneSOData.soId_);
    dataMap["cc_zone_id"] = std::to_string(ccZoneSOData.ccZoneId_);

    return true;
}
bool structToMap(DetectorData& detectorD, std::map<std::string, std::string>& dataMap)
{
    dataMap.clear();
    dataMap["tablename"] = "detector";
    dataMap["vendor"] = detectorD.vendor_;
    dataMap["serial_number"] = detectorD.serialN_;

    return true;
}
bool structToMap(DistrictData &districtData, std::map<std::string, std::string> &dataMap)
{
    dataMap.clear();
    dataMap["tablename"] = "district";

    dataMap["name"] = districtData.name_;
    dataMap["description"] = districtData.description_;

    return true;
}
bool structToMap(LogbookData& logbookData, std::map<std::string, std::string> &dataMap)
{
    dataMap.clear();
    dataMap["tablename"] = "logbook";

    dataMap["so_id"] = std::to_string(logbookData.soId_);
    dataMap["entry"] = logbookData.entry_;
    return true;
}
bool structToMap(PhaseData& phaseData, std::map<std::string, std::string>& dataMap)
{
    dataMap.clear();
    dataMap["tablename"] = "phase";

    dataMap["so_id"] = std::to_string(phaseData.soId_);
    dataMap["number_on_so"] = std::to_string(phaseData.numOnSO_);
    dataMap["img_uri"] = phaseData.imgUri_;
    return true;
}
bool structToMap(PlanData& planData, std::map<std::string, std::string>& dataMap)
{
    dataMap.clear();
    dataMap["tablename"] = "plan";
    dataMap["so_id"] = std::to_string(planData.soId_);
    dataMap["number_on_so"] = std::to_string(planData.numOnSO_);
    dataMap["description"] = planData.description_;
    return true;
}
bool structToMap(PlanPhaseData& planPhaseData, std::map<std::string, std::string>& dataMap)
{
    dataMap.clear();
    dataMap["tablename"] = "plan_has_phase";

    dataMap["so_id"] = std::to_string(planPhaseData.soId_);
    dataMap["plan_number_on_so"] = std::to_string(planPhaseData.planNum_);
    dataMap["phase_number_on_so"] = std::to_string(planPhaseData.phaseNum_);
    dataMap["main_time"] = std::to_string(planPhaseData.mainTime_);
    dataMap["min_time"] = std::to_string(planPhaseData.minTime_);
    dataMap["max_time"] = std::to_string(planPhaseData.maxTime_);
    return true;
}
bool structToMap(SOData& soData, std::map<std::string, std::string> &dataMap)
{
    dataMap.clear();
    dataMap["tablename"] = "so";
    dataMap["name"] = soData.name_;
    dataMap["ip"] = soData.ip_;
    dataMap["address"] = soData.address_;
    dataMap["vendor"] = soData.vendor_;
    dataMap["passport_uri"] = soData.passportUri_;
    dataMap["district_id"] = std::to_string(soData.districtId_);
    dataMap["latitude"] = soData.latitude_;
    dataMap["longitude"] = soData.longitude_;

    return true;
}
bool structToMap(SODetectorData& soDetectorD, std::map<std::string, std::string>& dataMap)
{
    dataMap.clear();
    dataMap["tablename"] = "so_detector";
    dataMap["so_id"] = std::to_string(soDetectorD.soId_);
    dataMap["number_on_so"] = std::to_string(soDetectorD.numOnSO_);
    dataMap["detector_id"] = std::to_string(soDetectorD.detectorId_);
    return true;
}
bool structToMap(UserData& userData, std::map<std::string, std::string> &dataMap)
{
    dataMap.clear();
    dataMap["tablename"] = "users";

    dataMap["user_name"] = userData.userName_;
    dataMap["pwd"] = userData.pwd_;
    dataMap["lvl"] = std::to_string(userData.lvl_);

    return true;
}


//Parsing result: Qquery -> struct
bool parseResult(QSqlQuery& query, std::vector<CCPlanData>& result)
{
    QSqlRecord rec = query.record();

    TObjId id = 0;
    std::string name;
    std::string description;
    unsigned int cycleTime = 0;
    std::string mapImgUri;
    std::string cycleImgUri;
    TObjId zoneCCId = 0;

    while (query.next())
    {
        id = query.value(rec.indexOf("id")).toInt();
        name = query.value(rec.indexOf("name")).toString().toStdString();
        description = query.value(rec.indexOf("description")).toString().toStdString();
        cycleTime = query.value(rec.indexOf("cycle_time")).toInt();
        mapImgUri = query.value(rec.indexOf("map_img_uri")).toString().toStdString();
        cycleImgUri = query.value(rec.indexOf("cycle_img_uri")).toString().toStdString();
        zoneCCId = query.value(rec.indexOf("cc_zone_id")).toInt();

        result.push_back(CCPlanData(id, name, description, cycleTime, mapImgUri, cycleImgUri, zoneCCId));
    }

    return true;
}
bool parseResult(QSqlQuery &query, std::vector<CCPlanPhaseData> &result)
{
    QSqlRecord rec = query.record();

    TObjId ccPlanId = 0;
    TObjId soId;
    unsigned int phaseNum = 0;
    unsigned int timeOn = 0;

    while (query.next())
    {
        ccPlanId = query.value(rec.indexOf("cc_plan_id")).toInt();
        soId = query.value(rec.indexOf("so_id")).toInt();
        phaseNum = query.value(rec.indexOf("phase_number_on_so")).toInt();
        timeOn = query.value(rec.indexOf("time_on")).toInt();

        result.push_back(CCPlanPhaseData(ccPlanId, soId, phaseNum, timeOn));
    }

    return true;
}
bool parseResult(QSqlQuery& query, std::vector<CCZoneData>& result)
{
    QSqlRecord rec = query.record();

    TObjId id = 0;
    std::string name;
    std::string description;

    while (query.next())
    {
        id = query.value(rec.indexOf("id")).toInt();
        name = query.value(rec.indexOf("name")).toString().toStdString();
        description = query.value(rec.indexOf("description")).toString().toStdString();

        result.push_back(CCZoneData(id, name, description));
    }

    return true;
}
bool parseResult(QSqlQuery& query, std::vector<CCZoneSOData>& result)
{
    QSqlRecord rec = query.record();

    TObjId soId = 0;
    TObjId ccZoneId = 0;

    while (query.next())
    {
        soId = query.value(rec.indexOf("so_id")).toInt();
        ccZoneId = query.value(rec.indexOf("cc_zone_id")).toInt();

        result.push_back(CCZoneSOData(soId, ccZoneId));
    }

    return true;
}
bool parseResult(QSqlQuery& query, std::vector<DetectorData>& result)
{
    QSqlRecord rec = query.record();

    TObjId id = 0;
    std::string vendor;
    std::string serialN;

    while (query.next())
    {
        id = query.value(rec.indexOf("id")).toInt();
        vendor = query.value(rec.indexOf("vendor")).toString().toStdString();
        serialN = query.value(rec.indexOf("serial_number")).toString().toStdString();

        result.push_back(DetectorData(id, vendor, serialN));
    }

    return true;
}
bool parseResult(QSqlQuery &query, std::vector<DistrictData> &result)
{
    QSqlRecord rec = query.record();

    TObjId id;
    std::string name;
    std::string description;

    while (query.next())
    {
        id = query.value(rec.indexOf("id")).toInt();
        name = query.value(rec.indexOf("name")).toString().toStdString();
        description = query.value(rec.indexOf("description")).toString().toStdString();

        result.push_back(DistrictData(id, name, description));
    }
    return true;
}
bool parseResult(QSqlQuery& query, std::vector<LogbookData>& result)
{
    QSqlRecord rec = query.record();

    TObjId so_id = 0;
    std::string entry;

    while ( query.next() )
    {
        so_id = query.value(rec.indexOf("so_id")).toInt();
        entry = query.value(rec.indexOf("entry")).toString().toStdString();

        result.push_back(LogbookData(so_id, entry));
    }
    return true;
}
bool parseResult(QSqlQuery& query, std::vector<PhaseData>& result)
{
    QSqlRecord rec = query.record();

    TObjId soId = 0;
    unsigned int numOnSO = 0;
    std::string imgUri;

    while (query.next())
    {
        soId = query.value(rec.indexOf("so_id")).toInt();
        numOnSO = query.value(rec.indexOf("number_on_so")).toInt();
        imgUri = query.value(rec.indexOf("img_uri")).toString().toStdString();

        result.push_back(PhaseData(soId, numOnSO, imgUri));
    }

    return true;
}
bool parseResult(QSqlQuery& query, std::vector<PlanData>& result)
{
    QSqlRecord rec = query.record();

    TObjId soId = 0;
    unsigned int numOnSO = 0;
    std::string description;

    while (query.next())
    {
        soId = query.value(rec.indexOf("so_id")).toInt();
        numOnSO = query.value(rec.indexOf("number_on_so")).toInt();
        description = query.value(rec.indexOf("description")).toString().toStdString();

        result.push_back(PlanData(soId, numOnSO, description));
    }

    return true;
}
bool parseResult(QSqlQuery& query, std::vector<PlanPhaseData>& result)
{
    QSqlRecord rec = query.record();

    TObjId soId = 0;
    unsigned int planNum = 0;
    unsigned int phaseNum = 0;
    unsigned int mainTime = 0;
    unsigned int minTime = 0;
    unsigned int maxTime = 0;

    while (query.next())
    {
        soId = query.value(rec.indexOf("so_id")).toInt();
        planNum = query.value(rec.indexOf("plan_number_on_so")).toInt();
        phaseNum = query.value(rec.indexOf("phase_number_on_so")).toInt();
        mainTime = query.value(rec.indexOf("main_time")).toInt();
        minTime = query.value(rec.indexOf("min_time")).toInt();
        maxTime = query.value(rec.indexOf("max_time")).toInt();

        result.push_back(PlanPhaseData(soId, planNum, phaseNum, mainTime, minTime, maxTime));
    }

    return true;
}
bool parseResult(QSqlQuery &query, std::vector<SOData> &result)
{
    QSqlRecord rec = query.record();

    TObjId id;
    std::string name;
    std::string ip;
    std::string address;
    std::string vendor;
    std::string passportUri;
    TObjId districtId;
    std::string latitude;
    std::string longitude;

    while (query.next())
    {
        id = query.value(rec.indexOf("id")).toInt();
        name = query.value(rec.indexOf("name")).toString().toStdString();
        ip = query.value(rec.indexOf("ip")).toString().toStdString();
        address = query.value(rec.indexOf("address")).toString().toStdString();
        vendor = query.value(rec.indexOf("vendor")).toString().toStdString();
        passportUri = query.value(rec.indexOf("passport_uri")).toString().toStdString();
        districtId = query.value(rec.indexOf("district_id")).toInt();
        latitude = query.value(rec.indexOf("latitude")).toString().toStdString();
        longitude = query.value(rec.indexOf("longitude")).toString().toStdString();
        result.push_back(SOData(id, name, ip, address, vendor, passportUri, districtId, latitude, longitude));
    }
    return true;
}
bool parseResult(QSqlQuery& query, std::vector<SODetectorData>& result)
{
    QSqlRecord rec = query.record();

    TObjId soId = 0;
    unsigned int numOnSO = 0;
    TObjId detectorId = 0;

    while (query.next())
    {
        soId = query.value(rec.indexOf("so_id")).toInt();
        numOnSO = query.value(rec.indexOf("number_on_so")).toInt();
        detectorId = query.value(rec.indexOf("detector_id")).toInt();

        result.push_back(SODetectorData(soId, numOnSO, detectorId));
    }

    return true;
}
bool parseResult(QSqlQuery &query, std::vector<UserData> &result)
{
    QSqlRecord rec = query.record();

    TObjId id;
    std::string userName;
    std::string pwd;
    UserLevel lvl;

    while (query.next())
    {
        id = query.value(rec.indexOf("id")).toInt();
        userName = query.value(rec.indexOf("user_name")).toString().toStdString();
        pwd = query.value(rec.indexOf("pwd")).toString().toStdString();
        lvl = (UserLevel)query.value(rec.indexOf("lvl")).toInt();
        result.push_back(UserData(id, userName, pwd, lvl));
    }
    return true;
}


//template funcs with specialty
//INSERT
template bool DbMgr::InsertData<CCPlanData>(CCPlanData& structToInsert);
template bool DbMgr::InsertData<CCPlanPhaseData>(CCPlanPhaseData& structToInsert);
template bool DbMgr::InsertData<CCZoneData>(CCZoneData& structToInsert);
template bool DbMgr::InsertData<CCZoneSOData>(CCZoneSOData& structToInsert);
template bool DbMgr::InsertData<DetectorData>(DetectorData& structToInsert);
template bool DbMgr::InsertData<DistrictData>(DistrictData& structToInsert);
template bool DbMgr::InsertData<LogbookData>(LogbookData& logbookData);
template bool DbMgr::InsertData<PhaseData>(PhaseData& structToInsert);
template bool DbMgr::InsertData<PlanData>(PlanData& structToInsert);
template bool DbMgr::InsertData<PlanPhaseData>(PlanPhaseData& structToInsert);
template bool DbMgr::InsertData<SOData>(SOData& structToInsert);
template bool DbMgr::InsertData<SODetectorData>(SODetectorData& structToInsert);
template bool DbMgr::InsertData<UserData>(UserData& structToInsert);


template <typename T> bool DbMgr::InsertData(T& structToInsert)
{
    //prepare data to insert
    std::map<std::string, std::string> dataMap;
    if ( !structToMap(structToInsert, dataMap) )
    {
        //always returns 'true'
        LG_ERR_RET_FALSE;
    }

    //prepare query
    QSqlQuery query;
    if ( !prepareInsert(query, dataMap) )
    {
        lastError_ = "InsertData: smth wrong in preparing query ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    //exec query
    if ( !query.exec() )
    {
        lastError_ = "InsertData: smth wrong in execution query ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    //check for query errors
    std::string info;
    if ( !checkQueryError(query, &info ) ) LG_ERR_RET_FALSE;

    std::cerr << info << std::endl; // aux, info about success
    return true;
}

//SELECT
template bool DbMgr::SelectQuery<CCPlanData, CCPlanQuery> (std::vector<CCPlanData>& result, CCPlanQuery& structToSelect, std::string* orderColumn, unsigned int limit);
template bool DbMgr::SelectQuery<CCPlanPhaseData, CCPlanPhaseQuery> (std::vector<CCPlanPhaseData>& result, CCPlanPhaseQuery& structToSelect, std::string* orderColumn, unsigned int limit);
template bool DbMgr::SelectQuery<CCZoneData, CCZoneQuery> (std::vector<CCZoneData>& result, CCZoneQuery& structToSelect, std::string* orderColumn, unsigned int limit);
template bool DbMgr::SelectQuery<CCZoneSOData, CCZoneSOQuery> (std::vector<CCZoneSOData>& result, CCZoneSOQuery& structToSelect, std::string* orderColumn, unsigned int limit);
template bool DbMgr::SelectQuery<DetectorData, DetectorQuery> (std::vector<DetectorData>& result, DetectorQuery& structToSelect, std::string* orderColumn, unsigned int limit);
template bool DbMgr::SelectQuery<DistrictData, DistrictQuery> (std::vector<DistrictData>& result, DistrictQuery& structToSelect, std::string* orderColumn, unsigned int limit);
template bool DbMgr::SelectQuery<LogbookData, LogbookQuery> (std::vector<LogbookData>& result, LogbookQuery& structToSelect, std::string* orderColumn, unsigned int limit);
template bool DbMgr::SelectQuery<PhaseData, PhaseQuery> (std::vector<PhaseData>& result, PhaseQuery& structToSelect, std::string* orderColumn, unsigned int limit);
template bool DbMgr::SelectQuery<PlanData, PlanQuery> (std::vector<PlanData>& result, PlanQuery& structToSelect, std::string* orderColumn, unsigned int limit);
template bool DbMgr::SelectQuery<PlanPhaseData, PlanPhaseQuery> (std::vector<PlanPhaseData>& result, PlanPhaseQuery& structToSelect, std::string* orderColumn, unsigned int limit);
template bool DbMgr::SelectQuery<SOData, SOQuery> (std::vector<SOData>& result, SOQuery& structToSelect, std::string* orderColumn, unsigned int limit);
template bool DbMgr::SelectQuery<SODetectorData, SODetectorQuery> (std::vector<SODetectorData>& result, SODetectorQuery& structToSelect, std::string* orderColumn, unsigned int limit);
template bool DbMgr::SelectQuery<UserData, UserQuery> (std::vector<UserData>& result, UserQuery& structToSelect, std::string* orderColumn, unsigned int limit);

template <typename D, typename Q> bool DbMgr::SelectQuery(std::vector<D>& result, Q& structToSelect, std::string* orderColumn, unsigned int limit)
{
    std::map<std::string, std::string> queryMap;
    if ( !pStructToMap(structToSelect, queryMap))
    {
        //pStructToMap always returns 'true'
        LG_ERR_RET_FALSE;
    }
    //prepare query
    QSqlQuery query;
    if (!prepareSelect(query, queryMap, orderColumn, limit))
    {
        lastError_ = "SelectQuery: smth wrong in preparing query ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    //send Q
    if ( !query.exec() )
    {
        lastError_ = "SelectQuery: smth wrong in execution query ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    //check errors in Q
    std::string info;
    if ( !checkQueryError(query, &info ) ) LG_ERR_RET_FALSE;

    std::cerr << info << std::endl; // aux, info about success query

    //parsing result
    if ( !parseResult(query, result))
    {
        lastError_ = "SelectQuery: trouble in parse result ";
        LG_ERR_RET_FALSE;
    }

    return true;
}

//UPDATE
template bool DbMgr::UpdateData<SODetectorQuery>(SODetectorQuery& dataToSet, SODetectorQuery& queryWhere);
template bool DbMgr::UpdateData<DetectorQuery>(DetectorQuery& dataToSet, DetectorQuery& queryWhere);
template bool DbMgr::UpdateData<PlanQuery>(PlanQuery& dataToSet, PlanQuery& queryWhere);
template bool DbMgr::UpdateData<PhaseQuery>(PhaseQuery& dataToSet, PhaseQuery& queryWhere);
template bool DbMgr::UpdateData<PlanPhaseQuery>(PlanPhaseQuery& dataToSet, PlanPhaseQuery& queryWhere);
template bool DbMgr::UpdateData<CCZoneQuery>(CCZoneQuery& dataToSet, CCZoneQuery& queryWhere);
template bool DbMgr::UpdateData<CCZoneSOQuery>(CCZoneSOQuery& dataToSet, CCZoneSOQuery& queryWhere);
template bool DbMgr::UpdateData<CCPlanQuery>(CCPlanQuery& dataToSet, CCPlanQuery& queryWhere);
template bool DbMgr::UpdateData<CCPlanPhaseQuery>(CCPlanPhaseQuery& dataToSet, CCPlanPhaseQuery& queryWhere);
template bool DbMgr::UpdateData<SOQuery>(SOQuery& dataToSet, SOQuery& queryWhere);
template bool DbMgr::UpdateData<UserQuery>(UserQuery& dataToSet, UserQuery& queryWhere);
template bool DbMgr::UpdateData<DistrictQuery>(DistrictQuery& dataToSet, DistrictQuery& queryWhere);

template <typename Q> bool DbMgr::UpdateData(Q& dataToSet, Q& queryWhere)
{
    //get SET-map
    std::map<std::string, std::string> mapSet;
    if ( !pStructToMap(dataToSet, mapSet) )
    {
        //always returns 'true'
        LG_ERR_RET_FALSE;
    }

    //get WHERE-map
    std::map<std::string, std::string> mapWhere;
    if ( !pStructToMap(queryWhere, mapWhere))
    {
        //always returns 'true'
        LG_ERR_RET_FALSE;
    }

    //prepare query
    QSqlQuery query;
    if ( !prepareUpdate(query, mapSet, mapWhere))
    {
        lastError_ = "UpdateData: smth wrong in preparing query ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    //send Q
    if ( !query.exec() )
    {
        lastError_ = "UpdateData: smth wrong in execution query ";
        lastError_ += query.lastError().text().toStdString();
        LG_ERR_RET_FALSE;
    }

    //check errors in Q
    std::string info;
    if ( !checkQueryError(query, &info ) ) LG_ERR_RET_FALSE;

    std::cerr << info << std::endl; // aux, info about success query

    return true;
}



//depricated: QueryStruct -> QueryString; Uses for Select
bool pStructToSelect(DetectorQuery& detectorToSelect, std::string& select)
{
    select = "SELECT * FROM `detector` ";
    bool flagEmptyStruct = true;
    bool flagNeedAnd = false;
    //case for SELECT with condition
    select += "WHERE ";
    if (nullptr != detectorToSelect.pId_)
    {
        flagEmptyStruct = false;
        flagNeedAnd = true;
        std::string buf("id = " + std::to_string(*detectorToSelect.pId_) );
        select += buf;
    }
    if (nullptr != detectorToSelect.pVendor_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;

        select += "vendor = \"";
        select += *detectorToSelect.pVendor_ ;
        select += "\"";

    }
    if (nullptr != detectorToSelect.pSerialN_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "serial_number = \"";
        select += *detectorToSelect.pSerialN_ ;
        select += "\"";
    }

    //case for SELECT ALL
    if (flagEmptyStruct)
        select = "SELECT * FROM `detector` ";
    return true;
}
bool pStructToSelect(SODetectorQuery& soDetectorToSelect, std::string& select)
{
    select = "SELECT * FROM `so_detector` ";
    bool flagEmptyStruct = true;
    bool flagNeedAnd = false;
    //case for SELECT with condition
    select += "WHERE ";
    if (nullptr != soDetectorToSelect.pSoId_)
    {
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "so_id = \"";
        select += std::to_string(*soDetectorToSelect.pSoId_);
        select += "\"";
    }
    if (nullptr != soDetectorToSelect.pNumOnSO_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "number_on_so = \"";
        select += std::to_string(*soDetectorToSelect.pNumOnSO_);
        select += "\"";
    }
    if (nullptr != soDetectorToSelect.pDetectorId_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "detector_id = \"";
        select += std::to_string(*soDetectorToSelect.pDetectorId_);
        select += "\"";
    }
    return true;
}
bool pStructToSelect(PlanQuery& planToSelect, std::string& select)
{
    select = "SELECT * FROM `plan` ";
    bool flagEmptyStruct = true;
    bool flagNeedAnd = false;
    //case for SELECT with condition
    select += "WHERE ";
    if (nullptr != planToSelect.pSoId_)
    {
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "so_id = \"";
        select += std::to_string(*planToSelect.pSoId_);
        select += "\"";
    }
    if (nullptr != planToSelect.pNumOnSO_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "number_on_so = \"";
        select += std::to_string(*planToSelect.pNumOnSO_);
        select += "\"";
    }
    if (nullptr != planToSelect.pDescription_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "description = \"";
        select += *planToSelect.pDescription_;
        select += "\"";
    }
    return true;
}
bool pStructToSelect(PhaseQuery& phaseToSelect, std::string& select)
{
    select = "SELECT * FROM `phase` ";
    bool flagEmptyStruct = true;
    bool flagNeedAnd = false;
    //case for SELECT with condition
    select += " WHERE ";
    if (nullptr != phaseToSelect.pSoId_)
    {
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "so_id = \"";
        select += std::to_string(*phaseToSelect.pSoId_);
        select += "\"";
    }
    if (nullptr != phaseToSelect.pNumOnSO_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "number_on_so = \"";
        select += std::to_string(*phaseToSelect.pNumOnSO_);
        select += "\"";
    }
    if (nullptr != phaseToSelect.pImgUri_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "img_uri = \"";
        select += *phaseToSelect.pImgUri_;
        select += "\"";
    }
    return true;
}
bool pStructToSelect(PlanPhaseQuery& planPhaseToSelect, std::string& select)
{
    select = "SELECT * FROM `plan_has_phase` ";
    bool flagEmptyStruct = true;
    bool flagNeedAnd = false;
    //case for SELECT with condition
    select += "WHERE ";
    if (nullptr != planPhaseToSelect.pSoId_)
    {
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "so_id = \"";
        select += std::to_string(*planPhaseToSelect.pSoId_);
        select += "\"";
    }
    if (nullptr != planPhaseToSelect.pPlanNum_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "plan_number_on_so = \"";
        select += std::to_string(*planPhaseToSelect.pPlanNum_);
        select += "\"";
    }
    if (nullptr != planPhaseToSelect.pPhaseNum_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "phase_number_on_so = \"";
        select += std::to_string(*planPhaseToSelect.pPhaseNum_);
        select += "\"";
    }
    if (nullptr != planPhaseToSelect.pMainTime_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "main_time = \"";
        select += std::to_string(*planPhaseToSelect.pMainTime_);
        select += "\"";
    }
    if (nullptr != planPhaseToSelect.pMaxTime_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "max_time = \"";
        select += std::to_string(*planPhaseToSelect.pMaxTime_);
        select += "\"";
    }
    if (nullptr != planPhaseToSelect.pMinTime_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "min_time = \"";
        select += std::to_string(*planPhaseToSelect.pMinTime_);
        select += "\"";
    }
    return true;
}
bool pStructToSelect(CCZoneQuery& ccZoneToSelect, std::string& select)
{
    select = "SELECT * FROM `cc_zone` ";
    bool flagEmptyStruct = true;
    bool flagNeedAnd = false;
    //case for SELECT with condition
    select += "WHERE ";
    if (nullptr != ccZoneToSelect.pId_)
    {
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "id = \"";
        select += std::to_string(*ccZoneToSelect.pId_);
        select += "\"";
    }
    if (nullptr != ccZoneToSelect.pName_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "name = \"";
        select += *ccZoneToSelect.pName_;
        select += "\"";
    }
    if (nullptr != ccZoneToSelect.pDescription_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "description = \"";
        select += *ccZoneToSelect.pDescription_;
        select += "\"";
    }

    return true;
}
bool pStructToSelect(CCZoneSOQuery& ccZoneSOToSelect, std::string& select)
{
    select = "SELECT * FROM `cc_zone` ";
    bool flagEmptyStruct = true;
    bool flagNeedAnd = false;
    //case for SELECT with condition
    select += "WHERE ";
    if (nullptr != ccZoneSOToSelect.pSoId_)
    {
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "so_id = \"";
        select += std::to_string(*ccZoneSOToSelect.pSoId_);
        select += "\"";
    }
    if (nullptr != ccZoneSOToSelect.pCCZoneId_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "cc_zone_id = \"";
        select += std::to_string(*ccZoneSOToSelect.pCCZoneId_);
        select += "\"";
    }
    return true;
}
bool pStructToSelect(CCPlanQuery &ccPlanToSelect, std::string &select)
{
    select = "SELECT * FROM `cc_plan` ";
    bool flagEmptyStruct = true;
    bool flagNeedAnd = false;
    //case for SELECT with condition
    select += "WHERE ";
    if (nullptr != ccPlanToSelect.pId_)
    {
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "id = \"";
        select += std::to_string(*ccPlanToSelect.pId_);
        select += "\"";
    }
    if (nullptr != ccPlanToSelect.pName_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "name = \"";
        select += *ccPlanToSelect.pName_;
        select += "\"";
    }
    if (nullptr != ccPlanToSelect.pDescription_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "description = \"";
        select += *ccPlanToSelect.pDescription_;
        select += "\"";
    }
    if (nullptr != ccPlanToSelect.pCycleTime_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "cycle_time = \"";
        select += std::to_string(*ccPlanToSelect.pCycleTime_);
        select += "\"";
    }
    if (nullptr != ccPlanToSelect.pZoneCCId_)
    {
        if (flagNeedAnd)
            select += " AND ";
        flagEmptyStruct = false;
        flagNeedAnd = true;
        select += "cc_zone_id = \"";
        select += std::to_string(*ccPlanToSelect.pZoneCCId_);
        select += "\"";
    }
    return true;
}

bool DbMgr::makeSODictionaries()
{
    SOQuery emptySO;
    std::vector<SOData> ctrls;
    SelectQuery(ctrls, emptySO);
    for (auto iter = ctrls.begin(); iter != ctrls.end(); ++ iter)
    {
        TObjId id = iter->id_;
        std::string ip = iter->ip_;
        soIdToIp_.insert(std::make_pair(id, ip));
//        soIpToId_.insert(std::pair<std::string, TObjId>(iter->ip_, iter->id_));
        soIpToId_[iter->ip_] = iter->id_;
    }
    check = "this is a check";
    return true;
}

std::string DbMgr::SoIdToTp(TObjId id)
{
    auto iter = soIdToIp_.find(id);
    if ( iter != soIdToIp_.end())
        return iter->second;
    return std::string();
}

TObjId DbMgr::SoIpToTd(std::string soIp)
{
    auto iter = soIpToId_.find(soIp);
    if ( iter != soIpToId_.end())
        return iter->second;
    return 0;
}

DbMgr &GetDbMgr()
{
    static DbMgr mgr; //replace with call_once for thread safity @AU
    return mgr;
}

} //namespace db
