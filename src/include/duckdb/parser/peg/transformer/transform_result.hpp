#pragma once

#include "duckdb/common/common.hpp"

namespace duckdb {

struct TransformResultValue {
	virtual ~TransformResultValue() = default;

	//! Identifies the concrete TypedTransformResult<T> without relying on RTTI
	virtual const void *TypeTag() const = 0;
};

template <class T>
struct TypedTransformResult : public TransformResultValue {
	explicit TypedTransformResult(T value_p) : value(std::move(value_p)) {
	}

	//! The address of this member is unique per instantiation, which makes it a type identity
	static const char TYPE_TAG;

	const void *TypeTag() const override {
		return &TYPE_TAG;
	}

	T value;
};

template <class T>
const char TypedTransformResult<T>::TYPE_TAG = 0;

//! Casts to TypedTransformResult<T> if the result holds exactly that type, and returns nullptr otherwise
template <class T>
TypedTransformResult<T> *TryCastTransformResult(TransformResultValue *result) {
	if (!result || result->TypeTag() != &TypedTransformResult<T>::TYPE_TAG) {
		return nullptr;
	}
	return static_cast<TypedTransformResult<T> *>(result);
}

template <class T>
TypedTransformResult<T> *TryCastTransformResult(TransformResultValue &result) {
	return TryCastTransformResult<T>(&result);
}

} // namespace duckdb
