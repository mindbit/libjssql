function foo() {
	var conn, pstmt, rs, i = 0;

	conn = DriverManager.getConnection("postgresql://127.0.0.1/licenta", "claudiu", "1234%asd");
	println("Connection succeeded");
	
	return 0;
}

dump("12ășț45");
foo();
