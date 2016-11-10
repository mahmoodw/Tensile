/*******************************************************************************
 * Copyright (C) 2016 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************/


#include "StructOperations.h"
#include "Solution.h"
#include <assert.h>
#include <string>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <limits>
#include <cmath>

namespace Tensile {

#define TENTILE_ENUM_TO_STRING_CASE(X) case X: return #X;
std::string toString( TensileStatus status ) {
  switch( status ) {

    // success
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusSuccess )
  
  // tensor errors
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusTensorNumDimensionsInvalid )
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusTensorDimensionOrderInvalid )
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusTensorDimensionStrideInvalid )
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusTensorDimensionSizeInvalid )
  
  // operation errors
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusOperandNumDimensionsMismatch )
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusOperationOperandNumIndicesMismatch )
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusOperationIndexAssignmentInvalidA )
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusOperationIndexAssignmentInvalidB )
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusOperationIndexAssignmentDuplicateA )
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusOperationIndexAssignmentDuplicateB )
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusOperationNumFreeIndicesInvalid )
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusOperationNumSummationIndicesInvalid )
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusOperationIndexUnassigned )
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusOperationSummationIndexAssignmentsInvalid )

  /* tensileGetSolution() */
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusDeviceProfileNumDevicesInvalid )
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusDeviceProfileNotSupported)
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusProblemNotSupported )

  /* control errors */
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusControlInvalid )

  /* misc */
  TENTILE_ENUM_TO_STRING_CASE( tensileStatusInvalidParameter )


  // causes clang warning
  //default:
  //  return "Error in toString(TensileStatus): no switch case for: "
  //      + std::to_string(status);
  };
}

std::string toString( TensileDataType dataType ) {
  switch( dataType ) {
    TENTILE_ENUM_TO_STRING_CASE( tensileDataTypeSingle )
    TENTILE_ENUM_TO_STRING_CASE( tensileDataTypeDouble )
    TENTILE_ENUM_TO_STRING_CASE( tensileDataTypeComplexSingle )
    TENTILE_ENUM_TO_STRING_CASE( tensileDataTypeComplexDouble )
    TENTILE_ENUM_TO_STRING_CASE( tensileDataTypeComplexConjugateSingle)
    TENTILE_ENUM_TO_STRING_CASE( tensileDataTypeComplexConjugateDouble)
    TENTILE_ENUM_TO_STRING_CASE( tensileDataTypeNone )
    TENTILE_ENUM_TO_STRING_CASE( tensileNumDataTypes )
#ifdef Tensile_ENABLE_FP16
    TENTILE_ENUM_TO_STRING_CASE( tensileDataTypeHalf )
    TENTILE_ENUM_TO_STRING_CASE( tensileDataTypeComplexHalf )
    TENTILE_ENUM_TO_STRING_CASE( tensileDataTypeComplexConjugateHalf)
#endif
  //default:
  //  return "Error in toString(TensileDataType): no switch case for: "
  //      + std::to_string(dataType);
  };
}

std::string toString( TensileOperationType type ) {
  switch( type ) {
    TENTILE_ENUM_TO_STRING_CASE( tensileOperationTypeContraction )
    TENTILE_ENUM_TO_STRING_CASE( tensileOperationTypeConvolution )
  //default:
  //  return "Error in toString(TensileDataType): no switch case for: "
  //      + std::to_string(type);
  };
}


template<>
std::string tensorElementToString<float> ( float element ) {
  std::ostringstream state;
  state.precision(3);
  state << std::scientific << element;
  return state.str();
}
template<>
std::string tensorElementToString<double> ( double element ) {
  std::ostringstream state;
  state.precision(3);
  state << std::scientific << element;
  return state.str();
}
template<>
std::string tensorElementToString<TensileComplexFloat> ( TensileComplexFloat element ) {
  std::ostringstream state;
  state.precision(3);
  state << std::scientific << element.x << ", " << element.y;
  return state.str();
}
template<>
std::string tensorElementToString<TensileComplexDouble> ( TensileComplexDouble element ) {
  std::ostringstream state;
  state.precision(3);
  state << std::scientific << element.x << ", " << element.y;
  return state.str();
}


template<>
std::ostream& appendElement<float>(std::ostream& os, const float& element) {
  os << element;
  return os;
}
template<>
std::ostream& appendElement<double>(std::ostream& os, const double& element) {
  os << element;
  return os;
}
template<>
std::ostream& appendElement<TensileComplexFloat>(std::ostream& os, const TensileComplexFloat& element) {
  os << element.x << "," << element.y;
  return os;
}
template<>
std::ostream& appendElement<TensileComplexDouble>(std::ostream& os, const TensileComplexDouble& element) {
  os << element.x << "," << element.y;
  return os;
}


std::string toStringXML( const Tensile::Solution *solution, size_t indentLevel ) {
  return solution->toString(indentLevel);
}

// get size of TensileDataType
size_t sizeOf( TensileDataType type ) {
  switch( type ) {
  case tensileDataTypeSingle:
    return sizeof(float);
  case tensileDataTypeDouble:
    return sizeof(double);
  case tensileDataTypeComplexSingle:
    return sizeof(TensileComplexFloat);
  case tensileDataTypeComplexDouble:
    return sizeof(TensileComplexDouble);
  case tensileDataTypeComplexConjugateSingle:
    return sizeof(TensileComplexFloat);
  case tensileDataTypeComplexConjugateDouble:
    return sizeof(TensileComplexDouble);
#ifdef Tensile_ENABLE_FP16
  case tensileDataTypeHalf:
    return 2;
  case tensileDataTypeComplexHalf:
    return 4;
  case tensileDataTypeComplexConjugateHalf:
    return 4;
#endif
  case tensileNumDataTypes:
    return 0;
  case tensileDataTypeNone:
    return 0;
  //default:
  //  return -1;
  }
}

size_t flopsPerMadd( TensileDataType type ) {
  switch( type ) {
  case tensileDataTypeSingle:
  case tensileDataTypeDouble:
    return 2;

  case tensileDataTypeComplexSingle:
  case tensileDataTypeComplexDouble:
  case tensileDataTypeComplexConjugateSingle:
  case tensileDataTypeComplexConjugateDouble:
    return 8;

  case tensileDataTypeNone:
  case tensileNumDataTypes:
    return 0;
  }
}





} // namespace

  // TensileDimension
bool operator<(const TensileDimension & l, const TensileDimension & r) {

  if (l.stride > r.stride) {
    return true;
  } else if (r.stride > l.stride) {
    return false;
  }
  if (l.size > r.size) {
    return true;
  } else if (r.size > l.size) {
    return false;
  }
  // identical
  return false;
}

// TensileControl
bool operator< (const TensileControl & l, const TensileControl & r) {
  if (l.validate < r.validate) {
    return true;
  } else if (r.validate < l.validate) {
    return false;
  }
  if (l.benchmark < r.benchmark) {
    return true;
  } else if (r.benchmark < l.benchmark) {
    return false;
  }
#if Tensile_BACKEND_OPENCL12
  if (l.numQueues < r.numQueues) {
    return true;
  } else if (r.numQueues < l.numQueues) {
    return false;
  }
  for (unsigned int i = 0; i < l.numQueues; i++) {
    if (l.queues[i] < r.queues[i]) {
      return true;
    } else if (r.queues[i] < l.queues[i]) {
      return false;
    }
  }
#endif
  // identical
  return false;
}

bool operator==(const TensileDimension & l, const TensileDimension & r) {
  return l.size == r.size && l.stride == r.stride;
}

bool operator==(const TensileComplexFloat & l, const TensileComplexFloat & r) {
  return std::fabs(l.x - r.x) < std::numeric_limits<float>::epsilon()
      && std::fabs(l.y - r.y) < std::numeric_limits<float>::epsilon();
}
bool operator==(const TensileComplexDouble & l, const TensileComplexDouble & r) {
  return std::fabs(l.x - r.x) < std::numeric_limits<double>::epsilon()
      && std::fabs(l.y - r.y) < std::numeric_limits<double>::epsilon();
}
