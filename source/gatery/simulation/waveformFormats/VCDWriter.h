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
#pragma once
#include <string>
#include <fstream>
#include <functional>

#include "../BitVectorState.h"

namespace gtry::sim
{
	class VCDWriter
	{
	public:
		class Scope
		{
		public:
			Scope(std::function<void()> exitFunc) : m_EndScope(exitFunc) {}
			~Scope() { m_EndScope(); }
		private:
			std::function<void()> m_EndScope;
		};

		VCDWriter(std::string filename);
		~VCDWriter();

		bool commit();

		explicit operator bool () const { return (bool)m_File; }

		Scope beginModule(std::string_view name);
		void declareWire(size_t width, std::string_view code, std::string_view label);
		void declareReal(std::string_view code, std::string_view label);
		void declareString(std::string_view code, std::string_view label);

		Scope beginDumpVars();
		void writeState(std::string_view code, const DefaultBitVectorState& state, size_t offset, size_t size);
		void writeState(std::string_view code, size_t size, uint64_t defined, uint64_t valid);
		void writeString(std::string_view code, size_t size, std::string_view text);
		void writeString(std::string_view code, std::string_view text);
		void writeBitState(std::string_view code, bool defined, bool value);
		void writeTime(size_t time);

	protected:
		void openFile(bool createEmpty);

		std::ofstream m_File;
		std::string m_FileName;
		bool m_EndDefinitions = false;
		void* m_Transaction = nullptr;
	};
}
