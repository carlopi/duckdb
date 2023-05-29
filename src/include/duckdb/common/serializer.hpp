//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/serializer.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/catalog/catalog.hpp"
#include "duckdb/common/common.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/vector.hpp"
#include <type_traits>
#include <unordered_set>
#include <set>

namespace duckdb {

//! The Serialize class is a base class that can be used to serializing objects into a binary buffer
class Serializer {
private:
	uint64_t version = 0L;

public:
	bool is_query_plan = false;

	virtual ~Serializer() {
	}

	//! Sets the version of the serialization that writers are expected to use
	//! The version is mostly the most recent one, unless modifying old data or streaming to
	//! an older version
	void SetVersion(uint64_t v) {
		D_ASSERT(this->version == 0); // version can only be set once
		this->version = v;
	}

	//! Returns the version of serialization that writers are expected to use
	uint64_t GetVersion() {
		return version;
	}

	virtual void WriteData(const_data_ptr_t buffer, idx_t write_size) = 0;

	template <class T>
	void Write(T element) {
		static_assert(std::is_trivially_destructible<T>(), "Write element must be trivially destructible");

		WriteData(const_data_ptr_cast(&element), sizeof(T));
	}

	//! Write data from a string buffer directly (without length prefix)
	void WriteBufferData(const string &str) {
		WriteData(const_data_ptr_cast(str.c_str()), str.size());
	}
	//! Write a string with a length prefix
	void WriteString(const string &val) {
		WriteStringLen(const_data_ptr_cast(val.c_str()), val.size());
	}
	void WriteStringLen(const_data_ptr_t val, idx_t len) {
		Write<uint32_t>((uint32_t)len);
		if (len > 0) {
			WriteData(val, len);
		}
	}

	template <class T>
	void WriteList(const vector<unique_ptr<T>> &list) {
		Write<uint32_t>((uint32_t)list.size());
		for (auto &child : list) {
			child->Serialize(*this);
		}
	}

	void WriteStringVector(const vector<string> &list) {
		Write<uint32_t>((uint32_t)list.size());
		for (auto &child : list) {
			WriteString(child);
		}
	}

	template <class T>
	void WriteOptional(const unique_ptr<T> &element) {
		Write<bool>(element ? true : false);
		if (element) {
			element->Serialize(*this);
		}
	}

	template <typename T, typename std::enable_if<std::is_integral<T>::value || std::is_enum<T>::value || std::is_floating_point<T>::value, bool>::type = true>
	void Serialize(const T& x) {
		WriteData(const_data_ptr_cast(&x), sizeof(T));
	}
	template <class T, typename std::enable_if<!std::is_integral<T>::value && !std::is_enum<T>::value && !std::is_floating_point<T>::value, bool>::type = true>
	void Serialize(const T& t) {
		t.Serialize(*this);
	}
	template <>
	void Serialize(const bool& t) {
		Write<bool>(t);
	}
	template <>
	void Serialize(const string& t) {
		WriteString(t);
	}
	template <class T>
	void Serialize(const std::vector<T>& v) {
		Serialize(v.size());
		for (auto& x : v)
			Serialize(x);
	}
	template <class T>
	void Serialize(const std::unordered_set<T>& v) {
		// FIXME: Should be somehow ordered???
		Serialize(v.size());
		for (auto& x : v)
			Serialize(x);
	}
	template <class T>
	void Serialize(const std::set<T>& v) {
		Serialize(v.size());
		for (auto& x : v)
			Serialize(x);
	}
	template <class T>
	void Serialize(const vector<T, true>& v) {
		Serialize(v.size());
		for (auto& x : v)
			Serialize(x);
	}
	template <class T>
	void Serialize(const vector<T, false>& v) {
		Serialize(v.size());
		for (auto& x : v)
			Serialize(x);
	}
	template <>
	void Serialize(const std::vector<bool>& v) {
		Serialize(v.size());
		for (int i=0; i<v.size(); i++)
			Serialize((bool)v[i]);
	}
	void Serialize(const vector<bool, false>& v) {
		Serialize(v.size());
		for (int i=0; i<v.size(); i++)
			Serialize((bool)v[i]);
	}
	void Serialize(const vector<bool, true>& v) {
		Serialize(v.size());
		for (int i=0; i<v.size(); i++)
			Serialize((bool)v[i]);
	}
	template <class T>
	void Serialize(const std::unique_ptr<T>& v) {
		Serialize<bool>(v ? true : false);
		if (v) {
			Serialize<T>(*v);
		}
	}
	template <class T>
	void Serialize(const unique_ptr<T>& v) {
		Serialize<bool>(v ? true : false);
		if (v) {
			Serialize<T>(*v);
		}
	}
};

//! The Deserializer class assists in deserializing a binary blob back into an
//! object
class Deserializer {
private:
	uint64_t version = 0L;

public:
	virtual ~Deserializer() {
	}

	//! Sets the version of the serialization that readers are expected to use
	//! The version is mostly the most recent one, unless reading old data or streaming from
	//! an older version
	void SetVersion(uint64_t v) {
		D_ASSERT(this->version == 0); // version can only be set once
		this->version = v;
	}

	//! Returns the version of serialization that readers are expected to use
	uint64_t GetVersion() {
		return version;
	}

	//! Reads [read_size] bytes into the buffer
	virtual void ReadData(data_ptr_t buffer, idx_t read_size) = 0;

	//! Gets the context for the deserializer
	virtual ClientContext &GetContext() {
		throw InternalException("This deserializer does not have a client-context");
	};

	//! Gets the catalog for the deserializer
	virtual optional_ptr<Catalog> GetCatalog() {
		return nullptr;
	};

	template <class T>
	T Read() {
		T value;
		ReadData(data_ptr_cast(&value), sizeof(T));
		return value;
	}

	template <class T, typename... ARGS>
	void ReadList(vector<unique_ptr<T>> &list, ARGS &&... args) {
		auto select_count = Read<uint32_t>();
		for (uint32_t i = 0; i < select_count; i++) {
			auto child = T::Deserialize(*this, std::forward<ARGS>(args)...);
			list.push_back(std::move(child));
		}
	}

	template <class T, class RETURN_TYPE = T, typename... ARGS>
	unique_ptr<RETURN_TYPE> ReadOptional(ARGS &&... args) {
		auto has_entry = Read<bool>();
		if (has_entry) {
			return T::Deserialize(*this, std::forward<ARGS>(args)...);
		}
		return nullptr;
	}

	void ReadStringVector(vector<string> &list);

	template <typename T, typename std::enable_if<std::is_integral<T>::value || std::is_enum<T>::value || std::is_floating_point<T>::value, bool>::type = true>
	explicit operator T() {
		return Read<T>();
	}
	template <typename T>
	explicit operator unique_ptr<T>() {
		auto has_entry = Read<bool>();
		if (has_entry) {
			return make_uniq<T>(*this);
		}
		return nullptr;
	}
};

template <>
DUCKDB_API string Deserializer::Read();

} // namespace duckdb
