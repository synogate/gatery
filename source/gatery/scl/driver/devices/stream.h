#pragma once
#pragma pack(push, 4)

struct TransactionalDevice
{
	void rollback() { rollback_ = 1; }
	void commit() { commit_ = 1; }

protected:
	volatile unsigned long	rollback_ = 0;
	volatile unsigned long	commit_ = 0;
};

template<typename PayloadT>
struct StreamTxDevice
{
	bool ready() const { return ready_ != 0; }
	bool valid() const { return valid_ != 0; }

	void push_unsafe(PayloadT value)
	{
		payload_ = (unsigned long)value;
		valid_ = 1;
	}

	void push(PayloadT value)
	{
		push_unsafe(value);
		while (valid());
	}

protected:
	volatile unsigned long	payload_ = 0;
	volatile unsigned long	ready_ = 0;
	volatile unsigned long	valid_ = 0;
};

template<typename PayloadT>
struct StreamRxDevice
{
	bool ready() const { return ready_ != 0; }
	bool valid() const { return valid_ != 0; }

	void pop_unsafe()
	{
		ready_ = 1;
	}

	void pop()
	{
		pop_unsafe();
		while (ready());
	}

	PayloadT payload() const { return PayloadT(payload_); }

protected:
	volatile unsigned long	payload_ = 0;
	volatile unsigned long	valid_ = 0;
	volatile unsigned long	ready_ = 0;
};


struct PacketTxDevice
{
	void eop(bool value)
	{
		eop_ = value ? 1 : 0;
	}

protected:
	volatile unsigned long	eop_ = 0;
};

struct PacketRxDevice
{
	bool eop() const { return eop_ != 0; }

protected:
	volatile unsigned long	eop_ = 0;
};

template<typename... PartsT>
struct Stream : PartsT...
{};

#pragma pack(pop)
