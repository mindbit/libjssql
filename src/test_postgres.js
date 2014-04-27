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
	stmt = conn.createStatement();

	if (stmt == null)
		return "FAIL";
	else
		return "PASS";
}

function execute_test() {
	var conn, stmt;

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	stmt = conn.createStatement();

	if (stmt.execute("BEGIN") == false)
		return "FAIL";
	else
		return "PASS";
}

function executeQuery_test() {
	var conn, stmt;

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	stmt = conn.createStatement();

	result = stmt.executeQuery("select * from information_schema.tables");

	if (result == null)
		return "FAIL";
	else
		return "PASS";
}

function executeUpdate_test() {
	var conn, stmt;

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	stmt = conn.createStatement();

	ret = stmt.executeUpdate("BEGIN");
	if (ret != 0)
		return "FAIL";
	else
		return "PASS";
}

function foo() {
	println("[Test 1] Testing connection .................. " + connection_test());
	println("[Test 2] Testing createStatement ............. " + createStatement_test());
	println("[Test 3] Testing execute ..................... " + execute_test());
	println("[Test 4] Testing executeQuery ................ " + executeQuery_test());
	println("[Test 5] Testing executeUpdate ............... " + executeUpdate_test());
	return 0;
}

foo();
