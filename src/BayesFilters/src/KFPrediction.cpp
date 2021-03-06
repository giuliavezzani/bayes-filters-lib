#include <BayesFilters/KFPrediction.h>

using namespace bfl;
using namespace Eigen;


KFPrediction::KFPrediction(std::unique_ptr<LinearStateModel> state_model) noexcept :
    state_model_(std::move(state_model))
{ }


KFPrediction::KFPrediction(std::unique_ptr<LinearStateModel> state_model, std::unique_ptr<ExogenousModel> exogenous_model) noexcept :
    state_model_(std::move(state_model)),
    exogenous_model_(std::move(exogenous_model))
{ }


KFPrediction::KFPrediction(KFPrediction&& kf_prediction) noexcept:
    state_model_(std::move(kf_prediction.state_model_)),
    exogenous_model_(std::move(kf_prediction.exogenous_model_))
{ }


KFPrediction::~KFPrediction() noexcept
{ }


void KFPrediction::predictStep(const GaussianMixture& prev_state, GaussianMixture& pred_state)
{
    bool skip_exogenous = getSkipExogenous() || (exogenous_model_ == nullptr);
    if (getSkipState() && skip_exogenous)
    {
        /* Skip prediction step entirely. */
        pred_state = prev_state;
        return;
    }

    /* From now on, either (!getSkipState) == true or (!skip_exogenous) == true. */

    if (!getSkipState())
    {
        /* Evaluate predicted mean
           x_{k+1} = F_{k} x_{k}   */
        MatrixXd F = state_model_->getStateTransitionMatrix();
        pred_state.mean().noalias() = F * prev_state.mean();

        /* Evaluate predicted covariance.
           P_{k+1} = F_{k} * P_{k} * F_{k}' + Q */
        for (size_t i=0; i < prev_state.components; i++)
            pred_state.covariance(i).noalias() = F * prev_state.covariance(i) * F.transpose() + state_model_->getNoiseCovarianceMatrix();
    }
    else
    {
        /* Assuming that also the uncertainty due to the noise is neglected if (getSkipState == true). */
        pred_state.covariance() = prev_state.covariance();
    }

    if (!skip_exogenous)
    {
        if (getSkipState())
        {
            exogenous_model_->propagate(prev_state.mean(), pred_state.mean());
        }
        else
        {
            /* Since it is not clear whether ExogenousModel::propagate takes
               into account aliasing or not, then a temporary is used here. */
            MatrixXd tmp(pred_state.mean().rows(), pred_state.mean().cols());

            exogenous_model_->propagate(pred_state.mean(), tmp);

            pred_state.mean() = std::move(tmp);
        }
    }
}
