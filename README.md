jssql
=====

jssql is a database connectivity library for embeddable JavaScript. The
database API that is exposed inside the JavaScript environment mimics
the JDBC API. Currently, MySQL/MariaDB and PostgreSQL database drivers
are implemented and supported.

The library is built on top of the
[SpiderMonkey](https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey)
engine. Unfortunately, it relies on the SpiderMonkey C API, which was
dropped in version 24. There is no plan to port jssql to C++ and
likewise there is no plan for SpiderMonkey to bring back the C API.
There is an ongoing effort to port jssql to use the
[Duktape](http://duktape.org/) engine, which is written in pure C and is
a better choice for embedding into a different program. SpiderMonkey is
supported up to and including release 1.1 of jssql.

# JavaScript API Example

This is not meant to be an exhaustive description of the jssql
JavaScript API. Instead, it's a simple example that shows what the API
looks like.

Note that some non-standard JavaScript functions such as `println` are
implemented in the [jsmisc](https://github.com/mindbit/libjsmisc)
library, which is used by jssql as a support library.

```javascript
conn = DriverManager.getConnection("mysql://centos6/test_js_sql", "test_js_sql", "123456");

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
