#include <optimization/MBSGD.h>
#include <Optimizable.h>
#include <optimization/StoppingCriteria.h>
#include <AssertionMacros.h>
#include <Test/Stopwatch.h>
#include <numeric>

namespace OpenANN {

MBSGD::MBSGD(fpt learningRate, fpt learningRateDecay, fpt minimalLearningRate,
             fpt momentum, fpt momentumGain, fpt maximalMomentum,
             int batchSize, fpt minGain, fpt maxGain)
  : debugLogger(Logger::CONSOLE),
    alpha(learningRate), alphaDecay(learningRateDecay),
    minAlpha(minimalLearningRate), eta(momentum), etaGain(momentumGain),
    maxEta(maximalMomentum), batchSize(batchSize), minGain(minGain),
    maxGain(maxGain), useGain(minGain != (fpt) 1.0 || maxGain != (fpt) 1.0),
    iteration(-1)
{
}

MBSGD::~MBSGD()
{
}

void MBSGD::setOptimizable(Optimizable& opt)
{
  this->opt = &opt;
}

void MBSGD::setStopCriteria(const StoppingCriteria& stop)
{
  this->stop = stop;
}

void MBSGD::optimize()
{
  OPENANN_CHECK(opt->providesInitialization());
  while(step())
  {
    if(debugLogger.isActive())
    {
      debugLogger << "Iteration " << iteration << " finished\n"
          << "Error = " << opt->error() << "\n";
    }
  }
}

bool MBSGD::step()
{
  OPENANN_CHECK(opt->providesInitialization());
  if(iteration < 0)
    initialize();

  for(int n = 0; n < N; n++)
    batchAssignment[rng.generateIndex(batches)].push_back(n);
  for(int b = 0; b < batches; b++)
  {
    gradient.fill(0.0);
    for(std::list<int>::const_iterator it = batchAssignment[b].begin();
        it != batchAssignment[b].end(); it++)
      gradient += opt->gradient(*it);
    gradient /= (fpt) batchSize;
    batchAssignment[b].clear();

    if(useGain)
    {
      for(int p = 0; p < P; p++)
      {
        if(momentum(p)*gradient(p) >= (fpt) 0.0)
          gains(p) += 0.05;
        else
          gains(p) *= 0.95;
        gains(p) = std::min<fpt>(maxGain, std::max<fpt>(minGain, gains(p)));
        gradient(p) *= gains(p);
      }
    }

    momentum = eta * momentum - alpha * gradient;
    parameters += momentum;
    opt->setParameters(parameters);

    // Decay alpha, increase momentum
    alpha *= alphaDecay;
    alpha = std::max(alpha, minAlpha);
    eta += etaGain;
    eta = std::min(eta, maxEta);
  }

  iteration++;
  debugLogger << "alpha = " << alpha << ", eta = " << eta << "\n";
  opt->finishedIteration();

  const bool run = (stop.maximalIterations == // Maximum iterations reached?
      StoppingCriteria::defaultValue.maximalFunctionEvaluations ||
      iteration <= stop.maximalIterations) &&
      (stop.minimalSearchSpaceStep == // Gradient too small?
      StoppingCriteria::defaultValue.minimalSearchSpaceStep ||
      momentum.norm() >= stop.minimalSearchSpaceStep);
  if(!run)
    iteration = -1;
  return run;
}

Vt MBSGD::result()
{
  return optimum;
}

std::string MBSGD::name()
{
  return "Mini-Batch Stochastic Gradient Descent";
}

void MBSGD::initialize()
{
  P = opt->dimension();
  N = opt->examples();
  batches = N / batchSize;
  gradient.resize(P);
  gradient.fill(0.0);
  gains.resize(P);
  gains.fill(1.0);
  parameters = opt->currentParameters();
  momentum.resize(P);
  momentum.fill(0.0);
  batchAssignment.resize(batches);
  iteration = 0;
}

}