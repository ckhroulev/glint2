/*
 * GLINT2: A Coupling Library for Ice Models and GCMs
 * Copyright (c) 2013 by Robert Fischer
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

///////////////////////////////////////////////////////////////////////////////
// bitfield.hpp: defines the bitfield_base type
//
// Copyright 2005 Frank Laub
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ENUM_BITFIELD_HPP
#define BOOST_ENUM_BITFIELD_HPP

// MS compatible compilers support #pragma once
#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <sstream>
#include <boost/operators.hpp>

namespace boost {
namespace detail {

	// Befriending Templates requires the need for all this mess.
	// So that we can allow the templated ostream insertion operator to access private members.
	template <typename T>
	class bitfield_base;

	template <typename T>
	std::ostream& operator << (std::ostream& os, const bitfield_base<T>& value);

	class bitfield_access
	{
# if defined(BOOST_NO_MEMBER_TEMPLATE_FRIENDS) \
	|| BOOST_WORKAROUND(__BORLANDC__, BOOST_TESTED_AT(0x551))
	// Tasteless as this may seem, making all members public allows member templates
	// to work in the absence of member template friends.
	public:
# else
		template <typename T>
		friend class bitfield_base;

		template <typename T>
		friend std::ostream& operator << (std::ostream& os, const bitfield_base<T>& value);
#endif

		template <typename T>
		static const char* names(BOOST_DEDUCED_TYPENAME T::domain index)
		{
			return T::names(index);
		}

		template <typename T>
		static BOOST_DEDUCED_TYPENAME T::optional_value values(
			BOOST_DEDUCED_TYPENAME T::domain index)
		{
			return T::values(index);
		}

		template <typename T>
		static T get_by_value(BOOST_DEDUCED_TYPENAME T::value_type value)
		{
			T ret(value, 0);
			return ret;
		}

	private:
		// objects of this class are useless
		bitfield_access(); //undefined
	};

	template <typename T>
	class bitfield_base 
		: private boost::bitwise<T>
		, private boost::totally_ordered<T>
	{
	public:
		typedef bitfield_base<T> this_type;
		typedef size_t index_type;
		typedef size_t value_type;
		typedef enum_iterator<T> const_iterator;
		typedef boost::optional<T> optional;

	protected:
		bitfield_base(value_type value, int) : m_value(value) {}

	public:
		bitfield_base() : m_value(0) {}
		bitfield_base(index_type index)
		{
			optional_value value = bitfield_access::values<T>(enum_cast<T>(index));
			if(value)
				m_value = *value;
		}

		static const_iterator begin()
		{
			return const_iterator(0);
		}

		static const_iterator end()
		{
			return const_iterator(T::size);
		}

		static optional get_by_value(value_type value)
		{
			// make sure that 'value' is valid
			optional_value not_mask = bitfield_access::values<T>(T::not_mask);
			BOOST_ASSERT(not_mask);
			if(value & *not_mask)
				return optional();
			return bitfield_access::get_by_value<T>(value);
		}

		static optional get_by_index(index_type index)
		{
			if(index >= T::size) return optional();
			return optional(enum_cast<T>(index));
		}

		std::string str() const
		{
			std::stringstream ss;
			ss << *this;
			return ss.str();
		}

		value_type value() const
		{
			return m_value;
		}

		bool operator == (const this_type& rhs) const
		{
			return m_value == rhs.m_value;
		}

		bool operator < (const this_type& rhs) const
		{
			return m_value < rhs.m_value;
		}

		T& operator |= (const this_type& rhs) 
		{
			m_value |= rhs.m_value;
			return static_cast<T&>(*this);
		}

		T& operator &= (const this_type& rhs)
		{
			m_value &= rhs.m_value;
			return static_cast<T&>(*this);
		}

		T& operator ^= (const this_type& rhs)
		{
			m_value ^= rhs.m_value;
			return static_cast<T&>(*this);
		}

		bool operator[] (index_type pos) const
		{
			optional element = get_by_index(pos);
			if(!element) return false;
			return operator[](*element);
		}

		bool operator[] (const this_type& rhs) const
		{
			return (m_value & rhs.m_value) != 0;
		}

		bool set(index_type pos, bool bit = true)
		{
			if(!bit) return reset(pos);
			optional element = get_by_index(pos);
			if(!element) return false;
			return set(*element, bit);
		}

		bool set(const this_type& rhs, bool bit = true)
		{
			if(!bit) return reset(rhs);
			value_type new_value = m_value | rhs.m_value;
			if(!get_by_value(new_value))
				return false;

			m_value = new_value;
			return true;
		}

		bool reset(index_type pos)
		{
			optional element = get_by_index(pos);
			if(!element) return false;
			return reset(*element);
		}

		bool reset(const this_type& rhs)
		{
			value_type new_value = m_value & ~(rhs.m_value);
			if(!get_by_value(new_value)) return false;
			m_value = new_value;
			return true;
		}

		// TODO: implement me
		size_t count() const
		{
			return 0;
		}

		// TODO: implement me
		bool any() const
		{
			return false;
		}

		// TODO: implement me
		bool none() const
		{
			return false;
		}

	private:
		typedef boost::optional<value_type> optional_value;
		friend class bitfield_access;
		value_type m_value;
	};

	template <typename T>
	std::ostream& operator << (std::ostream& os, const bitfield_base<T>& rhs)
	{
		typedef BOOST_DEDUCED_TYPENAME T::value_type value_type;
		typedef BOOST_DEDUCED_TYPENAME T::index_type index_type;
		typedef boost::optional<value_type> optional_value;

		value_type remain = rhs.value();
		optional_value all_mask = bitfield_access::values<T>(T::all_mask);
		if(remain == *all_mask)
		{
			os << "all_mask";
			return os;
		}

		optional_value not_mask = bitfield_access::values<T>(T::not_mask);
		if(remain == *not_mask)
		{
			os << "not_mask";
			return os;
		}
		// FIXME: there might be a reason the user wants to define the value 0
		//        or perhaps 0 is never legitimate for their usage
		bool isZero = (remain == 0);

		bool isFirst = true;
		for(index_type i = 0; i < T::size; ++i)
		{
			optional_value mask = bitfield_access::values<T>(enum_cast<T>(i));
			if(*mask == 0 && isZero)
			{
				const char* name = bitfield_access::names<T>(enum_cast<T>(i));
				BOOST_ASSERT(name);
				os << name;
				return os;
			}
			else if(remain & *mask)
			{
				if(isFirst)
					isFirst = false;
				else
					os << '|';

				const char* name = bitfield_access::names<T>(enum_cast<T>(i));
				BOOST_ASSERT(name);
				os << name;
				remain &= ~(*mask);
				if(remain == 0)
					return os;
			}
		}
		if(remain)
		{
			if(!isFirst)
				os << '|';
			os.fill('0');
			os.width(8);
			os << std::hex << remain;
		}
		else if(isZero)
		{
			os << "<null>";
		}
		return os;
	}

	// TODO: this should be symmetrical to operator <<
	//       it needs to be able to take a string like A|B and
	//       return a value that represents A|B.
	template <typename T>
	std::istream& operator >> (std::istream& is, bitfield_base<T>& rhs)
	{
		std::string str;
		is >> str;
		BOOST_DEDUCED_TYPENAME T::optional ret = T::get_by_name(str.c_str());
		if(ret)
			rhs = *ret;
		else
			is.setstate(std::ios::badbit);
		return is;
	}

} // detail
} // boost

#endif
