import polars as pl
import duckdb

pldf = pl.DataFrame({'mynum': [1,2,3,4]})

duckdb.sql("SELECT * FROM pldf").show()

con = duckdb.connect("file.db")
con.sql("CREATE TABLE integers AS (FROM pldf)")
con.sql("SELECT * FROM integers").show()
