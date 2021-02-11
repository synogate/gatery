#include <vector>
#include "Bit.h"

namespace hcl
{
	template<typename T>
	class Vector : public std::vector<T>
	{
	public:
		using std::vector<T>::vector;

		Vector& operator = (const Vector& rhs)
		{
			std::vector<T>::resize(rhs.size());

			for (size_t i = 0; i < std::vector<T>::size(); ++i)
				std::vector<T>::at(i) = rhs[i];

			return *this;
		}

	};
}
