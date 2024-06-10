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
#include <cstdint>

namespace gtry::scl::usb
{
	template<typename T>
	concept DescriptorType = std::is_aggregate_v<T> && requires() { T::TYPE; };

	namespace ClassCode
	{
		enum
		{
			Interface_Descriptors = 0x00,
			Audio = 0x01,
			Communications_and_CDC_Control = 0x02,
			Human_Interface_Device = 0x03,
			Physical = 0x05,
			Image = 0x06,
			Printer = 0x07,
			Mass_Storage = 0x08,
			Hub = 0x09,
			CDC_Data = 0x0A,
			Smart_Card = 0x0B,
			Content_Security = 0x0D,
			Video = 0x0E,
			Personal_Healthcare = 0x0F,
			Audio_Video_Devices = 0x10,
			Billboard_Device_Class = 0x11,
			USB_TypeC_Bridge_Class = 0x12,
			I3C_Device_Class = 0x3C,
			Diagnostic_Device = 0xDC,
			Wireless_Controller = 0xE0,
			Miscellaneous = 0xEF,
			Application_Specific = 0xFE,
			Vendor_Specific = 0xFF,
		};
	}

	enum class SetupRequest
	{
		GET_STATUS = 0,
		CLEAR_FEATURE = 1,
		SET_FEATURE = 3,
		SET_ADDRESS = 5,
		GET_DESCRIPTOR = 6,
		SET_DESCRIPTOR = 7,
		GET_CONFIGURATION = 8,
		SET_CONFIGURATION = 9,
	};

	enum class Handshake
	{
		ACK,
		NAK,
		STALL,
	};

#pragma pack(push, 1)
	struct StringId
	{
		uint8_t id = 0;
	};

	struct DeviceDescriptor
	{
		enum { TYPE = 1 };

		uint16_t USB = 0x110; // BCD USB 1.1 or 2.0

		// usually set at interface level
		uint8_t Class = 0;
		uint8_t SubClass = 0;
		uint8_t Protocol = 0;

		uint8_t MaxPacketSize = 64; // 8, 16, 32 or 64

		// buy your own Vendor id or use 
		// http://wiki.openmoko.org/wiki/USB_Product_IDs
		// for foss designs. 
		uint16_t Vendor = 0x1d50; // openmoko
		uint16_t Product = 0;

		uint16_t Device = 0x100; // BCD Device Release Number

		// make sure to add string descriptors for each used string
		StringId ManufacturerName;
		StringId ProductName;
		StringId SerialNumber;

		uint8_t NumConfigurations = 0;
	};

	namespace ConfigurationAttributes
	{
		enum {
			RemoteWakeup = 1 << 5,
			SelfPowered = 1 << 6,
			Reserved = 1 << 7,
		};
	}

	struct ConfigurationDescriptor
	{
		enum { TYPE = 2 };

		uint16_t TotalLength = 0; // including all sub descriptors
		uint8_t NumInterfaces = 0;
		uint8_t ConfigurationValue = 0;
		StringId Name;
		uint8_t Attributes = ConfigurationAttributes::Reserved;
		uint8_t MaxPower = 50; // *2 mA
	};

	struct InterfaceAssociationDescriptor
	{
		enum { 
			TYPE = 11,
			DevClass = ClassCode::Miscellaneous,
			DevSubClass = 2,
			DevProtocol = 1,
		};

		uint8_t FirstInterface = 0;
		uint8_t InterfaceCount = 0;

		// set equal to first interface values
		uint8_t FunctionClass = 0;
		uint8_t FunctionSubClass = 0;

		uint8_t FunctionProtocol = 0;
		StringId Name;
	};

	struct InterfaceDescriptor
	{
		enum { TYPE = 4 };

		uint8_t InterfaceNumber = 0;
		uint8_t AlternateSetting = 0;
		uint8_t NumEndpoints = 0;
		uint8_t Class = 0;
		uint8_t SubClass = 0;
		uint8_t Protocol = 0;
		StringId Name;
	};

	enum class EndpointDirection
	{
		out = 0,
		in = 1,
	};

	struct EndpointAddress
	{
		uint8_t addr = 0;

		static EndpointAddress create(uint8_t index, EndpointDirection direction);
	};

	namespace EndpointAttribute
	{
		enum {
			control = 0,
			isochronous = 1,
			bulk = 2,
			interrupt = 3,
		};
	}

	struct EndpointDescriptor
	{
		enum { TYPE = 5 };

		EndpointAddress Address;
		uint8_t Attributes = EndpointAttribute::bulk;
		uint16_t MaxPacketSize = 64;
		uint8_t Interval = 1; // interrupt poll interval
	};
#pragma pack(pop)

	enum class LangID
	{
		Afrikaans = 0x0436,
		Albanian = 0x041c,
		Arabic_Saudi_Arabia = 0x0401,
		Arabic_Iraq = 0x0801,
		Arabic_Egypt = 0x0c01,
		Arabic_Libya = 0x1001,
		Arabic_Algeria = 0x1401,
		Arabic_Morocco = 0x1801,
		Arabic_Tunisia = 0x1c01,
		Arabic_Oman = 0x2001,
		Arabic_Yemen = 0x2401,
		Arabic_Syria = 0x2801,
		Arabic_Jordan = 0x2c01,
		Arabic_Lebanon = 0x3001,
		Arabic_Kuwait = 0x3401,
		Arabic_UAE = 0x3801,
		Arabic_Bahrain = 0x3c01,
		Arabic_Qatar = 0x4001,
		Armenian = 0x042b,
		Assamese = 0x044d,
		Azeri_Latin = 0x042c,
		Azeri_Cyrillic = 0x082c,
		Basque = 0x042d,
		Belarussian = 0x0423,
		Bengali = 0x0445,
		Bulgarian = 0x0402,
		Burmese = 0x0455,
		Catalan = 0x0403,
		Chinese_Taiwan = 0x0404,
		Chinese_PRC = 0x0804,
		Chinese_Hong_Kong_SAR_PRC = 0x0c04,
		Chinese_Singapore = 0x1004,
		Chinese_Macau_SAR = 0x1404,
		Croatian = 0x041a,
		Czech = 0x0405,
		Danish = 0x0406,
		Dutch_Netherlands = 0x0413,
		Dutch_Belgium = 0x0813,
		English_United_States = 0x0409,
		English_United_Kingdom = 0x0809,
		English_Australian = 0x0c09,
		English_Canadian = 0x1009,
		English_New_Zealand = 0x1409,
		English_Ireland = 0x1809,
		English_South_Africa = 0x1c09,
		English_Jamaica = 0x2009,
		English_Caribbean = 0x2409,
		English_Belize = 0x2809,
		English_Trinidad = 0x2c09,
		English_Zimbabwe = 0x3009,
		English_Philippines = 0x3409,
		Estonian = 0x0425,
		Faeroese = 0x0438,
		Farsi = 0x0429,
		Finnish = 0x040b,
		French_Standard = 0x040c,
		French_Belgian = 0x080c,
		French_Canadian = 0x0c0c,
		French_Switzerland = 0x100c,
		French_Luxembourg = 0x140c,
		French_Monaco = 0x180c,
		Georgian = 0x0437,
		German_Standard = 0x0407,
		German_Switzerland = 0x0807,
		German_Austria = 0x0c07,
		German_Luxembourg = 0x1007,
		German_Liechtenstein = 0x1407,
		Greek = 0x0408,
		Gujarati = 0x0447,
		Hebrew = 0x040d,
		Hindi = 0x0439,
		Hungarian = 0x040e,
		Icelandic = 0x040f,
		Indonesian = 0x0421,
		Italian_Standard = 0x0410,
		Italian_Switzerland = 0x0810,
		Japanese = 0x0411,
		Kannada = 0x044b,
		Kashmiri_India = 0x0860,
		Kazakh = 0x043f,
		Konkani = 0x0457,
		Korean = 0x0412,
		Korean_Johab = 0x0812,
		Latvian = 0x0426,
		Lithuanian = 0x0427,
		Lithuanian_Classic = 0x0827,
		Macedonian = 0x042f,
		Malay_Malaysian = 0x043e,
		Malay_Brunei_Darussalam = 0x083e,
		Malayalam = 0x044c,
		Manipuri = 0x0458,
		Marathi = 0x044e,
		Nepali_India = 0x0861,
		Norwegian_Bokmal = 0x0414,
		Norwegian_Nynorsk = 0x0814,
		Oriya = 0x0448,
		Polish = 0x0415,
		Portuguese_Brazil = 0x0416,
		Portuguese_Standard = 0x0816,
		Punjabi = 0x0446,
		Romanian = 0x0418,
		Russian = 0x0419,
		Sanskrit = 0x044f,
		Serbian_Cyrillic = 0x0c1a,
		Serbian_Latin = 0x081a,
		Sindhi = 0x0459,
		Slovak = 0x041b,
		Slovenian = 0x0424,
		Spanish_Traditional_Sort = 0x040a,
		Spanish_Mexican = 0x080a,
		Spanish_Modern_Sort = 0x0c0a,
		Spanish_Guatemala = 0x100a,
		Spanish_Costa_Rica = 0x140a,
		Spanish_Panama = 0x180a,
		Spanish_Dominican_Republic = 0x1c0a,
		Spanish_Venezuela = 0x200a,
		Spanish_Colombia = 0x240a,
		Spanish_Peru = 0x280a,
		Spanish_Argentina = 0x2c0a,
		Spanish_Ecuador = 0x300a,
		Spanish_Chile = 0x340a,
		Spanish_Uruguay = 0x380a,
		Spanish_Paraguay = 0x3c0a,
		Spanish_Bolivia = 0x400a,
		Spanish_El_Salvador = 0x440a,
		Spanish_Honduras = 0x480a,
		Spanish_Nicaragua = 0x4c0a,
		Spanish_Puerto_Rico = 0x500a,
		Sutu = 0x0430,
		Swahili_Kenya = 0x0441,
		Swedish = 0x041d,
		Swedish_Finland = 0x081d,
		Tamil = 0x0449,
		Tatar_Tatarstan = 0x0444,
		Telugu = 0x044a,
		Thai = 0x041e,
		Turkish = 0x041f,
		Ukrainian = 0x0422,
		Urdu_Pakistan = 0x0420,
		Urdu_India = 0x0820,
		Uzbek_Latin = 0x0443,
		Uzbek_Cyrillic = 0x0843,
		Vietnamese = 0x042a,
		HID_Usage_Data_Descriptor = 0x04ff,
		HID_Vendor1 = 0xf0ff,
		HID_Vendor2 = 0xf4ff,
		HID_Vendor3 = 0xf8ff,
		HID_Vendor4 = 0xfcff,
	};

	struct DescriptorEntry
	{
		uint8_t type() const { return data[1]; }

		uint8_t index;
		std::optional<LangID> language;
		std::vector<uint8_t> data;

		template<DescriptorType T> T& decode();
	};

	class Descriptor
	{
	public:
		void add(StringId index, std::wstring_view string, LangID language = LangID::English_United_States);
		void add(std::vector<uint8_t>&& data, uint8_t index = 0);

		template<DescriptorType T>
		void add(T descriptor, uint8_t index = 0);

		StringId allocateStringIndex() { return StringId{ m_nextStringIndex++ }; }
		StringId allocateStringIndex(std::wstring_view string, LangID language = LangID::English_United_States);

		// fix numbers, counts, size. do not call in case you added final descriptors 
		void finalize();
		void changeMaxPacketSize(size_t value);

		const std::vector<DescriptorEntry>& entries() const { return m_entries; }

		DeviceDescriptor* device();
	private:
		std::vector<DescriptorEntry> m_entries;
		uint8_t m_nextStringIndex = 1;
	};



	template<DescriptorType T>
	inline void Descriptor::add(T descriptor, uint8_t index)
	{
		std::vector<uint8_t> data(sizeof(descriptor) + 2, 0);
		data[0] = (uint8_t)data.size();
		data[1] = decltype(descriptor)::TYPE;
		memcpy(&data[2], &descriptor, sizeof(descriptor));
		add(std::move(data), index);
	}

	template<DescriptorType T>
	inline T& DescriptorEntry::decode()
	{
		if(sizeof(T) != data.size() - 2)
			throw std::runtime_error("wrong descriptor size");
		if(T::TYPE != data[1])
			throw std::runtime_error("wrong descriptor type");
		return *(T*)&data[2];
	}

}
