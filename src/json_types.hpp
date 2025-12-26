#pragma once

#include <nlohmann/json.hpp>
#include "fifo_map.hpp"
#include <map>
template<class K, class V, class dummy_compare, class A>
using fifo_map = nlohmann::fifo_map<K, V, nlohmann::fifo_map_compare<K>, A>;

using ordered_json = nlohmann::basic_json<fifo_map>;
