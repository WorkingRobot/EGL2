#pragma once

#include "egfs.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

template<class T>
class DirTree {
public:
	static constexpr auto Separators = L"\\/";

	using node_type = typename std::variant<DirTree<T>, T>;
	using container_type = typename std::unordered_map<std::wstring, node_type>;
	using iterator = typename container_type::iterator;
	using const_iterator = typename container_type::const_iterator;

	DirTree() : Parent(nullptr) { }

	DirTree(DirTree<T>* parent)
	{
		Parent = parent;
	}

	~DirTree() {

	}

	void AddFile(std::wstring_view& path, T&& data) {
		auto separator = path.find_first_of(Separators);
		if (separator != std::wstring_view::npos) {
			DirTree<T>* child;
			{
				// emplace creates a new value if it doesn't exist and otherwise grabs by the key
				auto& emplaced = Children.emplace(std::wstring(path.substr(0, separator)), DirTree<T>(this));
				if (emplaced.second) {
					child = &std::get<DirTree<T>>(emplaced.first->second);
					child->Name = emplaced.first->first;
				}
				else {
					if (auto folderPtr = std::get_if<DirTree<T>>(&emplaced.first->second)) {
						child = folderPtr;
					}
					else {
						return; // child existed as a file, not a folder
					}
				}
			}
			child->AddFile(path.substr(separator + 1), std::forward<T>(data));
		}
		else {
			Children[std::wstring(path)] = data;
		}
	}

	void AddFile(const wchar_t* path, T&& data) {
		AddFile(std::wstring_view(path), std::forward<T>(data));
	}

	node_type* GetFile(std::wstring_view& path) {
		auto separator = path.find_first_of(Separators);
		if (separator != std::wstring_view::npos) {
			auto n = Children.find(std::wstring(path.substr(0, separator)));
			if (n == Children.end()) {
				return nullptr;
			}
			auto& child = n->second;
			if (auto folder = std::get_if<DirTree<T>>(&child)) {
				return folder->GetFile(path.substr(separator + 1));
			}
			return nullptr;
		}
		else {
			auto n = Children.find(std::wstring(path));
			if (n == Children.end()) {
				return nullptr;
			}
			return &n->second;
		}
	}

	node_type* GetFile(const wchar_t* path) {
		return GetFile(std::wstring_view(path));
	}

	inline iterator begin() noexcept { return Children.begin(); }
	inline const_iterator cbegin() const noexcept { return Children.cbegin(); }
	inline iterator end() noexcept { return Children.end(); }
	inline const_iterator cend() const noexcept { return Children.cend(); }

	const DirTree<T>* Parent;
	std::wstring Name;
    container_type Children;
};

template<class T>
class RootTree {
public:
	using node_type = typename DirTree<T>::node_type;
	using container_type = typename DirTree<T>::container_type;
	using iterator = typename container_type::iterator;
	using const_iterator = typename container_type::const_iterator;

	RootTree() :
		_RootNode(DirTree<T>()),
		RootNode(std::get_if<DirTree<T>>(&_RootNode))
	{ }

	~RootTree() {

	}

	node_type* GetFile(const wchar_t* path) {
		if (path[1] == L'\0') {
			return &_RootNode;
		}
		return RootNode->GetFile(path + 1);
	}

	void AddFile(const wchar_t* path, T&& data) {
		RootNode->AddFile(std::wstring_view(path), std::forward<T>(data));
	}

	inline iterator begin() noexcept { return RootNode->begin(); }
	inline const_iterator cbegin() const noexcept { return RootNode->cbegin(); }
	inline iterator end() noexcept { return RootNode->end(); }
	inline const_iterator cend() const noexcept { return RootNode->cend(); }

	node_type _RootNode;
	DirTree<T>* RootNode;
};