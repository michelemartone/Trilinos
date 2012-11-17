/*
// @HEADER
// ************************************************************************
//
//        Piro: Strategy package for embedded analysis capabilitites
//                  Copyright (2010) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Andy Salinger (agsalin@sandia.gov), Sandia
// National Laboratories.
//
// ************************************************************************
// @HEADER
*/

#include "Piro_ConfigDefs.hpp"

#ifdef Piro_ENABLE_Rythmos
#include "Piro_Epetra_RythmosSolver.hpp"

#include "MockModelEval_A.hpp"

#include "Teuchos_UnitTestHarness.hpp"

#include "Thyra_AmesosLinearOpWithSolveFactory.hpp"
#include "Thyra_EpetraModelEvaluator.hpp"

#include "Teuchos_RCP.hpp"
#include "Teuchos_Array.hpp"
#include "Teuchos_Tuple.hpp"
#include "Teuchos_Assert.hpp"

#include <stdexcept>

using namespace Teuchos;
using namespace Piro;

#include "Epetra_LocalMap.h"
#include "Epetra_Vector.h"
#include "Epetra_Import.h"

Array<double> arrayFromVector(const Epetra_Vector &vec) {
  const Epetra_BlockMap &vecMap = vec.Map();
  const Epetra_LocalMap localMap(vecMap.NumGlobalElements(), 0, vec.Comm());
  const Epetra_Import localImporter(localMap, vecMap);

  Epetra_Vector localVec(localMap, false);
  localVec.Import(vec, localImporter, Insert);

  return Array<double>(localVec.Values(), localVec.Values() + localVec.MyLength());
}

Array<double> arrayFromVector(const Epetra_MultiVector &mv, int col)
{
  return arrayFromVector(*mv(col));
}

RCP<EpetraExt::ModelEvaluator> epetraModelNew()
{
#ifdef HAVE_MPI
  const MPI_Comm comm = MPI_COMM_WORLD;
#else /*HAVE_MPI*/
  const int comm = 0;
#endif /*HAVE_MPI*/
  return rcp(new MockModelEval_A(comm));
}

RCP<Epetra::RythmosSolver> solverNew(const RCP<EpetraExt::ModelEvaluator> &model, double finalTime)
{
  const RCP<Thyra::LinearOpWithSolveFactoryBase<double> > lowsFactory(
      new Thyra::AmesosLinearOpWithSolveFactory);

  const RCP<Rythmos::DefaultIntegrator<double> > integrator =
    Rythmos::defaultIntegrator<double>();
  const RCP<Rythmos::TimeStepNonlinearSolver<double> > stepSolver =
    Rythmos::timeStepNonlinearSolver<double>();
  const RCP<Rythmos::SolverAcceptingStepperBase<double> > stepper =
    Rythmos::backwardEulerStepper<double>(Thyra::epetraModelEvaluator(model, lowsFactory), stepSolver);

  return rcp(new Epetra::RythmosSolver(
        integrator,
        stepper,
        stepSolver,
        model,
        lowsFactory,
        finalTime));
}

RCP<Epetra_Vector> vectorNew(const Epetra_BlockMap &map)
{
  return rcp(new Epetra_Vector(map));
}

RCP<Epetra_MultiVector> multiVectorNew(const Epetra_BlockMap &map, int vectorCount)
{
  return rcp(new Epetra_MultiVector(map, vectorCount));
}

RCP<Epetra_MultiVector> multiVectorNew(const Epetra_BlockMap &map, const Epetra_BlockMap &colMap)
{
  TEUCHOS_ASSERT(colMap.NumGlobalElements() == colMap.NumMyElements());
  return multiVectorNew(map, colMap.NumGlobalElements());
}

EpetraExt::ModelEvaluator::InArgs createNominalInArgs(const EpetraExt::ModelEvaluator &model)
{
  EpetraExt::ModelEvaluator::InArgs result = model.createInArgs();

  if (nonnull(model.get_x_init())) {
    result.set_x(model.get_x_init());
  }

  const int parameterCount = result.Np();
  for (int l = 0; l < parameterCount; ++l) {
    if (nonnull(model.get_p_init(l))) {
      result.set_p(l, model.get_p_init(l));
    }
  }

  return result;
}

// Floating point tolerance
const double tol = 1.0e-8;

TEUCHOS_UNIT_TEST(Epetra_RythmosSolver, Spaces)
{
  const RCP<EpetraExt::ModelEvaluator> model = epetraModelNew();

  const double finalTime = 0.0;
  const RCP<Epetra::RythmosSolver> solver = solverNew(model, finalTime);

  TEST_ASSERT(solver->Np() == model->createInArgs().Np());
  TEST_ASSERT(solver->Ng() == model->createOutArgs().Ng() + 1);

  const int parameterIndex = 0;
  const int responseIndex = 0;
  const int solutionResponseIndex = solver->Ng() - 1;

  TEST_ASSERT(solver->get_p_map(parameterIndex)->SameAs(*model->get_p_map(parameterIndex)));
  TEST_ASSERT(solver->get_g_map(responseIndex)->SameAs(*model->get_g_map(responseIndex)));
  TEST_ASSERT(solver->get_g_map(solutionResponseIndex)->SameAs(*model->get_x_map()));

  TEST_ASSERT(is_null(solver->get_x_map()));
  TEST_ASSERT(is_null(solver->get_f_map()));
}

TEUCHOS_UNIT_TEST(Epetra_RythmosSolver, Nominal)
{
  const RCP<EpetraExt::ModelEvaluator> model = epetraModelNew();

  const double finalTime = 0.0;
  const RCP<Epetra::RythmosSolver> solver = solverNew(model, finalTime);

  TEST_ASSERT(is_null(solver->get_x_init()));
  TEST_ASSERT(is_null(solver->get_x_dot_init()));

  const int parameterIndex = 0;

  const Array<double> expected = arrayFromVector(*solver->get_p_init(parameterIndex));
  const Array<double> actual = arrayFromVector(*model->get_p_init(parameterIndex));
  TEST_COMPARE_FLOATING_ARRAYS(actual, expected, tol);
}

TEUCHOS_UNIT_TEST(Epetra_RythmosSolver, TimeZero_Solution)
{
  const RCP<EpetraExt::ModelEvaluator> model = epetraModelNew();

  const double finalTime = 0.0;
  const RCP<Epetra::RythmosSolver> solver = solverNew(model, finalTime);

  const int solutionResponseIndex = solver->Ng() - 1;

  const RCP<Epetra_Vector> solution = vectorNew(*solver->get_g_map(solutionResponseIndex));
  {
    const EpetraExt::ModelEvaluator::InArgs inArgs = createNominalInArgs(*solver);
    EpetraExt::ModelEvaluator::OutArgs outArgs = solver->createOutArgs();
    outArgs.set_g(solutionResponseIndex, solution);
    solver->evalModel(inArgs, outArgs);
  }

  const Array<double> expected = arrayFromVector(*model->get_x_init());
  const Array<double> actual = arrayFromVector(*solution);
  TEST_COMPARE_FLOATING_ARRAYS(actual, expected, tol);
}

TEUCHOS_UNIT_TEST(Epetra_RythmosSolver, TimeZero_Response)
{
  const RCP<EpetraExt::ModelEvaluator> model = epetraModelNew();

  const double finalTime = 0.0;
  const RCP<Epetra::RythmosSolver> solver = solverNew(model, finalTime);

  const int responseIndex = 0;
  const RCP<Epetra_Vector> expectedResponse = vectorNew(*model->get_g_map(responseIndex));
  {
    const EpetraExt::ModelEvaluator::InArgs modelInArgs = createNominalInArgs(*model);
    EpetraExt::ModelEvaluator::OutArgs modelOutArgs = model->createOutArgs();
    modelOutArgs.set_g(responseIndex, expectedResponse);
    model->evalModel(modelInArgs, modelOutArgs);
  }

  const RCP<Epetra_Vector> response = vectorNew(*solver->get_g_map(responseIndex));
  {
    const EpetraExt::ModelEvaluator::InArgs inArgs = createNominalInArgs(*solver);
    EpetraExt::ModelEvaluator::OutArgs outArgs = solver->createOutArgs();
    outArgs.set_g(responseIndex, response);
    solver->evalModel(inArgs, outArgs);
  }

  const Array<double> expected = arrayFromVector(*expectedResponse);
  const Array<double> actual = arrayFromVector(*response);
  TEST_COMPARE_FLOATING_ARRAYS(actual, expected, tol);
}

TEUCHOS_UNIT_TEST(Epetra_RythmosSolver, TimeZero_NominalSolutionSensitivityMv)
{
  const RCP<EpetraExt::ModelEvaluator> model = epetraModelNew();

  const double finalTime = 0.0;
  const RCP<Epetra::RythmosSolver> solver = solverNew(model, finalTime);

  const int parameterIndex = 0;
  const int solutionResponseIndex = solver->Ng() - 1;

  const RCP<Epetra_MultiVector> solutionSensitivity =
    multiVectorNew(*solver->get_g_map(solutionResponseIndex), *solver->get_p_map(parameterIndex));
  {
    const EpetraExt::ModelEvaluator::InArgs inArgs = createNominalInArgs(*solver);

    EpetraExt::ModelEvaluator::OutArgs outArgs = solver->createOutArgs();
    outArgs.set_DgDp(solutionResponseIndex, parameterIndex, solutionSensitivity);
    solver->evalModel(inArgs, outArgs);
  }

  const Array<Array<double> > expected = tuple(
      Array<double>(tuple(0.0, 0.0, 0.0, 0.0)),
      Array<double>(tuple(0.0, 0.0, 0.0, 0.0)));

  TEST_EQUALITY(solutionSensitivity->NumVectors(), expected.size());
  for (int i = 0; i < expected.size(); ++i) {
  TEST_EQUALITY(solutionSensitivity->GlobalLength(), expected[i].size());
    const Array<double> actual = arrayFromVector(*solutionSensitivity, i);
    TEST_COMPARE_FLOATING_ARRAYS(actual, expected[i], tol);
  }
}

TEUCHOS_UNIT_TEST(Epetra_RythmosSolver, TimeZero_NominalResponseSensitivityMvJac)
{
  const RCP<EpetraExt::ModelEvaluator> model = epetraModelNew();

  const double finalTime = 0.0;
  const RCP<Epetra::RythmosSolver> solver = solverNew(model, finalTime);

  const int parameterIndex = 0;
  const int responseIndex = 0;

  const RCP<Epetra_MultiVector> expectedResponseSensitivity =
    multiVectorNew(*model->get_g_map(responseIndex), *model->get_p_map(parameterIndex));
  {
    const EpetraExt::ModelEvaluator::InArgs modelInArgs = createNominalInArgs(*model);

    EpetraExt::ModelEvaluator::OutArgs modelOutArgs = model->createOutArgs();
    modelOutArgs.set_DgDp(responseIndex, parameterIndex, expectedResponseSensitivity);
    model->evalModel(modelInArgs, modelOutArgs);
  }

  const RCP<Epetra_MultiVector> responseSensitivity =
    multiVectorNew(*solver->get_g_map(responseIndex), *solver->get_p_map(parameterIndex));
  {
    const EpetraExt::ModelEvaluator::InArgs inArgs = createNominalInArgs(*solver);

    EpetraExt::ModelEvaluator::OutArgs outArgs = solver->createOutArgs();
    outArgs.set_DgDp(responseIndex, parameterIndex, responseSensitivity);
    solver->evalModel(inArgs, outArgs);
  }

  TEST_EQUALITY(responseSensitivity->NumVectors(), expectedResponseSensitivity->NumVectors());
  for (int i = 0; i < expectedResponseSensitivity->NumVectors(); ++i) {
    const Array<double> expected = arrayFromVector(*expectedResponseSensitivity, i);
    const Array<double> actual = arrayFromVector(*responseSensitivity, i);
    TEST_COMPARE_FLOATING_ARRAYS(actual, expected, tol);
  }
}

TEUCHOS_UNIT_TEST(Epetra_RythmosSolver, TimeZero_NominalResponseSensitivityMvGrad)
{
  const RCP<EpetraExt::ModelEvaluator> model = epetraModelNew();

  const double finalTime = 0.0;
  const RCP<Epetra::RythmosSolver> solver = solverNew(model, finalTime);

  const int parameterIndex = 0;
  const int responseIndex = 0;

  const RCP<Epetra_MultiVector> expectedResponseSensitivity =
    multiVectorNew(*model->get_p_map(parameterIndex), *model->get_g_map(responseIndex));
  {
    const EpetraExt::ModelEvaluator::InArgs modelInArgs = createNominalInArgs(*model);

    const EpetraExt::ModelEvaluator::Derivative deriv_grad(
        expectedResponseSensitivity, EpetraExt::ModelEvaluator::DERIV_TRANS_MV_BY_ROW);
    EpetraExt::ModelEvaluator::OutArgs modelOutArgs = model->createOutArgs();
    modelOutArgs.set_DgDp(responseIndex, parameterIndex, deriv_grad);
    model->evalModel(modelInArgs, modelOutArgs);
  }

  const RCP<Epetra_MultiVector> responseSensitivity =
    multiVectorNew(*solver->get_p_map(parameterIndex), *solver->get_g_map(responseIndex));
  {
    const EpetraExt::ModelEvaluator::InArgs inArgs = createNominalInArgs(*solver);

    const EpetraExt::ModelEvaluator::Derivative deriv_grad(
        responseSensitivity, EpetraExt::ModelEvaluator::DERIV_TRANS_MV_BY_ROW);
    EpetraExt::ModelEvaluator::OutArgs outArgs = solver->createOutArgs();
    outArgs.set_DgDp(responseIndex, parameterIndex, deriv_grad);
    solver->evalModel(inArgs, outArgs);
  }

  TEST_EQUALITY(responseSensitivity->NumVectors(), expectedResponseSensitivity->NumVectors());
  for (int i = 0; i < expectedResponseSensitivity->NumVectors(); ++i) {
    const Array<double> expected = arrayFromVector(*expectedResponseSensitivity, i);
    const Array<double> actual = arrayFromVector(*responseSensitivity, i);
    TEST_COMPARE_FLOATING_ARRAYS(actual, expected, tol);
  }
}
#endif /* Piro_ENABLE_Rythmos */