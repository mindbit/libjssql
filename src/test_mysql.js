function test() {
	var conn;
	conn = DriverManager.getConnection("mysql://127.0.0.1/test_js_sql", "test_js_sql", "123456");

	query = "CREATE TABLE IF NOT EXISTS people(id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, name VARCHAR(30), age INT UNSIGNED) DEFAULT CHARSET utf8"
	println("Executing statement: ", query);
	stmt = conn.createStatement();
	stmt.execute(query);

	query = "INSERT INTO people(name, age) VALUES ('x', 20), ('y', 21), ('z', 22)";
	println("Executing prepared statement: ", query);
	pstmt = conn.createStatement();
	i = pstmt.executeUpdate(query, 1);
	println(i, " rows affected");

	rs = pstmt.getGeneratedKeys();
	while(rs.next())
		println("GeneratedKeys: ", rs.getNumber(1));

	i = 1;
	println("Reading table:");
	pstmt = conn.prepareStatement("SELECT id, name, age FROM people");
	rs = pstmt.executeQuery();
	while (rs.next()) {
		println("Row #", i++, ":");
		dump(rs.getNumber(1), rs.getString(2), rs.getNumber(3));
	}

	println("Deleting table data");
	stmt.executeUpdate("DELETE FROM people");

	println("Test complete");

	return 0;
}

test();
