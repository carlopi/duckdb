#include "duckdb/execution/index/index_type.hpp"
#include "duckdb/execution/index/index_type_set.hpp"
#include "duckdb/execution/index/art/art.hpp"
#include <iostream>
namespace duckdb {

IndexTypeSet::IndexTypeSet() {
	std::cout << "IndexTypeSet 1\n";
	// Register the ART index type
	std::cout << "IndexTypeSet 2\n";
	IndexType art_index_type;
	std::cout << "IndexTypeSet 3\n";
	art_index_type.name = ART::TYPE_NAME;
	std::cout << "IndexTypeSet 4\n";
	art_index_type.create_instance = ART::Create;
	std::cout << "IndexTypeSet 5\n";
	RegisterIndexType(art_index_type);
	std::cout << "IndexTypeSet 6\n";
}

optional_ptr<IndexType> IndexTypeSet::FindByName(const string &name) {
	lock_guard<mutex> g(lock);
	auto entry = functions.find(name);
	if (entry == functions.end()) {
		return nullptr;
	}
	return &entry->second;
}

void IndexTypeSet::RegisterIndexType(const IndexType &index_type) {
	lock_guard<mutex> g(lock);
	if (functions.find(index_type.name) != functions.end()) {
		throw CatalogException("Index type with name \"%s\" already exists!", index_type.name.c_str());
	}
	functions[index_type.name] = index_type;
}

} // namespace duckdb
