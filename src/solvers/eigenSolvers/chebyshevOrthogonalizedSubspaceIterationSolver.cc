// ---------------------------------------------------------------------
//
// Copyright (c) 2017-2018 The Regents of the University of Michigan and DFT-FE authors.
//
// This file is part of the DFT-FE code.
//
// The DFT-FE code is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE at
// the top level of the DFT-FE distribution.
//
// ---------------------------------------------------------------------
//
// @author Phani Motamarri, Sambit Das

#include <chebyshevOrthogonalizedSubspaceIterationSolver.h>
#include <linearAlgebraOperations.h>
#include <vectorUtilities.h>
#include <dftUtils.h>


namespace dftfe{

  namespace internal
  {
      unsigned int setChebyshevOrder(const unsigned int upperBoundUnwantedSpectrum)
      {
	unsigned int chebyshevOrder;
	if(upperBoundUnwantedSpectrum <= 500)
	  chebyshevOrder = 24;
	else if(upperBoundUnwantedSpectrum > 500  && upperBoundUnwantedSpectrum <= 1000)
	  chebyshevOrder = 30;
	else if(upperBoundUnwantedSpectrum > 1000 && upperBoundUnwantedSpectrum <= 1500)
          chebyshevOrder = 39;
        else if(upperBoundUnwantedSpectrum > 1500 && upperBoundUnwantedSpectrum <= 2000)
	  chebyshevOrder = 43;
	else if(upperBoundUnwantedSpectrum > 2000 && upperBoundUnwantedSpectrum <= 3000)
	  chebyshevOrder = 50;
	else if(upperBoundUnwantedSpectrum > 3000 && upperBoundUnwantedSpectrum <= 4000)
	  chebyshevOrder = 58;
        else if(upperBoundUnwantedSpectrum > 4000 && upperBoundUnwantedSpectrum <= 5000)
          chebyshevOrder = 65;
	else if(upperBoundUnwantedSpectrum > 5000 && upperBoundUnwantedSpectrum <= 9000)
	  chebyshevOrder = 73;
	else if(upperBoundUnwantedSpectrum > 9000 && upperBoundUnwantedSpectrum <= 14000)
	  chebyshevOrder = 100;
	else if(upperBoundUnwantedSpectrum > 14000 && upperBoundUnwantedSpectrum <= 20000)
	  chebyshevOrder = 115;
	else if(upperBoundUnwantedSpectrum > 20000 && upperBoundUnwantedSpectrum <= 30000)
	  chebyshevOrder = 162;
	else if(upperBoundUnwantedSpectrum > 30000 && upperBoundUnwantedSpectrum <= 50000)
	  chebyshevOrder = 300;
	else if(upperBoundUnwantedSpectrum > 50000 && upperBoundUnwantedSpectrum <= 80000)
	  chebyshevOrder = 450;
	else if(upperBoundUnwantedSpectrum > 80000 && upperBoundUnwantedSpectrum <= 1e5)
	  chebyshevOrder = 550;
	else if(upperBoundUnwantedSpectrum > 1e5 && upperBoundUnwantedSpectrum <= 2e5)
	  chebyshevOrder = 700;
	else if(upperBoundUnwantedSpectrum > 2e5 && upperBoundUnwantedSpectrum <= 5e5)
	  chebyshevOrder = 1000;
	else if(upperBoundUnwantedSpectrum > 5e5)
	  chebyshevOrder = 1250;

	return chebyshevOrder;
      }
  }

  //
  // Constructor.
  //
  chebyshevOrthogonalizedSubspaceIterationSolver::chebyshevOrthogonalizedSubspaceIterationSolver
  (const MPI_Comm &mpi_comm,
   double lowerBoundWantedSpectrum,
   double lowerBoundUnWantedSpectrum):
    d_lowerBoundWantedSpectrum(lowerBoundWantedSpectrum),
    d_lowerBoundUnWantedSpectrum(lowerBoundUnWantedSpectrum),
    pcout(std::cout, (dealii::Utilities::MPI::this_mpi_process(MPI_COMM_WORLD) == 0)),
    computing_timer(mpi_comm,
	            pcout,
		    dftParameters::reproducible_output ||
		    dftParameters::verbosity<4? dealii::TimerOutput::never : dealii::TimerOutput::summary,
		    dealii::TimerOutput::wall_times)
  {

  }

  //
  // Destructor.
  //
  chebyshevOrthogonalizedSubspaceIterationSolver::~chebyshevOrthogonalizedSubspaceIterationSolver()
  {

    //
    //
    //
    return;

  }

  //
  //reinitialize spectrum bounds
  //
  void
  chebyshevOrthogonalizedSubspaceIterationSolver::reinitSpectrumBounds(double lowerBoundWantedSpectrum,
								       double lowerBoundUnWantedSpectrum)
  {
    d_lowerBoundWantedSpectrum = lowerBoundWantedSpectrum;
    d_lowerBoundUnWantedSpectrum = lowerBoundUnWantedSpectrum;
  }


  //
  // solve
  //
  eigenSolverClass::ReturnValueType
  chebyshevOrthogonalizedSubspaceIterationSolver::solve(operatorDFTClass  & operatorMatrix,
							std::vector<dataTypes::number> & eigenVectorsFlattened,
							std::vector<dataTypes::number> & eigenVectorsRotFracDensityFlattened,
							vectorType  & tempEigenVec,
							const unsigned int totalNumberWaveFunctions,
							std::vector<double>        & eigenValues,
							std::vector<double>        & residualNorms,
							const MPI_Comm &interBandGroupComm,
							const bool useMixedPrec,
                                                        const bool isFirstScf,
							const bool useFullMassMatrixGEP)
  {


    if (dftParameters::verbosity>=4)
      dftUtils::printCurrentMemoryUsage(operatorMatrix.getMPICommunicator(),
					"Before Lanczos k-step upper Bound");

    computing_timer.enter_section("Lanczos k-step Upper Bound");
    operatorMatrix.reinit(1);
    const double upperBoundUnwantedSpectrum = linearAlgebraOperations::lanczosUpperBoundEigenSpectrum(operatorMatrix,
												      tempEigenVec);
    computing_timer.exit_section("Lanczos k-step Upper Bound");

    unsigned int chebyshevOrder = dftParameters::chebyshevOrder;


    //
    //set Chebyshev order
    //
    if(chebyshevOrder == 0)
      chebyshevOrder=internal::setChebyshevOrder(upperBoundUnwantedSpectrum);

    chebyshevOrder=(isFirstScf && dftParameters::isPseudopotential)?chebyshevOrder*1.34:chebyshevOrder;

    if (dftParameters::lowerBoundUnwantedFracUpper>1e-6)
      d_lowerBoundUnWantedSpectrum=dftParameters::lowerBoundUnwantedFracUpper*upperBoundUnwantedSpectrum;
    //
    //output statements
    //
    if (dftParameters::verbosity>=2)
      {
	char buffer[100];

	sprintf(buffer, "%s:%18.10e\n", "upper bound of unwanted spectrum", upperBoundUnwantedSpectrum);
	pcout << buffer;
	sprintf(buffer, "%s:%18.10e\n", "lower bound of unwanted spectrum", d_lowerBoundUnWantedSpectrum);
	pcout << buffer;
	sprintf(buffer, "%s: %u\n\n", "Chebyshev polynomial degree", chebyshevOrder);
	pcout << buffer;
      }


    //
    //Set the constraints to zero
    //
    //operatorMatrix.getOverloadedConstraintMatrix()->set_zero(eigenVectorsFlattened,
    //	                                                    totalNumberWaveFunctions);


    if (dftParameters::verbosity>=4)
      dftUtils::printCurrentMemoryUsage(operatorMatrix.getMPICommunicator(),
					"Before starting chebyshev filtering");

    const unsigned int localVectorSize = eigenVectorsFlattened.size()/totalNumberWaveFunctions;


    //band group parallelization data structures
    const unsigned int numberBandGroups=
      dealii::Utilities::MPI::n_mpi_processes(interBandGroupComm);
    const unsigned int bandGroupTaskId = dealii::Utilities::MPI::this_mpi_process(interBandGroupComm);
    std::vector<unsigned int> bandGroupLowHighPlusOneIndices;
    dftUtils::createBandParallelizationIndices(interBandGroupComm,
					       totalNumberWaveFunctions,
					       bandGroupLowHighPlusOneIndices);
    const unsigned int totalNumberBlocks=std::ceil((double)totalNumberWaveFunctions/(double)dftParameters::chebyWfcBlockSize);
    const unsigned int vectorsBlockSize=std::min(dftParameters::chebyWfcBlockSize,
	                                         bandGroupLowHighPlusOneIndices[1]);


    //
    //allocate storage for eigenVectorsFlattenedArray for multiple blocks
    //
    dealii::parallel::distributed::Vector<dataTypes::number> eigenVectorsFlattenedArrayBlock;
    operatorMatrix.reinit(vectorsBlockSize,
			  eigenVectorsFlattenedArrayBlock,
			  true);
    int startIndexBandParal=totalNumberWaveFunctions;
    int numVectorsBandParal=0;
    for (unsigned int jvec = 0; jvec < totalNumberWaveFunctions; jvec += vectorsBlockSize)
      {

	// Correct block dimensions if block "goes off edge of" the matrix
	const unsigned int BVec = std::min(vectorsBlockSize, totalNumberWaveFunctions-jvec);

	if ((jvec+BVec)<=bandGroupLowHighPlusOneIndices[2*bandGroupTaskId+1] &&
	    (jvec+BVec)>bandGroupLowHighPlusOneIndices[2*bandGroupTaskId])
	  {
	    if (jvec<startIndexBandParal)
		startIndexBandParal=jvec;
	    numVectorsBandParal= jvec+BVec-startIndexBandParal;

	    //create custom partitioned dealii array
	    if (BVec!=vectorsBlockSize)
	      operatorMatrix.reinit(BVec,
				    eigenVectorsFlattenedArrayBlock,
				    true);

	    //fill the eigenVectorsFlattenedArrayBlock from eigenVectorsFlattenedArray
	    computing_timer.enter_section("Copy from full to block flattened array");
	    for(unsigned int iNode = 0; iNode < localVectorSize; ++iNode)
	      for(unsigned int iWave = 0; iWave < BVec; ++iWave)
		eigenVectorsFlattenedArrayBlock.local_element(iNode*BVec+iWave)
		  =eigenVectorsFlattened[iNode*totalNumberWaveFunctions+jvec+iWave];

	    computing_timer.exit_section("Copy from full to block flattened array");

	    //
	    //call Chebyshev filtering function only for the current block to be filtered
	    //and does in-place filtering
	    computing_timer.enter_section("Chebyshev filtering opt");
	    if (jvec+BVec<dftParameters::numAdaptiveFilterStates)
	      {
		const double chebyshevOrd=(double)chebyshevOrder;
		const double adaptiveOrder=0.5*chebyshevOrd
		  +jvec*0.3*chebyshevOrd/dftParameters::numAdaptiveFilterStates;
		linearAlgebraOperations::chebyshevFilter(operatorMatrix,
							 eigenVectorsFlattenedArrayBlock,
							 BVec,
							 std::ceil(adaptiveOrder),
							 d_lowerBoundUnWantedSpectrum,
							 upperBoundUnwantedSpectrum,
							 d_lowerBoundWantedSpectrum);
	      }
	    else
	      linearAlgebraOperations::chebyshevFilter(operatorMatrix,
						       eigenVectorsFlattenedArrayBlock,
						       BVec,
						       chebyshevOrder,
						       d_lowerBoundUnWantedSpectrum,
						       upperBoundUnwantedSpectrum,
						       d_lowerBoundWantedSpectrum);
	    computing_timer.exit_section("Chebyshev filtering opt");

	    if (dftParameters::verbosity>=4)
	      dftUtils::printCurrentMemoryUsage(operatorMatrix.getMPICommunicator(),
						"During blocked chebyshev filtering");

	    //copy the eigenVectorsFlattenedArrayBlock into eigenVectorsFlattenedArray after filtering
	    computing_timer.enter_section("Copy from block to full flattened array");
	    for(unsigned int iNode = 0; iNode < localVectorSize; ++iNode)
	       for(unsigned int iWave = 0; iWave < BVec; ++iWave)
	   	   eigenVectorsFlattened[iNode*totalNumberWaveFunctions+jvec+iWave]
		     = eigenVectorsFlattenedArrayBlock.local_element(iNode*BVec+iWave);

	    computing_timer.exit_section("Copy from block to full flattened array");
	  }
	else
	  {
	    //set to zero wavefunctions which wont go through chebyshev filtering inside a given band group
	    for(unsigned int iNode = 0; iNode < localVectorSize; ++iNode)
	      for(unsigned int iWave = 0; iWave < BVec; ++iWave)
	        eigenVectorsFlattened[iNode*totalNumberWaveFunctions+jvec+iWave]
                = dataTypes::number(0.0);

	  }
      }//block loop

    eigenVectorsFlattenedArrayBlock.reinit(0);

    if (numberBandGroups>1)
      {
	if (!dftParameters::bandParalOpt)
	{
	    computing_timer.enter_section("MPI All Reduce wavefunctions across all band groups");
	    MPI_Barrier(interBandGroupComm);
	    const unsigned int blockSize=dftParameters::mpiAllReduceMessageBlockSizeMB*1e+6/sizeof(dataTypes::number);
	    for (unsigned int i=0; i<totalNumberWaveFunctions*localVectorSize;i+=blockSize)
	      {
		const unsigned int currentBlockSize=std::min(blockSize,totalNumberWaveFunctions*localVectorSize-i);
		MPI_Allreduce(MPI_IN_PLACE,
			      &eigenVectorsFlattened[0]+i,
			      currentBlockSize,
			      dataTypes::mpi_type_id(&eigenVectorsFlattened[0]),
			      MPI_SUM,
			      interBandGroupComm);
	      }
	    computing_timer.exit_section("MPI All Reduce wavefunctions across all band groups");
        }
	else
	{
	    computing_timer.enter_section("MPI_Allgatherv across band groups");
	    MPI_Barrier(interBandGroupComm);
	    std::vector<dataTypes::number> eigenVectorsBandGroup(numVectorsBandParal*localVectorSize,0);
	    std::vector<dataTypes::number> eigenVectorsBandGroupTransposed(numVectorsBandParal*localVectorSize,0);
	    std::vector<dataTypes::number> eigenVectorsTransposed(totalNumberWaveFunctions*localVectorSize,0);

	    for(unsigned int iNode = 0; iNode < localVectorSize; ++iNode)
	       for(unsigned int iWave = 0; iWave < numVectorsBandParal; ++iWave)
		   eigenVectorsBandGroup[iNode*numVectorsBandParal+iWave]
		     = eigenVectorsFlattened[iNode*totalNumberWaveFunctions+startIndexBandParal+iWave];


	    for(unsigned int iNode = 0; iNode < localVectorSize; ++iNode)
	       for(unsigned int iWave = 0; iWave < numVectorsBandParal; ++iWave)
		   eigenVectorsBandGroupTransposed[iWave*localVectorSize+iNode]
		     = eigenVectorsBandGroup[iNode*numVectorsBandParal+iWave];

	    std::vector<int> recvcounts(numberBandGroups,0);
	    std::vector<int> displs(numberBandGroups,0);

	    int recvcount=numVectorsBandParal*localVectorSize;
	    MPI_Allgather(&recvcount,
			  1,
			  MPI_INT,
			  &recvcounts[0],
			  1,
			  MPI_INT,
			  interBandGroupComm);

	    int displ=startIndexBandParal*localVectorSize;
	    MPI_Allgather(&displ,
			  1,
			  MPI_INT,
			  &displs[0],
			  1,
			  MPI_INT,
			  interBandGroupComm);

	    MPI_Allgatherv(&eigenVectorsBandGroupTransposed[0],
			   numVectorsBandParal*localVectorSize,
			   dataTypes::mpi_type_id(&eigenVectorsBandGroupTransposed[0]),
			   &eigenVectorsTransposed[0],
			   &recvcounts[0],
			   &displs[0],
			   dataTypes::mpi_type_id(&eigenVectorsTransposed[0]),
			   interBandGroupComm);


	    for(unsigned int iNode = 0; iNode < localVectorSize; ++iNode)
	       for(unsigned int iWave = 0; iWave < totalNumberWaveFunctions; ++iWave)
		   eigenVectorsFlattened[iNode*totalNumberWaveFunctions+iWave]
		     = eigenVectorsTransposed[iWave*localVectorSize+iNode];
	    MPI_Barrier(interBandGroupComm);
	    computing_timer.exit_section("MPI_Allgatherv across band groups");
	}
      }


    if(dftParameters::verbosity >= 4)
      pcout<<"ChebyShev Filtering Done: "<<std::endl;

    if (dftParameters::rrGEP && dftParameters::isPseudopotential)
    {
	 computing_timer.enter_section("Rayleigh-Ritz GEP");
	 if (eigenValues.size()!=totalNumberWaveFunctions)
	   {
	      linearAlgebraOperations::rayleighRitzGEPSpectrumSplitDirect(operatorMatrix,
		   							     eigenVectorsFlattened,
									     eigenVectorsRotFracDensityFlattened,
									     totalNumberWaveFunctions,
									     totalNumberWaveFunctions-eigenValues.size(),
									     interBandGroupComm,
									     operatorMatrix.getMPICommunicator(),
									     useMixedPrec,
									     eigenValues);
	   }
	 else
	   {

	     if(useFullMassMatrixGEP)
	       {
		 linearAlgebraOperations::rayleighRitzGEPFullMassMatrix(operatorMatrix,
									eigenVectorsFlattened,
									totalNumberWaveFunctions,
									interBandGroupComm,
									operatorMatrix.getMPICommunicator(),
									eigenValues,
									useMixedPrec);
	       }
	     else
	       {
		 linearAlgebraOperations::rayleighRitzGEP(operatorMatrix,
							  eigenVectorsFlattened,
							  totalNumberWaveFunctions,
							  interBandGroupComm,
							  operatorMatrix.getMPICommunicator(),
							  eigenValues,
							  useMixedPrec);
	       }
	   }
 	 computing_timer.exit_section("Rayleigh-Ritz GEP");

        computing_timer.enter_section("eigen vectors residuals opt");
	if (eigenValues.size()!=totalNumberWaveFunctions)
	{
	    linearAlgebraOperations::computeEigenResidualNorm(operatorMatrix,
							      eigenVectorsRotFracDensityFlattened,
							      eigenValues,
							      operatorMatrix.getMPICommunicator(),
							      interBandGroupComm,
							      residualNorms);
	}
	else
	{
            if(!useFullMassMatrixGEP)
		linearAlgebraOperations::computeEigenResidualNorm(operatorMatrix,
								  eigenVectorsFlattened,
								  eigenValues,
								  operatorMatrix.getMPICommunicator(),
								  interBandGroupComm,
								  residualNorms);
	}
	computing_timer.exit_section("eigen vectors residuals opt");
    }
    else
    {
	if(dftParameters::orthogType.compare("LW") == 0)
	  {
	    computing_timer.enter_section("Lowden Orthogn Opt");
	    const unsigned int flag = linearAlgebraOperations::lowdenOrthogonalization(eigenVectorsFlattened,
										       totalNumberWaveFunctions,
										       operatorMatrix.getMPICommunicator());

	    if (flag==1)
	      {
		if(dftParameters::verbosity >= 1)
		  pcout<<"Switching to Gram-Schimdt orthogonalization as Lowden orthogonalization was not successful"<<std::endl;

		computing_timer.enter_section("Gram-Schmidt Orthogn Opt");
		linearAlgebraOperations::gramSchmidtOrthogonalization(eigenVectorsFlattened,
								      totalNumberWaveFunctions,
								      operatorMatrix.getMPICommunicator());
		computing_timer.exit_section("Gram-Schmidt Orthogn Opt");
	      }
	    computing_timer.exit_section("Lowden Orthogn Opt");
	  }
	else if (dftParameters::orthogType.compare("PGS") == 0)
	  {
	    computing_timer.enter_section("Pseudo-Gram-Schmidt");
	    const unsigned int flag=linearAlgebraOperations::pseudoGramSchmidtOrthogonalization
	      (operatorMatrix,
	       eigenVectorsFlattened,
	       totalNumberWaveFunctions,
	       interBandGroupComm,
	       operatorMatrix.getMPICommunicator(),
	       useMixedPrec);

	    if (flag==1)
	      {
		if(dftParameters::verbosity >= 1)
		  pcout<<"Switching to Gram-Schimdt orthogonalization as Pseudo-Gram-Schimdt orthogonalization was not successful"<<std::endl;

		computing_timer.enter_section("Gram-Schmidt Orthogn Opt");
		linearAlgebraOperations::gramSchmidtOrthogonalization(eigenVectorsFlattened,
								      totalNumberWaveFunctions,
								      operatorMatrix.getMPICommunicator());
		computing_timer.exit_section("Gram-Schmidt Orthogn Opt");
	      }
	    computing_timer.exit_section("Pseudo-Gram-Schmidt");
	  }
	else if (dftParameters::orthogType.compare("GS") == 0)
	  {
	    computing_timer.enter_section("Gram-Schmidt Orthogn Opt");
	    linearAlgebraOperations::gramSchmidtOrthogonalization(eigenVectorsFlattened,
								  totalNumberWaveFunctions,
								  operatorMatrix.getMPICommunicator());
	    computing_timer.exit_section("Gram-Schmidt Orthogn Opt");
	  }

	if(dftParameters::verbosity >= 4)
	  pcout<<"Orthogonalization Done: "<<std::endl;

	computing_timer.enter_section("Rayleigh-Ritz proj Opt");

	if (eigenValues.size()!=totalNumberWaveFunctions)
	  {
	    linearAlgebraOperations::rayleighRitzSpectrumSplitDirect(operatorMatrix,
								     eigenVectorsFlattened,
								     eigenVectorsRotFracDensityFlattened,
								     totalNumberWaveFunctions,
								     totalNumberWaveFunctions-eigenValues.size(),
								     interBandGroupComm,
								     operatorMatrix.getMPICommunicator(),
								     useMixedPrec,
								     eigenValues);
	  }
	else
	  {


	    linearAlgebraOperations::rayleighRitz(operatorMatrix,
						  eigenVectorsFlattened,
						  totalNumberWaveFunctions,
						  false,
						  interBandGroupComm,
						  operatorMatrix.getMPICommunicator(),
						  eigenValues,
						  false);
	  }


	computing_timer.exit_section("Rayleigh-Ritz proj Opt");

	if(dftParameters::verbosity >= 4)
	  {
	    pcout<<"Rayleigh-Ritz Done: "<<std::endl;
	    pcout<<std::endl;
	  }

        computing_timer.enter_section("eigen vectors residuals opt");

	if (eigenValues.size()!=totalNumberWaveFunctions)
	  {
	    linearAlgebraOperations::computeEigenResidualNorm(operatorMatrix,
							      eigenVectorsRotFracDensityFlattened,
							      eigenValues,
							      operatorMatrix.getMPICommunicator(),
							      interBandGroupComm,
							      residualNorms);
	  }
	else
	  linearAlgebraOperations::computeEigenResidualNorm(operatorMatrix,
							    eigenVectorsFlattened,
							    eigenValues,
							    operatorMatrix.getMPICommunicator(),
							    interBandGroupComm,
							    residualNorms);
	computing_timer.exit_section("eigen vectors residuals opt");
    }



    if(dftParameters::verbosity >= 4)
      {
	pcout<<"EigenVector Residual Computation Done: "<<std::endl;
	pcout<<std::endl;
      }

    if (dftParameters::verbosity>=4)
      dftUtils::printCurrentMemoryUsage(operatorMatrix.getMPICommunicator(),
					"After all steps of subspace iteration");

    return;

  }


  //
  // solve
  //
  eigenSolverClass::ReturnValueType
  chebyshevOrthogonalizedSubspaceIterationSolver::solve(operatorDFTClass           & operatorMatrix,
							std::vector<vectorType>    & eigenVectors,
							std::vector<double>        & eigenValues,
							std::vector<double>        & residualNorms)
  {


    computing_timer.enter_section("Lanczos k-step Upper Bound");
    operatorMatrix.reinit(1);
    double upperBoundUnwantedSpectrum = linearAlgebraOperations::lanczosUpperBoundEigenSpectrum(operatorMatrix,
												eigenVectors[0]);

    computing_timer.exit_section("Lanczos k-step Upper Bound");

    unsigned int chebyshevOrder = dftParameters::chebyshevOrder;

    const unsigned int totalNumberWaveFunctions = eigenVectors.size();

    //set Chebyshev order
    if(chebyshevOrder == 0)
      chebyshevOrder=internal::setChebyshevOrder(upperBoundUnwantedSpectrum);

    //
    //output statements
    //
    if (dftParameters::verbosity>=2)
      {
	char buffer[100];

	sprintf(buffer, "%s:%18.10e\n", "upper bound of unwanted spectrum", upperBoundUnwantedSpectrum);
	pcout << buffer;
	sprintf(buffer, "%s:%18.10e\n", "lower bound of unwanted spectrum", d_lowerBoundUnWantedSpectrum);
	pcout << buffer;
	sprintf(buffer, "%s: %u\n\n", "Chebyshev polynomial degree", chebyshevOrder);
	pcout << buffer;
      }


    //
    //Set the constraints to zero
    //
    for(unsigned int i = 0; i < totalNumberWaveFunctions; ++i)
      operatorMatrix.getConstraintMatrixEigen()->set_zero(eigenVectors[i]);


    if(dftParameters::verbosity >= 4)
      {
#ifdef USE_PETSC
	PetscLogDouble bytes;
	PetscMemoryGetCurrentUsage(&bytes);
	FILE *dummy;
	unsigned int this_mpi_process = dealii::Utilities::MPI::this_mpi_process(operatorMatrix.getMPICommunicator());
	PetscSynchronizedPrintf(operatorMatrix.getMPICommunicator(),"[%d] Memory Usage before starting eigen solution  %e\n",this_mpi_process,bytes);
	PetscSynchronizedFlush(operatorMatrix.getMPICommunicator(),dummy);
#endif
      }

    operatorMatrix.reinit(totalNumberWaveFunctions);

    //
    //call chebyshev filtering routine
    //
    computing_timer.enter_section("Chebyshev filtering");

    linearAlgebraOperations::chebyshevFilter(operatorMatrix,
					     eigenVectors,
					     chebyshevOrder,
					     d_lowerBoundUnWantedSpectrum,
					     upperBoundUnwantedSpectrum,
					     d_lowerBoundWantedSpectrum);

    computing_timer.exit_section("Chebyshev filtering");


    computing_timer.enter_section("Gram-Schmidt Orthogonalization");

    linearAlgebraOperations::gramSchmidtOrthogonalization(operatorMatrix,
							  eigenVectors);


    computing_timer.exit_section("Gram-Schmidt Orthogonalization");


    computing_timer.enter_section("Rayleigh Ritz Projection");

    linearAlgebraOperations::rayleighRitz(operatorMatrix,
					  eigenVectors,
					  eigenValues);

    computing_timer.exit_section("Rayleigh Ritz Projection");


    computing_timer.enter_section("compute eigen vectors residuals");
    //linearAlgebraOperations::computeEigenResidualNorm(operatorMatrix,
    //						      eigenVectors,
    //						      eigenValues,
    //						      residualNorms);
    computing_timer.exit_section("compute eigen vectors residuals");

    return;

  }

}
