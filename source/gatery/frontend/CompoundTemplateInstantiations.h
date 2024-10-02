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

#if !defined(__clang__) || (__clang_major__ >= 14)
	#define GTRY_EXTERN_TEMPLATE_COMPOUND_MINIMAL(...) \
		extern template void gtry::connect(__VA_ARGS__ &, __VA_ARGS__&); \
		extern template __VA_ARGS__ gtry::constructFrom(const __VA_ARGS__ &); \
		extern template void gtry::setName(__VA_ARGS__ &, std::string_view); \
		extern template void gtry::pinIn(__VA_ARGS__&&, std::string, const PinNodeParameter&); \
		extern template void gtry::pinOut(__VA_ARGS__&&, std::string, const PinNodeParameter&); \
		extern template auto gtry::upstream(__VA_ARGS__&); \
		extern template auto gtry::downstream(__VA_ARGS__&);
#else
	#define GTRY_EXTERN_TEMPLATE_COMPOUND_MINIMAL(...) \
		extern template void gtry::connect(__VA_ARGS__&, __VA_ARGS__&); \
		extern template __VA_ARGS__ gtry::constructFrom(const __VA_ARGS__ &); \
		extern template void gtry::setName(__VA_ARGS__ &, std::string_view); \
		extern template void gtry::pinIn(__VA_ARGS__&&, std::string, const PinNodeParameter&); \
		extern template void gtry::pinOut(__VA_ARGS__&&, std::string, const PinNodeParameter&);
#endif

#if !defined(__clang__) || (__clang_major__ >= 14)
	#define GTRY_INSTANTIATE_TEMPLATE_COMPOUND_MINIMAL(...) \
		template void gtry::connect(__VA_ARGS__&, __VA_ARGS__&); \
		template __VA_ARGS__ gtry::constructFrom(const __VA_ARGS__ &); \
		template void gtry::setName(__VA_ARGS__ &, std::string_view); \
		template void gtry::pinIn(__VA_ARGS__&&, std::string, const PinNodeParameter&); \
		template void gtry::pinOut(__VA_ARGS__&&, std::string, const PinNodeParameter&); \
		template auto gtry::upstream(__VA_ARGS__&); \
		template auto gtry::downstream(__VA_ARGS__&);
#else
	#define GTRY_INSTANTIATE_TEMPLATE_COMPOUND_MINIMAL(...) \
		template void gtry::connect(__VA_ARGS__&, __VA_ARGS__&); \
		template __VA_ARGS__ gtry::constructFrom(const __VA_ARGS__ &); \
		template void gtry::setName(__VA_ARGS__ &, std::string_view); \
		template void gtry::pinIn(__VA_ARGS__&&, std::string, const PinNodeParameter&); \
		template void gtry::pinOut(__VA_ARGS__&&, std::string, const PinNodeParameter&);
#endif



#define GTRY_EXTERN_TEMPLATE_COMPOUND(...) \
	GTRY_EXTERN_TEMPLATE_COMPOUND_MINIMAL(__VA_ARGS__) \
	extern template __VA_ARGS__ gtry::dontCare(const __VA_ARGS__ &); \
	extern template __VA_ARGS__ gtry::undefined(const __VA_ARGS__ &); \
	extern template __VA_ARGS__ gtry::pipeinput(const __VA_ARGS__&, PipeBalanceGroup&); \
	extern template gtry::BitWidth gtry::width(const __VA_ARGS__ &);


#define GTRY_INSTANTIATE_TEMPLATE_COMPOUND(...) \
	GTRY_INSTANTIATE_TEMPLATE_COMPOUND_MINIMAL(__VA_ARGS__) \
	template __VA_ARGS__ gtry::dontCare(const __VA_ARGS__ &); \
	template __VA_ARGS__ gtry::undefined(const __VA_ARGS__ &); \
	template __VA_ARGS__ gtry::pipeinput(const __VA_ARGS__&, PipeBalanceGroup&); \
	template gtry::BitWidth gtry::width(const __VA_ARGS__ &);



#define GTRY_EXTERN_TEMPLATE_STREAM(...) \
	GTRY_EXTERN_TEMPLATE_COMPOUND_MINIMAL(__VA_ARGS__) \
	extern template __VA_ARGS__ gtry::scl::strm::regDownstreamBlocking(__VA_ARGS__&&, const RegisterSettings&); \
	extern template __VA_ARGS__ gtry::scl::strm::regReady(__VA_ARGS__&&, const RegisterSettings&); \
	extern template __VA_ARGS__ gtry::scl::strm::delay(__VA_ARGS__&&, size_t); \
	extern template __VA_ARGS__ gtry::scl::strm::regDecouple(__VA_ARGS__&, const RegisterSettings&); \
	extern template __VA_ARGS__ gtry::scl::strm::pipeinput(__VA_ARGS__&&);


#define GTRY_INSTANTIATE_TEMPLATE_STREAM(...) \
	GTRY_INSTANTIATE_TEMPLATE_COMPOUND_MINIMAL(__VA_ARGS__) \
	template __VA_ARGS__ gtry::scl::strm::regDownstreamBlocking(__VA_ARGS__&&, const RegisterSettings&); \
	template __VA_ARGS__ gtry::scl::strm::regReady(__VA_ARGS__&&, const RegisterSettings&); \
	template __VA_ARGS__ gtry::scl::strm::delay(__VA_ARGS__&&, size_t); \
	template __VA_ARGS__ gtry::scl::strm::regDecouple(__VA_ARGS__&, const RegisterSettings&); \
	template __VA_ARGS__ gtry::scl::strm::pipeinput(__VA_ARGS__&&);

