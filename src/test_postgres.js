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



function foo() {
	println("[Test 1] Testing connection ......................................... " + connection_test());
	println("[Test 2] Testing createStatement .................................... " + createStatement_test());
	println("[Test 3] Testing execute for simple statement ....................... " + execute_test());
	println("[Test 4] Testing executeQuery for simple statement .................. " + executeQuery_test());
	println("[Test 5] Testing executeUpdate for simple statement ................. " + executeUpdate_test());

	return 0;
}

foo();
