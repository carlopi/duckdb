
#pragma once

#include "duckdb/parallel/interrupt.hpp"

namespace duckdb {

struct BlockableState {
public:
	BlockableState(StateWithBlockableTasks &blockable_state, InterruptState &interrupt_state) : blockable_state(blockable_state), interrupt_state(interrupt_state) {}
	StateWithBlockableTasks &blockable_state;
	InterruptState &interrupt_state;
};

} // namespace duckdb
