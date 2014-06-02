function connection_test() {
	var conn;

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	if (conn == null)
		return "FAIL";
	else
		return "PASS";
}

function createStatement_test() {
	var conn, stmt;

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
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

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
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

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
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

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	if (conn == null)
		return "FAIL";
	
	stmt = conn.createStatement();
	if (stmt == null)
		return "FAIL";
	
	ret = stmt.executeUpdate("update students set age = 21 where name = 'Mihai'");
	if (ret == -1)
		return "FAIL";
	else
		return "PASS";
}

function preparedStatement_test() {
	var conn, stmt;

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
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

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	if (conn == null)
		return "FAIL";

	stmt = conn.prepareStatement("select * from information_schema.tables where table_name = ? and table_type = ?");
	if (stmt == null)
		return "FAIL";

	/* sanity check */
	if (stmt.setNumber(1, "two") == true)
		return "FAIL";

	if (stmt.setString(1, 3) == true)
		return "FAIL";

	if (stmt.setNumber(10, 2) == true)
		return "FAIL";

	if (stmt.setNumber(2, 99) == false)
		return "FAIL";

	if (stmt.setString(1, "Test") == false)
		return "FAIL";

	return "PASS";
}

function preparedStatement_execute_test() {
	var conn, stmt;

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	if (conn == null)
		return "FAIL";

	stmt = conn.prepareStatement("select * from information_schema.tables where table_name = ? and table_type = ?");
	if (stmt == null)
		return "FAIL";

	if (stmt.setNumber(2, 99) == false)
		return "FAIL";

	if (stmt.setString(1, "Test") == false)
		return "FAIL";
	
	if (stmt.execute() == false)
		return "FAIL";
	else
	 	return "PASS";
}

function preparedStatement_executeQuery_test() {
	var conn, stmt;

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	if (conn == null)
		return "FAIL";

	stmt = conn.prepareStatement("select * from information_schema.tables where table_name = ? and table_type = ?");
	if (stmt == null)
		return "FAIL";

	if (stmt.setNumber(2, 99) == false)
		return "FAIL";

	if (stmt.setString(1, "Test") == false)
		return "FAIL";
	
	result = stmt.executeQuery();
	if (result == null)
		return "FAIL";
	else
	 	return "PASS";
}

function preparedStatement_executeUpdate_test() {
	var conn, stmt;

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	if (conn == null)
		return "FAIL";

	stmt = conn.prepareStatement("update students set age = ? where name = ?");
	if (stmt == null)
		return "FAIL";

	if (stmt.setNumber(1, 99) == false)
		return "FAIL";

	if (stmt.setString(2, "Mihai") == false)
		return "FAIL";
	
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

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	if (conn == null)
		return "FAIL";

	stmt = conn.createStatement();
	if (stmt == null)
		return "FAIL";
	
	/* sanity checks*/
	i = stmt.executeUpdate("insert into students (name, age) values ('Test1',13)", false);
	if (i == -1)
		return "FAIL";
	rs = stmt.getGeneratedKeys();
	if (rs != null)
		return "FAIL";

	i = stmt.executeUpdate("insert into students (name, age) values ('Test2',133)");
	if (i == -1)
		return "FAIL";
	rs = stmt.getGeneratedKeys();
	if (rs != null)
		return "FAIL";

	/* the correct call */
	i = stmt.executeUpdate("insert into students (name, age) values ('Test1',13)", 1);
	if (i == -1)
		return "FAIL";
	rs = stmt.getGeneratedKeys();
	if (rs == null)
		return "FAIL";

	while(rs.next())
		println("GeneratedKeys: ID ", rs.getNumber(1));

	return "PASS";
	return "PASS";
}

function getResultSet_test() {
	var conn, stmt;

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	if (conn == null)
		return "FAIL";

	stmt = conn.createStatement();
	if (stmt == null)
		return "FAIL";

	result = stmt.executeQuery("select * from students");	
	if (result == null)
		return "FAIL";

	if (compareTwoObjects(result, stmt.getResultSet()) == false)
		return "FAIL";

	return "PASS";
}

function getUpdateCount_test() {
	var conn, stmt;

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	if (conn == null)
		return "FAIL";

	stmt = conn.createStatement();
	if (stmt == null)
		return "FAIL";
	
	result = stmt.executeUpdate("update students set age = 24 where name = 'Mihai'");
	if (result == -1)
		return "FAIL";

	if (result != stmt.getUpdateCount())
		return "FAIL";

	return "PASS";
}

function getConnection_test() {
	var conn, stmt;

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	if (conn == null)
		return "FAIL";

	stmt = conn.createStatement();
	if (stmt == null)
		return "FAIL";
	
	result = stmt.executeQuery("select * from students");
	if (result == -1)
		return "FAIL";

	if (compareTwoObjects(conn, stmt.getConnection()) == false)
		return "FAIL";

	return "PASS";
}

function preparedStatement_getConnection_test() {
	var conn, stmt;

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	if (conn == null)
		return "FAIL";

	stmt = conn.prepareStatement("select * from information_schema.tables where table_name = ? and table_type = ?");
	if (stmt == null)
		return "FAIL";

	if (stmt.setNumber(2, 99) == false)
		return "FAIL";

	if (stmt.setString(1, "Test") == false)
		return "FAIL";

	if (compareTwoObjects(conn, stmt.getConnection()) == false)
		return "FAIL";

	return "PASS";
}

function preparedStatement_getGeneratedKeys_test() {
	var conn, stmt, rs;

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	if (conn == null)
		return "FAIL";

	stmt = conn.prepareStatement("insert into students (age, name) values (?,?)", true);
	if (stmt == null)
		return "FAIL";

	if (stmt.setNumber(1, 13) == false)
	 	return "FAIL";

	if (stmt.setString(2, "Test21") == false)
		return "FAIL";
	
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

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	if (conn == null)
		return "FAIL";

	stmt = conn.prepareStatement("select * from information_schema.tables where table_name = ? and table_type = ?");
	if (stmt == null)
		return "FAIL";

	if (stmt.setNumber(2, 99) == false)
		return "FAIL";

	if (stmt.setString(1, "Test") == false)
		return "FAIL";
	
	result = stmt.executeQuery();	
	if (result == null)
		return "FAIL";

	if (compareTwoObjects(result, stmt.getResultSet()) == false)
		return "FAIL";

	return "PASS";
}

function preparedStatement_getUpdateCount_test() {
	var conn, stmt;

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	if (conn == null)
		return "FAIL";

	stmt = conn.prepareStatement("update students set age = ? where name = ?");
	if (stmt == null)
		return "FAIL";

	if (stmt.setNumber(1, 12) == false)
		return "FAIL";

	if (stmt.setString(2, "Mihai") == false)
		return "FAIL";
	
	result = stmt.executeUpdate();
	if (result == -1)
		return "FAIL";

	if (result != stmt.getUpdateCount())
		return "FAIL";
	return "PASS";
}

function simpleStatementResult_test() {
	var conn, stmt;

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	if (conn == null)
		return "FAIL";
	
	stmt = conn.createStatement();
	if(stmt == null)
		return "FAIL";

	result = stmt.executeQuery("select * from students");
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
			result.getNumber(2) != null ||
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
	var studentName = "Cristina";

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	if (conn == null)
		return "FAIL";

	// check for a number
	stmt = conn.prepareStatement("select age from students where name = ?");
	if (stmt == null)
		return "FAIL";
	if (stmt.setString(1, studentName) == false)
		return "FAIL";
	
	result = stmt.executeQuery();
	if (result == null)
		return "FAIL";

	if (result.next() == false)
		return "FAIL";
	
	age = result.getNumber(1);	//use column index
	if (age == null)
		return "FAIL";

	//check for a string
	stmt = conn.prepareStatement("select name from students where age = ?");
	if (stmt == null)
		return "FAIL";

	if (stmt.setNumber(1, age) == false)
		return "FAIL";

	result = stmt.executeQuery();
	if (result == null)
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

function foo() {
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

	//TODO add more complex tests (example: create a table, insert an element, get the result and check if it is the one expected)
	return 0;
}

foo();
