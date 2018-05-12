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

/** @file dftUtils.h
 *  @brief Contains repeatedly used functions in the KSDFT calculations
 *
 *  @author Sambit Das
 */

#ifndef dftUtils_H_
#define dftUtils_H_

#include <mpi.h>
#include <headers.h>

namespace dftfe {

  namespace dftUtils
    {
      /** @brief Calculates partial occupancy of the atomic orbital using
       *  Fermi-Dirac smearing.
       *
       *  @param  eigenValue
       *  @param  fermiEnergy
       *  @param  kb Boltzmann constant
       *  @param  T smearing temperature
       *  @return double The partial occupancy of the orbital
       */
      double getPartialOccupancy(const double eigenValue,const double fermiEnergy,const double kb,const double T);

      /** @brief Applies an affine transformation to the domain bounding vectors
       *
       *  @param  d_domainBoundingVectors the bounding vectors of the domain given as a 2d array
       *  @param  deformationGradient
       *  @return void.
       */
       void transformDomainBoundingVectors(std::vector<std::vector<double> > & domainBoundingVectors,
					   const dealii::Tensor<2,3,double> & deformationGradient);

      /** @brief Writes to vtu file only from the lowest pool id
       *
       *  @param  dataOut  DataOut class object
       *  @param  intralpoolcomm mpi communicator of domain decomposition inside each pool
       *  @param  interpoolcomm  mpi communicator across pools
       *  @param  fileName
       */
       void writeDataVTUParallelLowestPoolId(const dealii::DataOut<3> & dataOut,
					     const MPI_Comm & intrapoolcomm,
					     const MPI_Comm & interpoolcomm,
					     const std::string & fileName);

      /**
       * A class to split the given communicator into a number of pools
       */
      class Pool
      {
      public:
	Pool(const MPI_Comm &mpi_communicator,
	     const unsigned int n_pools);

	/**
	 * FIXME: document
	 */
	MPI_Comm &get_interpool_comm();

	/**
	 * FIXME: document
	 */
	MPI_Comm &get_intrapool_comm();

	/**
	 * FIXME: document
	 */
	MPI_Comm &get_replica_comm();

      private:
	/// FIXME: document
	MPI_Comm interpoolcomm;

	/// FIXME: document
	MPI_Comm intrapoolcomm;

	/// FIXME: document
	MPI_Comm mpi_comm_replica;
      };

      /// Exception handler for not implemented functionality
      DeclExceptionMsg (ExcNotImplementedYet,"This functionality is not implemented yet or not needed to be implemented.");

      /// Exception handler for DFT-FE internal error
      DeclExceptionMsg (ExcInternalError,"DFT-FE internal error.");
    }

}
#endif
