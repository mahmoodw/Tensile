#include "Cobalt.h"
#include <stdio.h>

CobaltTensor createTensorForMatrix(
    CobaltDataType dataType,
    size_t initialStride,
    size_t dim0,
    size_t dim1,
    size_t dimBatch );
CobaltProblem createProblemGEMM(
    bool transA,
    bool transB,
    size_t M,
    size_t N,
    size_t K,
    size_t initialStride,
    size_t numBatches,
    bool alpha,
    bool beta,
    CobaltDataType dataTypeC,
    CobaltDataType dataTypeA,
    CobaltDataType dataTypeB
  );

/*******************************************************************************
 * main
 ******************************************************************************/
int main( char * argv[], int argc ) {
  // transA, transB, strideMultiple, M, N, K
  const size_t numSizes = 1;
  size_t sizes[] = {2048};
  const size_t numStrides = 1;
  size_t initialStrides[] = { 1, 2 }; // , 64 };
  const size_t numBatchSizes = 1;
  size_t batches[] = { 1, 2 };
  const size_t numDataTypes = 1;
  const CobaltDataType dataTypes[][3] = {
    { cobaltDataTypeSingle, cobaltDataTypeSingle, cobaltDataTypeSingle },
    { cobaltDataTypeDouble, cobaltDataTypeDouble, cobaltDataTypeDouble },
    
    { cobaltDataTypeComplexSingle, cobaltDataTypeComplexSingle, cobaltDataTypeComplexSingle },
    { cobaltDataTypeComplexDouble, cobaltDataTypeComplexDouble, cobaltDataTypeComplexDouble },

    { cobaltDataTypeComplexSingle, cobaltDataTypeComplexConjugateSingle, cobaltDataTypeComplexSingle },
    { cobaltDataTypeComplexSingle, cobaltDataTypeComplexSingle, cobaltDataTypeComplexConjugateSingle },
    { cobaltDataTypeComplexSingle, cobaltDataTypeComplexConjugateSingle, cobaltDataTypeComplexConjugateSingle },

    { cobaltDataTypeComplexDouble, cobaltDataTypeComplexConjugateDouble, cobaltDataTypeComplexDouble },
    { cobaltDataTypeComplexDouble, cobaltDataTypeComplexDouble, cobaltDataTypeComplexConjugateDouble },
    { cobaltDataTypeComplexDouble, cobaltDataTypeComplexConjugateDouble, cobaltDataTypeComplexConjugateDouble }
  };
  const size_t numAlphas = 2;
  const bool alphas[] = { true, false };
  const size_t numBetas = 2;
  const bool betas[] = { true, false };
  const size_t numTransA = 1;
  const bool transAs[] = {false, true};
  const size_t numTransB = 1;
  const bool transBs[] = {true, false};
  size_t numProblems = 0;
  cobaltSetup("GEMM");
  for (size_t transA = 0; transA < numTransA; transA++) {
    for (size_t transB = 0; transB < numTransB; transB++) {
      for (size_t mIdx = 0; mIdx < numSizes; mIdx++) {
        for (size_t nIdx = 0; nIdx < numSizes; nIdx++) {
          for (size_t kIdx = 0; kIdx < numSizes; kIdx++) {
            for (size_t sIdx = 0; sIdx < numStrides; sIdx++) {
              for (size_t dtIdx = 0; dtIdx < numDataTypes; dtIdx++) {
                for (size_t bIdx = 0; bIdx < numBatchSizes; bIdx++) {
                  for (size_t alphaIdx = 0; alphaIdx < numAlphas; alphaIdx++) {
                    for (size_t betaIdx = 0; betaIdx < numBetas; betaIdx++) {
                      size_t numBatches = batches[bIdx];
                      size_t M = sizes[mIdx];
                      size_t N = sizes[nIdx];
                      size_t K = sizes[kIdx];
                      if (M != N || M != K || N != K) continue;
                      size_t initStride = initialStrides[sIdx];
                      bool alpha = alphas[alphaIdx];
                      bool beta = betas[betaIdx];
                      printf("%s%s\n", transA == 1 ? "T" : "N", transB == 1 ? "T" : "N");
                      CobaltProblem problem = createProblemGEMM(
                          transAs[transA],
                          transBs[transB],
                          M, N, K,
                          initStride,
                          numBatches,
                          alpha,
                          beta,
                          dataTypes[dtIdx][0],
                          dataTypes[dtIdx][1],
                          dataTypes[dtIdx][2]
                        );
                      
                      CobaltStatus status;
                      CobaltSolution solution = cobaltGetSolutionForProblem( problem, &status );
                      numProblems++;
                    } // beta
                  } // alpha
                } // batch
              } // data type
            } // stride
          } // K
        } // N
      } // M
    } // transB
  } // transA
  printf("Num Problems: %llu\n", numProblems );
  cobaltTeardown();

  return 0;

} // main


/*******************************************************************************
 * createProblemGEMM
 ******************************************************************************/
CobaltProblem createProblemGEMM(
    bool transA,
    bool transB,
    size_t M,
    size_t N,
    size_t K,
    size_t initialStride,
    size_t numBatches,
    bool alpha,
    bool beta,
    CobaltDataType dataTypeC,
    CobaltDataType dataTypeA,
    CobaltDataType dataTypeB
  ) {

  // problem - tensor
  CobaltTensor tensorC = createTensorForMatrix(
    dataTypeC,
    initialStride,
    M,
    N,
    numBatches);
  CobaltTensor tensorA = createTensorForMatrix(
    dataTypeA,
    initialStride,
    transA ? K : M,
    transA ? M : K,
    numBatches );
  CobaltTensor tensorB = createTensorForMatrix(
    dataTypeB,
    initialStride,
    transB ? N : K,
    transB ? K : N,
    numBatches );

  // operation
  CobaltOperationType operationType = cobaltOperationTypeContraction;
  CobaltDataType alphaType = alpha ? dataTypeC : cobaltDataTypeNone;
  CobaltDataType betaType = beta ? dataTypeC : cobaltDataTypeNone;
  unsigned int indexAssignmentsA[CobaltTensor::maxDimensions];
  unsigned int indexAssignmentsB[CobaltTensor::maxDimensions];
  if (numBatches > 1) {
    indexAssignmentsA[0] = transA ? 3 : 0;
    indexAssignmentsA[1] = transA ? 0 : 3;
    indexAssignmentsA[2] = 2;
    indexAssignmentsB[0] = transB ? 1 : 3;
    indexAssignmentsB[1] = transB ? 3 : 1;
    indexAssignmentsB[2] = 2;
  } else {
    indexAssignmentsA[0] = transA ? 2 : 0;
    indexAssignmentsA[1] = transA ? 0 : 2;
    indexAssignmentsB[0] = transB ? 1 : 2;
    indexAssignmentsB[1] = transB ? 2 : 1;
  }

  // problem - device problem
  CobaltDeviceProfile deviceProfile;
  deviceProfile.numDevices = 1;
  sprintf_s(deviceProfile.devices[0].name, "Hawaii" );
  deviceProfile.devices[0].numComputeUnits = 44;
  deviceProfile.devices[0].clockFrequency = 900; // MHz


  CobaltStatus status;
  CobaltProblem problem = cobaltCreateProblem(
      tensorC,
      tensorA,
      tensorB,
      indexAssignmentsA,
      indexAssignmentsB,
      operationType,
      alphaType,
      betaType,
      deviceProfile,
      &status );
  cobaltStatusCheck(status);
  unsigned int problemStringSize;
  cobaltProblemToString(problem, nullptr, &problemStringSize);
  char *problemString = new char[problemStringSize];
  cobaltProblemToString(problem, problemString, &problemStringSize);
  printf("%s\n", problemString);


  // problem - validate
  CobaltStatus validationStatus = cobaltValidateProblem( problem );
  cobaltStatusCheck(validationStatus);
  //unsigned int strLength;
  //cobaltStatusToString(validationStatus, nullptr, &strLength);
  //char *statusStr = new char[strLength];
  //cobaltStatusToString(validationStatus, statusStr, &strLength);
  //printf("%s\n", statusStr );
  //delete[] statusStr;
  if (validationStatus != cobaltStatusSuccess) {
    cobaltValidateProblem( problem );
  }

  return problem;
}

// TODO - debug this
CobaltTensor createTensorForMatrix(
    CobaltDataType dataType,
    size_t initialStride,
    size_t dim0,
    size_t dim1,
    size_t dimBatch
    ) {
  CobaltTensor tensor;
  tensor.dataType = dataType;
  tensor.numDimensions = 2;
  tensor.dimensions[0].stride = (unsigned int)initialStride;
  tensor.dimensions[0].size = (unsigned int)dim0;
  tensor.dimensions[1].stride = (unsigned int)(tensor.dimensions[0].stride*tensor.dimensions[0].size*initialStride);
  tensor.dimensions[1].size = (unsigned int)dim1;

  if (dimBatch > 1) {
    tensor.numDimensions++;
    tensor.dimensions[2].stride = (unsigned int)(tensor.dimensions[1].stride*tensor.dimensions[1].size*initialStride);
    tensor.dimensions[2].size = (unsigned int) dimBatch;
  }
  return tensor;
}
