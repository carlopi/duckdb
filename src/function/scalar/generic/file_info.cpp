#include "duckdb/function/scalar/generic_functions.hpp"
#include "duckdb/common/multi_file/multi_file_reader.hpp"
#include "duckdb/common/extended_file_info_file_system.hpp"
#include "duckdb/common/vector/vector_writer.hpp"

namespace duckdb {

namespace {

//! file_info_pack accepts exactly what a multi-file reader accepts as a file list entry
unique_ptr<FunctionData> FileInfoPackBind(BindScalarFunctionInput &input) {
	auto &arguments = input.GetArguments();
	auto &type = arguments[0]->GetReturnType();
	switch (type.id()) {
	case LogicalTypeId::VARCHAR:
	case LogicalTypeId::STRUCT:
	case LogicalTypeId::VARIANT:
	case LogicalTypeId::UNKNOWN:
	case LogicalTypeId::SQLNULL:
		return nullptr;
	default:
		throw BinderException("file_info_pack takes a VARCHAR path, or a STRUCT or VARIANT holding the path in the "
		                      "\"%s\" field together with the options to open the file with - got %s",
		                      MultiFileReader::FILE_PATH_FIELD, type.ToString());
	}
}

void FileInfoPackFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &input = args.data[0];
	auto count = args.size();
	auto reader = MultiFileReader::CreateDefault("file_info_pack");
	auto writer = FlatVector::Writer<string_t>(result, count);
	for (idx_t i = 0; i < count; i++) {
		auto value = input.GetValue(i);
		if (value.IsNull()) {
			writer.WriteNull();
			continue;
		}
		auto file = reader->ParseFileEntry(value);
		writer.WriteValue(ExtendedFileInfoFileSystem::EncodePath(file));
	}
	if (input.GetVectorType() == VectorType::CONSTANT_VECTOR) {
		result.SetVectorType(VectorType::CONSTANT_VECTOR);
	}
}

void FileInfoUnpackFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &input = args.data[0];
	auto count = args.size();
	result.SetVectorType(VectorType::FLAT_VECTOR);
	for (idx_t i = 0; i < count; i++) {
		auto value = input.GetValue(i);
		if (value.IsNull()) {
			result.SetValue(i, Value(LogicalType::VARIANT()));
			continue;
		}
		auto file = ExtendedFileInfoFileSystem::Decode(StringValue::Get(value));
		result.SetValue(i, ExtendedFileInfoFileSystem::ToValue(file).DefaultCastAs(LogicalType::VARIANT()));
	}
	if (input.GetVectorType() == VectorType::CONSTANT_VECTOR) {
		result.SetVectorType(VectorType::CONSTANT_VECTOR);
	}
}

} // namespace

ScalarFunction FileInfoPackFun::GetFunction() {
	ScalarFunction function("file_info_pack", {LogicalType::ANY}, LogicalType::VARCHAR, FileInfoPackFunction,
	                        FileInfoPackBind);
	function.SetFallible();
	return function;
}

ScalarFunction FileInfoUnpackFun::GetFunction() {
	ScalarFunction function("file_info_unpack", {LogicalType::VARCHAR}, LogicalType::VARIANT(), FileInfoUnpackFunction);
	function.SetFallible();
	return function;
}

} // namespace duckdb
