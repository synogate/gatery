#include "CallReturnTraceLoader.h"
/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2021 Michael Offel, Andreas Ley

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
#include "gatery/scl_pch.h"
#include "CallReturnTraceLoader.h"

#ifdef _WIN32
#pragma warning(push, 0)
#pragma warning(disable : 4018) // boost process environment "'<': signed/unsigned mismatch"
#endif
#include <boost/process.hpp>
#ifdef _WIN32
#pragma warning(pop)
#endif


#include <boost/asio.hpp>
#include <boost/format.hpp>

namespace bp = boost::process;

namespace gtry::scl::riscv
{
	class SymbolLookup
	{
	public:
		SymbolLookup(std::filesystem::path elfSymbolPath) { init(elfSymbolPath); }

		std::string_view lookup(uint64_t address)
		{
			auto cache_it = m_cache.find(address);
			if (cache_it != m_cache.end())
				return cache_it->second;


			m_writeBuf = (boost::format("%x\n") % address).str();

			auto nop = [](const boost::system::error_code& ec, std::size_t size) {};
			m_ios.restart();
			boost::asio::async_write(m_wr, boost::asio::buffer(m_writeBuf), nop);

			std::string function;
			boost::asio::streambuf readBuf;
			boost::asio::async_read_until(m_rd, readBuf, '\n',
				[&](const boost::system::error_code& ec, std::size_t size) {
					std::istream s{ &readBuf };
					std::getline(s, function);
					boost::asio::async_read_until(m_rd, readBuf, '\n', nop);
				});
			m_ios.run();

			boost::trim(function);
			return m_cache[address] = function;
		}

	protected:
		void init(std::filesystem::path elfSymbolPath)
		{
			auto a2lPath = bp::search_path("riscv32-unknown-elf-addr2line");
			if (a2lPath.empty())
				throw std::runtime_error{ "could not locate riscv32-unknown-elf-addr2line" };

			m_a2l = bp::child(a2lPath, "-fe", elfSymbolPath.string(), bp::std_in < m_wr, bp::std_out > m_rd);
			if (!m_a2l.running())
				throw std::runtime_error{ "launch riscv32-unknown-elf-addr2line failed" };
		}

	private:
		boost::asio::io_service m_ios;
		bp::async_pipe m_rd = { m_ios };
		bp::async_pipe m_wr = { m_ios };
		bp::child m_a2l;

		std::string m_writeBuf;

		std::map<uint64_t, std::string> m_cache;

	};


	void CallReturnTraceLoader::load(std::filesystem::path sourceFileName, std::filesystem::path elfSymbolPath)
	{
		std::ifstream f{ sourceFileName.string().c_str(), std::ifstream::binary };
		load(f, elfSymbolPath);
	}

	void CallReturnTraceLoader::load(std::istream& source, std::filesystem::path elfSymbolPath)
	{
		SymbolLookup sym{ elfSymbolPath };

		source >> std::hex;
		while (source)
		{
			char op;
			uint64_t cycle, ip, target;
			source >> cycle >> ip >> op >> target;
			if (!source)
				break;

			std::string_view function = sym.lookup(target);
			if (op == 'C')
				onCall(cycle, target, function);
			else if (op == 'R')
				onReturn(cycle, target, function);
		}
	}

	void CallReturnTraceProfiler::printFunctionsByCycles(std::ostream& o, size_t cycleNs) const
	{
		for (auto* f : functionsByCycles())
		{
			o <<
				(f->cycles * cycleNs / 1000) << "us\t" <<
				(f->cyclesChilds * cycleNs / 1000) << "us\t" <<
				f->name << '\t';
			for (auto& c : f->childs)
				o << c << ',';
			o << '\n';
		}
		o << std::flush;
	}

	std::vector<const CallReturnTraceProfiler::FunctionInfo*> CallReturnTraceProfiler::functionsByCycles() const
	{
		std::vector<const FunctionInfo*> functions;
		for (auto& it : m_func)
			functions.push_back(&it.second);

		std::ranges::sort(functions, std::greater{}, [](auto* f) { return f->cycles; });
		return functions;
	}

	void CallReturnTraceProfiler::onCall(uint64_t cycle, uint64_t target, std::string_view function)
	{
		std::string f{ function };
		if (!m_stack.empty())
		{
			auto& tos = m_stack.back();
			tos.f->childs.insert(f);
			tos.f->cycles += cycle - tos.cyclesStart;
			tos.cyclesStart = cycle;
		}

		FunctionInfo& t = m_func[f];
		if (t.name.empty())
			t.name = std::move(f);

		m_stack.push_back(
			StackFrame{ &t, cycle, function }
		);
	}

	void CallReturnTraceProfiler::onReturn(uint64_t cycle, uint64_t target, std::string_view function)
	{
		if (m_stack.size() > 1 && m_stack[m_stack.size() - 2].name != function)
		{
			// try to fix stack
			auto it = std::ranges::find_if(m_stack, 
				[&](std::string_view name) { return name == function; }, &StackFrame::name);

			if (it != m_stack.end())
			{
				// we have no idea which function took how long. just drop them
				m_stack.erase(it + 1, m_stack.end());

				auto& tos = m_stack.back();
				tos.f->cyclesChilds += cycle - tos.cyclesStart;
				tos.cyclesStart = cycle;
			}
			else
			{
				// we may have missed multiple calls, so pretend to have seen a call to function
				m_stack.push_back(
					{ &m_func[std::string{function}], cycle, function }
				);
			}
			return;
		}
			
		if (!m_stack.empty())
		{
			auto& tos = m_stack.back();
			tos.f->cycles += cycle - tos.cyclesStart;
			m_stack.pop_back();
		}

		if (!m_stack.empty())
		{
			auto& tos = m_stack.back();
			tos.f->cyclesChilds += cycle - tos.cyclesStart;
			tos.cyclesStart = cycle;
		}
	}
}
