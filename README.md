# ctrl_dbmgr
Интерфейс для работы с конкретной базой данных (sqlite), которая хранила информацию о контроллерах и работе с ними. Проект, в котором использовался этот код, реализовывался на QT, работа с базой осуществляется через модуль QT SQL. Представленный здесь класс является посредником между базой и остальной частью программы.
Для каждой сущности базы созданы две структуры, одна для добавления в базу, другая для запроса. Класс DbMgr обладает методами создания базы, открытия существующей базы, добавление, удаление и редактирования данных.
Согласно логике проекта в базе в общем случае было два типа сущностей. Для работы с одними использовались шаблонные методы, для других были созданы персональные.
Класс DbMgr реализует шаблон Singltone.
