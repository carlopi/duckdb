//===----------------------------------------------------------------------===//
//                         DuckDB
//
// wire_serialization.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "wire_transport.hpp"

namespace duckdb_shell {

//! Simple binary serialization for wire protocol types.
//! Format: length-prefixed strings, fixed-width integers, packed bools.
class WireSerializer {
public:
	//! Serialize WireResultMetadata to binary
	static string Serialize(const WireResultMetadata &meta);
	//! Deserialize WireResultMetadata from binary
	static WireResultMetadata DeserializeResultMetadata(const string &blob);

	//! Serialize TableInfo result (column name/type pairs) to binary
	static string SerializeTableInfo(const vector<pair<string, string>> &columns);
	//! Deserialize TableInfo result from binary
	static vector<pair<string, string>> DeserializeTableInfo(const string &blob);

private:
	//! Helpers — write to / read from a byte buffer
	class Writer {
	public:
		void WriteUint8(uint8_t val);
		void WriteUint32(uint32_t val);
		void WriteUint64(uint64_t val);
		void WriteBool(bool val);
		void WriteString(const string &str);
		string GetResult() const;

	private:
		string buffer;
	};

	class Reader {
	public:
		explicit Reader(const string &blob);
		uint8_t ReadUint8();
		uint32_t ReadUint32();
		uint64_t ReadUint64();
		bool ReadBool();
		string ReadString();

	private:
		const string &data;
		size_t pos = 0;
	};
};

} // namespace duckdb_shell
