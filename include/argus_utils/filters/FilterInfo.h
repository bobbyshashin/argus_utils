#pragma once

#include "argus_utils/utils/LinalgTypes.h"

namespace argus
{

/*! \brief Information from a filter predict step used to learn
 * noise models. */
struct PredictInfo
{
	MatrixType Spre; // State covariance before predict step
	double dt; // The predict time step size
	MatrixType F; // Matrix used to propagate covariance
};

/*! \brief Information from a filter update step used to learn 
 * noise models. */
struct UpdateInfo
{
	MatrixType Spre; // State covariance before update
	VectorType innovation; // Observation prediction error
	MatrixType H; // Matrix used to map state covariance to observation
};

}