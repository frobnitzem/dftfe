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
// @author Sambit Das and Phani Motamarri
//

namespace internal{

  extern "C"{
    //
    // lapack Ax=b
    //
    void dgesv_(int *N, int * NRHS, double* A, int * LDA, int* IPIV,
		double *B, int * LDB, int *INFO);

  }


  std::vector<double> getFractionalCoordinates(const std::vector<double> & latticeVectors,
	                                       const Point<3> & point,                                                                        const Point<3> & corner)
  {
    //
    // recenter vertex about corner
    //
    std::vector<double> recenteredPoint(3);
    for(unsigned int i = 0; i < 3; ++i)
      recenteredPoint[i] = point[i]-corner[i];

    std::vector<double> latticeVectorsDup = latticeVectors;

    //
    // to get the fractionalCoords, solve a linear
    // system of equations
    //
    int N = 3;
    int NRHS = 1;
    int LDA = 3;
    int IPIV[3];
    int info;

    dgesv_(&N, &NRHS, &latticeVectorsDup[0], &LDA, &IPIV[0], &recenteredPoint[0], &LDA,&info);
    AssertThrow(info == 0, ExcMessage("LU solve in finding fractional coordinates failed."));
    return recenteredPoint;
  }

  std::vector<double> wrapAtomsAcrossPeriodicBc(const Point<3> & cellCenteredCoord,
	                                        const Point<3> & corner,
					        const std::vector<double> & latticeVectors,
						const std::vector<bool> & periodicBc)
  {
    const double tol=1e-8;
    std::vector<double> fracCoord= getFractionalCoordinates(latticeVectors,
							    cellCenteredCoord,                                                                                                corner);


    //if(Utilities::MPI::this_mpi_process(MPI_COMM_WORLD) == 0)
    //std::cout<<"Fractional Coordinates before wrapping: "<<fracCoord[0]<<" "<<fracCoord[1]<<" "<<fracCoord[2]<<std::endl;


    //wrap fractional coordinate
    for(unsigned int i = 0; i < 3; ++i)
      {
	if (periodicBc[i])
	  {
	    if (fracCoord[i]<-tol)
	      fracCoord[i]+=1.0;
	    else if (fracCoord[i]>1.0+tol)
	      fracCoord[i]-=1.0;

            if(Utilities::MPI::this_mpi_process(MPI_COMM_WORLD) == 0)
	     std::cout<<fracCoord[i]<<" ";

	    AssertThrow(fracCoord[i]>-2.0*tol && fracCoord[i]<1.0+2.0*tol,ExcMessage("Moved atom position doesnt't lie inside the cell after wrapping across periodic boundary"));
	  }
      }

    //if(Utilities::MPI::this_mpi_process(MPI_COMM_WORLD) == 0)
    //std::cout<<std::endl;

    return fracCoord;
  }

}

// Function to update the atom positions and mesh based on the provided displacement input.
// Depending on the maximum displacement magnitude this function decides wether to do auto remeshing
// or move mesh using Gaussian functions.
template<unsigned int FEOrder>
void dftClass<FEOrder>::updateAtomPositionsAndMoveMesh(const std::vector<Tensor<1,3,double> > & globalAtomsDisplacements,
	                                               double maximumForceToBeRelaxed,
						       const bool useSingleAtomSolutions)

{
  const int numberGlobalAtoms = atomLocations.size();
  int numberImageCharges = d_imageIds.size();
  int totalNumberAtoms = numberGlobalAtoms + numberImageCharges;

  std::vector<double> latticeVectorsFlattened(9,0.0);
  for (unsigned int idim=0; idim<3; idim++)
    for(unsigned int jdim=0; jdim<3; jdim++)
      latticeVectorsFlattened[3*idim+jdim]=d_domainBoundingVectors[idim][jdim];
  Point<3> corner;
  for (unsigned int idim=0; idim<3; idim++)
    {
      corner[idim]=0;
      for(unsigned int jdim=0; jdim<3; jdim++)
	corner[idim]-=d_domainBoundingVectors[jdim][idim]/2.0;
    }
  std::vector<bool> periodicBc(3,false);
  periodicBc[0]=dftParameters::periodicX;periodicBc[1]=dftParameters::periodicY;periodicBc[2]=dftParameters::periodicZ;

  std::vector<Point<C_DIM> > controlPointLocationsInitialMove;
  std::vector<Tensor<1,C_DIM,double> > controlPointDisplacementsInitialMove;

  std::vector<Point<C_DIM> > controlPointLocationsCurrentMove;
  std::vector<Tensor<1,C_DIM,double> > controlPointDisplacementsCurrentMove;

  d_gaussianMovementAtomsNetDisplacements.resize(numberGlobalAtoms);
  std::vector<Tensor<1,3,double> > tempGaussianMovementAtomsNetDisplacements;

  double maxDispAtom=-1;
  for(unsigned int iAtom=0;iAtom < numberGlobalAtoms; iAtom++)
    {
      d_gaussianMovementAtomsNetDisplacements[iAtom]+=globalAtomsDisplacements[iAtom];
      const double netDisp = d_gaussianMovementAtomsNetDisplacements[iAtom].norm();

      if(netDisp>maxDispAtom)
	maxDispAtom=netDisp;
    }

  double maxCurrentDispAtom=-1;
  for(unsigned int iAtom=0;iAtom < numberGlobalAtoms; iAtom++)
    {
      const double currentDisp = globalAtomsDisplacements[iAtom].norm();

      if(currentDisp>maxCurrentDispAtom)
	maxCurrentDispAtom=currentDisp;
    }

  tempGaussianMovementAtomsNetDisplacements = d_gaussianMovementAtomsNetDisplacements;

  unsigned int useGaussian = 0;
  const double tol=1e-6;
  const double break1 = 1.0;

  if(maxDispAtom <= break1+tol)
    useGaussian = 1;

  //for synchrozination in case the updateCase are different in different processors due to floating point comparison
  MPI_Bcast(&(useGaussian),
	    1,
	    MPI_INT,
	    0,
	    MPI_COMM_WORLD);





  if((dftParameters::periodicX || dftParameters::periodicY || dftParameters::periodicZ) && useGaussian == 0)
    {
      for (unsigned int iAtom = 0; iAtom < numberGlobalAtoms; iAtom++)
	{
	  Point<C_DIM> atomCoor;
	  int atomId=iAtom;
	  atomCoor[0] = atomLocations[iAtom][2];
	  atomCoor[1] = atomLocations[iAtom][3];
	  atomCoor[2] = atomLocations[iAtom][4];

	  Point<C_DIM> newCoord;
	  for(unsigned int idim=0; idim<C_DIM; ++idim)
	    newCoord[idim]=atomCoor[idim]+globalAtomsDisplacements[atomId][idim];

	  std::vector<double> newFracCoord=internal::wrapAtomsAcrossPeriodicBc(newCoord,
									       corner,
									       latticeVectorsFlattened,
									       periodicBc);
	  //for synchrozination
	  MPI_Bcast(&(newFracCoord[0]),
		    3,
		    MPI_DOUBLE,
		    0,
		    MPI_COMM_WORLD);

	  atomLocationsFractional[iAtom][2]=newFracCoord[0];
	  atomLocationsFractional[iAtom][3]=newFracCoord[1];
	  atomLocationsFractional[iAtom][4]=newFracCoord[2];
	}
    }
  else if((dftParameters::periodicX || dftParameters::periodicY || dftParameters::periodicZ) && useGaussian == 1)
    {
      for(unsigned int iAtom = 0; iAtom < numberGlobalAtoms; ++iAtom)
	{
	  Point<C_DIM> atomCoor;
	  int atomId=iAtom;

	  atomCoor[0] = atomLocations[iAtom][2];
	  atomCoor[1] = atomLocations[iAtom][3];
	  atomCoor[2] = atomLocations[iAtom][4];

	  Point<C_DIM> newCoord;
	  for(unsigned int idim=0; idim<C_DIM; ++idim)
	    newCoord[idim]=atomCoor[idim]+globalAtomsDisplacements[atomId][idim];

	  std::vector<double> newFracCoord = internal::getFractionalCoordinates(latticeVectorsFlattened,
										newCoord,
										corner);

	  atomLocationsFractional[iAtom][2]=newFracCoord[0];
	  atomLocationsFractional[iAtom][3]=newFracCoord[1];
	  atomLocationsFractional[iAtom][4]=newFracCoord[2];

	}

      initImageChargesUpdateKPoints(false);

    }
  else
    {
      for (unsigned int iAtom=0;iAtom < numberGlobalAtoms; iAtom++)
	{
	  Point<C_DIM> atomCoor;
	  int atomId=iAtom;

	  atomLocations[iAtom][2]+=globalAtomsDisplacements[atomId][0];
	  atomLocations[iAtom][3]+=globalAtomsDisplacements[atomId][1];
	  atomLocations[iAtom][4]+=globalAtomsDisplacements[atomId][2];
	}
    }


  d_gaussianMovementAtomsNetDisplacements.clear();
  numberImageCharges = d_imageIdsAutoMesh.size();
  totalNumberAtoms = numberGlobalAtoms + numberImageCharges;

  for(unsigned int iAtom = 0; iAtom < totalNumberAtoms; ++iAtom)
    {
      dealii::Point<C_DIM> temp;
      int atomId;
      Point<3> atomCoor;
      if(iAtom < numberGlobalAtoms)
	{
	  d_gaussianMovementAtomsNetDisplacements.push_back(tempGaussianMovementAtomsNetDisplacements[iAtom]);
	}
      else
	{
	  const int atomId=d_imageIdsAutoMesh[iAtom-numberGlobalAtoms];
	  d_gaussianMovementAtomsNetDisplacements.push_back(d_gaussianMovementAtomsNetDisplacements[atomId]);
	}

      controlPointLocationsInitialMove.push_back(d_closestTriaVertexToAtomsLocation[iAtom]);
      controlPointDisplacementsInitialMove.push_back(d_dispClosestTriaVerticesToAtoms[iAtom]);
      controlPointDisplacementsCurrentMove.push_back(d_gaussianMovementAtomsNetDisplacements[iAtom]);

    }

  MPI_Barrier(mpi_communicator);
  d_autoMesh=0;

  const bool useHybridMeshUpdateScheme = true;// dftParameters::electrostaticsHRefinement?false:true;

  if(!useHybridMeshUpdateScheme)//always remesh
    {
      if (!dftParameters::reproducible_output)
	pcout << "Auto remeshing and reinitialization of dft problem for new atom coordinates" << std::endl;

      if (maxDispAtom < 0.2 && dftParameters::isPseudopotential)
	{
	  init(dftParameters::reuseWfcGeoOpt && maxDispAtom<0.1?2:(dftParameters::reuseDensityGeoOpt?1:0));
	}
      else
	init(0);

      //for (unsigned int iAtom=0;iAtom <numberGlobalAtoms; iAtom++)
	//d_dispClosestTriaVerticesToAtoms[iAtom]= 0.0;

      if (!dftParameters::reproducible_output)
	pcout << "...Reinitialization end" << std::endl;


      d_autoMesh=1;
      MPI_Bcast(&(d_autoMesh),
		1,
		MPI_INT,
		0,
		MPI_COMM_WORLD);

    }
  else
    {
      /*d_mesh.resetMesh(d_mesh.getParallelMeshUnmoved(),
	d_mesh.getParallelMeshMoved());*/


      // Re-generate serial and parallel meshes from saved refinement flags
      // to get back the unmoved meshes as Gaussian movement can only be done starting from
      // the unmoved meshes.
      // While parallel meshes are always generated, serial meshes are only generated
      // for following three cases: symmetrization is on, ionic optimization is on as well
      // as reuse wfcs and density from previous ionic step is on, or if serial constraints
      // generation is on.
      d_mesh.generateResetMeshes(d_domainBoundingVectors,
				 dftParameters::useSymm
				 || (dftParameters::isIonOpt && (dftParameters::reuseWfcGeoOpt || dftParameters::reuseDensityGeoOpt))
				 || dftParameters::createConstraintsFromSerialDofhandler,
				 dftParameters::electrostaticsHRefinement);

      //initUnmovedTriangulation(d_mesh.getParallelMeshMoved());

      dofHandler.clear();dofHandlerEigen.clear();
      dofHandler.initialize(d_mesh.getParallelMeshMoved(),FE);
      dofHandlerEigen.initialize(d_mesh.getParallelMeshMoved(),FEEigen);
      dofHandler.distribute_dofs (FE);
      dofHandlerEigen.distribute_dofs (FEEigen);


      forcePtr->initUnmoved(d_mesh.getParallelMeshMoved(),
			    d_mesh.getSerialMeshUnmoved(),
			    d_domainBoundingVectors,
			    false,
			    d_gaussianConstantForce);

      forcePtr->initUnmoved(d_mesh.getParallelMeshMoved(),
			    d_mesh.getSerialMeshUnmoved(),
			    d_domainBoundingVectors,
			    true,
			    d_gaussianConstantForce);

      //meshMovementGaussianClass gaussianMove(mpi_communicator);
      d_gaussianMovePar.init(d_mesh.getParallelMeshMoved(),
			     d_mesh.getSerialMeshUnmoved(),
			     d_domainBoundingVectors);

      const double tol=1e-6;
      //Heuristic values
      const double maxJacobianRatio=2.0;
      //const double break1=0.1;

      //unsigned int useGaussian=0;
      //if (maxDispAtom <(break1+tol))
      //useGaussian=1;

      //for synchrozination in case the updateCase are different in different processors due to floating point comparison
      //MPI_Bcast(&(useGaussian),
      //	1,
      //	MPI_INT,
      //	0,
      //	MPI_COMM_WORLD);

      if(useGaussian!=1)
	{
          if (!dftParameters::reproducible_output)
	   pcout << "Auto remeshing and reinitialization of dft problem for new atom coordinates as max net displacement magnitude: "<<maxDispAtom<< " is greater than: "<< break1 << " Bohr..." << std::endl;
	  init(0);

	  d_autoMesh=1;
	  MPI_Bcast(&(d_autoMesh),
		    1,
		    MPI_INT,
		    0,
		    MPI_COMM_WORLD);

	  if (!dftParameters::reproducible_output)
	    pcout << "...Reinitialization end" << std::endl;
	}
      else
	{
          if (!dftParameters::reproducible_output)
	     pcout << "Trying to Move using Gaussian with same Gaussian constant for computing the forces: "<<forcePtr->getGaussianGeneratorParameter()<<" as net max displacement magnitude: "<< maxDispAtom<< " is below " << break1 <<" Bohr"<<std::endl;
	  if (!dftParameters::reproducible_output)
	     pcout << "Max current disp magnitude: "<<maxCurrentDispAtom<<" Bohr"<<std::endl;

	  const std::pair<bool,double> meshQualityMetrics=d_gaussianMovePar.moveMeshTwoStep(controlPointLocationsInitialMove,d_controlPointLocationsCurrentMove,controlPointDisplacementsInitialMove,controlPointDisplacementsCurrentMove,d_gaussianConstantAutoMove,forcePtr->getGaussianGeneratorParameter());
          double factor;
          if(maximumForceToBeRelaxed >= 1e-03)
            factor = 2.00;
	  else if(maximumForceToBeRelaxed < 1e-03 && maximumForceToBeRelaxed >= 1e-04)
            factor = 1.25;
          else if(maximumForceToBeRelaxed < 1e-04)
            factor = 1.15;

	  if (meshQualityMetrics.first || meshQualityMetrics.second > factor*d_autoMeshMaxJacobianRatio)
	    d_autoMesh=1;
	  MPI_Bcast(&(d_autoMesh),
		    1,
		    MPI_INT,
		    0,
		    MPI_COMM_WORLD);
	  if (d_autoMesh==1)
	    {
              if (!dftParameters::reproducible_output)
	      {
	        if (meshQualityMetrics.first)
		  pcout<< " Auto remeshing and reinitialization of dft problem for new atom coordinates due to negative jacobian after Gaussian mesh movement using Gaussian constant: "<< forcePtr->getGaussianGeneratorParameter()<<std::endl;
	        else
		  pcout<< " Auto remeshing and reinitialization of dft problem for new atom coordinates due to maximum jacobian ratio: "<< meshQualityMetrics.second<< " exceeding set bound of: "<< factor*d_autoMeshMaxJacobianRatio<<" after Gaussian mesh movement using Gaussian constant: "<< forcePtr->getGaussianGeneratorParameter()<<std::endl;
              }

	      if(dftParameters::periodicX || dftParameters::periodicY || dftParameters::periodicZ)
		{
		  for (unsigned int iAtom=0;iAtom <numberGlobalAtoms; iAtom++)
		    {
		      Point<C_DIM> atomCoor;
		      int atomId=iAtom;
		      atomCoor[0] = atomLocations[iAtom][2];
		      atomCoor[1] = atomLocations[iAtom][3];
		      atomCoor[2] = atomLocations[iAtom][4];

		      Point<C_DIM> newCoord;
		      for(unsigned int idim=0; idim<C_DIM; ++idim)
			newCoord[idim]=atomCoor[idim]+globalAtomsDisplacements[atomId][idim];

		      std::vector<double> newFracCoord=internal::wrapAtomsAcrossPeriodicBc(newCoord,
											   corner,
											   latticeVectorsFlattened,
											   periodicBc);
		      //for synchrozination
		      MPI_Bcast(&(newFracCoord[0]),
				3,
				MPI_DOUBLE,
				0,
				MPI_COMM_WORLD);

		      atomLocationsFractional[iAtom][2]=newFracCoord[0];
		      atomLocationsFractional[iAtom][3]=newFracCoord[1];
		      atomLocationsFractional[iAtom][4]=newFracCoord[2];
		    }

		}
	      init(0);

	      if (!dftParameters::reproducible_output)
	         pcout << "...Reinitialization end" << std::endl;
	    }
	  else
	    {
	      if (!dftParameters::reproducible_output)
	         pcout<< " Mesh quality check for Gaussian movement of mesh along with atoms: maximum jacobian ratio after movement: "<< meshQualityMetrics.second<<std::endl;
	      if (!dftParameters::reproducible_output)
	         pcout << "Now Reinitializing all moved triangulation dependent objects..." << std::endl;

	      initNoRemesh(false,(!dftParameters::reproducible_output && maxCurrentDispAtom>0.06) || useSingleAtomSolutions?true:false);
	      if (!dftParameters::reproducible_output)
	         pcout << "...Reinitialization end" << std::endl;
	    }
	}
    }
}
