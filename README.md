jssql
=====

jssql is a database connectivity library for embeddable JavaScript. The
database API that is exposed inside the JavaScript environment mimics
the JDBC API. Currently, MySQL/MariaDB and PostgreSQL database drivers
are implemented and supported.

The library is written in C and built on top of the
[Duktape](http://duktape.org/) embeddable JavaScript engine.

# JavaScript API Example

This is not meant to be an exhaustive description of the jssql
JavaScript API. Instead, it's a simple example that shows what the API
looks like.

Note that some non-standard JavaScript functions such as `println` are
implemented in the [jsmisc](https://github.com/mindbit/libjsmisc)
library, which is used by jssql as a support library.

```javascript
conn = DriverManager.getConnection("mysql://localhost/example", "example", "123456");

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
```
