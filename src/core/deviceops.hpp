#ifndef DEVICEOPS_HPP
#define DEVICEOPS_HPP

#include <iostream>

#include "deviceops_decl.hpp"
#include "atomic.hpp"

using namespace nanos;

inline DeviceOpsPtr::DeviceOpsPtr( DeviceOpsPtr const &p ) {
   DeviceOps *tmpValue = p._value;
   _value = NULL;
   if ( tmpValue != NULL ) {
      if ( tmpValue->addRef( this, const_cast<DeviceOpsPtr &>( p ) ) )
         _value = tmpValue;
   }
}

inline DeviceOpsPtr::DeviceOpsPtr( DeviceOpsPtr &p ) {
   DeviceOps *tmpValue = p._value;
   _value = NULL;
   if ( tmpValue != NULL ) {
      if ( tmpValue->addRef( this, p ) )
         _value = tmpValue;
   }
}

inline DeviceOpsPtr::~DeviceOpsPtr() {
   if ( _value != NULL)  {
      _value->delRef( this );
      //sys.printBt();
      //std::cerr << __FUNCTION__ << " maybe I should delete my ref."<< std::endl;
   }
}

inline DeviceOpsPtr & DeviceOpsPtr::operator=( DeviceOpsPtr const &p ) {
   DeviceOps *tmpValue = p._value;
   _value = NULL;
   if ( tmpValue != NULL ) {
      if ( tmpValue->addRef( this, const_cast<DeviceOpsPtr &>( p ) ) )
         _value = tmpValue;
   }
   return *this;
}

inline DeviceOpsPtr & DeviceOpsPtr::operator=( DeviceOpsPtr &p ) {
   DeviceOps *tmpValue = p._value;
   _value = NULL;
   if ( tmpValue != NULL ) {
      if ( tmpValue->addRef( this, p ) )
         _value = tmpValue;
   }
   return *this;
}

inline DeviceOps & DeviceOpsPtr::operator*() const {
   return *_value;
}

inline DeviceOps * DeviceOpsPtr::operator->() const {
   return _value;
}

inline void DeviceOpsPtr::set( DeviceOps *ops ) {
   _value = ops;
   _value->addFirstRef( this );
}

inline DeviceOps *DeviceOpsPtr::get() const {
   return _value;
}

inline void DeviceOpsPtr::clear() {
   _value = NULL;
}

inline bool DeviceOpsPtr::isNotSet() const {
   return _value == NULL;
}

inline DeviceOps::DeviceOps() : _pendingDeviceOps ( 0 ) {
}

inline DeviceOps::~DeviceOps() {
}

inline unsigned int DeviceOps::getNumOps() const {
   return _pendingDeviceOps.value();
}

inline void DeviceOps::addOp() {
   _pendingDeviceOps++;
}

inline bool DeviceOps::allCompleted() const {
   return ( _pendingDeviceOps.value() == 0);
}

inline void DeviceOps::delRef( DeviceOpsPtr *opsPtr ) {
  _lock.acquire();
  _refs.erase( opsPtr );
  _lock.release();
}

inline void DeviceOps::addFirstRef( DeviceOpsPtr *opsPtr ) {
   _lock.acquire();
   _refs.insert( opsPtr );
   _lock.release();
}

#endif /* DEVICEOPS_HPP */
