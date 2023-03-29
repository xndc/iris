#include "engine/deferred.hh"
#include <deque>

struct DeferredAction {
	DeferredCallback callback;
	void* data;
};

std::deque<DeferredAction> DeferredActions;

void Defer(DeferredCallback callback, void* data) {
	DeferredActions.push_back({.callback = callback, .data = data});
}

uint32_t RunDeferredAction(Engine& engine) {
	size_t size = DeferredActions.size();
	if (size > 0) {
		DeferredAction& action = DeferredActions.front();
		action.callback(engine, action.data);
		DeferredActions.pop_front();
		size--;
	}
	return size;
}
