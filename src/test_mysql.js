function foo() {
	var conn, pstmt, rs, i = 0;

	conn = DriverManager.getConnection("mysql://127.0.0.1/licenta", "claudiu", "1234%asd");
	pstmt = conn.prepareStatement("select * from students");

	rs = pstmt.executeQuery();
	while (rs.next()) {
		println("Row #", i++, ":");
		dump(rs.getNumber(1), rs.getString(2));
	}

	println();
	pstmt = conn.prepareStatement("insert into students (id, name) values (?,?)");
	pstmt.setNumber(1, 99);
	pstmt.setString(2, "Dragos");
	i = pstmt.executeUpdate();
	println(i, " rows affected");

	rs = pstmt.getGeneratedKeys();
	rs.next();
	println("GeneratedKeys: ", rs.getNumber(1));

	return 0;
}

dump("12ășț45");
foo();
