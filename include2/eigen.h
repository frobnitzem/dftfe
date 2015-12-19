#ifndef eigen_H_
#define eigen_H_
#include "headers.h"

//Define eigenClass class
class eigenClass
{
  friend class dftClass;
public:
  eigenClass(dftClass* _dftPtr);
  void HX(const std::vector<vectorType*> &src, std::vector<vectorType*> &dst);
  void XHX(const std::vector<vectorType*> &src); 
  void vmult (vectorType &dst, const vectorType &src) const;
 private:
  void implementHX(const dealii::MatrixFree<3,double>  &data,
		   std::vector<vectorType*>  &dst, 
		   const std::vector<vectorType*>  &src,
		   const std::pair<unsigned int,unsigned int> &cell_range) const;
  void init ();
  void computeMassVector();
  void computeVEffectiveRHS(std::map<dealii::CellId,std::vector<double> >* rhoValues, const vectorType& phi);  
  void MX (const dealii::MatrixFree<3,double>           &data,
	   vectorType                &dst,
	   const vectorType                &src,
	   const std::pair<unsigned int,unsigned int> &cell_range) const;
  void computeVEffective(std::map<dealii::CellId,std::vector<double> >* rhoValues, const vectorType& phi);

  //pointer to dft class
  dftClass* dftPtr;

  //FE data structres
  dealii::FE_Q<3>   FE;
 
  //constraints
  dealii::ConstraintMatrix  constraintsNone; 

  //data structures
  vectorType massVector, vEffective, rhsVeff;
  std::vector<double> XHXValue;
  std::vector<double>* XHXValuePtr;
  std::vector<vectorType*> HXvalue;

  //parallel objects
  MPI_Comm mpi_communicator;
  const unsigned int n_mpi_processes;
  const unsigned int this_mpi_process;
  dealii::ConditionalOStream   pcout;

  //compute-time logger
  dealii::TimerOutput computing_timer;
  //mutex thread for managing multi-thread writing to XHXvalue
  mutable dealii::Threads::Mutex  assembler_lock;
};

#endif