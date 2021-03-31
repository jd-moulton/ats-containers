/* -*-  mode: c++; indent-tabs-mode: nil -*- */

/*
  Interface for a heat capacity of soil model.

  License: BSD
  Authors: Svetlana Tokareva (tokareva@lanl.gov)
*/

#include "soil_heat_capacity_evaluator.hh"

namespace Amanzi {
namespace SoilThermo {

SoilHeatCapacityEvaluator::SoilHeatCapacityEvaluator(
      Teuchos::ParameterList& plist) :
    SecondaryVariableFieldEvaluator(plist) {
  if (my_key_ == std::string("")) {
    my_key_ = plist_.get<std::string>("soil heat capacity key",
            "surface-heat_capacity");
  }

  Key domain = Keys::getDomain(my_key_);

  // Set up my dependencies.
  std::string domain_name = Keys::getDomain(my_key_);

//  // -- temperature
//  temperature_key_ = Keys::readKey(plist_, domain_name, "temperature", "temperature");
//  dependencies_.insert(temperature_key_);

//  // -- water content
//  water_content_key_ = Keys::readKey(plist_, domain_name, "soil water content", "soil_water_content");
//  dependencies_.insert(water_content_key_);

//  // -- ice content
//  ice_content_key_ = Keys::readKey(plist_, domain_name, "soil ice content", "soil_ice_content");
//  dependencies_.insert(ice_content_key_);

//  AMANZI_ASSERT(plist_.isSublist("soil heat capacity parameters"));
//  Teuchos::ParameterList sublist = plist_.sublist("soil heat capacity parameters");

  double row  = 1000.; // density of water
  double roi  = 917.;  // density of ice

  cw    = 3990./row;    // specific heat of water
  ci    = 2150./roi;    // specific heat of ice
  cg    = 800.;         // specific heat of dry soils

}


SoilHeatCapacityEvaluator::SoilHeatCapacityEvaluator(
      const SoilHeatCapacityEvaluator& other) :
    SecondaryVariableFieldEvaluator(other),
    cg(other.cg),
    cw(other.cw),
    ci(other.ci),
    water_content_key_(other.water_content_key_),
    ice_content_key_(other.ice_content_key_){}


Teuchos::RCP<FieldEvaluator>
SoilHeatCapacityEvaluator::Clone() const {
  return Teuchos::rcp(new SoilHeatCapacityEvaluator(*this));
}

void SoilHeatCapacityEvaluator::EvaluateField_(
      const Teuchos::Ptr<State>& S,
      const Teuchos::Ptr<CompositeVector>& result) {

//  // get water content
//  Teuchos::RCP<const CompositeVector> wc = S->GetFieldData(water_content_key_);
//
//  // get ice content
//  Teuchos::RCP<const CompositeVector> ic = S->GetFieldData(ice_content_key_);

  // get mesh
  Teuchos::RCP<const AmanziMesh::Mesh> mesh = result->Mesh();

  for (CompositeVector::name_iterator comp=result->begin();
         comp!=result->end(); ++comp) {
      // much more efficient to pull out vectors first
//      const Epetra_MultiVector& wc_v = *wc->ViewComponent(*comp,false);
//      const Epetra_MultiVector& ic_v = *ic->ViewComponent(*comp,false);
      Epetra_MultiVector& result_v = *result->ViewComponent(*comp,false);

      int ncomp = result->size(*comp, false);

      for (int i=0; i!=ncomp; ++i) {

//          double W = wc_v[0][i];
//          double I = ic_v[0][i];

          result_v[0][i] = cg; // + cw*W + ci*I;

      } // i
    }

}


void SoilHeatCapacityEvaluator::EvaluateFieldPartialDerivative_(
      const Teuchos::Ptr<State>& S, Key wrt_key,
      const Teuchos::Ptr<CompositeVector>& result) {
  std::cout<<"SOIL HEAT CAPACITY: Derivative not implemented yet!"<<wrt_key<<"\n";
  AMANZI_ASSERT(0); // not implemented, not yet needed
  result->Scale(1.e-6); // convert to MJ
}

} //namespace
} //namespace
