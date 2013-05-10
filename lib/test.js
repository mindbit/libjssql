function foo() {
	var conn, pstmt, rs, i = 0;

	conn = DriverManager.getConnection("mysql://centos6/test_js_sql", "foo", null);
	pstmt = conn.prepareStatement("select id, a_int, a_str from foo");

	rs = pstmt.executeQuery();
	while (rs.next()) {
		println("Row #", i++, ":");
		dump(rs.getNumber(1), rs.getNumber(2), rs.getString(3));
	}

	println();
	pstmt = conn.prepareStatement("insert into foo(a_str) values (?)");
	pstmt.setString(1, "mimi");
	i = pstmt.executeUpdate();
	println(i, " rows affected");
}

dump("12ășț45");
foo();
