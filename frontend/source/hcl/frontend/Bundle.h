#pragma once
#include "Compound.h"

#include <string_view>
#include <any>
#include <typeinfo>
#include <vector>
#include <functional>

namespace hcl
{
	class Bundle
	{
		struct Item
		{
			std::string_view name = "";
			size_t index = 0;
			std::any instance;

			std::function<void(core::frontend::CompoundVisitor& v)> visitCopy;
			std::function<void(core::frontend::CompoundVisitor& v)> visitConst;
			std::function<void(core::frontend::CompoundVisitor& v)> visitMutate;
		};

	public:
		template<typename T> T& get(std::string_view name = "", size_t index = 0);
		template<typename T> const T& get(std::string_view name = "", size_t index = 0) const;
		template<typename T> T& add(T&& item, std::string_view name = "", size_t index = 0);
		template<typename T> bool has(std::string_view name = "", size_t index = 0) const;

	protected:
		Item* find(const std::type_info& type, std::string_view name, size_t index);
		const Item* find(const std::type_info& type, std::string_view name, size_t index) const;

	private:
		std::vector<Item> m_member;
	};
}

