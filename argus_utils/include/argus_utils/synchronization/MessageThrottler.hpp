#pragma once

#include <deque>
#include <cmath>
#include <boost/foreach.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include "argus_utils/synchronization/SynchronizationTypes.h"

namespace argus
{

/*! \brief Weighted subsampling and delaying of message streams to 
 * achieve a target message rate. 
 * // NOTE Accessing outputs is synchronized, but setting parameters is not!
 */
 template <typename Msg, typename Key = std::string>
 class MessageThrottler
 {
public:

    typedef std::pair<Key, Msg> KeyedData;

    MessageThrottler() 
    {
        SetTargetRate( 10.0 );
        SetMinRate( 0.0 );
        SetBufferLength( 10 );
    }

    void SetMinRate( double min )
    {
        if( min < 0 )
        {
            throw std::invalid_argument( "Min rate must be positive." );
        }

        _minRate = min;
        ComputeBufferRates();
    }

    void SetTargetRate( double rate )
    {
        if( rate < 0 )
        {
            throw std::invalid_argument( "Rate must be positive." );
        }
        _overallRate = rate;
        ComputeBufferRates();
    }

    void SetBufferLength( unsigned int buffLen )
    {
        if( buffLen != _bufferLen && _registry.size() > 0 )
        {
            std::cerr << "Warning: Changing buffer length does not modify existing buffers." << std::endl;
        }
        _bufferLen = buffLen;
    }

    void RegisterSource( const Key& key )
    {
        CheckStatus( key, false );
        _registry.emplace( std::piecewise_construct,
                           std::forward_as_tuple( key ), 
                           std::forward_as_tuple( _bufferLen ) );
        ComputeBufferRates();
    }

    void SetSourceWeight( const Key& key, double w )
    {
        CheckStatus( key, true );
        if( w < 0 )
        {
            throw std::invalid_argument( "Weights must be positive." );
        }
        _registry.at( key ).SetWeight( w );
        ComputeBufferRates();
    }

    void BufferData( const Key& key,
                     const Msg& m )
    {
        CheckStatus( key, true );
        _registry.at( key ).Buffer( m );
    }

    bool GetOutput( double now, KeyedData& out )
    {
        typedef typename SourceRegistry::value_type Item;

        WriteLock outlock( _mutex );

        if( _registry.size() == 0 ) { return false; }

        std::vector<Key> keys;
        std::vector<double> scores;
        BOOST_FOREACH( const Item& item, _registry )
        {
            keys.push_back( item.first );
            scores.push_back( item.second.ComputeNumToOutput( now ) );
        }
        
        // If no scores were nonzero, there are no outputs to be had!
        double maxScore = *std::max_element( scores.begin(), scores.end() );
        if( maxScore == 0 )
        {
            return false;
        }

        std::vector<Key> maxKeys;
        for( unsigned int i = 0; i < scores.size(); ++i )
        {
            if( scores[i] == maxScore ) { maxKeys.push_back( keys[i] ); }
        }

        std::string maxKey;
        if( maxKeys.size() == 1 )
        {
            maxKey = maxKeys[0];
        }
        else
        {
            boost::random::uniform_int_distribution<> tiebreak( 0, maxKeys.size()-1 );
            maxKey = maxKeys[tiebreak(_randGen)];
        }

        Msg m = _registry.at( maxKey ).PopAndMark( now );
        out = KeyedData( maxKey, m );
        return true;
    }

private:

    // Compute the bandwidth allocations for each buffer
    void ComputeBufferRates()
    {
        typedef typename SourceRegistry::value_type Item;

        double assignableRate = _overallRate - _registry.size() * _minRate;
        double effectiveMin = _minRate;
        if( assignableRate < 0 )
        {
            std::cerr << "Warning: min rate " << _minRate << " with " << _registry.size()
                      << " sources exceeds overall rate " << _overallRate << std::endl;
            effectiveMin = _overallRate / _registry.size();
            assignableRate = 0;
        }

        double sumWeights = 0;
        BOOST_FOREACH( const Item& item, _registry )
        {
            sumWeights += item.second.weight;
        }
        if( sumWeights == 0 ) { sumWeights = 1.0; }
        BOOST_FOREACH( Item& item, _registry )
        {
            SourceRegistration& reg = item.second;
            reg.rate = assignableRate * reg.weight / sumWeights + effectiveMin;
        }
    }

    void CheckStatus( const std::string& key, bool expect_reg )
    {
        bool is_reg = _registry.count( key ) > 0;
        if( expect_reg != is_reg )
        {
            std::stringstream ss;
            ss << "Source: " << key << (expect_reg ? " not registered!" : " already_registered");
            throw std::invalid_argument( ss.str() );
        }
    }

    // TODO Clean up public/private here
    struct SourceRegistration
    {
        mutable Mutex mutex;
        boost::circular_buffer<Msg> buffer;
        double weight;
        double rate;
        double lastOutputTime;

        SourceRegistration( unsigned int len )
        : mutex(), buffer( len ), weight( 0 ), rate( 0 ),
          lastOutputTime( -std::numeric_limits<double>::infinity() ) {}

        double ComputeNumToOutput( double now ) const
        {
            ReadLock lock( mutex );
            
            double elapsed = now - lastOutputTime;
            if( elapsed < 0 )
            {
                return 0;
                //throw std::runtime_error( "Negative time elapsed!" );
            }
            double maxOutput = elapsed * rate;
            double numBuffered = double( buffer.size() );
            return std::floor( std::min( maxOutput, numBuffered ) );
        }

        void SetWeight( double w )
        {
            WriteLock lock( mutex );
            weight = w;
        }

        void Buffer( const Msg& m )
        {
            WriteLock lock( mutex );
            buffer.push_back( m );
        }

        Msg PopAndMark( double now )
        {
            WriteLock lock( mutex );

            lastOutputTime = now;
            Msg m = buffer.front();
            buffer.pop_front();
            return m;
        }
    };

    mutable Mutex _mutex;

    boost::mt19937 _randGen;

    typedef std::map<Key, SourceRegistration> SourceRegistry;
    SourceRegistry _registry;

    double _lastOutput;
    std::deque<KeyedData> _outputBuffer;

    // Parameters
    unsigned int _bufferLen;
    double _overallRate;
    double _minRate;
 };

}