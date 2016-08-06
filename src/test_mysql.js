// create table people(id int unsigned not null auto_increment primary key, name varchar(40), age int);

function test() {
	var conn, query, stmt, pstmt, rs, i = 0;

	conn = DriverManager.getConnection("mysql://127.0.0.1/test_js_sql", "test_js_sql", "123456");

	query = "INSERT INTO people(name, age) VALUES (?,?)";
	println("Executing prepared statement: ", query);
	pstmt = conn.prepareStatement(query);
	pstmt.setString(1, "John Smith");
	pstmt.setNumber(2, 30);
	i = pstmt.executeUpdate();
	println(i, " rows affected");
	rs = pstmt.getGeneratedKeys();
	rs.next();
	println("GeneratedKeys: ", rs.getNumber(1));

	println("Reading table:");
	pstmt = conn.prepareStatement("SELECT * FROM people");
	rs = pstmt.executeQuery();
	while (rs.next()) {
		println("Row #", i++, ":");
		dump(rs.getNumber(1), rs.getString(2));
	}

	println("Deleting table data");
	stmt = conn.createStatement();
	stmt.executeUpdate("DELETE FROM people");

	println("Test complete");
	return 0;
}

test();
