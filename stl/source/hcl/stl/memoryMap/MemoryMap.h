#pragma once
#include <hcl/frontend.h>

namespace hcl::stl
{
	class MemoryMap
	{
	public:
		struct RegDesc
		{
			std::string name;
			std::string desc;
		};

		enum Flags
		{
			F_READ = 1,
			F_WRITE = 2
		};

		virtual void ro(const BVec& value, RegDesc desc) {}
		virtual void ro(const Bit& value, RegDesc desc) {}
		virtual Bit rw(BVec& value, RegDesc desc) { return Bit{}; }
		virtual Bit rw(Bit& value, RegDesc desc) { return Bit{}; }
		virtual Bit wo(BVec& value, RegDesc desc) { return rw(value, desc); }
		virtual Bit wo(Bit& value, RegDesc desc) { return rw(value, desc); }

		Bit add(Bit& value, RegDesc desc);
		Bit add(BVec& value, RegDesc desc);

		template<typename T> void stage(Memory<T>& mem);
		template<typename T> void stage(std::vector<Memory<T>>& mem);

		void flags(size_t f) { m_flags = f; }
		bool readEnabled() const { return (m_flags & F_READ) != 0; }
		bool writeEnabled() const { return (m_flags & F_WRITE) != 0; }

	protected:
		size_t m_flags = F_READ | F_WRITE;
	};

	inline Bit hcl::stl::MemoryMap::add(Bit& value, RegDesc desc)
	{
		if (m_flags == (F_READ | F_WRITE))
			return rw(value, desc);
		else if (m_flags == F_WRITE)
			return wo(value, desc);
		else if (m_flags == F_READ)
			ro(value, desc);
		else
			HCL_DESIGNCHECK_HINT(false, "unsupported combination of flags");

		return '0';
	}

	inline Bit stl::MemoryMap::add(BVec& value, RegDesc desc)
	{
		if (m_flags == (F_READ | F_WRITE))
			return rw(value, desc);
		else if (m_flags == F_WRITE)
			return wo(value, desc);
		else if (m_flags == F_READ)
			ro(value, desc);
		else
			HCL_DESIGNCHECK_HINT(false, "unsupported combination of flags");

		return '0';
	}

	template<typename T>
	inline void MemoryMap::stage(Memory<T>& mem)
	{
		struct SigVis : CompoundNameVisitor
		{
			virtual void operator () (BVec& a) final
			{
				mmap->add(a, { .name = makeName() });
			}

			virtual void operator () (Bit& a) final
			{
				mmap->add(a, { .name = makeName() });
			}

			MemoryMap* mmap;
		};

		BVec cmdAddr = "32xX";
		Bit cmdTrigger = wo(cmdAddr, { 
			.name = "cmd"
		});
		HCL_NAMED(cmdTrigger);
		HCL_NAMED(cmdAddr);

		auto&& port = mem[cmdAddr(0, mem.addressWidth())];

		T memContent = port.read();
		T stage = constructFrom(memContent);

		SigVis v{ this };
        VisitCompound<T>{}(stage, v);
		stage = reg(stage);

		IF(writeEnabled() & cmdTrigger & cmdAddr.msb() == '0')
			port = stage;

		if (readEnabled())
		{
			Bit readCmd = reg(cmdTrigger & cmdAddr.msb() == '1', '0');
			T readData = reg(memContent);
			HCL_NAMED(readCmd);
			HCL_NAMED(readData);

			IF(readCmd)
				stage = readData;
		}
	}

	template<typename T>
	inline void stl::MemoryMap::stage(std::vector<Memory<T>>& mems)
	{
		if (mems.empty())
			return;

		struct SigVis : CompoundNameVisitor
		{
			virtual void operator () (BVec& a) final
			{
				mmap->add(a, { .name = makeName() });
				regCount++;
			}

			virtual void operator () (Bit& a) final
			{
				mmap->add(a, { .name = makeName() });
				regCount++;
			}

			MemoryMap* mmap;
			size_t regCount = 0;
		};

		BitWidth memTabWidth{ utils::Log2C(mems.size()) };
		BitWidth memWidth;
		for (const Memory<T>& m : mems)
			memWidth.value = std::max(memWidth.value, m.addressWidth().value);

		HCL_DESIGNCHECK_HINT(memWidth.value + memTabWidth.value + 2 <= 32, 
			"The memory vector stage command register is limited to 32bit including 2 command bits, the table selection bits and the memory address bits.");

		Selection memTabSel = Selection::Slice((int)memWidth.value, (int)memTabWidth.value);
		BVec cmdAddr = 32_b;
		Bit cmdTrigger = rw(cmdAddr, {
			.name = "cmd"
			});
		HCL_NAMED(cmdTrigger);
		HCL_NAMED(cmdAddr);

		T stage = constructFrom(mems.front().defaultValue());
		SigVis v;
		v.mmap = this;
		VisitCompound<T>{}(stage, v);
		stage = reg(stage);

		Bit readTrigger = reg(readEnabled() & cmdTrigger & cmdAddr.msb() == '1', '0');
		BVec readTabAddr = reg(cmdAddr(memTabSel));

		for (size_t t = 0; t < mems.size(); ++t)
		{
			auto&& port = mems[t][cmdAddr(0, mems[t].addressWidth().value)];

			IF(cmdAddr(memTabSel) == t)
			{
				IF(writeEnabled() & cmdTrigger & cmdAddr.msb() == '0')
					port = stage;
			}

			T readData = reg(port.read());
			IF(readTrigger & readTabAddr == t)
				stage = readData;
		}

		cmdAddr = pack(
			ConstBVec(m_flags, 8), 
			ConstBVec(mems.size(), 8),
			ConstBVec(memWidth.value, 8), 
			ConstBVec(v.regCount, 8)
		);
	}

}
