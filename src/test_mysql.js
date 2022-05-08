function test() {
	var conn;
	conn = DriverManager.getConnection("mysql://127.0.0.1/test_js_sql", "test_js_sql", "123456");

	query = "CREATE TABLE IF NOT EXISTS people(id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, name VARCHAR(30), age INT UNSIGNED) DEFAULT CHARSET utf8"
	println("Executing statement: ", query);
	stmt = conn.createStatement();
	stmt.execute(query);

	query = "INSERT INTO people(name, age) VALUES (?,?)";
	println("Executing prepared statement: ", query);
	pstmt = conn.prepareStatement(query, 1);
	pstmt.setString(1, "John Smith");
	pstmt.setNumber(2, 30);
	i = pstmt.executeUpdate();
	println(i, " rows affected");

	rs = pstmt.getGeneratedKeys();
	rs.next();
	println("GeneratedKeys: ", rs.getNumber(1));

	i = 1;
	println("Reading table:");
	pstmt = conn.prepareStatement("SELECT name, age FROM people");
	rs = pstmt.executeQuery();
	while (rs.next()) {
		println("Row #", i++, ":");
		dump(rs.getString(1), rs.getNumber(2));
	}

	println("Deleting table data");
	stmt.executeUpdate("DELETE FROM people");

	println("Test complete");

	return 0;
}

test();
