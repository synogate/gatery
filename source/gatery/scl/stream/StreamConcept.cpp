#include "gatery/scl_pch.h"
#include "StreamConcept.h"

#include "Stream.h"

namespace gtry::scl::strm
{
	void bar(gtry::scl::strm::StreamSignal auto&& t) {}

	void foo()
	{

		Stream<Bit> t;


		bar(t);


	}
}
