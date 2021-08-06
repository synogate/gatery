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

#include "../frontend/BitWidth.h"

namespace gtry::utils {

class ConfigTree {
    public:
        template<typename type, const char **names, size_t count>
        class EnumTranslator 
        {
            public:
                typedef std::string YamlType;
                typedef type Type;

                inline Type operator()(const YamlType &v) {
                    for (size_t i = 0; i < count; i++)
                        if (v == names[i]) return (type)i;
                    HCL_DESIGNCHECK_HINT(false, "Invalid enum value: " + v);
                }
                inline YamlType operator()(Type v) {
                    return std::string(names[(unsigned)v]);
                }
        };

        struct IntTranslator {
            typedef int YamlType;
            typedef int Type;

            inline int operator()(int v) { return v; }
        };
        struct SizeTTranslator {
            typedef size_t YamlType;
            typedef size_t Type;

            inline size_t operator()(size_t v) { return v; }
        };
        struct BoolTranslator {
            typedef bool YamlType;
            typedef bool Type;

            inline bool operator()(bool v) { return v; }
        };
        struct StringTranslator {
            typedef std::string YamlType;
            typedef std::string Type;

            inline std::string operator()(std::string v) { return std::move(v); }
        };
        struct BitWidthTranslator {
            typedef uint64_t YamlType;
            typedef BitWidth Type;

            inline Type operator()(YamlType v) { return BitWidth(v); }
            inline YamlType operator()(Type v) { return v.bits(); }
        };

        ConfigTree operator[](const std::string &name) {
            return m_node[name];
        }

        inline bool contains(const std::string &name) {
            return m_node[name];
        }

        inline bool isSequence() { return m_node.IsSequence(); }

        struct iterator {
            YAML::Node::iterator it;

            void operator++() { ++it; }
            ConfigTree operator*() { return ConfigTree(*it); }
            bool operator==(const iterator &rhs) const { return it == rhs.it; }
            bool operator!=(const iterator &rhs) const { return it == rhs.it; }
        };

        inline auto begin() { return iterator{m_node.begin()}; }
        inline auto end() { return iterator{m_node.end()}; }

        int get(const std::string &name, int def) { return get(name, def, IntTranslator()); }
        size_t get(const std::string &name, size_t def) { return get(name, def, SizeTTranslator()); }
        bool get(const std::string &name, bool def) { return get(name, def, BoolTranslator()); }
        BitWidth get(const std::string &name, BitWidth def) { return get(name, def, BitWidthTranslator()); }
        std::string get(const std::string &name, const std::string &def) { return get(name, def, StringTranslator()); }
        std::string get(const std::string &name, const char *def) { return get(name, def, StringTranslator()); }

        template<typename T>
        typename T::Type get(const std::string &name, const typename T::Type &def, T translator) {
            if (m_node[name]) {
                return translator(m_node[name].as<typename T::YamlType>());
            } else {
                m_node[name] = translator(def);
                return def;
            }
        }


        void loadFromFile(const std::filesystem::path &filename);
        void saveToFile(const std::filesystem::path &filename);

        ConfigTree() = default;
    protected:
        ConfigTree(YAML::Node node) : m_node(node) { }
        YAML::Node m_node;
};

}
