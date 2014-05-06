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
	
	ret = stmt.executeUpdate("BEGIN");
	if (ret == -1)
		return "FAIL";
	else
		return "PASS";
}

function prepareStatement_test() {
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

function prepareStatement_set_test() {
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

function prepareStatement_execute_test() {
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

function prepareStatement_executeQuery_test() {
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
	
	result = stmt.executeQuery()
	if (result == null)
		return "FAIL";
	else
	 	return "PASS";
}

function prepareStatement_executeUpdate_test() {
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
	
	if (stmt.executeUpdate() == -1)
		return "FAIL";
	else
	 	return "PASS";
}


function foo() {
	println("[Test  1] Testing connection ......................................... " + connection_test());
	println("[Test  2] Testing createStatement .................................... " + createStatement_test());
	println("[Test  3] Testing execute for simple statement ....................... " + execute_test());
	println("[Test  4] Testing executeQuery for simple statement .................. " + executeQuery_test());
	println("[Test  5] Testing executeUpdate for simple statement ................. " + executeUpdate_test());
	println("[Test  6] Testing prepareStatement ................................... " + prepareStatement_test());
	println("[Test  7] Testing set functions for prepared statement ............... " + prepareStatement_set_test());
	println("[Test  8] Testing execute for prepared statement ..................... " + prepareStatement_execute_test());
	println("[Test  9] Testing executeQuery for prepared statement ................ " + prepareStatement_executeQuery_test());
	println("[Test 10] Testing executeUpdate for prepared statement ............... " + prepareStatement_executeUpdate_test());

	return 0;
}

foo();
