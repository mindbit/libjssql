// // create table people(id serial primary key, name varchar(40), age int);

function getPgsqlConnection() {
	return DriverManager.getConnection("postgresql://127.0.0.1/test_js_sql", "test_js_sql", "123456");
}

// function comparison_test() {
// 	var t1, t2;
// 	var n = 100000;
// 	var simple_statements = [
// 		"select * from people",
// 		"select * from people where age > -1",
// 		"select age from people where name = 'Test' AND age < 30"
// 	];

// 	var prepared_parameters = ["10", "20"];
// 	var prepared_statements = [
// 		"select * from people where age < ?",
// 		"select * from people where age > ? and age < ?",
// 		"select age from people where name = 'Test' AND age < ?"
// 	];
// 	t1 = gettimeofday();
// 	var conn = getPgsqlConnection();
// 	if (conn == null)
// 		return "FAIL";
// 	var stmt = conn.createStatement(); //create simple stmt

// 	var pstmt = [];
// 	for (i = 0; i < 3; i++) {	//create prepared stmt
// 		pstmt[i] = conn.prepareStatement(prepared_statements[i]);
// 	}

// 	var crt1 = 0, crt2 = 0;
// 	//set parameters' value
// 	pstmt[0].setNumber(1, 10);
// 	pstmt[1].setNumber(1, 10);
// 	pstmt[2].setNumber(1, 10);
// 	pstmt[1].setNumber(2, 20);

// 	for (i = 0; i < n; i++) {
// 		if ((i % 2) == 0) {
// 			stmt.execute(simple_statements[(crt1++) % 2]);
// 		} else {
// 			pstmt[(crt2++) % 2].execute();
// 		}
// 	}

// 	t2 = gettimeofday();

// 	print("Time: ");
// 	println(t2 - t1);

// 	return "PASS";
// }

// function integration_test() {
// 	var connectionPgsql, connectionMysql;
// 	var statementPgsql, statementMysql;

// 	connectionPgsql = getPgsqlConnection();
// 	if (connectionPgsql == null) {
// 		println("Failed to connect to PostgreSQL database");
// 		return "FAIL";
// 	}

// 	connectionMysql = DriverManager.getConnection("mysql://127.0.0.1/test_js_sql", "test_js_sql", "123456");
// 	if (connectionMysql == null) {
// 		println("Failed to connect to MySQL database");
// 		return "FAIL";
// 	}

// 	statementMysql = connectionMysql.createStatement();
// 	statementPgsql = connectionPgsql.createStatement();
// 	if (statementMysql == null || statementPgsql == null) {
// 		println("Failed to create statement");
// 		return "FAIL";
// 	}

// 	statementPgsql.executeUpdate("DROP TABLE IF EXISTS test");
// 	if (statementPgsql.execute("CREATE TABLE test (id INT PRIMARY KEY, name VARCHAR(40) not null, age INT);") == false) {
// 		println("Failed to create the table");
// 		return "FAIL";
// 	}

// 	var preparedStatementPgsql = connectionPgsql.prepareStatement("INSERT INTO test (id, name, age) values (?, ?, ?) ");
// 	if (preparedStatementPgsql == null) {
// 		println("Failed to prepare the statement");
// 		return "FAIL";
// 	}

// 	//collect info from mysql
// 	var resultMysql = statementMysql.executeQuery("SELECT * from people");
// 	if (resultMysql == null) {
// 		println("Failed to collect restuls from MYSQL");
// 		return "FAIL";
// 	}

// 	//populate PostgreSQL database
// 	while(resultMysql.next()) {
// 		preparedStatementPgsql.setNumber(1, resultMysql.getNumber(1));
// 		preparedStatementPgsql.setString(2, resultMysql.getString(2));
// 		preparedStatementPgsql.setNumber(3, resultMysql.getNumber(3));
// 		preparedStatementPgsql.executeUpdate();
// 	}

// 	//collect info from postgresql
// 	var resultPgsql = statementPgsql.executeQuery("SELECT * from test");

// 	//check if they are identical
// 	if (!compareTwoObjects(resultMysql, resultPgsql)) {
// 		println("Different objects");
// 		return "FAIL";
// 	}

// 	return "PASS"
// }

function connection_test() {
	var conn;

	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";
	else
		return "PASS";
}

function createStatement_test() {
	var conn, stmt;

	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";

	stmt = conn.createStatement();
	if (stmt == null)
		return "FAIL";
	else
		return "PASS";
}

function execute_test() {
	var conn, stmt;

	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";

	stmt = conn.createStatement();

	if (stmt.execute("BEGIN") == false)
		return "FAIL";
	else
		return "PASS";
}

function executeQuery_test() {
	var conn, stmt;

	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";

	stmt = conn.createStatement();
	if(stmt == null)
		return "FAIL";

	result = stmt.executeQuery("select * from information_schema.tables");
	if (result == null)
		return "FAIL";
	else
		return "PASS";
}

function executeUpdate_test() {
	var conn, stmt;

	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";

	stmt = conn.createStatement();
	if (stmt == null)
		return "FAIL";

	ret = stmt.executeUpdate("update people set age = 21 where name = 'Mihai'");
	if (ret == -1)
		return "FAIL";
	else
		return "PASS";
}

function preparedStatement_test() {
	var conn, stmt;

	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";

	stmt = conn.prepareStatement("select * from information_schema.tables where table_name = ? and table_type = ?");
	if (stmt == null)
		return "FAIL";
	else
		return "PASS";
}

function preparedStatement_set_test() {
	var conn, stmt;

	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";

	stmt = conn.prepareStatement("select * from information_schema.tables where table_name = ? and table_type = ?");
	if (stmt == null)
		return "FAIL";

	stmt.setString(1, "people");
	stmt.setNumber(2, 99);

	return "PASS";
}

function preparedStatement_execute_test() {
	var conn, stmt;

	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";

	stmt = conn.prepareStatement("select * from information_schema.tables where table_name = ? and table_type = ?");
	if (stmt == null)
		return "FAIL";

	stmt.setNumber(2, 99);
	stmt.setString(1, "Test");

	if (stmt.execute() == false)
		return "FAIL";
	else
		return "PASS";
}

function preparedStatement_executeQuery_test() {
	var conn, stmt;

	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";

	stmt = conn.prepareStatement("select * from information_schema.tables where table_name = ? and table_type = ?");
	if (stmt == null)
		return "FAIL";

	stmt.setNumber(2, 99);
	stmt.setString(1, "Test");

	result = stmt.executeQuery();
	if (result == null)
		return "FAIL";
	else
		return "PASS";
}

function preparedStatement_executeUpdate_test() {
	var conn, stmt;

	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";

	stmt = conn.prepareStatement("update people set age = ? where name = ?");
	if (stmt == null)
		return "FAIL";

	stmt.setNumber(1, 99);
	stmt.setString(2, "Mihai");

	if (stmt.executeUpdate() == -1)
		return "FAIL";
	else
		return "PASS";
}

function compareTwoObjects(x, y) {
	/**
	 * This function was copied from
	 * http://stackoverflow.com/questions/1068834/object-comparison-in-javascript
	 */
	if ( x === y ) return true;
	// if both x and y are null or undefined and exactly the same

	if ( ! ( x instanceof Object ) || ! ( y instanceof Object ) ) return false;
	// if they are not strictly equal, they both need to be Objects

	if ( x.constructor !== y.constructor ) return false;
	// they must have the exact same prototype chain, the closest we can do is
	// test there constructor.

	for ( var p in x ) {
		if ( ! x.hasOwnProperty( p ) ) continue;
		// other properties were tested using x.constructor === y.constructor

		if ( ! y.hasOwnProperty( p ) ) return false;
		// allows to compare x[ p ] and y[ p ] when set to undefined

		if ( x[ p ] === y[ p ] ) continue;
		// if they have the same strict value or identity then they are equal

		if ( typeof( x[ p ] ) !== "object" ) return false;
		// Numbers, Strings, Functions, Booleans must be strictly equal

		if ( ! Object.equals( x[ p ],  y[ p ] ) ) return false;
		// Objects and Arrays must be tested recursively
	}

	for ( p in y ) {
		if ( y.hasOwnProperty( p ) && ! x.hasOwnProperty( p ) ) return false;
		// allows x[ p ] to be set to undefined
	}

	return true;
}

function getGeneratedKeys_test() {
	var conn, stmt, rs;
	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";

	stmt = conn.createStatement();
	if (stmt == null)
		return "FAIL";

	/* sanity checks*/
	i = stmt.executeUpdate("insert into people (name, age) values ('Test1',13)", Statement.NO_GENERATED_KEYS);
	if (i == -1)
		return "FAIL";
	rs = stmt.getGeneratedKeys();
	if (rs != null)
		return "FAIL";

	i = stmt.executeUpdate("insert into people (name, age) values ('Test2',133)");
	if (i == -1)
		return "FAIL";
	rs = stmt.getGeneratedKeys();
	if (rs != null)
		return "FAIL";

	/* the correct call */
	i = stmt.executeUpdate("insert into people (name, age) values ('Test1',13)", Statement.RETURN_GENERATED_KEYS);
	if (i == -1)
		return "FAIL";
	rs = stmt.getGeneratedKeys();
	if (rs == null)
		return "FAIL";

	while(rs.next())
		println("GeneratedKeys: ID ", rs.getNumber(1));

	return "PASS";
}

function getResultSet_test() {
	var conn, stmt;

	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";

	stmt = conn.createStatement();
	if (stmt == null)
		return "FAIL";

	result = stmt.executeQuery("select * from people");
	if (result == null)
		return "FAIL";

	if (compareTwoObjects(result, stmt.getResultSet()) == false)
		return "FAIL";

	return "PASS";
}

function getUpdateCount_test() {
	var conn, stmt;

	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";

	stmt = conn.createStatement();
	if (stmt == null)
		return "FAIL";

	result = stmt.executeUpdate("update people set age = 24 where name = 'Mihai'");
	if (result == -1)
		return "FAIL";

	if (result != stmt.getUpdateCount())
		return "FAIL";

	return "PASS";
}

function getConnection_test() {
	var conn, stmt;

	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";

	stmt = conn.createStatement();
	if (stmt == null)
		return "FAIL";

	result = stmt.executeQuery("select * from people");
	if (result == -1)
		return "FAIL";

	if (compareTwoObjects(conn, stmt.getConnection()) == false)
		return "FAIL";

	return "PASS";
}

function preparedStatement_getConnection_test() {
	var conn, stmt;

	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";

	stmt = conn.prepareStatement("select * from information_schema.tables where table_name = ? and table_type = ?");
	if (stmt == null)
		return "FAIL";

	stmt.setNumber(2, 99);
	stmt.setString(1, "Test");

	if (compareTwoObjects(conn, stmt.getConnection()) == false)
		return "FAIL";

	return "PASS";
}

function preparedStatement_getGeneratedKeys_test() {
	var conn, stmt, rs;

	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";

	stmt = conn.prepareStatement("insert into people (age, name) values (?,?)", Statement.RETURN_GENERATED_KEYS);
	if (stmt == null)
		return "FAIL";

	stmt.setNumber(1, 13);
	stmt.setString(2, "Test21");

	i = stmt.executeUpdate();
	if (i == -1)
		return "FAIL";

	rs = stmt.getGeneratedKeys();
	if (rs == null)
		return "FAIL";

	while(rs.next())
		println("GeneratedKeys: ", rs.getNumber(1));

	return "PASS";
}

function preparedStatement_getResultSet_test() {
	var conn, stmt;

	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";

	stmt = conn.prepareStatement("select * from information_schema.tables where table_name = ? and table_type = ?");
	if (stmt == null)
		return "FAIL";

	stmt.setNumber(2, 99);

	stmt.setString(1, "Test");

	result = stmt.executeQuery();
	if (result == null)
		return "FAIL";

	if (compareTwoObjects(result, stmt.getResultSet()) == false)
		return "FAIL";

	return "PASS";
}

function preparedStatement_getUpdateCount_test() {
	var conn, stmt;

	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";

	stmt = conn.prepareStatement("update people set age = ? where name = ?");
	if (stmt == null)
		return "FAIL";

	stmt.setNumber(1, 12);
	stmt.setString(2, "Mihai");

	result = stmt.executeUpdate();
	if (result == -1)
		return "FAIL";

	if (result != stmt.getUpdateCount())
		return "FAIL";
	return "PASS";
}

function simpleStatementResult_test() {
	var conn, stmt;

	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";

	stmt = conn.createStatement();
	if(stmt == null)
		return "FAIL";

	result = stmt.executeQuery("select * from people");
	if (result == null)
		return "FAIL";

	//check last() method
	if (!result.last() || result.next())
		return "FAIL";

	//check first() method
	if (!result.first())
		return "FAIL";

	//sanity checks (expected a number on position 1, a string on position 2 and another number on position 3)
	while(result.next()) {
		if (result.getString(1) == null ||
			result.getNumber(2) != 0 ||
			result.getString(3) == null)
			return "FAIL";
	}

	result.first();

	//check with column index
	while (result.next()) {
		print("Id: ");
		print(result.getNumber(1));
		print(" | Name: ")
		print(result.getString(2));
		print(" | Age: ");
		println(result.getNumber(3));
	}

	//check with column label
	result.first();
	while (result.next()) {
		print("Id: ");
		print(result.getNumber("id"));
		print(" | Name: ")
		print(result.getString("name"));
		print(" | Age: ");
		println(result.getNumber("age"));
	}

	return "PASS";
}

function preparedStatementResult_test() {
	var conn, stmt, age, name;
	var studentName = "Test1";

	conn = getPgsqlConnection();
	if (conn == null)
		return "FAIL";

	// check for a number
	stmt = conn.prepareStatement("select age from people where name = ?");
	if (stmt == null)
		return "FAIL";
	stmt.setString(1, studentName);

	result = stmt.executeQuery();
	if (result == null)
		return "FAIL";

	if (result.next() == false)
		return "FAIL";

	age = result.getNumber(1);	//use column index
	if (age == null)
		return "FAIL";

	//check for a string
	stmt = conn.prepareStatement("select name from people where age = ?");
	if (stmt == null)
		return "FAIL";

	stmt.setNumber(1, age);

	result = stmt.executeQuery();
	if (result == null)
		return "FAIL";

	if (result.next() == false)
		return "FAIL";

	name = result.getString(1);
	if (age == null)
		return "FAIL";

	while(result.next()) {
		name = result.getString("name"); //use column label
		if (name == null)
			return "FAIL";
		if (name.localeCompare(studentName) == 0)
			return "PASS";
	}

	return "FAIL";
}

function test() {
	//integration_test();
	println("[Test  1] Testing connection ......................................... " + connection_test());
	println("[Test  2] Testing createStatement .................................... " + createStatement_test());
	println("[Test  3] Testing execute for simple statement ....................... " + execute_test());
	println("[Test  4] Testing executeQuery for simple statement .................. " + executeQuery_test());
	println("[Test  5] Testing executeUpdate for simple statement ................. " + executeUpdate_test());
	println("[Test  6] Testing prepareStatement ................................... " + preparedStatement_test());
	println("[Test  7] Testing set functions for prepared statement ............... " + preparedStatement_set_test());
	println("[Test  8] Testing execute for prepared statement ..................... " + preparedStatement_execute_test());
	println("[Test  9] Testing executeQuery for prepared statement ................ " + preparedStatement_executeQuery_test());
	println("[Test 10] Testing executeUpdate for prepared statement ............... " + preparedStatement_executeUpdate_test());
	println("[Test 11] Testing getGeneratedKeys for simple statement .............. " + getGeneratedKeys_test());
	println("[Test 12] Testing getResultSet for simple statement .................. " + getResultSet_test());
	println("[Test 13] Testing getUpdateCount for simple statement ................ " + getUpdateCount_test());
	println("[Test 14] Testing getConnection for simple statement ................. " + getConnection_test());
	println("[Test 15] Testing getGeneratedKeys for prepared statement ............ " + preparedStatement_getGeneratedKeys_test());
	println("[Test 16] Testing getResultSet for prepared statement ................ " + preparedStatement_getResultSet_test());
	println("[Test 17] Testing getUpdateCount for prepared statement .............. " + preparedStatement_getUpdateCount_test());
	println("[Test 18] Testing getConnection for prepared statement ............... " + preparedStatement_getConnection_test());
	println("[Test 19] Testing ResultSet for a simple statement  .................. " + simpleStatementResult_test());
	println("[Test 20] Testing ResultSet for a prepared statement  ................ " + preparedStatementResult_test());
// 	//TODO add more complex tests (example: create a table, insert an element, get the result and check if it is the one expected)
	return 0;
}


test();
