/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

enum class resource_state {
    none,
    loading,
    good,
    failed
};

template <typename T>
struct copyable_atomic
{
  std::atomic<T> _a;

  copyable_atomic(T t) : _a(t)
  {}

  copyable_atomic(const std::atomic<T> &a)
    :_a(a.load())
  {}

  copyable_atomic(const copyable_atomic &other)
    :_a(other._a.load())
  {}

  operator T() const {
	  return _a.load();
  }

  copyable_atomic &operator=(const copyable_atomic &other)
  {
	_a.store(other._a.load());
	return *this;
  }
};

using resource_timestamp = copyable_atomic<int64_t>;

// takes containers providing access to specific element through operator[]
// with elements of std::pair<resource *, resource_timestamp>
// the element should provide method release() freeing resources owned by the element
template <class Container_>
class garbage_collector {

public:
// constructor:
    garbage_collector( Container_ &Container, unsigned int const Secondstolive, std::size_t const Sweepsize, std::string const Resourcename = "resource" ) :
	    m_unusedresourcetimetolive{ std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::seconds{ Secondstolive }).count() },
        m_unusedresourcesweepsize{ Sweepsize },
        m_resourcename{ Resourcename },
        m_container{ Container }
    {}

// methods:
    // performs resource sweep. returns: number of released resources
    int
        sweep() {
		    m_resourcetimestamp = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()).time_since_epoch().count();
            // garbage collection sweep is limited to a number of records per call, to reduce impact on framerate
            auto const sweeplastindex =
                std::min(
                    m_resourcesweepindex + m_unusedresourcesweepsize,
                    m_container.size() );
			int64_t const blanktimestamp { 0 };
            int releasecount{ 0 };
            for( auto resourceindex = m_resourcesweepindex; resourceindex < sweeplastindex; ++resourceindex ) {
				if( ( m_container[ resourceindex ].second != 0 )
				 && ( m_resourcetimestamp - m_container[ resourceindex ].second > m_unusedresourcetimetolive ) ) {

                    m_container[ resourceindex ].first->release();
                    m_container[ resourceindex ].second = blanktimestamp;
                    ++releasecount;
                }
            }
#if GC_LOG_INFO
            if( releasecount ) {
                WriteLog( "Resource garbage sweep released " + std::to_string( releasecount ) + " " + ( releasecount == 1 ? m_resourcename : m_resourcename + "s" ) );
            }
#endif
            m_resourcesweepindex = (
                m_resourcesweepindex + m_unusedresourcesweepsize >= m_container.size() ?
                    0 : // if the next sweep chunk is beyond actual data, so start anew
                    m_resourcesweepindex + m_unusedresourcesweepsize );
    
            return releasecount; }

	int64_t
        timestamp() const {
            return m_resourcetimestamp; }

private:
// members:
	int64_t const m_unusedresourcetimetolive;
    typename Container_::size_type const m_unusedresourcesweepsize;
    std::string const m_resourcename;
    Container_ &m_container;
    typename Container_::size_type m_resourcesweepindex { 0 };
	int64_t m_resourcetimestamp { std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()).time_since_epoch().count() };
};
