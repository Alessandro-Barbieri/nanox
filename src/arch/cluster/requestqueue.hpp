#ifndef REQUESTQUEUE_H
#define REQUESTQUEUE_H

#include "requestqueue_decl.hpp"
#include "atomic.hpp"

template <class T>
RequestQueue<T>::RequestQueue() : _queue(), _lock() {
}

template <class T>
RequestQueue<T>::~RequestQueue() {
}

template <class T>
void RequestQueue<T>::add( T *elem ) {
   _lock.acquire();
   _queue.push_back( elem );
   _lock.release();
}

template <class T>
T *RequestQueue<T>::fetch() {
   T *elem = NULL;
   _lock.acquire();
   //if ( !_queue.empty() ) {
   //   for ( std::list<T *>::iterator it = _delayedPutReqs.begin(); putReqsIt != _delayedPutReqs.end(); putReqsIt++ ) {
   //      if ( (*putReqsIt)->origAddr == destAddr ) {
   //         _putReqsLock.acquire();
   //         _putReqs.push_back( *putReqsIt );
   //         _putReqsLock.release();
   //      }
   //   }
   //}
   _lock.release();
}

template <class T>
T *RequestQueue<T>::tryFetch() {
   T *elem = NULL;
   if ( _lock.tryAcquire() ) {
      if ( !_queue.empty() ) {
         elem = _queue.front();
         _queue.pop_front();
      }
      _lock.release();
   }
   return elem;
}

template <class T>
RequestMap<T>::RequestMap() : _map(), _lock() {
}

template <class T>
RequestMap<T>::~RequestMap() {
}
template <class T>
void RequestMap<T>::add( uint64_t key, T *elem ) {
   _lock.acquire();
   typename std::map< uint64_t, T * >::iterator it = _map.lower_bound( key );
   if ( it != _map.end() || _map.key_comp()( key, it->first ) ) {
      _map.insert( it, std::map< uint64_t, T * >::value_type( key, elem ) );
   } else { 
      std::cerr << "Error, key already exists." << std::endl;
   }
   _lock.release();
}

template <class T>
T *RequestMap<T>::fetch( uint64_t key ) {
   T *elem = NULL;
   _lock.acquire();
   typename std::map< uint64_t, T * >::iterator it = _map.lower_bound( key );
   if ( it != _map.end() || _map.key_comp()( key, it->first ) ) {
      std::cerr << "Error, key not found." << std::endl;
   } else {
      elem = it->second;
   }
   _lock.release();
}

#endif /* REQUESTQUEUE_H */
