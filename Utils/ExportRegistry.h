#pragma once
#include <unordered_map>
#include <string>


using RawProc = void(*)();


struct HookSpec {
	const char* name;
	RawProc proxy;
	RawProc* originalOut; // where to store original proc address
};


class ExportRegistry {
public:
	void Add(const HookSpec& h) {
		map[h.name] = h;
	}
	const HookSpec* Find(const char* name) const {
		auto it = map.find(name);
		return it == map.end() ? nullptr : &it->second;
	}
private:
	std::unordered_map<std::string, HookSpec> map;
};