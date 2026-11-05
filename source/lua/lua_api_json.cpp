//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "lua_api_json.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace LuaAPI {

	// Forward declarations
	sol::object jsonToLua(const nlohmann::json& val, sol::state_view& lua);

	namespace {
		using VisitedTables = std::unordered_set<const void*>;

		struct TableVisitGuard {
			VisitedTables& visited;
			const void* ptr;

			~TableVisitGuard() {
				visited.erase(ptr);
			}
		};

		nlohmann::json luaToJsonImpl(const sol::object& obj, VisitedTables& visited);
	}

	// Convert nlohmann::json to Lua Object
	sol::object jsonToLua(const nlohmann::json& val, sol::state_view& lua) {
		if (val.is_null()) {
			return sol::nil;
		}
		if (val.is_boolean()) {
			return sol::make_object(lua, val.get<bool>());
		}
		if (val.is_number_integer()) {
			return sol::make_object(lua, val.get<int64_t>());
		}
		if (val.is_number_float()) {
			return sol::make_object(lua, val.get<double>());
		}
		if (val.is_string()) {
			return sol::make_object(lua, val.get<std::string>());
		}
		if (val.is_array()) {
			sol::table t = lua.create_table();
			for (size_t i = 0; i < val.size(); ++i) {
				t[i + 1] = jsonToLua(val[i], lua); // Lua 1-based indexing
			}
			return t;
		}
		if (val.is_object()) {
			sol::table t = lua.create_table();
			for (auto it = val.begin(); it != val.end(); ++it) {
				t[it.key()] = jsonToLua(it.value(), lua);
			}
			return t;
		}
		return sol::nil;
	}

	namespace {
		nlohmann::json luaToJsonImpl(const sol::object& obj, VisitedTables& visited) {
		switch (obj.get_type()) {
			case sol::type::nil:
				return nullptr;
			case sol::type::boolean:
				return obj.as<bool>();
			case sol::type::number: {
				double d = obj.as<double>();
				if (d == static_cast<int64_t>(d)) {
					return static_cast<int64_t>(d);
				}
				return d;
			}
			case sol::type::string:
				return obj.as<std::string>();
			case sol::type::table: {
				sol::table t = obj.as<sol::table>();
				const void* ptr = t.pointer();
				if (!ptr) {
					throw std::runtime_error("json.encode: encountered an invalid Lua table");
				}
				if (!visited.insert(ptr).second) {
					throw std::runtime_error("json.encode: cyclic Lua table detected");
				}
				TableVisitGuard guard { visited, ptr };

				// Determine if it's an array or object
				bool isArray = true;
				size_t maxKey = 0;
				size_t count = 0;

				for (auto& pair : t) {
					count++;
					if (pair.first.get_type() == sol::type::number) {
						double k = pair.first.as<double>();
						if (std::isfinite(k) && k >= 1.0 && k == std::floor(k)) {
							size_t idx = static_cast<size_t>(k);
							if (idx > maxKey) {
								maxKey = idx;
							}
						} else {
							isArray = false;
							break;
						}
					} else {
						isArray = false;
						break;
					}
				}

				if (isArray && count > 0 && maxKey == count) {
					nlohmann::json arr = nlohmann::json::array();
					for (size_t i = 1; i <= count; ++i) {
						if (t[i].valid()) {
							arr.push_back(luaToJsonImpl(t[i], visited));
						} else {
							arr.push_back(nullptr);
						}
					}
					return arr;
				} else {
					nlohmann::json objVal = nlohmann::json::object();
					for (auto& pair : t) {
						std::string key;
						if (pair.first.get_type() == sol::type::string) {
							key = pair.first.as<std::string>();
						} else if (pair.first.get_type() == sol::type::number) {
							double numericKey = pair.first.as<double>();
							if (std::isfinite(numericKey) && numericKey == std::floor(numericKey)) {
								key = std::to_string(static_cast<int64_t>(numericKey));
							} else {
								key = std::to_string(numericKey);
							}
						} else {
							throw std::runtime_error("json.encode: unsupported Lua table key type");
						}
						objVal[key] = luaToJsonImpl(pair.second, visited);
					}
					return objVal;
				}
			}
			default:
				throw std::runtime_error("json.encode: unsupported Lua value type");
		}
	}
	} // namespace

	nlohmann::json luaToJson(const sol::object& obj) {
		VisitedTables visited;
		return luaToJsonImpl(obj, visited);
	}

	void registerJson(sol::state& lua) {
		// Create "json" table
		sol::table jsonTable = lua.create_table();

		jsonTable.set_function("encode", [](sol::object obj, sol::this_state) -> std::string {
			nlohmann::json val = luaToJson(obj);
			return val.dump(); // minified output
		});

		jsonTable.set_function("encode_pretty", [](sol::object obj, sol::this_state) -> std::string {
			nlohmann::json val = luaToJson(obj);
			return val.dump(4); // pretty-printed with 4-space indent
		});

		jsonTable.set_function("decode", [](std::string jsonStr, sol::this_state s) -> sol::object {
			try {
				nlohmann::json val = nlohmann::json::parse(jsonStr);
				sol::state_view lua(s);
				return jsonToLua(val, lua);
			} catch (const nlohmann::json::exception&) {
				return sol::nil;
			}
		});

		lua["json"] = jsonTable;
	}

} // namespace LuaAPI
