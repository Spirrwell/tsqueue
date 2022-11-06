/*******************************************************************************
* MIT License
*
* Copyright (c) 2021 Spirrwell
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
********************************************************************************/

#pragma once

#include "shared_spinlock.h"
#include "scoped_lock.h"

#include <cstddef>
#include <optional>
#include <utility>
#include <type_traits>
#include <new>

namespace spl
{
	template <typename T>
	class tsqueue final
	{
		shared_spinlock spinlock;

		T* storage{};
		std::size_t storage_capacity{};
		std::size_t storage_size{};

		constexpr static void destroy( T* first, const T* const last )
		{
			if constexpr ( !std::is_trivially_destructible_v<T> )
			{
				for ( ; first != last; ++first ) {
					first->~T();
				}
			}
		}

		constexpr static void destroy_at( T* object )
		{
			if constexpr ( !std::is_trivially_destructible_v<T> ) {
				object->~T();
			}
		}

		constexpr static void uninitialized_move( T* first, const T* const last, T* dest )
		{
			for ( ; first != last; ++first, ++dest ) {
				new ( dest ) T( std::move( *first ) );
			}
		}

	public:
		~tsqueue()
		{
			destroy( storage, storage + storage_size );
			::operator delete[]( storage );
		}

		std::size_t was_size()
		{
			spl::scoped_shared_lock ssl( spinlock );
			return storage_size;
		}

		std::size_t was_capacity()
		{
			spl::scoped_shared_lock ssl( spinlock );
			return storage_capacity;
		}
		
		bool was_empty() { return was_size() == 0; }

		template <typename... Args>
		void push( Args&&... args )
		{
			spl::scoped_lock sl( spinlock );
			const std::size_t new_size = storage_size + 1;
			if ( new_size > storage_capacity )
			{
				T* tmp = static_cast<T*>( ::operator new[]( sizeof(T) * new_size ) );
				uninitialized_move( storage, storage + storage_size, tmp );
				destroy( storage, storage + storage_size );
				::operator delete[]( storage );

				storage = tmp;
				storage_capacity = new_size;
			}

			if constexpr ( sizeof...( Args ) > 0 )
			{
				new( &storage[storage_size] ) T( std::forward<Args>( args )... );
				storage_size = new_size;
			}
			else
			{
				new( &storage[storage_size] ) T;
				storage_size = new_size;
			}
		}

		void pop( std::optional<T>& out )
		{
			spl::scoped_lock sl( spinlock );

			if ( storage_size == 0 ) {
				out = std::nullopt;
				return;
			}

			--storage_size;

			out.emplace( std::move( storage[storage_size] ) );
			std::destroy_at( &storage[storage_size] );
		}

		void clear()
		{
			scoped_lock sl( spinlock );

			destroy( storage, storage + storage_size );
			storage_size = 0;
		}
	};
}