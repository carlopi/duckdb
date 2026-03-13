#include "wire_serialization.hpp"

#include <cstring>

namespace duckdb_shell {

// ===== Writer =====

void WireSerializer::Writer::WriteUint8(uint8_t val) {
	buffer.push_back(static_cast<char>(val));
}

void WireSerializer::Writer::WriteUint32(uint32_t val) {
	char buf[4];
	memcpy(buf, &val, 4);
	buffer.append(buf, 4);
}

void WireSerializer::Writer::WriteUint64(uint64_t val) {
	char buf[8];
	memcpy(buf, &val, 8);
	buffer.append(buf, 8);
}

void WireSerializer::Writer::WriteBool(bool val) {
	WriteUint8(val ? 1 : 0);
}

void WireSerializer::Writer::WriteString(const string &str) {
	WriteUint32(static_cast<uint32_t>(str.size()));
	buffer.append(str);
}

string WireSerializer::Writer::GetResult() const {
	return buffer;
}

// ===== Reader =====

WireSerializer::Reader::Reader(const string &blob) : data(blob) {
}

uint8_t WireSerializer::Reader::ReadUint8() {
	uint8_t val = static_cast<uint8_t>(data[pos]);
	pos += 1;
	return val;
}

uint32_t WireSerializer::Reader::ReadUint32() {
	uint32_t val;
	memcpy(&val, data.data() + pos, 4);
	pos += 4;
	return val;
}

uint64_t WireSerializer::Reader::ReadUint64() {
	uint64_t val;
	memcpy(&val, data.data() + pos, 8);
	pos += 8;
	return val;
}

bool WireSerializer::Reader::ReadBool() {
	return ReadUint8() != 0;
}

string WireSerializer::Reader::ReadString() {
	uint32_t len = ReadUint32();
	string result(data.data() + pos, len);
	pos += len;
	return result;
}

// ===== WireResultMetadata serialization =====

string WireSerializer::Serialize(const WireResultMetadata &meta) {
	Writer w;
	w.WriteBool(meta.has_error);
	w.WriteString(meta.error_message);
	w.WriteUint8(meta.statement_return_type);
	w.WriteUint8(meta.query_result_type);

	// Column count
	uint32_t ncols = static_cast<uint32_t>(meta.column_names.size());
	w.WriteUint32(ncols);

	// Column names
	for (auto &name : meta.column_names) {
		w.WriteString(name);
	}

	// Column types (LogicalTypeProperties)
	for (auto &type : meta.column_types) {
		w.WriteString(type.name);
		w.WriteBool(type.is_numeric);
		w.WriteBool(type.is_nested);
		w.WriteBool(type.is_json);
		w.WriteBool(type.is_boolean);
	}

	return w.GetResult();
}

WireResultMetadata WireSerializer::DeserializeResultMetadata(const string &blob) {
	Reader r(blob);
	WireResultMetadata meta;
	meta.has_error = r.ReadBool();
	meta.error_message = r.ReadString();
	meta.statement_return_type = r.ReadUint8();
	meta.query_result_type = r.ReadUint8();

	uint32_t ncols = r.ReadUint32();
	meta.column_names.reserve(ncols);
	for (uint32_t i = 0; i < ncols; i++) {
		meta.column_names.push_back(r.ReadString());
	}

	meta.column_types.reserve(ncols);
	for (uint32_t i = 0; i < ncols; i++) {
		LogicalTypeProperties props;
		props.name = r.ReadString();
		props.is_numeric = r.ReadBool();
		props.is_nested = r.ReadBool();
		props.is_json = r.ReadBool();
		props.is_boolean = r.ReadBool();
		meta.column_types.push_back(std::move(props));
	}

	return meta;
}

// ===== TableInfo serialization =====

string WireSerializer::SerializeTableInfo(const vector<pair<string, string>> &columns) {
	Writer w;
	w.WriteUint32(static_cast<uint32_t>(columns.size()));
	for (auto &col : columns) {
		w.WriteString(col.first);
		w.WriteString(col.second);
	}
	return w.GetResult();
}

vector<pair<string, string>> WireSerializer::DeserializeTableInfo(const string &blob) {
	if (blob.empty()) {
		return {};
	}
	Reader r(blob);
	uint32_t ncols = r.ReadUint32();
	vector<pair<string, string>> result;
	result.reserve(ncols);
	for (uint32_t i = 0; i < ncols; i++) {
		auto name = r.ReadString();
		auto type = r.ReadString();
		result.push_back({std::move(name), std::move(type)});
	}
	return result;
}

} // namespace duckdb_shell
