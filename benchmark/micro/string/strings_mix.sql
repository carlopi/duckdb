CREATE TABLE TT (a VARCHAR, b VARCHAR);
CREATE TEMPORARY TABLE strings_temp AS SELECT ((i * 95853) % 1789)::VARCHAR AS s1, ((i * 847887) % 3017)::VARCHAR AS s2 FROM range(0, 30000000) tbl(i);
CREATE TABLE strings AS SELECT repeat(s1, 5) AS s1, repeat(s2, 5) AS s2 FROM strings_temp;
