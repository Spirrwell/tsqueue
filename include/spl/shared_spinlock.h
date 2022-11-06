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

#include <atomic>
#include <cstdint>
#include <limits>

namespace spl
{
	class shared_spinlock final
	{
		std::atomic<uint32_t> atomic_lock{};
		std::atomic_flag write_pending{};

		static constexpr uint32_t WRITE_MODE = (std::numeric_limits<uint32_t>::max)();
		static constexpr uint32_t READ_MAX = WRITE_MODE - 1;

	public:
		bool try_lock()
		{
			uint32_t was = atomic_lock;

			if ( was == 0 ) {
				return atomic_lock.compare_exchange_strong( was, WRITE_MODE );
			}

			return false;
		}

		bool try_lock_shared()
		{
			uint32_t was = atomic_lock;

			if ( was == 0 && write_pending.test() ) {
				return false;
			}

			if ( was < READ_MAX ) {
				return atomic_lock.compare_exchange_strong( was, was + 1 );
			}

			return false;
		}

		void lock_shared()
		{
			while ( true )
			{
				uint32_t was = atomic_lock;

				if ( was == 0 && write_pending.test() ) {
					continue;
				}

				if ( was < READ_MAX ) {
					if ( atomic_lock.compare_exchange_weak( was, was + 1 ) ) {
						return;
					}
				}
			}
		}

		void unlock_shared()
		{
			while ( true )
			{
				uint32_t was = atomic_lock;

				if ( was == 0 ) {
					return;
				}

				if ( was <= READ_MAX ) {
					if ( atomic_lock.compare_exchange_weak( was, was - 1 ) ) {
						return;
					}
				}
			}
		}

		void lock()
		{
			write_pending.test_and_set();

			while ( true )
			{
				uint32_t was = atomic_lock;

				if ( was == 0 ) {
					if ( atomic_lock.compare_exchange_weak( was, WRITE_MODE ) ) {
						return;
					}
				}
			}
		}

		void unlock()
		{
			write_pending.clear();

			while ( true )
			{
				uint32_t was = atomic_lock;

				if ( was != WRITE_MODE ) {
					return;
				}

				if ( atomic_lock.compare_exchange_weak( was, 0 ) ) {
					return;
				}
			}
		}
	};
}