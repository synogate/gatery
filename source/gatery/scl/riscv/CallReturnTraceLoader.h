/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

	Gatery is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 3 of the License, or (at your option) any later version.

	Gatery is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#pragma once
#include <gatery/frontend.h>

namespace gtry::scl::riscv
{
	class CallReturnTraceLoader
	{
	public:
		void load(std::filesystem::path sourceFileName, std::filesystem::path elfSymbolPath);
		void load(std::istream& source, std::filesystem::path elfSymbolPath);

	protected:
		virtual void onCall(uint64_t cycle, uint64_t target, std::string_view function) = 0;
		virtual void onReturn(uint64_t cycle, uint64_t target, std::string_view function) = 0;
	};

	class CallReturnTraceProfiler : public CallReturnTraceLoader
	{
	public:
		struct FunctionInfo
		{
			std::string name;
			size_t cycles = 0;
			size_t cyclesChilds = 0;
			std::set<std::string> childs;
		};

		struct StackFrame
		{
			FunctionInfo* f;
			size_t cyclesStart = 0;
			std::string_view name;
		};
	public:

		void printFunctionsByCycles(std::ostream& o, size_t cycleNs) const;
		std::vector<const FunctionInfo*> functionsByCycles() const;

	protected:
		void onCall(uint64_t cycle, uint64_t target, std::string_view function) override;
		void onReturn(uint64_t cycle, uint64_t target, std::string_view function) override;

	private:
		std::map<std::string, FunctionInfo> m_func;
		std::vector<StackFrame> m_stack;
	};
}
